import { prisma } from './prisma';
import mqtt from 'mqtt';
import net from 'net';

interface GlobalScheduler {
    isSchedulerRunning?: boolean;
    checkInterval?: NodeJS.Timeout | null;
    lockServer?: net.Server | null;
}

const globalForScheduler = global as unknown as GlobalScheduler;

/**
 * Publica un comando en el broker MQTT
 */
async function publishMqttCommand(commandId: number, type: string, targetNodeId: string, parameters: any) {
    try {
        const brokerUrl = process.env.MQTT_BROKER_URL || 'mqtt://127.0.0.1:1883';
        const mqttUser = process.env.MQTT_USER;
        const mqttPass = process.env.MQTT_PASS;

        const client = mqtt.connect(brokerUrl, {
            username: mqttUser,
            password: mqttPass,
            connectTimeout: 2000,
        });

        client.on('connect', () => {
            const message = JSON.stringify({
                command_id: commandId,
                type,
                target_node_id: targetNodeId,
                parameters: parameters || null,
            });
            client.publish('monica/commands', message, { qos: 1 }, () => {
                console.log(`[Scheduler MQTT] Publicado comando ${commandId} a monica/commands`);
                client.end();
            });
        });

        client.on('error', (err) => {
            console.error('[Scheduler MQTT] Error conectando al broker:', err);
            client.end();
        });
    } catch (err) {
        console.error('[Scheduler MQTT] Error al publicar comando:', err);
    }
}

/**
 * Helper para iniciar la ejecución de una rutina.
 * Crea la corrida y encola los PRE_PINGs y luego los pasos reales.
 */
export async function startRoutineExecution(routineId: number) {
    const routine = await prisma.routine.findUnique({
        where: { routine_id: routineId },
        include: {
            steps: {
                orderBy: { order: 'asc' }
            }
        }
    });

    if (!routine || routine.steps.length === 0) return null;

    const execution = await prisma.routineExecution.create({
        data: {
            routine_id: routine.routine_id,
            status: 'RUNNING',
            started_at: new Date(),
        }
    });

    // Encolar los pasos reales de la rutina
    for (const step of routine.steps) {
        await prisma.routineExecutionStep.create({
            data: {
                execution_id: execution.execution_id,
                step_id: step.step_id,
                node_id: step.target_node_id,
                type: step.type,
                status: 'PENDING',
                attempt: 1,
            }
        });
    }

    return execution;
}

/**
 * Bucle principal de ejecución del planificador (se ejecuta cada 10 segundos)
 */
