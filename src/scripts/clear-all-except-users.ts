import dotenv from 'dotenv';
import path from 'path';

// Cargar variables de entorno antes de importar Prisma
const envPath = path.resolve(__dirname, '../../.env');
dotenv.config({ path: envPath });

if (process.env.DATABASE_URL) {
    process.env.DATABASE_URL = process.env.DATABASE_URL.replace('localhost', '127.0.0.1');
}

async function main() {
    const { prisma } = await import('../lib/prisma');
    try {
        console.log('Iniciando borrado completo de registros (dejando solo usuarios)...');

        console.log('Borrando datos de sensor_readings...');
        await prisma.sensorReading.deleteMany({});
        
        console.log('Borrando datos de waste_detections...');
        await prisma.wasteDetection.deleteMany({});
        
        console.log('Borrando datos de nodes...');
        await prisma.node.deleteMany({});

        console.log('Base de datos limpiada con éxito. Solo la tabla de usuarios contiene registros.');
    } finally {
        await prisma.$disconnect();
    }
}

main()
    .catch((e) => {
        console.error('Error al limpiar la base de datos:', e);
        process.exit(1);
    });
