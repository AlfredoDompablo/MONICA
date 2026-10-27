"use client";

import React, { useState, useEffect, useCallback } from 'react';
import Link from 'next/link';
import { 
    AlertTriangle, 
    AlertCircle, 
    CheckCircle2, 
    Clock, 
    X, 
    ExternalLink, 
    RefreshCw, 
    CheckCheck,
    UserCheck,
    Radio
} from 'lucide-react';

export interface AlertLog {
    log_id: number;
    timestamp: string;
    level: string; // 'WARNING' | 'ERROR'
    message: string;
    node_id: string | null;
    is_read: boolean;
    acknowledged_at: string | null;
    acknowledged_by: string | null;
}

interface AlertsModalProps {
    isOpen: boolean;
    onClose: () => void;
    onAlertAcknowledged?: () => void;
}

export function formatRelativeTime(dateString: string): string {
    try {
        const now = new Date().getTime();
        const past = new Date(dateString).getTime();
        if (isNaN(past)) return 'recientemente';
        const diffMs = Math.max(0, now - past);
        const diffSec = Math.floor(diffMs / 1000);
        const diffMin = Math.floor(diffSec / 60);
        const diffHours = Math.floor(diffMin / 60);
        const diffDays = Math.floor(diffHours / 24);

        if (diffSec < 60) return 'hace unos momentos';
        if (diffMin === 1) return 'hace 1 minuto';
        if (diffMin < 60) return `hace ${diffMin} minutos`;
        if (diffHours === 1) return 'hace 1 hora';
        if (diffHours < 24) return `hace ${diffHours} horas`;
        if (diffDays === 1) return 'hace 1 día';
        return `hace ${diffDays} días`;
    } catch {
        return 'fecha desconocida';
    }
}

