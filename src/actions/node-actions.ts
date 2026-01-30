'use server';

import { prisma } from '@/lib/prisma';
import { revalidatePath } from 'next/cache';
import crypto from 'crypto';
import { z } from 'zod';

const createNodeSchema = z.object({
    node_id: z.string().min(3).max(20),
    description: z.string().min(5).max(100),
    latitude: z.number().min(-90).max(90),
    longitude: z.number().min(-180).max(180),
});

export async function createNode(prevState: any, formData: FormData) {
    try {
        const rawData = {
            node_id: formData.get('node_id'),
            description: formData.get('description'),
            latitude: parseFloat(formData.get('latitude') as string),
            longitude: parseFloat(formData.get('longitude') as string),
        };

        const data = createNodeSchema.parse(rawData);

        await prisma.node.create({
            data: {
                ...data,
                is_active: true,
            },
        });

        revalidatePath('/dashboard/nodes');
        return { message: 'Nodo creado exitosamente', success: true };
    } catch (e: any) {
        console.error(e);
        return { message: 'Error al crear nodo: ' + e.message, success: false };
    }
}

export async function updateNode(prevState: any, formData: FormData) {
    try {
        const rawData = {
            description: formData.get('description') as string,
            latitude: parseFloat(formData.get('latitude') as string),
            longitude: parseFloat(formData.get('longitude') as string),
        };
        const node_id = formData.get('node_id') as string;

        // Basic validation (using partial or manual)
        if (rawData.latitude < -90 || rawData.latitude > 90) throw new Error('Latitud inválida');
        if (rawData.longitude < -180 || rawData.longitude > 180) throw new Error('Longitud inválida');

        await prisma.node.update({
            where: { node_id },
            data: {
                description: rawData.description,
                latitude: rawData.latitude,
                longitude: rawData.longitude
            },
        });

        revalidatePath('/dashboard/nodes');
        return { message: 'Nodo actualizado exitosamente', success: true };
    } catch (e: any) {
        return { message: 'Error al actualizar: ' + e.message, success: false };
    }
}

export async function deleteNode(node_id: string) {
    try {
        await prisma.node.delete({
            where: { node_id },
        });
        revalidatePath('/admin/nodes');
        return { success: true };
    } catch (e) {
        return { success: false, message: 'Error al eliminar nodo' };
    }
}

export async function generateNodeKey(node_id: string) {
    try {
        // 1. Generar Key
        const buffer = crypto.randomBytes(32);
        const apiKey = buffer.toString('hex');
        const hash = crypto.createHash('sha256').update(apiKey).digest('hex');

        // 2. Guardar Hash en DB
        await prisma.node.update({
            where: { node_id },
            data: { key_hash: hash },
        });

        revalidatePath('/admin/nodes');
        // 3. Retornar la Key original al cliente (SOLO ESTA VEZ)
        return { success: true, apiKey };
    } catch (e) {
        console.error(e);
        return { success: false, message: 'Error al generar Key' };
    }
}

export async function toggleNodeStatus(node_id: string, isActive: boolean) {
    try {
        await prisma.node.update({
            where: { node_id },
            data: { is_active: isActive }
        });
        revalidatePath('/admin/nodes');
        return { success: true };
    } catch (e) {
        return { success: false, message: 'Error actualizando estado' };
    }
}
