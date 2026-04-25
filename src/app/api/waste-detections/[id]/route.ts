import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { s3Client } from '@/lib/s3';
import { DeleteObjectCommand } from '@aws-sdk/client-s3';

/**
 * DELETE /api/waste-detections/[id]
 * 
 * Elimina una detección de la base de datos y sus imágenes asociadas en MinIO.
 */
export async function DELETE(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const detectionId = parseInt(id);

        // 1. Buscar la detección para obtener las keys de las imágenes
        const detection = await prisma.wasteDetection.findUnique({
            where: { detection_id: detectionId },
            select: {
                image_original: true,
                image_masked: true,
            }
        });

        if (!detection) {
            return NextResponse.json({ error: 'Detección no encontrada' }, { status: 404 });
        }

        // 2. Eliminar objetos de MinIO si existen
        const deleteFromS3 = async (key: string | null) => {
            if (!key) return;
            try {
                await s3Client.send(new DeleteObjectCommand({
                    Bucket: process.env.S3_BUCKET_NAME,
                    Key: key,
                }));
            } catch (err) {
                console.error(`Error deleting S3 key ${key}:`, err);
            }
        };

        await Promise.all([
            deleteFromS3(detection.image_original),
            deleteFromS3(detection.image_masked),
        ]);

        // 3. Eliminar registro de la base de datos
        await prisma.wasteDetection.delete({
            where: { detection_id: detectionId },
        });

        return NextResponse.json({ message: 'Detección eliminada correctamente' });
    } catch (error) {
        console.error('Error deleting waste detection:', error);
        return NextResponse.json({ error: 'Error al eliminar la detección' }, { status: 500 });
    }
}
