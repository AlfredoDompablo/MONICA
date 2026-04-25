import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';
import { uploadFile } from '@/lib/s3';
import { v4 as uuidv4 } from 'uuid';

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
 * Registra una nueva detección de residuos.
 * Sube las imágenes a MinIO y guarda las referencias en la base de datos.
 * 
 * @param {Request} request - Metadatos de la detección e imágenes en base64.
 * @returns {Promise<NextResponse>} La detección creada.
 */
export async function POST(request: Request) {
    try {
        const body = await request.json();
        const {
            node_id,
            coverage_percent,
            image_original_base64,
            image_masked_base64,
            model_version,
            confidence
        } = body;

        if (!node_id) {
            return NextResponse.json(
                { error: 'El ID del nodo es obligatorio' },
                { status: 400 }
            );
        }

        const timestamp = new Date();
        const dateStr = timestamp.toISOString().split('T')[0]; // YYYY-MM-DD
        
        let originalKey = null;
        let maskedKey = null;

        // Subir imagen original a MinIO
        if (image_original_base64) {
            const buffer = Buffer.from(image_original_base64, 'base64');
            const fileName = `${dateStr}/${node_id}_${uuidv4()}_orig.jpg`;
            originalKey = await uploadFile(buffer, fileName);
        }
        
        // Subir imagen con máscara a MinIO
        if (image_masked_base64) {
            const buffer = Buffer.from(image_masked_base64, 'base64');
            const fileName = `${dateStr}/${node_id}_${uuidv4()}_mask.jpg`;
            maskedKey = await uploadFile(buffer, fileName);
        }

        const newDetection = await prisma.wasteDetection.create({
            data: {
                node_id,
                coverage_percent,
                model_version,
                confidence,
                timestamp,
                image_original: originalKey,
                image_masked: maskedKey,
            },
            select: {
                detection_id: true,
                node_id: true,
                timestamp: true,
                coverage_percent: true,
                model_version: true,
                confidence: true,
            }
        });

        return NextResponse.json(newDetection, { status: 201 });
    } catch (error) {
        console.error('Error creating waste detection:', error);
        if ((error as any).code === 'P2003') {
            return NextResponse.json({ error: 'El nodo especificado no existe' }, { status: 400 });
        }
        return NextResponse.json(
            { error: 'Error al registrar detección' },
            { status: 500 }
        );
    }
}