async function schedulerTick() {
    try {
        const now = new Date();
        const currentDay = now.getDay(); // 0 = Domingo, 1 = Lunes, ..., 6 = Sábado
        const currentHour = now.getHours();
        const currentMinute = now.getMinutes();

        // 1. COMPROBAR E INICIAR NUEVAS RUTINAS PROGRAMADAS
        const activeRoutines = await prisma.routine.findMany({
            where: {
                is_active: true,
                hour: currentHour,
                minute: currentMinute,
            },
            include: {
                steps: {
                    orderBy: { order: 'asc' }
                }
            }
        });

        // Filtrar por los días seleccionados
        const scheduledRoutines = activeRoutines.filter(routine => {
            const days = routine.days_of_week.split(',').map(Number);
            return days.includes(currentDay);
        });

        for (const routine of scheduledRoutines) {
            // Evitar doble disparo en el mismo minuto
            const lastExecutionInMinute = await prisma.routineExecution.findFirst({
                where: {
                    routine_id: routine.routine_id,
                    started_at: {
                        gte: new Date(now.getFullYear(), now.getMonth(), now.getDate(), currentHour, currentMinute, 0),
                        lte: new Date(now.getFullYear(), now.getMonth(), now.getDate(), currentHour, currentMinute, 59),
                    }
                }
            });

            if (lastExecutionInMinute) continue; // Ya se inició una ejecución de esta rutina en este minuto
            if (routine.steps.length === 0) continue; // Rutina vacía

            console.log(`[Scheduler] Iniciando rutina "${routine.name}" programada...`);
            await startRoutineExecution(routine.routine_id);
        }

        // 2. PROCESAR EJECUCIONES EN CURSO (SECUESTRADAS PASO A PASO)
        const activeExecutions = await prisma.routineExecution.findMany({
            where: { status: 'RUNNING' },
        });

        for (const exec of activeExecutions) {
            // Traer todos los pasos de esta corrida ordenados
            const execSteps = await prisma.routineExecutionStep.findMany({
                where: { execution_id: exec.execution_id },
                orderBy: { exec_step_id: 'asc' }
            });

            // Buscar si hay un paso en estado RUNNING
            const runningStep = execSteps.find((s: any) => s.status === 'RUNNING');

            if (runningStep) {
                if (runningStep.command_id) {
                    // Verificar el estado del comando asociado en la DB
                    const cmd = await prisma.networkCommand.findUnique({
                        where: { command_id: runningStep.command_id }
                    });

                    if (cmd && (cmd.status === 'COMPLETED' || cmd.status === 'FAILED')) {
                        console.log(`[Scheduler] Paso ${runningStep.exec_step_id} (${runningStep.type}) en ${runningStep.node_id} finalizado con ${cmd.status}`);
                        await prisma.routineExecutionStep.update({
                            where: { exec_step_id: runningStep.exec_step_id },
                            data: {
                                status: cmd.status,
                                finished_at: new Date()
                            }
                        });
                    } else {
                        // Comprobar timeout de seguridad del planificador (evita bloqueos si el comando se queda colgado)
                        const elapsed = (new Date().getTime() - new Date(runningStep.started_at || new Date()).getTime()) / 1000;
                        const isProcessing = cmd && cmd.status === 'PROCESSING';
                        
                        let timeoutLimit = 30;
                        if (isProcessing) {
                            if (runningStep.type === 'POLL_IMAGE') timeoutLimit = 900; // 15 minutos para permitir transferencias lentas y con reintentos LoRa
                            else if (runningStep.type === 'POLL_TELEMETRY') timeoutLimit = 90;
                            else timeoutLimit = 60;
                        } else {
                            // Sigue en PENDING (concentrador no ha recibido/procesado el comando aún por estar ocupado descargando de otro nodo)
                            if (runningStep.type === 'POLL_IMAGE') timeoutLimit = 900; // 15 minutos de margen de espera en cola
                            else if (runningStep.type === 'POLL_TELEMETRY') timeoutLimit = 120;
                            else timeoutLimit = 60;
                        }

                        if (elapsed > timeoutLimit) {
                            console.warn(`[Scheduler] Timeout del planificador para paso ${runningStep.exec_step_id} (${runningStep.type}) en nodo ${runningStep.node_id} tras ${elapsed.toFixed(1)} segundos.`);
                            
                            // Marcar comando asociado como FAILED en la DB para consistencia
                            if (cmd && cmd.status === 'PENDING') {
                                await prisma.networkCommand.update({
                                    where: { command_id: cmd.command_id },
                                    data: { status: 'FAILED' }
                                });
                            }

                            // Marcar el paso de ejecución como FAILED
                            await prisma.routineExecutionStep.update({
                                where: { exec_step_id: runningStep.exec_step_id },
                                data: {
                                    status: 'FAILED',
                                    finished_at: new Date()
                                }
                            });
                        }
                    }
                } else {
                    // Si por algún motivo está en RUNNING pero no tiene command_id, marcar como FAILED
                    await prisma.routineExecutionStep.update({
                        where: { exec_step_id: runningStep.exec_step_id },
                        data: {
                            status: 'FAILED',
                            finished_at: new Date()
                        }
                    });
                }
                continue; // Esperar al siguiente tick
            }

            // Si no hay pasos en RUNNING, buscar el primer paso en PENDING
            const nextPendingStep = execSteps.find((s: any) => s.status === 'PENDING');

            if (nextPendingStep) {
                // Aplicar un tiempo de respiro (cooldown) de 4 segundos entre comandos consecutivos para dejar asentar la red LoRa y el hardware
                const finishedSteps = execSteps.filter((s: any) => s.status === 'COMPLETED' || s.status === 'FAILED');
                if (finishedSteps.length > 0) {
                    const lastFinished = finishedSteps.sort((a: any, b: any) => new Date(b.finished_at!).getTime() - new Date(a.finished_at!).getTime())[0];
                    if (lastFinished && lastFinished.finished_at) {
                        const secondsSinceLast = (new Date().getTime() - new Date(lastFinished.finished_at).getTime()) / 1000;
                        const cooldownLimit = 4.0;
                        if (secondsSinceLast < cooldownLimit) {
                            console.log(`[Scheduler] Tiempo de respiro activo: Han pasado ${secondsSinceLast.toFixed(1)}s de ${cooldownLimit}s desde el último comando. Esperando...`);
                            continue; // Esperar al siguiente tick (2 segundos después)
                        }
                    }
                }

                // Ejecutar el paso
                console.log(`[Scheduler] Iniciando paso ${nextPendingStep.exec_step_id}: ${nextPendingStep.type} en ${nextPendingStep.node_id}`);
                
                const cmdType = nextPendingStep.type!;
                const newCommand = await prisma.networkCommand.create({
                    data: {
                        type: cmdType,
                        target_node_id: nextPendingStep.node_id!,
                        status: 'PENDING',
                        timestamp: new Date(),
                    }
                });

                // Publicar en MQTT
                await publishMqttCommand(newCommand.command_id, newCommand.type, newCommand.target_node_id, null);

                // Actualizar el paso de ejecución a RUNNING
                await prisma.routineExecutionStep.update({
                    where: { exec_step_id: nextPendingStep.exec_step_id },
                    data: {
                        status: 'RUNNING',
                        command_id: newCommand.command_id,
                        started_at: new Date()
                    }
                });
            } else {
                // Si no hay más pasos en PENDING ni en RUNNING, marcar la ejecución completa como terminada
                console.log(`[Scheduler] Rutina de ejecución ${exec.execution_id} finalizada por completo.`);
                
                // Si todos los pasos no-PRE_PING han fallado, o si se completaron con fallas mixtas
                const normalSteps = execSteps.filter((s: any) => s.type !== 'PRE_PING');
                const hasFailed = normalSteps.some((s: any) => s.status === 'FAILED');
                const finalStatus = hasFailed ? 'FAILED' : 'COMPLETED';

                await prisma.routineExecution.update({
                    where: { execution_id: exec.execution_id },
                    data: {
                        status: finalStatus,
                        finished_at: new Date()
                    }
                });
            }
        }

    } catch (err) {
        console.error('[Scheduler Error] Error en el tick del planificador:', err);
    }
}
/**
 * Inicializa el planificador en segundo plano
 */
