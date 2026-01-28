
import { PrismaClient } from '@prisma/client';
import dotenv from 'dotenv';
import path from 'path';

import fs from 'fs';

const envPath = path.resolve(process.cwd(), '.env');
dotenv.config({ path: envPath });

// Fallback: manually read if dotenv failed but file exists
if (!process.env.DATABASE_URL && fs.existsSync(envPath)) {
    const envContent = fs.readFileSync(envPath, 'utf-8');
    const match = envContent.match(/DATABASE_URL=["']?(.*?)["']?$/m);
    if (match) process.env.DATABASE_URL = match[1];
}

async function main() {
    // Import prisma instance dynamically
    const { prisma } = await import('../lib/prisma');

    try {
        const nodeCount = await prisma.node.count();
        const readingCount = await prisma.sensorReading.count();

        console.log(`Nodes: ${nodeCount}`);
        console.log(`SensorReadings: ${readingCount}`);

        const readings = await prisma.sensorReading.findMany({
            take: 5,
            orderBy: { timestamp: 'asc' },
            include: { node: true },
        });
        const sample = readings[0];
        console.log('Sample reading conductivity:', sample?.conductivity);
        console.log('Sample reading:', JSON.stringify(readings[0], null, 2));

    } finally {
        await prisma.$disconnect();
    }
}

main().catch(console.error);
