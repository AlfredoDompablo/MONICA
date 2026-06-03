'use server';

import { prisma } from '@/lib/prisma';
import { revalidatePath } from 'next/cache';
import crypto from 'crypto';
import { z } from 'zod';

const createNodeSchema = z.object({
    node_id: z.string().min(3).max(20),
    description: z.string().min(5).max(100),
});

export async function createNode(prevState: any, formData: FormData) {
    try {
        const rawData = {
            node_id: formData.get('node_id'),
            description: formData.get('description'),
        };

        const data = createNodeSchema.parse(rawData);

        await prisma.node.create({
            data: {
                node_id: data.node_id,
                description: data.description,
                latitude: 19.4326,  // Coordenadas base iniciales por defecto (Cuauhtémoc)
                longitude: -99.1332,
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
        };
        const node_id = formData.get('node_id') as string;

        // Validación básica
        if (!rawData.description || rawData.description.length < 5 || rawData.description.length > 100) {
            throw new Error('La descripción debe tener entre 5 y 100 caracteres');
        }

        await prisma.node.update({
            where: { node_id },
            data: {
                description: rawData.description,
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
