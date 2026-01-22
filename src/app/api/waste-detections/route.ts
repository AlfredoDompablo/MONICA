import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';

// GET /api/waste-detections - Listar detecciones (sin datos binarios pesados)
export async function GET(request: NextRequest) {
    try {
        const searchParams = request.nextUrl.searchParams;
        const node_id = searchParams.get('node_id');
        const limit = parseInt(searchParams.get('limit') || '20');

        const where: any = {};
        if (node_id) where.node_id = node_id;

        // No incluimos image_data por defecto para no sobrecargar la respuesta
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
                // image_data: false // Excluido explícitamente
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

// POST /api/waste-detections - Registrar nueva detección
export async function POST(request: Request) {
    try {
        const body = await request.json();
        const {
            node_id,
            coverage_percent,
            image_data_base64, // Esperamos base64
            model_version,
            confidence
        } = body;

        if (!node_id) {
            return NextResponse.json(
                { error: 'El ID del nodo es obligatorio' },
                { status: 400 }
            );
        }

        let imageBuffer: Buffer | undefined;
        if (image_data_base64) {
            try {
                // Convertir base64 a Buffer
                imageBuffer = Buffer.from(image_data_base64, 'base64');
            } catch (e) {
                console.error('Error converting base64 image:', e);
                return NextResponse.json({ error: 'Formato de imagen inválido' }, { status: 400 });
            }
        }

        const newDetection = await prisma.wasteDetection.create({
            data: {
                node_id,
                coverage_percent,
                image_data: imageBuffer as any,
                model_version,
                confidence,
                timestamp: new Date(),
            },
            // Seleccionamos campos de retorno para evitar devolver la imagen binaria en la respuesta
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
