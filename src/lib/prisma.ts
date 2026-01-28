import { PrismaClient } from '@prisma/client';
import { Pool } from 'pg';
import { PrismaPg } from '@prisma/adapter-pg';

const connectionString = `${process.env.DATABASE_URL}`;

const globalForPrisma = global as unknown as { prisma: PrismaClient };

// Configuración del Pool de conexiones para PostgreSQL
// Se utiliza un pool para mejorar el rendimiento y manejo de conexiones en entornos serverless/edge
const pool = new Pool({ connectionString });
const adapter = new PrismaPg(pool);

/**
 * Instancia del cliente Prisma exportada globalmente.
 * Implementa el patrón Singleton para evitar múltiples instancias durante el hot-reloading de desarrollo.
 * Configura el adaptador de PostgreSQL y niveles de log.
 */
export const prisma =
  globalForPrisma.prisma ||
  new PrismaClient({
    adapter,
    log: ['query', 'error', 'warn'],
  });

if (process.env.NODE_ENV !== 'production') globalForPrisma.prisma = prisma;