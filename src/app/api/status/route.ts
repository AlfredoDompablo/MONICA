import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

/**
 * GET /api/status
 * 
 * Endpoint de monitoreo de salud (Health Check).
 * Verifica la conectividad con la base de datos realizando una consulta ligera.
 * 
 * @returns {Promise<NextResponse>} Estado de la conexión y conteo de usuarios.
 */
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
