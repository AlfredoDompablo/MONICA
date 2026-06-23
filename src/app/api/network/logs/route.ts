import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';
import crypto from 'crypto';

/**
 * GET /api/network/logs
 * 
 * Devuelve el historial de logs de red para la interfaz web.
 * Permite filtrar por nivel ('INFO', 'WARNING', 'ERROR', 'SUCCESS') y limitar resultados.
 */
export async function GET(request: NextRequest) {
    try {
        const searchParams = request.nextUrl.searchParams;
        const level = searchParams.get('level');
        const node_id = searchParams.get('node_id');
        const limit = parseInt(searchParams.get('limit') || '100');

        const where: any = {};
        if (level) {
            where.level = level;
        }
        if (node_id) {
            where.node_id = node_id;
        }

        const logs = await prisma.networkLog.findMany({
            where,
            orderBy: { timestamp: 'desc' },
            take: limit,
        });

        return NextResponse.json(logs);
    } catch (error) {
        console.error('Error fetching network logs:', error);
        return NextResponse.json(
            { error: 'Error al obtener los logs de red' },
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
