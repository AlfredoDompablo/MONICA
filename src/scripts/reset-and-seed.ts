import dotenv from 'dotenv';
import path from 'path';

// Cargar variables de entorno antes de importar Prisma
const envPath = path.resolve(__dirname, '../../.env');
dotenv.config({ path: envPath });

console.log('DATABASE_URL cargada de .env:', process.env.DATABASE_URL);

if (process.env.DATABASE_URL) {
    process.env.DATABASE_URL = process.env.DATABASE_URL.replace('localhost', '127.0.0.1');
}

console.log('DATABASE_URL modificada:', process.env.DATABASE_URL);

import bcrypt from 'bcryptjs';

async function main() {
    // Importar dinámicamente para asegurar que process.env.DATABASE_URL modificado surta efecto
    const { prisma } = await import('../lib/prisma');
    try {
        console.log('Iniciando limpieza y semilla de la base de datos...');

        // 1. Borrar lecturas, detecciones y nodos históricos
        console.log('Borrando datos históricos de sensor_readings...');
        await prisma.sensorReading.deleteMany({});
        
        console.log('Borrando datos históricos de waste_detections...');
        await prisma.wasteDetection.deleteMany({});
        
        console.log('Borrando nodos antiguos de la tabla nodes...');
        await prisma.node.deleteMany({});

        console.log('Base de datos limpiada con éxito.');

        // 2. Crear Nodos desde cero
        console.log('Creando nodos nuevos...');
        
        // Nodo Concentrador (Master Gateway) con la API Key configurada
        const concentradorHash = 'ed39d1d166504e678aa2fee762ed795bb1028920b53fbec3a795e4a3be0a20ed';
        await prisma.node.create({
            data: {
                node_id: 'NODE_C',
                description: 'Master Concentrador Gateway',
                latitude: 19.4326,
                longitude: -99.1332,
                is_active: true,
                key_hash: concentradorHash,
                last_seen: new Date(),
            }
        });
        console.log('Se creó el Nodo Concentrador (NODE_C) con API Key maestra.');

        // Crear los 4 nodos sensores
        const sensorNodes = [
            { node_id: 'NODE_001', description: 'Sensor Nodo 1', latitude: 19.4326, longitude: -99.1332 },
            { node_id: 'NODE_002', description: 'Sensor Nodo 2', latitude: 19.4340, longitude: -99.1350 },
            { node_id: 'NODE_003', description: 'Sensor Nodo 3', latitude: 19.4355, longitude: -99.1375 },
            { node_id: 'NODE_004', description: 'Sensor Nodo 4', latitude: 19.4370, longitude: -99.1390 },
        ];

        for (const node of sensorNodes) {
            await prisma.node.create({
                data: {
                    node_id: node.node_id,
                    description: node.description,
                    latitude: node.latitude,
                    longitude: node.longitude,
                    is_active: true,
                    key_hash: null, // No necesitan su propia clave activa ya que reportan a través del Master
                    last_seen: new Date(),
                }
            });
            console.log(`Se creó el ${node.description} (${node.node_id}) desde cero.`);
        }

        // 3. Asegurar que exista el usuario admin
        const email = 'admin@admin.com';
        const password = 'adminpassword';
        const existingUser = await prisma.user.findUnique({
            where: { email },
        });

        if (!existingUser) {
            const hashedPassword = await bcrypt.hash(password, 10);
            await prisma.user.create({
                data: {
                    full_name: 'Super Admin',
                    email,
                    password_hash: hashedPassword,
                    role: 'admin',
                    is_active: true,
                },
            });
            console.log(`Usuario administrador creado: ${email}`);
        } else {
            console.log(`El usuario administrador ${email} ya existe.`);
        }

        console.log('¡Limpieza y inicialización de base de datos finalizada con éxito!');
    } finally {
        await prisma.$disconnect();
    }
}

main()
    .catch((e) => {
        console.error('Error al resetear la base de datos:', e);
        process.exit(1);
    });
