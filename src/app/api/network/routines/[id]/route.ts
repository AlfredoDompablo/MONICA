import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { startRoutineExecution } from '@/lib/routineScheduler';

/**
 * DELETE /api/network/routines/[id]
 * 
 * Elimina una rutina programada. Los pasos y ejecuciones se borran en cascada.
 */
export async function DELETE(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const routineId = parseInt(id);

        if (isNaN(routineId)) {
            return NextResponse.json({ error: 'ID de rutina inválido' }, { status: 400 });
        }

        await prisma.routine.delete({
            where: { routine_id: routineId }
        });

        return NextResponse.json({ success: true, message: 'Rutina eliminada con éxito' });
    } catch (error) {
        console.error('Error deleting routine:', error);
        return NextResponse.json(
            { error: 'Error al eliminar la rutina' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/network/routines/[id]
 * 
 * Ejecuta manualmente una rutina de manera inmediata (desencadena PRE_PING y luego pasos).
 */
export async function POST(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const routineId = parseInt(id);

        if (isNaN(routineId)) {
            return NextResponse.json({ error: 'ID de rutina inválido' }, { status: 400 });
        }

        const execution = await startRoutineExecution(routineId);

        if (!execution) {
            return NextResponse.json({ error: 'La rutina no se pudo ejecutar o no tiene pasos' }, { status: 400 });
        }

        console.log(`[Scheduler MANUAL] Lanzada ejecución manual para rutina ID ${routineId} (Exec ID ${execution.execution_id})`);

        return NextResponse.json({
            success: true,
            message: 'Rutina encolada para ejecución manual con fase de PING preliminar',
            execution_id: execution.execution_id
        });
    } catch (error) {
        console.error('Error executing routine manually:', error);
        return NextResponse.json(
            { error: 'Error al iniciar la ejecución manual' },
            { status: 500 }
        );
    }
}

/**
 * PUT /api/network/routines/[id]
 * 
 * Modifica una rutina existente y recrea sus pasos en una transacción.
 */
export async function PUT(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const routineId = parseInt(id);

        if (isNaN(routineId)) {
            return NextResponse.json({ error: 'ID de rutina inválido' }, { status: 400 });
        }

        const body = await request.json();
        const { name, description, days_of_week, hour, minute, steps } = body;

        if (!name?.trim()) {
            return NextResponse.json({ error: 'El nombre es obligatorio' }, { status: 400 });
        }

        const updatedRoutine = await prisma.$transaction(async (tx) => {
            const routine = await tx.routine.update({
                where: { routine_id: routineId },
                data: {
                    name,
                    description,
                    days_of_week,
                    hour: parseInt(hour),
                    minute: parseInt(minute)
                }
            });

            await tx.routineStep.deleteMany({
                where: { routine_id: routineId }
            });

            if (steps && steps.length > 0) {
                const stepData = steps.map((step: any, index: number) => ({
                    routine_id: routineId,
                    type: step.type,
                    target_node_id: step.target_node_id,
                    order: index + 1
                }));

                await tx.routineStep.createMany({
                    data: stepData
                });
            }

            return routine;
        });

        return NextResponse.json({ success: true, routine: updatedRoutine });
    } catch (error) {
        console.error('Error updating routine:', error);
        return NextResponse.json(
            { error: 'Error al actualizar la rutina' },
            { status: 500 }
        );
    }
}
