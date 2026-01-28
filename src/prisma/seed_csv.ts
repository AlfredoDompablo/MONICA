import fs from 'fs';
import path from 'path';
import dotenv from 'dotenv';

// Cargar variables de entorno desde el archivo .env en la raíz de app-web
const envPath = path.resolve(__dirname, '../../.env');
console.log('CWD:', process.cwd());
console.log('Ruta Env:', envPath);
console.log('Env Existe:', fs.existsSync(envPath));

const result = dotenv.config({ path: envPath });
if (result.error) {
    console.error('Error Dotenv:', result.error);
}

// Fallback: lectura manual si dotenv falla pero el archivo existe
if (!process.env.DATABASE_URL && fs.existsSync(envPath)) {
    console.log('Fallback manual para análisis de .env...');
    const envContent = fs.readFileSync(envPath, 'utf-8');
    const match = envContent.match(/DATABASE_URL=["']?(.*?)["']?$/m);
    if (match) {
        process.env.DATABASE_URL = match[1];
        console.log('DATABASE_URL Establecida Manualmente');
    }
}

/**
 * Script de Semilla desde CSV
 * Lee datos históricos desde un archivo CSV y puebla la base de datos.
 * Crea nodos predeterminados si no existen y asigna lecturas a estos nodos secuencialmente.
 */
async function main() {
    console.log('Iniciando carga de semilla CSV...');
    console.log('URL de Base de Datos disponible:', !!process.env.DATABASE_URL);

    if (!process.env.DATABASE_URL) {
        throw new Error('Falta DATABASE_URL');
    }

    // Importar instancia de prisma dinámicamente para asegurar carga de variables de entorno primero
    const { prisma } = await import('../lib/prisma');

    try {
        // 1. Asegurar que existan 4 nodos
        const nodes = [
            { node_id: 'NODE_001', description: 'Sensor Nodo 1', latitude: 19.4326, longitude: -99.1332 },
            { node_id: 'NODE_002', description: 'Sensor Nodo 2', latitude: 19.4340, longitude: -99.1350 },
            { node_id: 'NODE_003', description: 'Sensor Nodo 3', latitude: 19.4355, longitude: -99.1375 },
            { node_id: 'NODE_004', description: 'Sensor Nodo 4', latitude: 19.4370, longitude: -99.1390 },
        ];

        for (const node of nodes) {
            await prisma.node.upsert({
                where: { node_id: node.node_id },
                update: {},
                create: {
                    node_id: node.node_id,
                    description: node.description,
                    latitude: node.latitude,
                    longitude: node.longitude,
                    last_seen: new Date(),
                },
            });
        }
        console.log('Se aseguraron 4 nodos existentes.');

        // 2. Leer y parsear CSV
        const csvPath = '/home/oscar/PT2-Web/web/datos_Cuauhtemoc.csv';

        if (!fs.existsSync(csvPath)) {
            console.error(`Archivo CSV no encontrado en ${csvPath}`);
            return;
        }

        const fileContent = fs.readFileSync(csvPath, 'utf-8');
        const lines = fileContent.split('\n').filter(line => line.trim() !== '');

        const parseLine = (line: string) => {
            // Regex para separar CSV manejando comillas
            return line.split(/,(?=(?:(?:[^"]*"){2})*[^"]*$)/).map(s => s.trim().replace(/^"|"$/g, '').replace(',', '.'));
        };

        // Fecha de inicio simulada
        let startDate = new Date('2024-01-01T08:00:00Z');
        const oneWeek = 7 * 24 * 60 * 60 * 1000;

        let recordsCreated = 0;

        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            const cols = parseLine(line);
            // Verificar longitud válida de columnas para filtrar encabezados o líneas corruptas
            if (cols.length < 10) continue;

            const conductivity = parseFloat(cols[5]) || 0;
            const ph = parseFloat(cols[6]) || 0;
            const dissolved_oxygen = parseFloat(cols[7]) || 0;
            const temperature = parseFloat(cols[8]) || 0;
            const turbidity = parseFloat(cols[9]) || 0;

            const nodeIndex = i % 4;
            const nodeId = nodes[nodeIndex].node_id;

            // Distribuir lecturas en el tiempo (1 semana entre bloques de lecturas)
            const weekIndex = Math.floor(i / 4);
            const currentDate = new Date(startDate.getTime() + (weekIndex * oneWeek));

            await prisma.sensorReading.create({
                data: {
                    node_id: nodeId,
                    timestamp: currentDate,
                    ph: ph,
                    dissolved_oxygen: dissolved_oxygen,
                    turbidity: turbidity,
                    conductivity: conductivity,

                    temperature: temperature,
                    battery_level: 99.9,
                },
            });

            recordsCreated++;
            if (recordsCreated % 100 === 0) process.stdout.write('.');
        }
        console.log(`\nSemilla finalizada. Se crearon ${recordsCreated} lecturas de sensores.`);

    } finally {
        await prisma.$disconnect();
    }
}

main().catch((e) => {
    console.error(e);
    process.exit(1);
});