export default function AlertsModal({ isOpen, onClose, onAlertAcknowledged }: AlertsModalProps) {
    const [alerts, setAlerts] = useState<AlertLog[]>([]);
    const [loading, setLoading] = useState(false);
    const [filterTab, setFilterTab] = useState<'unread' | 'all'>('unread');
    const [actionLoadingId, setActionLoadingId] = useState<number | null>(null);
    const [isAcknowledgingAll, setIsAcknowledgingAll] = useState(false);

    const fetchAlerts = useCallback(async () => {
        setLoading(true);
        try {
            const res = await fetch('/api/network/logs?summary=true&levels=WARNING,ERROR&limit=100');
            if (res.ok) {
                const data = await res.json();
                const list = Array.isArray(data) ? data : (data.logs || []);
                setAlerts(list);
            }
        } catch (error) {
            console.error('Error cargando alertas:', error);
        } finally {
            setLoading(false);
        }
    }, []);

    useEffect(() => {
        if (isOpen) {
            fetchAlerts();
        }
    }, [isOpen, fetchAlerts]);

    if (!isOpen) return null;

    const unreadAlerts = alerts.filter(a => !a.is_read);
    const displayedAlerts = filterTab === 'unread' ? unreadAlerts : alerts;

    const handleAcknowledge = async (logId: number) => {
        setActionLoadingId(logId);
        try {
            const res = await fetch('/api/network/logs', {
                method: 'PATCH',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ log_id: logId }),
            });

            if (res.ok) {
                const data = await res.json();
                setAlerts(prev => prev.map(a => 
                    a.log_id === logId 
                        ? { 
                            ...a, 
                            is_read: true, 
                            acknowledged_at: data.log?.acknowledged_at || new Date().toISOString(),
                            acknowledged_by: data.log?.acknowledged_by || 'Administrador'
                          } 
                        : a
                ));
                if (onAlertAcknowledged) onAlertAcknowledged();
            }
        } catch (err) {
            console.error('Error al atender alerta:', err);
        } finally {
            setActionLoadingId(null);
        }
    };

    const handleAcknowledgeAll = async () => {
        if (unreadAlerts.length === 0) return;
        setIsAcknowledgingAll(true);
        try {
            const res = await fetch('/api/network/logs', {
                method: 'PATCH',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ mark_all: true }),
            });

            if (res.ok) {
                const data = await res.json();
                setAlerts(prev => prev.map(a => ({
                    ...a,
                    is_read: true,
                    acknowledged_at: data.acknowledged_at || new Date().toISOString(),
                    acknowledged_by: data.acknowledged_by || 'Administrador'
                })));
                if (onAlertAcknowledged) onAlertAcknowledged();
            }
        } catch (err) {
            console.error('Error al atender todas las alertas:', err);
        } finally {
            setIsAcknowledgingAll(false);
        }
    };

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/50 backdrop-blur-sm animate-fade-in">
            <div 
                className="bg-white rounded-2xl shadow-2xl border border-gray-200 w-full max-w-3xl max-h-[90vh] flex flex-col overflow-hidden animate-scale-in"
                onClick={(e) => e.stopPropagation()}
            >
                {/* Header */}
                <div className="px-6 py-4 bg-gradient-to-r from-gray-50 to-white border-b border-gray-200 flex items-center justify-between">
                    <div className="flex items-center space-x-3">
                        <div className="p-2.5 bg-yellow-100 rounded-xl text-yellow-700">
                            <AlertTriangle className="h-6 w-6" />
                        </div>
                        <div>
                            <div className="flex items-center gap-2">
                                <h2 className="text-xl font-bold text-gray-900">Centro de Alertas de Red</h2>
                                {unreadAlerts.length > 0 ? (
                                    <span className="px-2.5 py-0.5 rounded-full text-xs font-bold bg-red-100 text-red-700 animate-pulse">
                                        {unreadAlerts.length} pendientes
                                    </span>
                                ) : (
                                    <span className="px-2.5 py-0.5 rounded-full text-xs font-semibold bg-green-100 text-green-700">
                                        Al día
                                    </span>
                                )}
                            </div>
                            <p className="text-xs text-gray-500">
                                Eventos clasificados como advertencia (WARNING) y error crítico (ERROR) reportados por el concentrador.
                            </p>
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

                {/* Sub-bar: Tabs & Actions */}
                <div className="px-6 py-3 bg-gray-50/70 border-b border-gray-200 flex flex-wrap items-center justify-between gap-3">
                    <div className="flex space-x-1 bg-gray-200/80 p-1 rounded-lg">
                        <button
                            onClick={() => setFilterTab('unread')}
                            className={`px-3 py-1.5 text-xs font-semibold rounded-md transition-all ${
                                filterTab === 'unread'
                                    ? 'bg-white text-gray-900 shadow-sm'
                                    : 'text-gray-600 hover:text-gray-900'
                            }`}
                        >
                            Pendientes ({unreadAlerts.length})
                        </button>
                        <button
                            onClick={() => setFilterTab('all')}
                            className={`px-3 py-1.5 text-xs font-semibold rounded-md transition-all ${
                                filterTab === 'all'
                                    ? 'bg-white text-gray-900 shadow-sm'
                                    : 'text-gray-600 hover:text-gray-900'
                            }`}
                        >
                            Historial Completo ({alerts.length})
                        </button>
                    </div>

                    <div className="flex items-center gap-2">
                        <button
                            onClick={fetchAlerts}
                            disabled={loading}
                            className="p-1.5 text-gray-500 hover:text-gray-700 hover:bg-gray-200 rounded-lg transition-colors text-xs inline-flex items-center gap-1"
                            title="Recargar alertas"
                        >
                            <RefreshCw className={`w-3.5 h-3.5 ${loading ? 'animate-spin' : ''}`} />
                            <span className="hidden sm:inline">Refrescar</span>
                        </button>

                        {unreadAlerts.length > 0 && (
                            <button
                                onClick={handleAcknowledgeAll}
                                disabled={isAcknowledgingAll}
                                className="px-3 py-1.5 bg-blue-600 hover:bg-blue-700 text-white rounded-lg text-xs font-semibold flex items-center gap-1.5 transition-colors shadow-sm disabled:opacity-50"
                            >
                                <CheckCheck className="w-3.5 h-3.5" />
                                <span>{isAcknowledgingAll ? 'Atendiendo...' : 'Marcar todas atendidas'}</span>
                            </button>
                        )}
                    </div>
                </div>

                {/* Content: Alerts list */}
                <div className="flex-1 overflow-y-auto p-6 space-y-3 max-h-[55vh]">
                    {loading && alerts.length === 0 ? (
                        <div className="py-12 flex flex-col items-center justify-center text-gray-500">
                            <RefreshCw className="w-8 h-8 animate-spin text-blue-500 mb-2" />
                            <p className="text-sm">Cargando eventos de alerta...</p>
                        </div>
                    ) : displayedAlerts.length === 0 ? (
                        <div className="py-12 flex flex-col items-center justify-center text-center">
                            <div className="w-14 h-14 bg-green-50 text-green-600 rounded-full flex items-center justify-center mb-3">
                                <CheckCircle2 className="w-7 h-7" />
                            </div>
                            <h3 className="text-base font-semibold text-gray-900">
                                {filterTab === 'unread' ? '¡No hay alertas pendientes!' : 'No hay registros de alerta'}
                            </h3>
                            <p className="text-sm text-gray-500 max-w-sm mt-1">
                                {filterTab === 'unread'
                                    ? 'Todas las advertencias y errores han sido atendidos por el equipo.'
                                    : 'Aún no se han recibido eventos clasificados con nivel de alerta.'}
                            </p>
                        </div>
                    ) : (
                        displayedAlerts.map((alert) => {
                            const isError = alert.level === 'ERROR';
                            const isUnread = !alert.is_read;

                            return (
                                <div
                                    key={alert.log_id}
                                    className={`relative p-4 rounded-xl border transition-all ${
                                        isUnread
                                            ? isError
                                                ? 'bg-red-50/60 border-red-200 shadow-sm'
                                                : 'bg-amber-50/60 border-amber-200 shadow-sm'
                                            : 'bg-gray-50/70 border-gray-200 opacity-80'
                                    }`}
                                >
                                    <div className="flex items-start justify-between gap-3">
                                        <div className="flex items-start gap-3">
                                            <div className="mt-0.5 flex-shrink-0">
                                                {isError ? (
                                                    <div className="p-1.5 bg-red-100 text-red-600 rounded-lg">
                                                        <AlertCircle className="w-5 h-5" />
                                                    </div>
                                                ) : (
                                                    <div className="p-1.5 bg-amber-100 text-amber-600 rounded-lg">
                                                        <AlertTriangle className="w-5 h-5" />
                                                    </div>
                                                )}
                                            </div>

                                            <div>
                                                <div className="flex flex-wrap items-center gap-2 mb-1">
                                                    <span className={`px-2 py-0.5 rounded text-[11px] font-bold ${
                                                        isError 
                                                            ? 'bg-red-200 text-red-800' 
                                                            : 'bg-amber-200 text-amber-800'
                                                    }`}>
                                                        {alert.level}
                                                    </span>

                                                    {alert.node_id && (
                                                        <span className="px-2 py-0.5 rounded text-[11px] font-semibold bg-gray-200 text-gray-800 inline-flex items-center gap-1">
                                                            <Radio className="w-3 h-3 text-gray-500" />
                                                            {alert.node_id}
                                                        </span>
                                                    )}

                                                    <span className="text-xs text-gray-500 inline-flex items-center gap-1">
                                                        <Clock className="w-3 h-3" />
                                                        {formatRelativeTime(alert.timestamp)}
                                                    </span>

                                                    <span className="text-[11px] text-gray-400" title={new Date(alert.timestamp).toLocaleString()}>
                                                        ({new Date(alert.timestamp).toLocaleTimeString()})
                                                    </span>
                                                </div>

                                                <p className="text-sm font-medium text-gray-900 leading-snug">
                                                    {alert.message}
                                                </p>

                                                {/* Auditoría si ya fue atendida */}
                                                {!isUnread && alert.acknowledged_by && (
                                                    <div className="mt-2 text-xs text-green-700 bg-green-50/80 inline-flex items-center gap-1.5 px-2 py-1 rounded border border-green-200">
                                                        <UserCheck className="w-3.5 h-3.5" />
                                                        <span>
                                                            Atendida por <strong>{alert.acknowledged_by}</strong> {alert.acknowledged_at ? formatRelativeTime(alert.acknowledged_at) : ''}
                                                        </span>
                                                    </div>
                                                )}
                                            </div>
                                        </div>

                                        {/* Botón de Atender individual */}
                                        {isUnread && (
                                            <button
                                                onClick={() => handleAcknowledge(alert.log_id)}
                                                disabled={actionLoadingId === alert.log_id}
                                                className="flex-shrink-0 px-3 py-1.5 bg-white hover:bg-gray-100 text-gray-700 border border-gray-300 rounded-lg text-xs font-semibold flex items-center gap-1 shadow-sm transition-all hover:border-gray-400 disabled:opacity-50"
                                            >
                                                <CheckCircle2 className="w-3.5 h-3.5 text-green-600" />
                                                <span>{actionLoadingId === alert.log_id ? 'Guardando...' : 'Atender'}</span>
                                            </button>
                                        )}
                                    </div>
                                </div>
                            );
                        })
                    )}
                </div>

                {/* Footer */}
                <div className="px-6 py-4 bg-gray-50 border-t border-gray-200 flex items-center justify-between">
                    <Link
                        href="/dashboard/red"
                        className="text-xs text-[#1e3570] hover:text-blue-800 font-semibold inline-flex items-center gap-1 group"
                    >
                        <span>Abrir Monitor de Red y Comandos</span>
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
