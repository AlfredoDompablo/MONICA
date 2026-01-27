import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';
import { nodeSchema } from '@/lib/schemas';
import { z } from 'zod';

/**
 * GET /api/nodes/{id}
 * 
 * Obtiene los detalles de un nodo específico por su ID.
 * 
 * @param {Request} request - La petición HTTP.
 * @param {Object} context - Contexto de la ruta.
 * @param {Promise<{ id: string }>} context.params - Parámetros de la URL (id del nodo).
 * @returns {Promise<NextResponse>} Detalles del nodo o error 404.
 */
export async function GET(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const nodeId = id; // node_id es string

        const node = await prisma.node.findUnique({
            where: { node_id: nodeId },
            include: {
                user: true, // Incluir información del usuario asignado
            },
        });

        if (!node) {
            return NextResponse.json({ error: 'Nodo no encontrado' }, { status: 404 });
        }

        return NextResponse.json(node);
    } catch (error) {
        console.error('Error fetching node:', error);
        return NextResponse.json({ error: 'Error al obtener nodo' }, { status: 500 });
    }
}

/**
 * PUT /api/nodes/{id}
 * 
 * Actualiza la información de un nodo existente.
 * No permite modificar el ID del nodo.
 * 
 * @param {Request} request - La petición HTTP con los datos a actualizar.
 * @param {Object} context - Contexto de la ruta.
 * @param {Promise<{ id: string }>} context.params - ID del nodo a actualizar.
 * @returns {Promise<NextResponse>} El nodo actualizado.
 */
export async function PUT(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const nodeId = id;
        const body = await request.json();

        // Evitar que actualicen el ID
        // Validate with Zod (partial because we might not update all fields, 
        // though the modal sends all. Let's use partial just in case for flexibility)
        // Actually, for a full PUT we often expect all, but let's allow partial updates for flexibility
        // But wait, the schema has ID which we shouldn't update.
        // Let's omit ID from the schema validation for updates
        const updateSchema = nodeSchema.omit({ node_id: true }).partial();

        const { description, latitude, longitude } = updateSchema.parse(body);

        const updatedNode = await prisma.node.update({
            where: { node_id: nodeId },
            data: {
                description,
                latitude,
                longitude,
            },
        });

        return NextResponse.json(updatedNode);
    } catch (error) {
        if (error instanceof z.ZodError) {
            return NextResponse.json({ error: error.issues }, { status: 400 });
        }
        console.error('Error updating node:', error);
        if ((error as any).code === 'P2025') {
            return NextResponse.json({ error: 'Nodo no encontrado' }, { status: 404 });
        }
        return NextResponse.json({ error: 'Error al actualizar nodo' }, { status: 500 });
    }
}

/**
 * DELETE /api/nodes/{id}
 * 
 * Elimina un nodo del sistema.
 * Puede fallar si existen registros asociados (lecturas) que dependen de este nodo.
 * 
 * @param {Request} request - La petición HTTP.
 * @param {Object} context - Contexto de la ruta.
 * @param {Promise<{ id: string }>} context.params - ID del nodo a eliminar.
 * @returns {Promise<NextResponse>} Mensaje de confirmación.
 */
export async function DELETE(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const nodeId = id;

        const deletedNode = await prisma.node.delete({
            where: { node_id: nodeId },
        });

        return NextResponse.json({ message: 'Nodo eliminado', node: deletedNode });
    } catch (error) {
        console.error('Error deleting node:', error);
        if ((error as any).code === 'P2025') {
            return NextResponse.json({ error: 'Nodo no encontrado' }, { status: 404 });
        }
        // Error de clave foránea si hay lecturas asociadas (depende de la configuración de cascada en BD, Prisma relation attributes)
        if ((error as any).code === 'P2003') {
            return NextResponse.json({ error: 'No se puede eliminar el nodo porque tiene registros asociados' }, { status: 409 });
        }
        return NextResponse.json({ error: 'Error al eliminar nodo' }, { status: 500 });
    }
}
