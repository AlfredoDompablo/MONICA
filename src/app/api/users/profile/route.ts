import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';
import bcrypt from 'bcryptjs';

/**
 * PUT /api/users/profile
 * 
 * Permite al usuario autenticado actualizar su propio perfil (nombre, contraseña).
 */
export async function PUT(request: Request) {
    const session = await getServerSession(authOptions);

    if (!session || !session.user?.id) {
        return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
    }

    try {
        const body = await request.json();
        const { full_name, current_password, new_password } = body;
        
        const userId = parseInt(session.user.id);

        const user = await prisma.user.findUnique({ where: { user_id: userId } });
        if (!user) {
            return NextResponse.json({ error: 'Usuario no encontrado' }, { status: 404 });
        }

        const dataToUpdate: any = {};

        if (full_name && full_name !== user.full_name) {
            dataToUpdate.full_name = full_name;
        }

        if (current_password && new_password) {
            const isPasswordValid = await bcrypt.compare(current_password, user.password_hash);
            if (!isPasswordValid) {
                return NextResponse.json({ error: 'La contraseña actual es incorrecta' }, { status: 400 });
            }
            dataToUpdate.password_hash = await bcrypt.hash(new_password, 10);
        }

        if (Object.keys(dataToUpdate).length === 0) {
            return NextResponse.json({ message: 'No hay datos nuevos para actualizar' });
        }

        await prisma.user.update({
            where: { user_id: userId },
            data: dataToUpdate
        });

        return NextResponse.json({ message: 'Perfil actualizado exitosamente' });
    } catch (error) {
        console.error('Error updating profile:', error);
        return NextResponse.json({ error: 'Error al actualizar el perfil' }, { status: 500 });
    }
}
