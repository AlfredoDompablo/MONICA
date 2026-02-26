import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';

/**
 * GET /api/readings
 * 
 * Endpoint público para obtener lecturas de sensores.
 * Utilizado por gráficos y tablas públicas.
 * Soporta paginación y filtrado.
 * 
 * @param {Request} request - Url con query params (node_id, start_date, end_date, page, limit).
 * @returns {Promise<NextResponse>} Datos paginados de lecturas.
 */
export async function GET(request: Request) {
    // Endpoint público para gráficos
    // const session = await getServerSession(authOptions);
    // if (!session) { return NextResponse.json({ error: 'Unauthorized' }, { status: 401 }); }

    const { searchParams } = new URL(request.url);
    const nodeId = searchParams.get('node_id');
    const startDate = searchParams.get('start_date');
    const endDate = searchParams.get('end_date');
    const page = parseInt(searchParams.get('page') || '1');
    const limit = parseInt(searchParams.get('limit') || '50');
    const skip = (page - 1) * limit;

    // Construir cláusula where dinámicamente
    const where: any = {};

    if (nodeId) {
        where.node_id = nodeId;
    }

    if (startDate || endDate) {
        where.timestamp = {};
        if (startDate) {
            where.timestamp.gte = new Date(startDate);
        }
        if (endDate) {
            // Establecer fecha final. Se asume formato ISO o fecha simple YYYY-MM-DD
            const end = new Date(endDate);
            // Usar 'lte' (menor o igual)
            where.timestamp.lte = end;
        }
    }

    try {
        const [readings, total] = await Promise.all([
            prisma.sensorReading.findMany({
                where,
                orderBy: {
                    timestamp: 'desc',
                },
                take: limit,
                skip: skip,
                include: {
                    node: {    // Incluir detalles básicos del nodo (descripción)
                        select: {
                            description: true
                        }
                    }
                }
            }),
            prisma.sensorReading.count({ where }),
        ]);

        return NextResponse.json({
            data: readings,
            pagination: {
                total,
                page,
                limit,
                totalPages: Math.ceil(total / limit),
            },
        });
    } catch (error) {
        console.error('Error fetching sensor readings:', error);
        return NextResponse.json(
            { error: 'Error fetching readings' },
            { status: 500 }
        );
    }
}
