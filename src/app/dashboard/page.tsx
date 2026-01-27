"use client";

import { useEffect, useState } from 'react';
import dynamic from 'next/dynamic';
import { Battery, Wifi, Activity, AlertTriangle } from 'lucide-react';

const Map = dynamic(() => import('@/components/Map'), { ssr: false });

interface Node {
    node_id: string;
    description: string;
    status: string;
    last_seen: string | null;
    sensor_readings: {
        battery_level: number;
        timestamp: string;
    }[];
}

export default function DashboardPage() {
    const [nodes, setNodes] = useState<Node[]>([]);
    const [isLoading, setIsLoading] = useState(true);

    useEffect(() => {
        const fetchNodes = async () => {
            try {
                const response = await fetch('/api/nodes');
                if (response.ok) {
                    const data = await response.json();
                    
                    // Process status locally for dashboard stats
                    const processedData = data.map((node: any) => {
                         let status = 'inactive';
                         if (node.last_seen) {
                             const diff = new Date().getTime() - new Date(node.last_seen).getTime();
                             if (diff < 30 * 60 * 1000) status = 'active';
                         }
                         return { ...node, status };
                    });
                    
                    setNodes(processedData);
                }
            } catch (error) {
                console.error("Error fetching nodes:", error);
            } finally {
                setIsLoading(false);
            }
        };

        fetchNodes();
        const interval = setInterval(fetchNodes, 30000); // Update every 30s
        return () => clearInterval(interval);
    }, []);

    const activeNodes = nodes.filter(n => n.status === 'active').length;
    const totalNodes = nodes.length;
    const lowBatteryNodes = nodes.filter(n => {
        const level = n.sensor_readings?.[0]?.battery_level;
        return level !== undefined && Number(level) < 20;
    }).length;

    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <h1 className="text-2xl font-bold text-gray-900">Panel de Control</h1>
                <span className="text-sm text-gray-500">Última actualización: {new Date().toLocaleTimeString()}</span>
            </div>

            {/* Stats Cards */}
            <div className="grid grid-cols-1 gap-5 sm:grid-cols-2 lg:grid-cols-4">
                <div className="bg-white overflow-hidden shadow rounded-lg">
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className="flex-shrink-0">
                                <Activity className="h-6 w-6 text-gray-400" />
                            </div>
                            <div className="ml-5 w-0 flex-1">
                                <dl>
                                    <dt className="text-sm font-medium text-gray-500 truncate">Nodos Totales</dt>
                                    <dd className="text-lg font-medium text-gray-900">{totalNodes}</dd>
                                </dl>
                            </div>
                        </div>
                    </div>
                </div>

                <div className="bg-white overflow-hidden shadow rounded-lg">
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className="flex-shrink-0">
                                <Wifi className={`h-6 w-6 ${activeNodes > 0 ? 'text-green-500' : 'text-gray-400'}`} />
                            </div>
                            <div className="ml-5 w-0 flex-1">
                                <dl>
                                    <dt className="text-sm font-medium text-gray-500 truncate">Nodos Activos</dt>
                                    <dd className="text-lg font-medium text-gray-900">{activeNodes}</dd>
                                </dl>
                            </div>
                        </div>
                    </div>
                </div>

                <div className="bg-white overflow-hidden shadow rounded-lg">
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className="flex-shrink-0">
                                <Battery className={`h-6 w-6 ${lowBatteryNodes > 0 ? 'text-red-500' : 'text-green-500'}`} />
                            </div>
                            <div className="ml-5 w-0 flex-1">
                                <dl>
                                    <dt className="text-sm font-medium text-gray-500 truncate">Batería Baja {'(<20%)'}</dt>
                                    <dd className="text-lg font-medium text-gray-900">{lowBatteryNodes}</dd>
                                </dl>
                            </div>
                        </div>
                    </div>
                </div>

                 <div className="bg-white overflow-hidden shadow rounded-lg">
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className="flex-shrink-0">
                                <AlertTriangle className="h-6 w-6 text-yellow-500" />
                            </div>
                            <div className="ml-5 w-0 flex-1">
                                <dl>
                                    <dt className="text-sm font-medium text-gray-500 truncate">Alertas Recientes</dt>
                                    <dd className="text-lg font-medium text-gray-900">0</dd>
                                </dl>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            {/* Map Section */}
            <div className="bg-white shadow rounded-lg p-6">
                <h2 className="text-lg font-medium text-gray-900 mb-4">Ubicación de Nodos en Tiempo Real</h2>
                <div className="h-[500px] w-full bg-gray-100 rounded-lg overflow-hidden border border-gray-200">
                    <Map showDetails={true} />
                </div>
            </div>
            
             {/* Node List Table */}
             <div className="bg-white shadow rounded-lg overflow-hidden">
                <div className="px-4 py-5 sm:px-6">
                    <h3 className="text-lg leading-6 font-medium text-gray-900">Estado de los Nodos</h3>
                </div>
                <div className="border-t border-gray-200">
                    <table className="min-w-full divide-y divide-gray-200">
                        <thead className="bg-gray-50">
                            <tr>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">ID</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Ubicación</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Estado</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Batería</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Última Conexión</th>
                            </tr>
                        </thead>
                        <tbody className="bg-white divide-y divide-gray-200">
                            {nodes.map((node) => (
                                <tr key={node.node_id}>
                                    <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">{node.node_id}</td>
                                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">{node.description}</td>
                                    <td className="px-6 py-4 whitespace-nowrap">
                                        <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${node.status === 'active' ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}`}>
                                            {node.status === 'active' ? 'Conectado' : 'Desconectado'}
                                        </span>
                                    </td>
                                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                                        {node.sensor_readings?.[0]?.battery_level ? `${node.sensor_readings[0].battery_level}%` : 'N/A'}
                                    </td>
                                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                                        {node.last_seen ? new Date(node.last_seen).toLocaleString() : 'Nunca'}
                                    </td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
             </div>
        </div>
    );
}
