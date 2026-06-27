import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import crypto from 'crypto';

/**
 * PUT /api/network/commands/[id]
 * 
 * Invocado por el concentrador remoto para actualizar el estado del comando.
 * Requiere autenticación con la API Key del concentrador (x-api-key).
 */
export async function PUT(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const commandId = parseInt(id);
        if (isNaN(commandId)) {
            return NextResponse.json({ error: 'Invalid command ID' }, { status: 400 });
        }

        // Validar API Key
        const apiKey = request.headers.get('x-api-key');
        if (!apiKey) {
            return NextResponse.json({ error: 'Missing API Key' }, { status: 401 });
        }

        const hash = crypto.createHash('sha256').update(apiKey).digest('hex');
        const node = await prisma.node.findUnique({
            where: { key_hash: hash }
        });

        if (!node || !node.is_active || node.node_id !== 'NODE_C') {
            return NextResponse.json({ error: 'Invalid or unauthorized API Key' }, { status: 401 });
        }

        const body = await request.json();
        const { status, response, parameters } = body;

        if (!status) {
            return NextResponse.json({ error: 'Status is required' }, { status: 400 });
        }

        const updateData: any = {
            status: String(status).toUpperCase(),
            response: response || null,
        };
        if (parameters !== undefined) {
            updateData.parameters = parameters ? (typeof parameters === 'string' ? parameters : JSON.stringify(parameters)) : null;
        }

        // Actualizar el comando en la base de datos
        const updatedCommand = await prisma.networkCommand.update({
            where: { command_id: commandId },
            data: updateData
        });

        return NextResponse.json(updatedCommand);
    } catch (error) {
        console.error('Error updating command status:', error);
        return NextResponse.json(
            { error: 'Error al actualizar estado del comando' },
            { status: 500 }
        );
    }
}

/**
 * GET /api/network/commands/[id]
 * 
 * Obtiene el estado y detalles de un comando específico.
 */
export async function GET(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const commandId = parseInt(id);
        if (isNaN(commandId)) {
            return NextResponse.json({ error: 'Invalid command ID' }, { status: 400 });
        }

        const command = await prisma.networkCommand.findUnique({
            where: { command_id: commandId }
        });

        if (!command) {
            return NextResponse.json({ error: 'Command not found' }, { status: 404 });
        }

        return NextResponse.json(command);
    } catch (error) {
        console.error('Error fetching command:', error);
        return NextResponse.json(
            { error: 'Error al obtener el comando' },
            { status: 500 }
        );
    }
}
