import { prisma } from '@/lib/prisma';
import NodesManager from '@/components/nodes/NodesManager';
import { getServerSession } from 'next-auth';

// Force dynamic rendering to ensure fresh data
export const dynamic = 'force-dynamic';

export default async function NodesDashboardPage() {
  const session = await getServerSession();

  const nodes = await prisma.node.findMany({
    orderBy: { node_id: 'asc' }
  });

  // Prisma Decimal types need to be serializable for Client Components
  const serializedNodes = nodes.map(node => ({
    ...node,
    latitude: node.latitude.toNumber(),
    longitude: node.longitude.toNumber(),
    // Dates are serializable by default to ISO strings if passed to client? 
    // Actually, Date objects are fine in server->client props in Next 13+ usually, but converting to strings is safer.
    // However, NodeTable expects Date | null. Next.js supports Date in props.
  }));

  return (
    <div className="p-6 w-full max-w-full mx-auto space-y-8">
      <div>
        <h1 className="text-3xl font-bold text-gray-900 dark:text-white mb-2">Gestión de Nodos</h1>
        <p className="text-gray-600 dark:text-gray-400">
          Administra los dispositivos físicos, monitorea su estado y genera claves de acceso seguras.
        </p>
      </div>

      <NodesManager nodes={serializedNodes} />
    </div>
  );
}
