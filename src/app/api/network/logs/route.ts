import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';
import crypto from 'crypto';

import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';

/**
 * GET /api/network/logs
 * 
 * Devuelve el historial de logs de red para la interfaz web.
 * Permite filtrar por nivel ('INFO', 'WARNING', 'ERROR', 'SUCCESS', o coma separada),
 * nodo, estado de lectura (unread_only) y limitar resultados.
 */
export async function GET(request: NextRequest) {
    try {
        const searchParams = request.nextUrl.searchParams;
        const level = searchParams.get('level');
        const levels = searchParams.get('levels');
        const node_id = searchParams.get('node_id');
        const unreadOnly = searchParams.get('unread_only') === 'true';
        const limit = parseInt(searchParams.get('limit') || '100');

        const where: any = {};

        if (levels) {
            const levelList = levels.split(',').map(l => l.trim().toUpperCase());
            where.level = { in: levelList };
        } else if (level) {
            if (level.includes(',')) {
                const levelList = level.split(',').map(l => l.trim().toUpperCase());
                where.level = { in: levelList };
            } else {
                where.level = level.toUpperCase();
            }
        }

        if (node_id) {
            where.node_id = node_id;
        }

        if (unreadOnly) {
            where.is_read = false;
        }

        const logs = await prisma.networkLog.findMany({
            where,
            orderBy: { timestamp: 'desc' },
            take: limit,
        });

        // Conteo rápido de no leídos de advertencia y error
        const unreadAlertsCount = await prisma.networkLog.count({
            where: {
                is_read: false,
                level: { in: ['WARNING', 'ERROR'] }
            }
        });

        if (searchParams.get('summary') === 'true' || searchParams.get('meta') === 'true') {
            return NextResponse.json({
                logs,
                unreadAlertsCount
            });
        }

        const response = NextResponse.json(logs);
        response.headers.set('X-Unread-Alerts', String(unreadAlertsCount));
        return response;
    } catch (error) {
        console.error('Error fetching network logs:', error);
        return NextResponse.json(
            { error: 'Error al obtener los logs de red' },
            { status: 500 }
        );
    }
}

/**
 * PATCH /api/network/logs
 * 
 * Marca una o todas las alertas/logs como atendidos/vistos con auditoría.
 * Requiere sesión de usuario autenticado.
 */
export async function PATCH(request: Request) {
    try {
        const session = await getServerSession(authOptions);
        if (!session || !session.user) {
            return NextResponse.json({ error: 'No autorizado' }, { status: 401 });
        }

        const userIdentifier = session.user.name || session.user.email || 'Administrador';
        const body = await request.json();
        const { log_id, mark_all } = body;

        const now = new Date();

        if (mark_all) {
            // Marcar todas las alertas no leídas (WARNING/ERROR) como atendidas
            const result = await prisma.networkLog.updateMany({
                where: {
                    is_read: false,
                    level: { in: ['WARNING', 'ERROR'] }
                },
                data: {
                    is_read: true,
                    acknowledged_at: now,
                    acknowledged_by: userIdentifier,
                }
            });

            return NextResponse.json({
                success: true,
                updatedCount: result.count,
                acknowledged_by: userIdentifier,
                acknowledged_at: now
            });
        }

        if (log_id) {
            const updated = await prisma.networkLog.update({
                where: { log_id: Number(log_id) },
                data: {
                    is_read: true,
                    acknowledged_at: now,
                    acknowledged_by: userIdentifier,
                }
            });

            return NextResponse.json({
                success: true,
                log: updated
            });
        }

        return NextResponse.json({ error: 'Se requiere log_id o mark_all: true' }, { status: 400 });
    } catch (error) {
        console.error('Error acknowledging network log:', error);
        return NextResponse.json(
            { error: 'Error al actualizar el estado de la alerta' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/network/logs
 * 
 * Registra un nuevo log de red. Invocado por el concentrador remoto sobre WiFi.
 * Requiere autenticación con la API Key del concentrador (x-api-key).
 */
export async function POST(request: Request) {
    try {
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
        const { level, message, node_id } = body;

        if (!level || !message) {
            return NextResponse.json({ error: 'Level and message are required' }, { status: 400 });
        }

        const newLog = await prisma.networkLog.create({
            data: {
                level: String(level).toUpperCase(),
                message,
                node_id: node_id || 'NODE_C',
                timestamp: new Date(),
            }
        });

        return NextResponse.json(newLog, { status: 201 });
    } catch (error) {
        console.error('Error creating network log:', error);
        return NextResponse.json(
            { error: 'Error al registrar log de red' },
            { status: 500 }
        );
    }
}
