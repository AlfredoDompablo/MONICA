import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

/**
 * POST /api/network/routines/executions/[id]/abort
 * 
 * Aborta una ejecución de rutina que se encuentra en ejecución ('RUNNING').
 * Marca la ejecución, sus pasos pendientes o en curso, y los comandos de red asociados como 'FAILED'.
 */
export async function POST(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const executionId = parseInt(id);

        if (isNaN(executionId)) {
            return NextResponse.json({ error: 'ID de ejecución inválido' }, { status: 400 });
        }

        // Obtener la ejecución
        const execution = await prisma.routineExecution.findUnique({
            where: { execution_id: executionId },
            include: { steps: true }
        });

        if (!execution) {
            return NextResponse.json({ error: 'Ejecución no encontrada' }, { status: 404 });
        }

        if (execution.status !== 'RUNNING') {
            return NextResponse.json(
                { error: `La ejecución no se puede abortar porque su estado actual es ${execution.status}` },
                { status: 400 }
            );
        }

        // Abortar la ejecución, marcar pasos pendientes/corriendo como FAILED y comandos asociados como FAILED en una transacción
        await prisma.$transaction(async (tx) => {
            // Actualizar la ejecución a FAILED
            await tx.routineExecution.update({
                where: { execution_id: executionId },
                data: {
                    status: 'FAILED',
                    finished_at: new Date()
                }
            });

            // Encontrar pasos PENDING o RUNNING
            const activeSteps = execution.steps.filter(
                (step) => step.status === 'PENDING' || step.status === 'RUNNING'
            );

            // Obtener todos los command_id asociados a los pasos activos
            const commandIds = activeSteps
                .map((step) => step.command_id)
                .filter((cmdId): cmdId is number => cmdId !== null && cmdId !== undefined);

            // Marcar comandos como FAILED
            if (commandIds.length > 0) {
                await tx.networkCommand.updateMany({
                    where: {
                        command_id: { in: commandIds },
                        status: { in: ['PENDING', 'PROCESSING'] }
                    },
                    data: {
                        status: 'FAILED',
                        response: 'Comando abortado manualmente por el usuario al detener la rutina.'
                    }
                });
            }

            // Marcar todos los pasos activos de la ejecución como FAILED
            await tx.routineExecutionStep.updateMany({
                where: {
                    execution_id: executionId,
                    status: { in: ['PENDING', 'RUNNING'] }
                },
                data: {
                    status: 'FAILED',
                    finished_at: new Date()
                }
            });
        });

        return NextResponse.json({
            success: true,
            message: 'Ejecución de rutina abortada con éxito.'
        });
    } catch (error) {
        console.error('Error aborting routine execution:', error);
        return NextResponse.json(
            { error: 'Error al abortar la ejecución de la rutina' },
            { status: 500 }
        );
    }
}
