import { S3Client, PutObjectCommand, GetObjectCommand } from "@aws-sdk/client-s3";

const s3Client = new S3Client({
    endpoint: process.env.S3_ENDPOINT,
    region: process.env.S3_REGION || "us-east-1",
    credentials: {
        accessKeyId: process.env.S3_ACCESS_KEY || "",
        secretAccessKey: process.env.S3_SECRET_KEY || "",
    },
    forcePathStyle: true, // Requerido para MinIO
});

/**
 * Sube un archivo (Buffer) a MinIO/S3.
 * 
 * @param buffer Contenido del archivo.
 * @param key Nombre/Ruta del archivo en el bucket.
 * @param contentType Tipo de contenido (ej: image/jpeg).
 * @returns La key del archivo subido.
 */
export async function uploadFile(buffer: Buffer, key: string, contentType: string = "image/jpeg") {
    const command = new PutObjectCommand({
        Bucket: process.env.S3_BUCKET_NAME,
        Key: key,
        Body: buffer,
        ContentType: contentType,
    });

    await s3Client.send(command);
    return key;
}

/**
 * Obtiene el contenido de un archivo desde MinIO/S3.
 * 
 * @param key Nombre del archivo.
 * @returns Buffer con el contenido del archivo.
 */
export async function getFile(key: string): Promise<Buffer> {
    const command = new GetObjectCommand({
        Bucket: process.env.S3_BUCKET_NAME,
        Key: key,
    });

    const response = await s3Client.send(command);
    const byteArray = await response.Body?.transformToByteArray();
    
    if (!byteArray) {
        throw new Error("No se pudo recuperar el archivo de S3");
    }

    return Buffer.from(byteArray);
}

export { s3Client };
