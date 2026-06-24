import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';
import crypto from 'crypto';
import mqtt from 'mqtt';


/**
 * GET /api/network/commands
 * 
 * Invocado por el concentrador remoto para consultar comandos pendientes (status = 'PENDING').
 * También usado por el panel web para ver el historial de comandos (si no se especifica auth de concentrador).
 */
export async function GET(request: NextRequest) {
    try {
        const apiKey = request.headers.get('x-api-key');

        // Si hay API Key, es el concentrador haciendo polling
        if (apiKey) {
            const hash = crypto.createHash('sha256').update(apiKey).digest('hex');
            const node = await prisma.node.findUnique({
                where: { key_hash: hash }
            });

            if (!node || !node.is_active || node.node_id !== 'NODE_C') {
                return NextResponse.json({ error: 'Invalid or unauthorized API Key' }, { status: 401 });
            }

            // Devolver sólo los comandos pendientes para el concentrador
            const pendingCommands = await prisma.networkCommand.findMany({
                where: { status: 'PENDING' },
                orderBy: { timestamp: 'asc' }, // Primero los más antiguos
            });

            return NextResponse.json(pendingCommands);
        }

        // Si no hay API Key, es la interfaz web queriendo ver el historial de comandos
        const limit = parseInt(request.nextUrl.searchParams.get('limit') || '50');
        const nodeId = request.nextUrl.searchParams.get('node_id');
        const cmdType = request.nextUrl.searchParams.get('type');

        const whereClause: any = {};
        if (nodeId) {
            whereClause.target_node_id = nodeId;
        }
        if (cmdType) {
            whereClause.type = cmdType;
        }

        const commands = await prisma.networkCommand.findMany({
            where: whereClause,
            orderBy: { timestamp: 'desc' },
            take: limit
        });

        return NextResponse.json(commands);
    } catch (error) {
        console.error('Error fetching commands:', error);
        return NextResponse.json(
            { error: 'Error al obtener comandos de red' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/network/commands
 * 
 * Encola un nuevo comando desde la interfaz web.
 */
export async function POST(request: Request) {
    try {
        const body = await request.json();
        const { type, target_node_id, parameters } = body;

        if (!type || !target_node_id) {
            return NextResponse.json({ error: 'Type and target_node_id are required' }, { status: 400 });
        }

        // Validar que el nodo sensor de destino exista y esté activo
        const targetNode = await prisma.node.findUnique({
            where: { node_id: target_node_id }
        });

        if (!targetNode || !targetNode.is_active) {
            return NextResponse.json({ error: 'El nodo destino no existe o está inactivo' }, { status: 400 });
        }

        const newCommand = await prisma.networkCommand.create({
            data: {
                type,
                target_node_id,
                parameters: parameters ? JSON.stringify(parameters) : null,
                status: 'PENDING',
                timestamp: new Date(),
            }
        });

        // Publicar el comando en MQTT de forma no-bloqueante/asíncrona
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
                    command_id: newCommand.command_id,
                    type: newCommand.type,
                    target_node_id: newCommand.target_node_id,
                    parameters: parameters || null,
                });
                client.publish('monica/commands', message, { qos: 1 }, () => {
                    console.log(`[MQTT] Publicado comando ${newCommand.command_id} a monica/commands`);
                    client.end();
                });
            });

            client.on('error', (err) => {
                console.error('[MQTT] Error conectando al broker:', err);
                client.end();
            });
        } catch (mqttErr) {
            console.error('[MQTT] Fallo al publicar comando:', mqttErr);
        }

        return NextResponse.json(newCommand, { status: 201 });
    } catch (error) {
        console.error('Error creating network command:', error);
        return NextResponse.json(
            { error: 'Error al registrar el comando' },
            { status: 500 }
        );
    }
}
