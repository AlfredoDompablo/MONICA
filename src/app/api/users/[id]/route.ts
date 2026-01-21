import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

export async function GET(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const userId = parseInt(id);

        if (isNaN(userId)) {
            return NextResponse.json({ error: 'ID inválido' }, { status: 400 });
        }

        const user = await prisma.user.findUnique({
            where: { user_id: userId },
            include: { nodes: true }, // Incluir nodos asociados
        });

        if (!user) {
            return NextResponse.json({ error: 'Usuario no encontrado' }, { status: 404 });
        }

        return NextResponse.json(user);
    } catch (error) {
        console.error('Error fetching user:', error);
        return NextResponse.json({ error: 'Error al obtener usuario' }, { status: 500 });
    }
}

export async function PUT(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const userId = parseInt(id);
        const body = await request.json();

        if (isNaN(userId)) {
            return NextResponse.json({ error: 'ID inválido' }, { status: 400 });
        }

        // Evitar que actualicen el ID
        delete body.user_id;

        const updatedUser = await prisma.user.update({
            where: { user_id: userId },
            data: body,
        });

        return NextResponse.json(updatedUser);
    } catch (error) {
        console.error('Error updating user:', error);
        if ((error as any).code === 'P2025') {
            return NextResponse.json({ error: 'Usuario no encontrado' }, { status: 404 });
        }
        return NextResponse.json({ error: 'Error al actualizar usuario' }, { status: 500 });
    }
}

export async function DELETE(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const userId = parseInt(id);

        if (isNaN(userId)) {
            return NextResponse.json({ error: 'ID inválido' }, { status: 400 });
        }

        // Eliminación lógica (soft delete) cambiando is_active
        const deletedUser = await prisma.user.update({
            where: { user_id: userId },
            data: { is_active: false },
        });

        return NextResponse.json({ message: 'Usuario desactivado', user: deletedUser });
    } catch (error) {
        console.error('Error deleting user:', error);
        if ((error as any).code === 'P2025') {
            return NextResponse.json({ error: 'Usuario no encontrado' }, { status: 404 });
        }
        return NextResponse.json({ error: 'Error al eliminar usuario' }, { status: 500 });
    }
}
