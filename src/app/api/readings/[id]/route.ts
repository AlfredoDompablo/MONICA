import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';
import { readingUpdateSchema } from '@/lib/schemas';
import { z } from 'zod';

/**
 * PUT /api/readings/{id}
 * 
 * Actualiza una lectura específica.
 * Requiere autenticación de administrador.
 * 
 * @param {Request} request - Cuerpo con datos a actualizar (ph, temp, etc).
 * @param {Object} context - Contexto con ID de lectura.
 * @returns {Promise<NextResponse>} Lectura actualizada.
 */
export async function PUT(
    request: Request,
    { params }: { params: { id: string } }
) {
    const session = await getServerSession(authOptions);

    if (!session) {
        return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
    }

    try {
        const id = parseInt(params.id);
        const body = await request.json();

        // Validar que el ID sea numérico
        if (isNaN(id)) {
            return NextResponse.json({ error: 'ID inválido' }, { status: 400 });
        }

        // Extraer campos relativos a lecturas de sensores
        const {
            ph,
            dissolved_oxygen,
            turbidity,
            conductivity,
            temperature,
            battery_level
        } = readingUpdateSchema.parse(body);

        const updatedReading = await prisma.sensorReading.update({
            where: { reading_id: id },
            data: {
                ph,
                dissolved_oxygen,
                turbidity,
                conductivity,
                temperature,
                battery_level,
            },
        });

        return NextResponse.json(updatedReading);
    } catch (error) {
        if (error instanceof z.ZodError) {
            return NextResponse.json({ error: error.issues }, { status: 400 });
        }
        console.error('Error updating reading:', error);
        return NextResponse.json(
            { error: 'Error actualizando lectura' },
            { status: 500 }
        );
    }
}

/**
 * DELETE /api/readings/{id}
 * 
 * Elimina permanentemente una lectura.
 * Requiere autenticación de administrador.
 * 
 * @param {Request} request - Petición HTTP.
 * @param {Object} context - Contexto con ID de lectura.
 * @returns {Promise<NextResponse>} Mensaje de éxito.
 */
export async function DELETE(
    request: Request,
    { params }: { params: { id: string } }
) {
    const session = await getServerSession(authOptions);

    if (!session) {
        return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
    }

    try {
        const id = parseInt(params.id);

        if (isNaN(id)) {
            return NextResponse.json({ error: 'ID inválido' }, { status: 400 });
        }

        await prisma.sensorReading.delete({
            where: { reading_id: id },
        });

        return NextResponse.json({ message: 'Lectura eliminada exitosamente' });
    } catch (error) {
        console.error('Error deleting reading:', error);
        return NextResponse.json(
            { error: 'Error eliminando lectura' },
            { status: 500 }
        );
    }
}
