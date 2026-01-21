import { PrismaClient } from '@prisma/client';

const globalForPrisma = global as unknown as { prisma: PrismaClient };

// En versiones recientes, no se usa un objeto anidado para la URL en el constructor
// simplemente se deja que Prisma la tome del entorno o se pasa vía 'datasourceUrl'
export const prisma =
  globalForPrisma.prisma ||
  new PrismaClient({
    log: ['query', 'error', 'warn'],
  });

if (process.env.NODE_ENV !== 'production') globalForPrisma.prisma = prisma;