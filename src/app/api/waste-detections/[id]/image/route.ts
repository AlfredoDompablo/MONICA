import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getFile } from '@/lib/s3';

/**
 * GET /api/waste-detections/[id]/image?type=original|masked
 * 
 * Recupera los datos binarios de una imagen desde MinIO/S3.
 */
export async function GET(
    request: Request,
    { params }: { params: Promise<{ id: string }> }
) {
    try {
        const { id } = await params;
        const { searchParams } = new URL(request.url);
        const type = searchParams.get('type') || 'original';
        
        const detectionId = parseInt(id);

        const detection = await prisma.wasteDetection.findUnique({
            where: { detection_id: detectionId },
            select: {
                image_original: type === 'original',
                image_masked: type === 'masked'
            }
        });

        if (!detection) {
            return NextResponse.json({ error: 'Detección no encontrada' }, { status: 404 });
        }

        const s3Key = type === 'original' ? detection.image_original : detection.image_masked;

        if (!s3Key) {
            return NextResponse.json({ error: 'Imagen no disponible' }, { status: 404 });
        }

        // Recuperar el buffer desde MinIO
        const imageData = await getFile(s3Key);

        // Devolver la imagen con el Content-Type adecuado
        return new NextResponse(imageData as any, {
            headers: {
                'Content-Type': 'image/jpeg',
                'Cache-Control': 'public, max-age=31536000, immutable'
            }
        });
    } catch (error) {
        console.error('Error fetching image:', error);
        return NextResponse.json({ error: 'Error al obtener la imagen' }, { status: 500 });
    }
}
