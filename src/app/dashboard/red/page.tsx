import { prisma } from '@/lib/prisma';
import NetworkManager from '@/components/network/NetworkManager';

export const dynamic = 'force-dynamic';

export default async function NetworkPage() {
    // Buscar todos los nodos sensores excepto el Concentrador (NODE_C)
    const nodes = await prisma.node.findMany({
        where: {
            node_id: {
                not: 'NODE_C'
            },
            is_active: true
        },
        orderBy: { node_id: 'asc' }
    });

    const serializedNodes = nodes.map(node => ({
        node_id: node.node_id,
        description: node.description,
    }));

    return (
        <div className="p-6 w-full max-w-full mx-auto space-y-8">
            <div>
                <h1 className="text-3xl font-bold text-gray-900 mb-2">Monitor de Red y Comandos</h1>
                <p className="text-gray-600">
                    Inspecciona el tráfico de red, audita el estado del concentrador y envía comandos remotos a los nodos sensores.
                </p>
            </div>

            <NetworkManager nodes={serializedNodes} />
        </div>
    );
}
