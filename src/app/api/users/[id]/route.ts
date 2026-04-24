import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

export const dynamic = 'force-dynamic';

/**
 * GET /api/users/{id}
 * 
 * Obtiene los detalles de un usuario específico, incluyendo sus nodos asignados.
 * 
 * @param {Request} request - La petición HTTP.
 * @param {Object} context - Contexto de la ruta.
 * @param {Promise<{ id: string }>} context.params - ID del usuario.
 * @returns {Promise<NextResponse>} Detalles del usuario.
 */
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
            where: { user_id: userId }
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

/**
 * PUT /api/users/{id}
 * 
 * Actualiza la información de un usuario.
 * No permite modificar el ID del usuario.
 * 
 * @param {Request} request - Datos a actualizar.
 * @param {Object} context - Contexto de la ruta.
 * @param {Promise<{ id: string }>} context.params - ID del usuario.
 * @returns {Promise<NextResponse>} El usuario actualizado.
 */
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

/**
 * DELETE /api/users/{id}
 * 
 * Realiza una eliminación lógica del usuario (lo marca como inactivo).
 * 
 * @param {Request} request - La petición HTTP.
 * @param {Object} context - Contexto de la ruta.
 * @param {Promise<{ id: string }>} context.params - ID del usuario a desactivar.
 * @returns {Promise<NextResponse>} Mensaje de confirmación.
 */
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

        // Eliminación física (hard delete)
        const deletedUser = await prisma.user.delete({
            where: { user_id: userId },
        });

        return NextResponse.json({ message: 'Usuario eliminado', user: deletedUser });
    } catch (error: any) {
        console.error('Error deleting user:', error);
        if (error.code === 'P2025') {
            return NextResponse.json({ error: 'Usuario no encontrado' }, { status: 404 });
        }
        return NextResponse.json({ error: 'Error al eliminar usuario', details: error.message }, { status: 500 });
    }
}
