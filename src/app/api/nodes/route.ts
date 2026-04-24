import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';
import { nodeSchema } from '@/lib/schemas';
import { z } from 'zod';

/**
 * GET /api/nodes
 * 
 * Recupera todos los nodos de monitoreo registrados.
 * 
 * @returns {Promise<NextResponse>} Lista de nodos ordenados por ID.
 */
export async function GET() {
    try {
        const nodes = await prisma.node.findMany({
            orderBy: { node_id: 'asc' },
            include: {
                sensor_readings: {
                    take: 1,
                    orderBy: { timestamp: 'desc' }
                }
            }
        });

        return NextResponse.json(nodes);
    } catch (error) {
        console.error('Error fetching nodes:', error);
        return NextResponse.json(
            { error: 'Error al obtener nodos' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/nodes
 * 
 * Registra un nuevo nodo en el sistema.
 * 
 * @param {Request} request - Datos del nodo (id, descripción, ubicación).
 * @returns {Promise<NextResponse>} El nodo creado o error si falla la validación.
 */
export async function POST(request: Request) {
    try {
        const body = await request.json();

        // Validar con Zod
        const { node_id, description, latitude, longitude } = nodeSchema.parse(body);

        const existingNode = await prisma.node.findUnique({
            where: { node_id },
        });

        if (existingNode) {
            return NextResponse.json(
                { error: 'ID de nodo ya existente' },
                { status: 400 }
            );
        }

        const newNode = await prisma.node.create({
            data: {
                node_id,
                description,
                latitude,
                longitude,
                last_seen: new Date(), // Inicializar last_seen
            },
        });

        return NextResponse.json(newNode, { status: 201 });
    } catch (error) {
        if (error instanceof z.ZodError) {
            return NextResponse.json({ error: error.issues }, { status: 400 });
        }
        console.error('Error creating node:', error);
        if (String(error).includes('Unique constraint')) {
            return NextResponse.json(
                { error: 'El ID del nodo ya existe' },
                { status: 409 }
            );
        }
        return NextResponse.json(
            { error: 'Error al crear nodo' },
            { status: 500 }
        );
    }
}
