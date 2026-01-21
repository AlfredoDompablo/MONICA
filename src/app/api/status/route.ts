import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

export async function GET() {
    try {
        // Intentamos hacer una consulta simple para verificar conexión
        const userCount = await prisma.user.count();

        return NextResponse.json({
            status: 'ok',
            message: 'Conexión a base de datos exitosa',
            userCount
        }, { status: 200 });
    } catch (error) {
        console.error('Error de conexión a BD:', error);
        return NextResponse.json({
            status: 'error',
            message: 'Error conectando a la base de datos',
            error: String(error)
        }, { status: 500 });
    }
}
