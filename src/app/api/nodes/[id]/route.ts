import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

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

export async function PUT(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const nodeId = id;
        const body = await request.json();

        // Evitar que actualicen el ID
        delete body.node_id;

        const updatedNode = await prisma.node.update({
            where: { node_id: nodeId },
            data: body,
        });

        return NextResponse.json(updatedNode);
    } catch (error) {
        console.error('Error updating node:', error);
        if ((error as any).code === 'P2025') {
            return NextResponse.json({ error: 'Nodo no encontrado' }, { status: 404 });
        }
        return NextResponse.json({ error: 'Error al actualizar nodo' }, { status: 500 });
    }
}

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
