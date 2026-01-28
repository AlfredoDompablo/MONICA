import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';
import { readingUpdateSchema } from '@/lib/schemas';
import { z } from 'zod';

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

        // Validate that id is a number
        if (isNaN(id)) {
            return NextResponse.json({ error: 'Invalid ID' }, { status: 400 });
        }

        // Extract fields relative to sensor readings
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
            { error: 'Error updating reading' },
            { status: 500 }
        );
    }
}

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
            return NextResponse.json({ error: 'Invalid ID' }, { status: 400 });
        }

        await prisma.sensorReading.delete({
            where: { reading_id: id },
        });

        return NextResponse.json({ message: 'Reading deleted successfully' });
    } catch (error) {
        console.error('Error deleting reading:', error);
        return NextResponse.json(
            { error: 'Error deleting reading' },
            { status: 500 }
        );
    }
}
