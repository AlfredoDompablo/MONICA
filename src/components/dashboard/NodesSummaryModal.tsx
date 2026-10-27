"use client";

import React from 'react';
import Link from 'next/link';
import { 
    Activity, 
    Wifi, 
    Battery, 
    BatteryWarning, 
    X, 
    ExternalLink, 
    Clock, 
    CheckCircle2, 
    XCircle,
    Cpu
} from 'lucide-react';
import { formatRelativeTime } from './AlertsModal';

export interface DashboardNode {
    node_id: string;
    description: string;
    status: string;
    last_seen: string | null;
    sensor_readings?: {
        battery_level: number;
        timestamp: string;
    }[];
}

interface NodesSummaryModalProps {
    isOpen: boolean;
    onClose: () => void;
    type: 'total' | 'active' | 'battery' | null;
    nodes: DashboardNode[];
}

export default function NodesSummaryModal({ isOpen, onClose, type, nodes }: NodesSummaryModalProps) {
    if (!isOpen || !type) return null;

    // Filter list according to modal type
    let filteredNodes = nodes;
    let title = 'Nodos Totales del Sistema';
    let description = 'Inventario general de estaciones de monitoreo registradas.';
    let Icon = Activity;
    let iconColor = 'text-blue-600 bg-blue-100';

    if (type === 'active') {
        filteredNodes = nodes.filter(n => n.status === 'active');
        title = 'Nodos Activos en Red';
        description = 'Estaciones que han reportado paquetes de telemetría en los últimos 30 minutos.';
        Icon = Wifi;
        iconColor = 'text-green-600 bg-green-100';
    } else if (type === 'battery') {
        filteredNodes = nodes.filter(n => {
            const level = n.sensor_readings?.[0]?.battery_level;
            return level !== undefined && Number(level) < 20;
        });
        title = 'Nodos con Batería Baja (< 20%)';
        description = 'Dispositivos con nivel crítico de energía que requieren recarga solar o reemplazo de celda.';
        Icon = BatteryWarning;
        iconColor = 'text-red-600 bg-red-100';
    }

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/50 backdrop-blur-sm animate-fade-in">
            <div 
                className="bg-white rounded-2xl shadow-2xl border border-gray-200 w-full max-w-3xl max-h-[90vh] flex flex-col overflow-hidden animate-scale-in"
                onClick={(e) => e.stopPropagation()}
            >
                {/* Header */}
                <div className="px-6 py-4 bg-gradient-to-r from-gray-50 to-white border-b border-gray-200 flex items-center justify-between">
                    <div className="flex items-center space-x-3">
                        <div className={`p-2.5 rounded-xl ${iconColor}`}>
                            <Icon className="h-6 w-6" />
                        </div>
                        <div>
                            <div className="flex items-center gap-2">
                                <h2 className="text-xl font-bold text-gray-900">{title}</h2>
                                <span className="px-2.5 py-0.5 rounded-full text-xs font-bold bg-gray-100 text-gray-700">
                                    {filteredNodes.length} {filteredNodes.length === 1 ? 'nodo' : 'nodos'}
                                </span>
                            </div>
                            <p className="text-xs text-gray-500">{description}</p>
                        </div>
                    </div>
                    <button 
                        onClick={onClose}
                        className="p-2 text-gray-400 hover:text-gray-600 hover:bg-gray-100 rounded-lg transition-colors"
                        title="Cerrar modal"
                    >
                        <X className="w-5 h-5" />
                    </button>
                </div>

                {/* Content Table / List */}
                <div className="flex-1 overflow-y-auto p-6 max-h-[55vh]">
                    {filteredNodes.length === 0 ? (
                        <div className="py-12 flex flex-col items-center justify-center text-center">
                            <div className="w-14 h-14 bg-gray-50 text-gray-400 rounded-full flex items-center justify-center mb-3">
                                <Cpu className="w-7 h-7" />
                            </div>
                            <h3 className="text-base font-semibold text-gray-900">
                                No hay nodos en esta categoría
                            </h3>
                            <p className="text-sm text-gray-500 max-w-sm mt-1">
                                {type === 'battery'
                                    ? 'Todos los nodos reportan un nivel de batería óptimo o no tienen telemetría crítica.'
                                    : 'No se encontraron dispositivos que coincidan con el criterio seleccionado.'}
                            </p>
                        </div>
                    ) : (
                        <div className="space-y-3">
                            {filteredNodes.map((node) => {
                                const battery = node.sensor_readings?.[0]?.battery_level;
                                const hasBattery = battery !== undefined && battery !== null;
                                const isLowBattery = hasBattery && Number(battery) < 20;

                                return (
                                    <div 
                                        key={node.node_id}
                                        className="p-4 bg-gray-50/60 border border-gray-200 rounded-xl hover:bg-white hover:border-gray-300 transition-all flex flex-col sm:flex-row sm:items-center justify-between gap-3"
                                    >
                                        <div className="flex items-start sm:items-center gap-3">
                                            <div className="p-2 bg-white rounded-lg border border-gray-200 shadow-xs">
                                                <Cpu className="w-5 h-5 text-gray-700" />
                                            </div>
                                            <div>
                                                <div className="flex items-center gap-2">
                                                    <span className="font-bold text-gray-900 text-sm">
                                                        {node.node_id}
                                                    </span>
                                                    <span className={`px-2 py-0.5 rounded-full text-[11px] font-semibold flex items-center gap-1 ${
                                                        node.status === 'active'
                                                            ? 'bg-green-100 text-green-800'
                                                            : 'bg-gray-200 text-gray-700'
                                                    }`}>
                                                        {node.status === 'active' ? (
                                                            <CheckCircle2 className="w-3 h-3 text-green-600" />
                                                        ) : (
                                                            <XCircle className="w-3 h-3 text-gray-500" />
                                                        )}
                                                        {node.status === 'active' ? 'En línea' : 'Inactivo'}
                                                    </span>
                                                </div>
                                                <p className="text-xs text-gray-500 mt-0.5">{node.description || 'Sin ubicación descrita'}</p>
                                            </div>
                                        </div>

                                        <div className="flex items-center gap-4 text-xs">
                                            {/* Batería */}
                                            <div className="flex items-center gap-2 min-w-[120px]">
                                                <Battery className={`w-4 h-4 ${isLowBattery ? 'text-red-500' : 'text-green-600'}`} />
                                                <div className="w-full">
                                                    <div className="flex justify-between text-[11px] font-semibold mb-1">
                                                        <span className="text-gray-500">Batería:</span>
                                                        <span className={isLowBattery ? 'text-red-600' : 'text-gray-900'}>
                                                            {hasBattery ? `${battery}%` : 'N/A'}
                                                        </span>
                                                    </div>
                                                    {hasBattery && (
                                                        <div className="w-full bg-gray-200 rounded-full h-1.5 overflow-hidden">
                                                            <div 
                                                                className={`h-1.5 rounded-full transition-all ${
                                                                    isLowBattery ? 'bg-red-500' : 'bg-green-500'
                                                                }`}
                                                                style={{ width: `${Math.min(100, Math.max(0, Number(battery)))}%` }}
                                                            />
                                                        </div>
                                                    )}
                                                </div>
                                            </div>

                                            {/* Última conexión */}
                                            <div className="text-right text-gray-500 min-w-[120px]">
                                                <div className="flex items-center justify-end gap-1 text-[11px]">
                                                    <Clock className="w-3 h-3 text-gray-400" />
                                                    <span>{node.last_seen ? formatRelativeTime(node.last_seen) : 'Nunca'}</span>
                                                </div>
                                                <div className="text-[10px] text-gray-400">
                                                    {node.last_seen ? new Date(node.last_seen).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : ''}
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                );
                            })}
                        </div>
                    )}
                </div>

                {/* Footer */}
                <div className="px-6 py-4 bg-gray-50 border-t border-gray-200 flex items-center justify-between">
                    <Link
                        href="/dashboard/nodes"
                        className="text-xs text-[#1e3570] hover:text-blue-800 font-semibold inline-flex items-center gap-1 group"
                    >
                        <span>Abrir Gestión y Configuración de Nodos</span>
                        <ExternalLink className="w-3.5 h-3.5 group-hover:translate-x-0.5 transition-transform" />
                    </Link>

                    <button
                        onClick={onClose}
                        className="px-4 py-2 bg-gray-200 hover:bg-gray-300 text-gray-800 rounded-lg text-xs font-semibold transition-colors"
                    >
                        Cerrar
                    </button>
                </div>
            </div>
        </div>
    );
}