export function initScheduler() {
    if (globalForScheduler.isSchedulerRunning) {
        if (process.env.NODE_ENV !== 'production') {
            console.log('[Scheduler] Recarga de código detectada. Reiniciando planificador...');
            if (globalForScheduler.checkInterval) {
                clearInterval(globalForScheduler.checkInterval);
                globalForScheduler.checkInterval = null;
            }
            if (globalForScheduler.lockServer) {
                try { globalForScheduler.lockServer.close(); } catch (e) {}
                globalForScheduler.lockServer = null;
            }
            globalForScheduler.isSchedulerRunning = false;
        } else {
            return;
        }
    }

    const port = 50432;
    const server = net.createServer();

    server.once('error', (err: any) => {
        if (err.code === 'EADDRINUSE') {
            console.log('[Scheduler] Planificador ya está ejecutándose en otro proceso (puerto 50432 ocupado). Omitiendo inicio.');
        } else {
            console.error('[Scheduler] Error en el servidor de bloqueo de puerto:', err);
        }
    });

    server.once('listening', () => {
        console.log('[Scheduler] Puerto de bloqueo 50432 adquirido con éxito.');
        globalForScheduler.isSchedulerRunning = true;
        globalForScheduler.lockServer = server;
        console.log('[Scheduler] Inicializando planificador de rutinas en segundo plano...');

        // Ejecutar un tick inicial después de 5 segundos
        setTimeout(schedulerTick, 5000);

        // Configurar intervalo de comprobación cada 2 segundos
        globalForScheduler.checkInterval = setInterval(schedulerTick, 2000);
    });

    server.listen(port, '127.0.0.1');
}

/**
 * Detiene el planificador (útil en pruebas)
 */
export function stopScheduler() {
    if (globalForScheduler.checkInterval) {
        clearInterval(globalForScheduler.checkInterval);
        globalForScheduler.checkInterval = null;
    }
    if (globalForScheduler.lockServer) {
        try { globalForScheduler.lockServer.close(); } catch (e) {}
        globalForScheduler.lockServer = null;
    }
    globalForScheduler.isSchedulerRunning = false;
    console.log('[Scheduler] Planificador de rutinas detenido.');
}
