"use client";

import { useEffect, useState, useCallback } from 'react';
import dynamic from 'next/dynamic';
import { 
    Battery, 
    Wifi, 
    Activity, 
    AlertTriangle, 
    ArrowRight, 
    CheckCircle2, 
    AlertCircle,
    Info
} from 'lucide-react';
import AlertsModal, { AlertLog, formatRelativeTime } from '@/components/dashboard/AlertsModal';
import NodesSummaryModal, { DashboardNode } from '@/components/dashboard/NodesSummaryModal';

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

/**
 * Página Principal del Dashboard
 * 
 * Muestra una vista general del estado del sistema, incluyendo:
 * - Tarjetas interactivas con estadísticas clave y modales de desglose.
 * - Monitor dinámico de alertas de red no atendidas (WARNING/ERROR).
 * - Mapa interactivo con la ubicación de los nodos.
 * - Tabla con el estado en tiempo real de cada nodo.
 */
export default function DashboardPage() {
    const [nodes, setNodes] = useState<Node[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    
    // Alertas dinámicas
    const [unreadAlertsCount, setUnreadAlertsCount] = useState<number>(0);
    const [previewAlerts, setPreviewAlerts] = useState<AlertLog[]>([]);
    
    // Estados para los modales interactivos
    const [isAlertsModalOpen, setIsAlertsModalOpen] = useState(false);
    const [nodesModalType, setNodesModalType] = useState<'total' | 'active' | 'battery' | null>(null);

    const fetchNodes = useCallback(async () => {
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
    }, []);

    const fetchAlertsSummary = useCallback(async () => {
        try {
            const response = await fetch('/api/network/logs?summary=true&levels=WARNING,ERROR&unread_only=true&limit=4');
            if (response.ok) {
                const data = await response.json();
                if (data && typeof data.unreadAlertsCount === 'number') {
                    setUnreadAlertsCount(data.unreadAlertsCount);
                    setPreviewAlerts(data.logs || []);
                }
            }
        } catch (error) {
            console.error("Error fetching alerts summary:", error);
        }
    }, []);

    useEffect(() => {
        fetchNodes();
        fetchAlertsSummary();

        // Actualizar cada 10s
        const interval = setInterval(() => {
            fetchNodes();
            fetchAlertsSummary();
        }, 10000);
        return () => clearInterval(interval);
    }, [fetchNodes, fetchAlertsSummary]);

    const activeNodes = nodes.filter(n => n.status === 'active').length;
    const totalNodes = nodes.length;
    const lowBatteryNodes = nodes.filter(n => {
        const level = n.sensor_readings?.[0]?.battery_level;
        return level !== undefined && Number(level) < 20;
    }).length;

    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <div>
                    <h1 className="text-2xl font-bold text-gray-900">Panel de Control</h1>
                    <p className="text-xs text-gray-500 mt-0.5">Monitoreo telemétrico, estado de flota e incidencias de red</p>
                </div>
                <div className="flex items-center gap-2">
                    <span className="inline-block w-2 h-2 rounded-full bg-green-500 animate-pulse" />
                    <span className="text-xs text-gray-500">En vivo · {new Date().toLocaleTimeString()}</span>
                </div>
            </div>

            {/* Stats Cards - Interactivas con Tooltips y Modales */}
            <div className="grid grid-cols-1 gap-5 sm:grid-cols-2 lg:grid-cols-4">
                
                {/* 1. Nodos Totales */}
                <div 
                    onClick={() => setNodesModalType('total')}
                    className="group relative bg-white overflow-hidden shadow-sm hover:shadow-md rounded-xl border border-gray-200 hover:border-blue-400 cursor-pointer transition-all duration-200 transform hover:-translate-y-0.5"
                >
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className="flex-shrink-0 p-3 bg-blue-50 group-hover:bg-blue-100 rounded-lg transition-colors">
                                <Activity className="h-6 w-6 text-blue-600" />
                            </div>
                            <div className="ml-4 w-0 flex-1">
                                <dl>
                                    <dt className="text-xs font-semibold text-gray-500 uppercase tracking-wider">Nodos Totales</dt>
                                    <dd className="text-2xl font-bold text-gray-900 mt-0.5">{totalNodes}</dd>
                                </dl>
                            </div>
                            <ArrowRight className="w-4 h-4 text-gray-400 opacity-0 group-hover:opacity-100 group-hover:translate-x-1 transition-all" />
                        </div>
                        <div className="mt-3 pt-3 border-t border-gray-100 flex items-center justify-between text-xs text-gray-500">
                            <span>{activeNodes} activos · {totalNodes - activeNodes} inactivos</span>
                            <span className="text-blue-600 font-medium group-hover:underline">Ver detalle</span>
                        </div>
                    </div>

                    {/* Tooltip Hover */}
                    <div className="pointer-events-none absolute bottom-full left-1/2 -translate-x-1/2 mb-2 hidden group-hover:flex flex-col items-center z-30">
                        <div className="bg-gray-900 text-white text-xs rounded-lg py-1.5 px-3 shadow-lg whitespace-nowrap">
                            <span>Inventario de nodos registrados. Clic para ver desglose.</span>
                        </div>
                        <div className="w-2 h-2 bg-gray-900 rotate-45 -mt-1" />
                    </div>
                </div>

                {/* 2. Nodos Activos */}
                <div 
                    onClick={() => setNodesModalType('active')}
                    className="group relative bg-white overflow-hidden shadow-sm hover:shadow-md rounded-xl border border-gray-200 hover:border-green-400 cursor-pointer transition-all duration-200 transform hover:-translate-y-0.5"
                >
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className={`flex-shrink-0 p-3 rounded-lg transition-colors ${
                                activeNodes > 0 ? 'bg-green-50 group-hover:bg-green-100' : 'bg-gray-100'
                            }`}>
                                <Wifi className={`h-6 w-6 ${activeNodes > 0 ? 'text-green-600' : 'text-gray-400'}`} />
                            </div>
                            <div className="ml-4 w-0 flex-1">
                                <dl>
                                    <dt className="text-xs font-semibold text-gray-500 uppercase tracking-wider">Nodos Activos</dt>
                                    <dd className="text-2xl font-bold text-gray-900 mt-0.5">{activeNodes}</dd>
                                </dl>
                            </div>
                            <ArrowRight className="w-4 h-4 text-gray-400 opacity-0 group-hover:opacity-100 group-hover:translate-x-1 transition-all" />
                        </div>
                        <div className="mt-3 pt-3 border-t border-gray-100 flex items-center justify-between text-xs text-gray-500">
                            <span>{totalNodes > 0 ? Math.round((activeNodes / totalNodes) * 100) : 0}% de operatividad</span>
                            <span className="text-green-600 font-medium group-hover:underline">Ver en línea</span>
                        </div>
                    </div>

                    {/* Tooltip Hover */}
                    <div className="pointer-events-none absolute bottom-full left-1/2 -translate-x-1/2 mb-2 hidden group-hover:flex flex-col items-center z-30">
                        <div className="bg-gray-900 text-white text-xs rounded-lg py-1.5 px-3 shadow-lg whitespace-nowrap">
                            <span>Nodos reportando en los últimos 30 min. Clic para ver lista.</span>
                        </div>
                        <div className="w-2 h-2 bg-gray-900 rotate-45 -mt-1" />
                    </div>
                </div>

                {/* 3. Batería Baja */}
                <div 
                    onClick={() => setNodesModalType('battery')}
                    className={`group relative bg-white overflow-hidden shadow-sm hover:shadow-md rounded-xl border transition-all duration-200 transform hover:-translate-y-0.5 cursor-pointer ${
                        lowBatteryNodes > 0 
                            ? 'border-amber-200 hover:border-red-400 bg-amber-50/20' 
                            : 'border-gray-200 hover:border-green-400'
                    }`}
                >
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className={`flex-shrink-0 p-3 rounded-lg transition-colors ${
                                lowBatteryNodes > 0 ? 'bg-red-50 group-hover:bg-red-100' : 'bg-green-50 group-hover:bg-green-100'
                            }`}>
                                <Battery className={`h-6 w-6 ${lowBatteryNodes > 0 ? 'text-red-500' : 'text-green-600'}`} />
                            </div>
                            <div className="ml-4 w-0 flex-1">
                                <dl>
                                    <dt className="text-xs font-semibold text-gray-500 uppercase tracking-wider">Batería Baja {'(<20%)'}</dt>
                                    <dd className={`text-2xl font-bold mt-0.5 ${lowBatteryNodes > 0 ? 'text-red-600' : 'text-gray-900'}`}>
                                        {lowBatteryNodes}
                                    </dd>
                                </dl>
                            </div>
                            <ArrowRight className="w-4 h-4 text-gray-400 opacity-0 group-hover:opacity-100 group-hover:translate-x-1 transition-all" />
                        </div>
                        <div className="mt-3 pt-3 border-t border-gray-100 flex items-center justify-between text-xs text-gray-500">
                            <span>{lowBatteryNodes > 0 ? 'Requieren revisión' : 'Baterías saludables'}</span>
                            <span className={`${lowBatteryNodes > 0 ? 'text-red-600' : 'text-green-600'} font-medium group-hover:underline`}>
                                Ver lista
                            </span>
                        </div>
                    </div>

                    {/* Tooltip Hover */}
                    <div className="pointer-events-none absolute bottom-full left-1/2 -translate-x-1/2 mb-2 hidden group-hover:flex flex-col items-center z-30">
                        <div className="bg-gray-900 text-white text-xs rounded-lg py-1.5 px-3 shadow-lg whitespace-nowrap">
                            <span>{lowBatteryNodes > 0 ? `${lowBatteryNodes} nodos con carga crítica. Clic para identificarlos.` : 'Sin incidencias de energía.'}</span>
                        </div>
                        <div className="w-2 h-2 bg-gray-900 rotate-45 -mt-1" />
                    </div>
                </div>

                {/* 4. Alertas Recientes (Conectado a WARNING/ERROR no atendidos) */}
                <div 
                    onClick={() => setIsAlertsModalOpen(true)}
                    className={`group relative bg-white overflow-hidden shadow-sm hover:shadow-md rounded-xl border transition-all duration-200 transform hover:-translate-y-0.5 cursor-pointer ${
                        unreadAlertsCount > 0 
                            ? 'border-yellow-300 hover:border-yellow-500 ring-2 ring-yellow-400/20 bg-yellow-50/20' 
                            : 'border-gray-200 hover:border-gray-400'
                    }`}
                >
                    <div className="p-5">
                        <div className="flex items-center">
                            <div className={`flex-shrink-0 p-3 rounded-lg transition-colors ${
                                unreadAlertsCount > 0 ? 'bg-yellow-100 text-yellow-700' : 'bg-gray-100 text-gray-500'
                            }`}>
                                <AlertTriangle className={`h-6 w-6 ${unreadAlertsCount > 0 ? 'text-yellow-600 animate-pulse' : 'text-gray-400'}`} />
                            </div>
                            <div className="ml-4 w-0 flex-1">
                                <dl>
                                    <div className="flex items-center justify-between">
                                        <dt className="text-xs font-semibold text-gray-500 uppercase tracking-wider">Alertas Recientes</dt>
                                        {unreadAlertsCount > 0 && (
                                            <span className="inline-flex items-center px-1.5 py-0.5 rounded text-[10px] font-bold bg-yellow-100 text-yellow-800">
                                                Sin atender
                                            </span>
                                        )}
                                    </div>
                                    <dd className={`text-2xl font-bold mt-0.5 ${unreadAlertsCount > 0 ? 'text-yellow-700' : 'text-gray-900'}`}>
                                        {unreadAlertsCount}
                                    </dd>
                                </dl>
                            </div>
                            <ArrowRight className="w-4 h-4 text-gray-400 opacity-0 group-hover:opacity-100 group-hover:translate-x-1 transition-all" />
                        </div>
                        <div className="mt-3 pt-3 border-t border-gray-100 flex items-center justify-between text-xs text-gray-500">
                            <span>
                                {unreadAlertsCount > 0 
                                    ? `${unreadAlertsCount} incidencia${unreadAlertsCount > 1 ? 's' : ''} pendiente${unreadAlertsCount > 1 ? 's' : ''}`
                                    : 'Sin alertas pendientes'}
                            </span>
                            <span className="text-yellow-700 font-medium group-hover:underline">
                                {unreadAlertsCount > 0 ? 'Revisar' : 'Historial'}
                            </span>
                        </div>
                    </div>

                    {/* Tooltip Hover con Resumen Rápido */}
                    <div className="pointer-events-none absolute bottom-full right-0 sm:left-1/2 sm:-translate-x-1/2 mb-2 hidden group-hover:flex flex-col items-center z-30 w-72">
                        <div className="bg-gray-900 text-white text-xs rounded-xl p-3 shadow-xl w-full">
                            <div className="flex items-center justify-between border-b border-gray-700 pb-1.5 mb-2 font-semibold">
                                <span>Alertas de Red (WARNING / ERROR)</span>
                                <span className={unreadAlertsCount > 0 ? 'text-yellow-400' : 'text-green-400'}>
                                    {unreadAlertsCount} no atendidas
                                </span>
                            </div>
                            {unreadAlertsCount === 0 ? (
                                <p className="text-gray-300 text-[11px] flex items-center gap-1.5">
                                    <CheckCircle2 className="w-3.5 h-3.5 text-green-400" />
                                    No hay advertencias ni errores pendientes.
                                </p>
                            ) : (
                                <div className="space-y-1.5">
                                    {previewAlerts.slice(0, 2).map((a) => (
                                        <div key={a.log_id} className="text-[11px] leading-tight">
                                            <div className="flex items-center gap-1 font-medium">
                                                <span className={a.level === 'ERROR' ? 'text-red-400' : 'text-yellow-400'}>
                                                    ● [{a.level}]
                                                </span>
                                                <span className="text-gray-400 text-[10px]">{formatRelativeTime(a.timestamp)}</span>
                                            </div>
                                            <p className="text-gray-300 truncate">{a.message}</p>
                                        </div>
                                    ))}
                                    {unreadAlertsCount > 2 && (
                                        <p className="text-[10px] text-gray-400 pt-1 text-center font-medium">
                                            +{unreadAlertsCount - 2} alerta(s) más...
                                        </p>
                                    )}
                                </div>
                            )}
                            <div className="mt-2 pt-2 border-t border-gray-800 text-[10px] text-blue-300 text-center font-semibold">
                                Clic para abrir el panel de atención
                            </div>
                        </div>
                        <div className="w-2 h-2 bg-gray-900 rotate-45 -mt-1" />
                    </div>
                </div>

            </div>

            {/* Modales Interactivos */}
            <AlertsModal 
                isOpen={isAlertsModalOpen}
                onClose={() => setIsAlertsModalOpen(false)}
                onAlertAcknowledged={() => {
                    fetchAlertsSummary();
                }}
            />

            <NodesSummaryModal 
                isOpen={nodesModalType !== null}
                onClose={() => setNodesModalType(null)}
                type={nodesModalType}
                nodes={nodes}
            />

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
