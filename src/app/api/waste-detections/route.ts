import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';
import { uploadFile } from '@/lib/s3';
import { v4 as uuidv4 } from 'uuid';
import crypto from 'crypto';

/**
 * GET /api/waste-detections
 * 
 * Lista las detecciones de residuos registradas.
 * No incluye los datos binarios de la imagen para optimizar la transferencia.
 * 
 * @param {NextRequest} request - Filtros opcionales (node_id, limit).
 * @returns {Promise<NextResponse>} Lista de detecciones (metadatos).
 */
export async function GET(request: NextRequest) {
    try {
        const searchParams = request.nextUrl.searchParams;
        const node_id = searchParams.get('node_id');
        const limit = parseInt(searchParams.get('limit') || '20');

        const where: any = {};
        if (node_id) where.node_id = node_id;

        // No incluimos datos binarios por defecto para no sobrecargar la respuesta
        const detections = await prisma.wasteDetection.findMany({
            where,
            orderBy: { timestamp: 'desc' },
            take: limit,
            select: {
                detection_id: true,
                node_id: true,
                timestamp: true,
                coverage_percent: true,
                model_version: true,
                confidence: true,
                // image_original: false, 
                // image_masked: false
            }
        });

        return NextResponse.json(detections);
    } catch (error) {
        console.error('Error fetching waste detections:', error);
        return NextResponse.json(
            { error: 'Error al obtener detecciones' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/waste-detections
 * 
 * Registra una nueva detección enviando la imagen al microservicio de IA.
 * 
 * @param {Request} request - Imagen en base64 y metadatos del nodo.
 */
export async function POST(request: Request) {
    try {
        // 1. Validar Hardware Auth (API Key)
        const apiKey = request.headers.get('x-api-key');
        if (!apiKey) {
            return NextResponse.json({ error: 'Missing API Key' }, { status: 401 });
        }

        // Calcular hash SHA-256
        const hash = crypto.createHash('sha256').update(apiKey).digest('hex');

        // Buscar nodo activo con ese hash
        const node = await prisma.node.findUnique({
            where: { key_hash: hash }
        });

        if (!node || !node.is_active) {
            return NextResponse.json({ error: 'Invalid or inactive API Key' }, { status: 401 });
        }

        const body = await request.json();
        let {
            node_id,
            image_original_base64,
            model_version // Opcional, el AI Service devolverá el real
        } = body;

        // Si el nodo autenticado es el Concentrador (Master Gateway), preservamos el node_id del payload
        if (node.node_id === 'NODE_C') {
            if (!node_id) {
                node_id = node.node_id;
            } else {
                // Validar que el nodo destino exista y esté activo
                const targetNode = await prisma.node.findUnique({
                    where: { node_id }
                });
                if (!targetNode || !targetNode.is_active) {
                    return NextResponse.json({ error: 'Target node is invalid or inactive' }, { status: 400 });
                }
            }
        } else {
            // Si es un nodo normal, forzar su propio node_id (previene spoofing)
            node_id = node.node_id;
        }

        if (!node_id || !image_original_base64) {
            return NextResponse.json(
                { error: 'El ID del nodo y la imagen base64 son obligatorios' },
                { status: 400 }
            );
        }

        // 1. Enviar imagen al Microservicio de IA (Python/FastAPI)
        const aiServiceUrl = process.env.AI_SERVICE_URL || 'http://localhost:8000';
        
        // Convertir base64 a Blob para el FormData
        const buffer = Buffer.from(image_original_base64, 'base64');
        const formData = new FormData();
        formData.append('file', new Blob([buffer]), 'image.jpg');
        formData.append('node_id', node_id);

        const aiResponse = await fetch(`${aiServiceUrl}/process`, {
            method: 'POST',
            body: formData,
        });

        if (!aiResponse.ok) {
            const errorText = await aiResponse.text();
            throw new Error(`AI Service Error: ${errorText}`);
        }

        const aiResult = await aiResponse.json();

        // 2. Guardar el resultado en la base de datos PostgreSQL
        const newDetection = await prisma.wasteDetection.create({
            data: {
                node_id: aiResult.node_id,
                coverage_percent: aiResult.coverage_percent,
                model_version: aiResult.model_version,
                confidence: aiResult.confidence,
                timestamp: new Date(),
                image_original: aiResult.image_original,
                image_masked: aiResult.image_masked,
            },
            select: {
                detection_id: true,
                node_id: true,
                timestamp: true,
                coverage_percent: true,
                confidence: true,
            }
        });

        return NextResponse.json(newDetection, { status: 201 });
    } catch (error) {
        console.error('Error in detection flow:', error);
        return NextResponse.json(
            { error: 'Error al procesar la detección con IA' },
            { status: 500 }
        );
    }
}

