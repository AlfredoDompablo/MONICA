import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';

/**
 * GET /api/sensor-readings
 * 
 * Obtiene el historial de lecturas de sensores.
 * Permite filtrar por nodo, rango de fechas y limitar resultados.
 * 
 * @param {NextRequest} request - Incluye parámetros de búsqueda (node_id, limit, startDate, endDate).
 * @returns {Promise<NextResponse>} Lista de lecturas.
 */
export async function GET(request: NextRequest) {
    try {
        const searchParams = request.nextUrl.searchParams;
        const node_id = searchParams.get('node_id');
        const limit = parseInt(searchParams.get('limit') || '50');
        const startDate = searchParams.get('startDate');
        const endDate = searchParams.get('endDate');

        const where: any = {};
        if (node_id) where.node_id = node_id;
        if (startDate || endDate) {
            where.timestamp = {};
            if (startDate) where.timestamp.gte = new Date(startDate);
            if (endDate) where.timestamp.lte = new Date(endDate);
        }

        const readings = await prisma.sensorReading.findMany({
            where,
            orderBy: { timestamp: 'desc' },
            take: limit,
            include: {
                node: {
                    select: { description: true } // Incluir descripción del nodo para contexto
                }
            }
        });

        return NextResponse.json(readings);
    } catch (error) {
        console.error('Error fetching sensor readings:', error);
        return NextResponse.json(
            { error: 'Error al obtener lecturas' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/sensor-readings
 * 
 * Registra una nueva lectura de sensores para un nodo específico.
 * Actualiza automáticamente la fecha de 'última vista' del nodo.
 * 
 * @param {Request} request - Datos de los sensores (ph, temperatura, etc.).
 * @returns {Promise<NextResponse>} La lectura creada.
 */
export async function POST(request: Request) {
    try {
        const body = await request.json();
        const {
            node_id,
            ph,
            dissolved_oxygen,
            turbidity,
            connectivity,
            temperature,
            battery_level
        } = body;

        if (!node_id) {
            return NextResponse.json(
                { error: 'El ID del nodo es obligatorio' },
                { status: 400 }
            );
        }

        const newReading = await prisma.sensorReading.create({
            data: {
                node_id,
                ph,
                dissolved_oxygen,
                turbidity,
                connectivity,
                temperature,
                battery_level,
                timestamp: new Date(), // Usar hora del servidor
            },
        });

        // Actualizar last_seen del nodo y batería si aplica
        await prisma.node.update({
            where: { node_id },
            data: { last_seen: new Date() }
        }).catch((err: any) => console.error('Error updating node last_seen:', err));

        return NextResponse.json(newReading, { status: 201 });
    } catch (error) {
        console.error('Error creating sensor reading:', error);
        if ((error as any).code === 'P2003') { // Foreign key constraint failed
            return NextResponse.json({ error: 'El nodo especificado no existe' }, { status: 400 });
        }
        return NextResponse.json(
            { error: 'Error al registrar lectura' },
            { status: 500 }
        );
    }
}
