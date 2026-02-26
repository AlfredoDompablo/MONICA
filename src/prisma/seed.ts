import { PrismaClient } from '@prisma/client';
import bcrypt from 'bcryptjs';
import { Pool } from 'pg';
import { PrismaPg } from '@prisma/adapter-pg';
import dotenv from 'dotenv';
import path from 'path';

// Cargar variables de entorno desde el archivo .env en la raíz de app-web
const envPath = path.resolve(__dirname, '../../.env');
dotenv.config({ path: envPath });

const connectionString = `${process.env.DATABASE_URL}`;

// Configuración del cliente Prisma con driver nativo de PG (necesario para Edge/Serverless)
const pool = new Pool({ connectionString });
const adapter = new PrismaPg(pool);
const prisma = new PrismaClient({ adapter });

/**
 * Script de Semilla (Seeding)
 * Inicializa la base de datos con un usuario administrador predeterminado si no existe.
 */
async function main() {
    const email = 'admin@admin.com';
    const password = 'adminpassword';

    const existingUser = await prisma.user.findUnique({
        where: { email },
    });

    if (existingUser) {
        console.log(`El usuario ${email} ya existe.`);
        return;
    }

    // Hashear contraseña antes de guardar
    const hashedPassword = await bcrypt.hash(password, 10);

    const user = await prisma.user.create({
        data: {
            full_name: 'Super Admin',
            email,
            password_hash: hashedPassword,
            role: 'admin',
            is_active: true,
        },
    });

    console.log(`Usuario admin creado: ${user.email}`);
    console.log(`Contraseña: ${password}`);
}

main()
    .catch((e) => {
        console.error(e);
        process.exit(1);
    })
    .finally(async () => {
        await prisma.$disconnect();
    });
