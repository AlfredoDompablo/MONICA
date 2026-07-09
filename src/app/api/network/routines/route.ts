import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

/**
 * GET /api/network/routines
 * 
 * Retorna las rutinas de monitoreo configuradas, incluyendo sus pasos y ejecuciones recientes.
 */
export async function GET() {
    try {
        const routines = await prisma.routine.findMany({
            include: {
                steps: {
                    orderBy: { order: 'asc' }
                },
                executions: {
                    orderBy: { started_at: 'desc' },
                    take: 10,
                    include: {
                        steps: {
                            orderBy: { exec_step_id: 'asc' }
                        }
                    }
                }
            },
            orderBy: { created_at: 'desc' }
        });

        return NextResponse.json(routines);
    } catch (error) {
        console.error('Error fetching routines:', error);
        return NextResponse.json(
            { error: 'Error al obtener las rutinas' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/network/routines
 * 
 * Crea una nueva rutina con su respectiva lista de pasos.
 */
export async function POST(request: Request) {
    try {
        const body = await request.json();
        const { name, description, days_of_week, hour, minute, steps } = body;

        if (!name || days_of_week === undefined || hour === undefined || minute === undefined) {
            return NextResponse.json(
                { error: 'name, days_of_week, hour y minute son requeridos' },
                { status: 400 }
            );
        }

        if (!Array.isArray(steps) || steps.length === 0) {
            return NextResponse.json(
                { error: 'Debe proporcionar al menos un paso para la rutina' },
                { status: 400 }
            );
        }

        // Crear la rutina y sus pasos en una transacción
        const newRoutine = await prisma.$transaction(async (tx) => {
            const routine = await tx.routine.create({
                data: {
                    name,
                    description: description || null,
                    days_of_week: String(days_of_week),
                    hour: parseInt(hour),
                    minute: parseInt(minute),
                    is_active: true,
                }
            });

            const stepPromises = steps.map((step: any, index: number) => {
                return tx.routineStep.create({
                    data: {
                        routine_id: routine.routine_id,
                        type: step.type,
                        target_node_id: step.target_node_id,
                        order: index + 1
                    }
                });
            });

            await Promise.all(stepPromises);

            return tx.routine.findUnique({
                where: { routine_id: routine.routine_id },
                include: {
                    steps: {
                        orderBy: { order: 'asc' }
                    }
                }
            });
        });

        return NextResponse.json(newRoutine, { status: 201 });
    } catch (error) {
        console.error('Error creating routine:', error);
        return NextResponse.json(
            { error: 'Error al crear la rutina' },
            { status: 500 }
        );
    }
}
