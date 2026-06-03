import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { type NextRequest } from 'next/server';
import { sensorReadingSchema } from '@/lib/schemas';
import { z } from 'zod';
import crypto from 'crypto';

/**
 * GET /api/sensor-readings
 * 
 * Obtiene el historial de lecturas de sensores.
 * Permite filtrar por nodo, rango de fechas y limitar resultados.
 * 
 * @param {NextRequest} request - Incluye parámetros de búsqueda (node_id, limit, startDate, endDate).
 * @returns {Promise<NextResponse>} Lista de lecturas.
 */
export async function GET(request: NextRequest) {
    try {
        const searchParams = request.nextUrl.searchParams;
        const node_id = searchParams.get('node_id');
        const limit = parseInt(searchParams.get('limit') || '50');
        const startDate = searchParams.get('startDate');
        const endDate = searchParams.get('endDate');

        const where: any = {};
        if (node_id) {
            if (node_id === 'NODE_C') {
                return NextResponse.json([]);
            }
            where.node_id = node_id;
        } else {
            where.node_id = {
                not: 'NODE_C'
            };
        }
        if (startDate || endDate) {
            where.timestamp = {};
            if (startDate) where.timestamp.gte = new Date(startDate);
            if (endDate) where.timestamp.lte = new Date(endDate);
        }

        const readings = await prisma.sensorReading.findMany({
            where,
            orderBy: { timestamp: 'desc' },
            take: limit,
            include: {
                node: {
                    select: { description: true } // Incluir descripción del nodo para contexto
                }
            }
        });

        return NextResponse.json(readings);
    } catch (error) {
        console.error('Error fetching sensor readings:', error);
        return NextResponse.json(
            { error: 'Error al obtener lecturas' },
            { status: 500 }
        );
    }
}

/**
 * POST /api/sensor-readings
 * 
 * Registra una nueva lectura de sensores para un nodo específico.
 * Actualiza automáticamente la fecha de 'última vista' del nodo.
 * 
 * @param {Request} request - Datos de los sensores (ph, temperatura, etc.).
 * @returns {Promise<NextResponse>} La lectura creada.
 */
export async function POST(request: Request) {
    try {
        // 1. Validar Hardware Auth (API Key)
        const apiKey = request.headers.get('x-api-key');
        if (!apiKey) {
            return NextResponse.json({ error: 'Missing API Key' }, { status: 401 });
        }

        // Calcular hash SHA-256
        const hash = crypto.createHash('sha256').update(apiKey).digest('hex');

        // Buscar nodo activo con ese hash
        const node = await prisma.node.findUnique({
            where: { key_hash: hash }
        });

        if (!node || !node.is_active) {
            return NextResponse.json({ error: 'Invalid or inactive API Key' }, { status: 401 });
        }

        const body = await request.json();

        // Si el nodo autenticado es el Concentrador (Master Gateway), preservamos el node_id del payload
        if (node.node_id === 'NODE_C') {
            if (!body.node_id || body.node_id === 'NODE_C') {
                return NextResponse.json({ error: 'Concentrador gateway does not submit its own readings' }, { status: 400 });
            } else {
                // Validar si el nodo destino existe
                let targetNode = await prisma.node.findUnique({
                    where: { node_id: body.node_id }
                });
                
                // Si el nodo no existe, lo registramos automáticamente (Auto-Registro)
                if (!targetNode) {
                    let lat = 19.4326;
                    let lng = -99.1332;
                    if (body.latitude !== undefined && body.latitude !== null) {
                        const parsedLat = parseFloat(body.latitude);
                        if (!isNaN(parsedLat)) lat = parsedLat;
                    }
                    if (body.longitude !== undefined && body.longitude !== null) {
                        const parsedLng = parseFloat(body.longitude);
                        if (!isNaN(parsedLng)) lng = parsedLng;
                    }

                    targetNode = await prisma.node.create({
                        data: {
                            node_id: body.node_id,
                            description: `Auto-registrado (${body.node_id})`,
                            latitude: lat,
                            longitude: lng,
                            is_active: true,
                            key_hash: null, // No necesita API key individual
                            last_seen: new Date(),
                        }
                    });
                    console.log(`[Auto-Registro] Nuevo nodo sensor '${body.node_id}' registrado automáticamente.`);
                } else if (!targetNode.is_active) {
                    return NextResponse.json({ error: 'Target node is inactive' }, { status: 400 });
                }
            }
        } else {
            // Si es un nodo normal, forzar su propio node_id (previene spoofing)
            body.node_id = node.node_id;
        }

        const {
            node_id,
            ph,
            dissolved_oxygen,
            turbidity,
            conductivity,
            temperature,
            battery_level
        } = sensorReadingSchema.parse(body);

        // Verificar si el nodo existe no es estrictamente necesario con FK, pero es bueno tenerlo.
        // Zod solo verifica TIPOS.

        const newReading = await prisma.sensorReading.create({
            data: {
                node_id,
                ph,
                dissolved_oxygen,
                turbidity,
                conductivity,
                temperature,
                battery_level,
                timestamp: new Date(), // Usar hora del servidor
            },
        });

        // Actualizar last_seen del nodo y coordenadas si se proporcionan
        const updateData: any = { last_seen: new Date() };
        if (body.latitude !== undefined && body.latitude !== null) updateData.latitude = body.latitude;
        if (body.longitude !== undefined && body.longitude !== null) updateData.longitude = body.longitude;

        await prisma.node.update({
            where: { node_id },
            data: updateData
        }).catch((err: any) => console.error('Error updating node status:', err));

        return NextResponse.json(newReading, { status: 201 });
    } catch (error) {
        console.error('Error creating sensor reading:', error);
        if (error instanceof z.ZodError) {
            return NextResponse.json({ error: error.issues }, { status: 400 });
        }
        if ((error as any).code === 'P2003') { // Foreign key constraint failed
            return NextResponse.json({ error: 'El nodo especificado no existe' }, { status: 400 });
        }
        return NextResponse.json(
            { error: 'Error al registrar lectura' },
            { status: 500 }
        );
    }
}
