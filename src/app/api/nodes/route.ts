import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

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
        const { node_id, description, latitude, longitude, user_id } = body;

        if (!node_id || !description || latitude === undefined || longitude === undefined) {
            return NextResponse.json(
                { error: 'Faltan campos obligatorios' },
                { status: 400 }
            );
        }

        const newNode = await prisma.node.create({
            data: {
                node_id,
                description,
                latitude,
                longitude,
                user_id: user_id || null,
                last_seen: new Date(),
            },
        });

        return NextResponse.json(newNode, { status: 201 });
    } catch (error) {
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
