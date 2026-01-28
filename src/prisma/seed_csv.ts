
import fs from 'fs';
import path from 'path';
import dotenv from 'dotenv';

// Load environment variables from .env file at the root of app-web
const envPath = path.resolve(__dirname, '../../.env');
console.log('CWD:', process.cwd());
console.log('Env Path:', envPath);
console.log('Env Exists:', fs.existsSync(envPath));

const result = dotenv.config({ path: envPath });
if (result.error) {
    console.error('Dotenv error:', result.error);
}

// Fallback: manually read if dotenv failed but file exists
if (!process.env.DATABASE_URL && fs.existsSync(envPath)) {
    console.log('Manual fallback for .env parsing...');
    const envContent = fs.readFileSync(envPath, 'utf-8');
    const match = envContent.match(/DATABASE_URL=["']?(.*?)["']?$/m);
    if (match) {
        process.env.DATABASE_URL = match[1];
        console.log('Manually Set DATABASE_URL');
    }
}

async function main() {
    console.log('Starting CSV seed...');
    console.log('Database URL available:', !!process.env.DATABASE_URL);

    if (!process.env.DATABASE_URL) {
        throw new Error('DATABASE_URL is missing');
    }

    // Import prisma instance dynamically to ensure env vars are loaded first
    const { prisma } = await import('../lib/prisma');

    try {
        // 1. Ensure 4 nodes exist
        const nodes = [
            { node_id: 'NODE_001', description: 'Sensor Node 1', latitude: 19.4326, longitude: -99.1332 },
            { node_id: 'NODE_002', description: 'Sensor Node 2', latitude: 19.4340, longitude: -99.1350 },
            { node_id: 'NODE_003', description: 'Sensor Node 3', latitude: 19.4355, longitude: -99.1375 },
            { node_id: 'NODE_004', description: 'Sensor Node 4', latitude: 19.4370, longitude: -99.1390 },
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
        console.log('Ensured 4 nodes exist.');

        // 2. Read and parse CSV
        const csvPath = '/home/oscar/PT2-Web/web/datos_Cuauhtemoc.csv';

        if (!fs.existsSync(csvPath)) {
            console.error(`CSV file not found at ${csvPath}`);
            return;
        }

        const fileContent = fs.readFileSync(csvPath, 'utf-8');
        const lines = fileContent.split('\n').filter(line => line.trim() !== '');

        const parseLine = (line: string) => {
            // Regex for CSV splitting handling quotes
            return line.split(/,(?=(?:(?:[^"]*"){2})*[^"]*$)/).map(s => s.trim().replace(/^"|"$/g, '').replace(',', '.'));
        };

        let startDate = new Date('2024-01-01T08:00:00Z');
        const oneWeek = 7 * 24 * 60 * 60 * 1000;

        let recordsCreated = 0;

        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            const cols = parseLine(line);
            // DLZAC2591M1 is 11 chars. If col 0 is DLZ..., it's data.
            // But verify length.
            if (cols.length < 10) continue;

            const conductivity = parseFloat(cols[5]) || 0;
            const ph = parseFloat(cols[6]) || 0;
            const dissolved_oxygen = parseFloat(cols[7]) || 0;
            const temperature = parseFloat(cols[8]) || 0;
            const turbidity = parseFloat(cols[9]) || 0;

            const nodeIndex = i % 4;
            const nodeId = nodes[nodeIndex].node_id;

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
        console.log(`\nFinished seeding. Created ${recordsCreated} sensor readings.`);

    } finally {
        await prisma.$disconnect();
    }
}

main().catch((e) => {
    console.error(e);
    process.exit(1);
});
