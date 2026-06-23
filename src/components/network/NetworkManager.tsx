"use client";

import React, { useState, useEffect, useCallback } from 'react';
import { 
    Terminal, 
    Send, 
    RefreshCw, 
    AlertTriangle, 
    CheckCircle2, 
    Play, 
    Activity, 
    Camera, 
    Radio, 
    Trash2 
} from 'lucide-react';

interface NodeInfo {
    node_id: string;
    description: string;
}

interface NetworkLog {
    log_id: number;
    timestamp: string;
    level: string;
    message: string;
    node_id: string | null;
}

interface NetworkCommand {
    command_id: number;
    timestamp: string;
    type: string;
    target_node_id: string;
    status: string;
    response: string | null;
}

export default function NetworkManager({ nodes }: { nodes: NodeInfo[] }) {
    const [selectedNode, setSelectedNode] = useState<string>(nodes[0]?.node_id || '');
    const [commandType, setCommandType] = useState<string>('POLL_TELEMETRY');
    const [logs, setLogs] = useState<NetworkLog[]>([]);
    const [commands, setCommands] = useState<NetworkCommand[]>([]);
    const [filterLevel, setFilterLevel] = useState<string>('ALL');
    const [isSending, setIsSending] = useState(false);
    const [isRefreshing, setIsRefreshing] = useState(false);

    // Cargar Logs y Comandos desde la API
    const fetchData = useCallback(async () => {
        try {
            const logsRes = await fetch('/api/network/logs?limit=50');
            const commandsRes = await fetch('/api/network/commands?limit=10');

            if (logsRes.ok && commandsRes.ok) {
                const logsData = await logsRes.json();
                const commandsData = await commandsRes.json();
                setLogs(logsData);
                setCommands(commandsData);
            }
        } catch (error) {
            console.error('Error fetching network data:', error);
        }
    }, []);

    // Configurar polling automático cada 3 segundos
    useEffect(() => {
        fetchData();
        const interval = setInterval(fetchData, 3000);
        return () => clearInterval(interval);
    }, [fetchData]);

    const handleRefresh = async () => {
        setIsRefreshing(true);
        await fetchData();
        setIsRefreshing(false);
    };

    // Enviar Comando
    const handleSendCommand = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!selectedNode || !commandType) return;

        setIsSending(true);
        try {
            const response = await fetch('/api/network/commands', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    type: commandType,
                    target_node_id: selectedNode,
                }),
            });

            if (response.ok) {
                // Agregar un log local de éxito al encolar
                fetchData();
            } else {
                const err = await response.json();
                alert(`Error: ${err.error || 'No se pudo enviar el comando'}`);
            }
        } catch (error) {
            console.error('Error sending command:', error);
        } finally {
            setIsSending(false);
        }
    };

    // Filtrado de logs
    const filteredLogs = logs.filter(log => {
        if (filterLevel === 'ALL') return true;
        return log.level === filterLevel;
    });

    const getLevelBadgeClass = (level: string) => {
        switch (level) {
            case 'SUCCESS':
                return 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30';
            case 'ERROR':
                return 'bg-rose-500/10 text-rose-400 border border-rose-500/30';
            case 'WARNING':
                return 'bg-amber-500/10 text-amber-400 border border-amber-500/30';
            default:
                return 'bg-sky-500/10 text-sky-400 border border-sky-500/30';
        }
    };

    const getCommandStatusIcon = (status: string) => {
        switch (status) {
            case 'COMPLETED':
                return <CheckCircle2 className="w-5 h-5 text-emerald-400" />;
            case 'FAILED':
                return <AlertTriangle className="w-5 h-5 text-rose-400" />;
            case 'PROCESSING':
                return <RefreshCw className="w-5 h-5 text-amber-400 animate-spin" />;
            default:
                return <Activity className="w-5 h-5 text-gray-400" />;
        }
    };

    return (
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
            {/* Panel de Comandos Izquierdo */}
            <div className="lg:col-span-1 space-y-6">
                <div className="bg-white rounded-2xl border border-gray-200 p-6 shadow-sm">
                    <div className="flex items-center gap-3 mb-6">
                        <div className="p-3 bg-indigo-50 text-indigo-600 rounded-xl">
                            <Radio className="w-6 h-6" />
                        </div>
                        <div>
                            <h2 className="text-xl font-bold text-gray-900">Consola de Control</h2>
                            <p className="text-sm text-gray-500">Enviar órdenes a los sensores</p>
                        </div>
                    </div>

                    <form onSubmit={handleSendCommand} className="space-y-5">
                        {/* Selector de Nodo Destino */}
                        <div>
                            <label className="block text-sm font-semibold text-gray-700 mb-2">
                                Seleccionar Nodo Sensor
                            </label>
                            <select
                                value={selectedNode}
                                onChange={(e) => setSelectedNode(e.target.value)}
                                className="w-full rounded-xl border border-gray-300 py-3 px-4 shadow-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                            >
                                <option value="" disabled>-- Elige un nodo --</option>
                                {nodes.map((node) => (
                                    <option key={node.node_id} value={node.node_id}>
                                        {node.node_id} - {node.description}
                                    </option>
                                ))}
                            </select>
                        </div>

                        {/* Selector de Tipo de Comando */}
                        <div>
                            <label className="block text-sm font-semibold text-gray-700 mb-2">
                                Acción / Comando LoRa
                            </label>
                            <div className="grid grid-cols-1 gap-3">
                                <label className={`flex items-center gap-3 p-4 rounded-xl border cursor-pointer transition ${
                                    commandType === 'POLL_TELEMETRY'
                                        ? 'border-indigo-600 bg-indigo-50/55'
                                        : 'border-gray-200 hover:bg-gray-50'
                                }`}>
                                    <input
                                        type="radio"
                                        name="command"
                                        value="POLL_TELEMETRY"
                                        checked={commandType === 'POLL_TELEMETRY'}
                                        onChange={() => setCommandType('POLL_TELEMETRY')}
                                        className="sr-only"
                                    />
                                    <Activity className={`w-5 h-5 ${commandType === 'POLL_TELEMETRY' ? 'text-indigo-600' : 'text-gray-500'}`} />
                                    <div>
                                        <p className="text-sm font-bold text-gray-900">Pedir Telemetría</p>
                                        <p className="text-xs text-gray-500">Muestreo ambiental directo</p>
                                    </div>
                                </label>

                                <label className={`flex items-center gap-3 p-4 rounded-xl border cursor-pointer transition ${
                                    commandType === 'POLL_IMAGE'
                                        ? 'border-indigo-600 bg-indigo-50/55'
                                        : 'border-gray-200 hover:bg-gray-50'
                                }`}>
                                    <input
                                        type="radio"
                                        name="command"
                                        value="POLL_IMAGE"
                                        checked={commandType === 'POLL_IMAGE'}
                                        onChange={() => setCommandType('POLL_IMAGE')}
                                        className="sr-only"
                                    />
                                    <Camera className={`w-5 h-5 ${commandType === 'POLL_IMAGE' ? 'text-indigo-600' : 'text-gray-500'}`} />
                                    <div>
                                        <p className="text-sm font-bold text-gray-900">Pedir Foto (Imagen)</p>
                                        <p className="text-xs text-gray-500">Disparo CMOS y envío LoRa</p>
                                    </div>
                                </label>

                                <label className={`flex items-center gap-3 p-4 rounded-xl border cursor-pointer transition ${
                                    commandType === 'PING'
                                        ? 'border-indigo-600 bg-indigo-50/55'
                                        : 'border-gray-200 hover:bg-gray-50'
                                }`}>
                                    <input
                                        type="radio"
                                        name="command"
                                        value="PING"
                                        checked={commandType === 'PING'}
                                        onChange={() => setCommandType('PING')}
                                        className="sr-only"
                                    />
                                    <Send className={`w-5 h-5 ${commandType === 'PING' ? 'text-indigo-600' : 'text-gray-500'}`} />
                                    <div>
                                        <p className="text-sm font-bold text-gray-900">Probar Comunicación (PING)</p>
                                        <p className="text-xs text-gray-500">Validar enlace RF</p>
                                    </div>
                                </label>
                            </div>
                        </div>

                        <button
                            type="submit"
                            disabled={isSending || !selectedNode}
                            className="w-full flex items-center justify-center gap-2 py-3.5 px-4 bg-indigo-600 hover:bg-indigo-700 text-white font-semibold rounded-xl transition shadow-md shadow-indigo-600/10 disabled:opacity-50 disabled:cursor-not-allowed"
                        >
                            {isSending ? (
                                <RefreshCw className="w-5 h-5 animate-spin" />
                            ) : (
                                <Play className="w-5 h-5 fill-current" />
                            )}
                            Transmitir Comando
                        </button>
                    </form>
                </div>

                {/* Comandos Recientes */}
                <div className="bg-white rounded-2xl border border-gray-200 p-6 shadow-sm">
                    <h3 className="text-base font-bold text-gray-900 mb-4">Comandos Recientes</h3>
                    <div className="space-y-3">
                        {commands.length === 0 ? (
                            <p className="text-sm text-gray-500 py-4 text-center">No hay comandos encolados recientemente</p>
                        ) : (
                            commands.map((cmd) => (
                                <div key={cmd.command_id} className="flex items-start justify-between p-3.5 rounded-xl bg-gray-55/60 border border-gray-100 text-xs">
                                    <div className="space-y-1">
                                        <p className="font-bold text-gray-900">
                                            {cmd.type === 'POLL_TELEMETRY' ? 'Telemetría' : cmd.type === 'POLL_IMAGE' ? 'Imagen' : 'PING'}
                                        </p>
                                        <p className="text-gray-500 font-medium">Nodo: {cmd.target_node_id}</p>
                                        {cmd.response && (
                                            <p className="text-gray-600 italic font-mono mt-1 border-l-2 pl-2 border-gray-300">
                                                {cmd.response}
                                            </p>
                                        )}
                                    </div>
                                    <div className="flex flex-col items-end gap-1.5">
                                        {getCommandStatusIcon(cmd.status)}
                                        <span className="text-[10px] text-gray-400 font-medium">
                                            {new Date(cmd.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })}
                                        </span>
                                    </div>
                                </div>
                            ))
                        )}
                    </div>
                </div>
            </div>

            {/* Consola de Logs Derecha */}
            <div className="lg:col-span-2 space-y-6">
                <div className="bg-[#0f172a] rounded-2xl border border-slate-800 shadow-2xl overflow-hidden flex flex-col h-[650px]">
                    {/* Cabecera Consola */}
                    <div className="bg-slate-900 border-b border-slate-800 px-6 py-4 flex items-center justify-between">
                        <div className="flex items-center gap-3">
                            <div className="p-2 bg-emerald-500/10 text-emerald-400 rounded-lg border border-emerald-500/25">
                                <Terminal className="w-5 h-5" />
                            </div>
                            <div>
                                <h2 className="text-lg font-bold text-slate-100 font-mono">live_network.log</h2>
                                <p className="text-[11px] text-slate-500 font-mono">Consola del concentrador en tiempo real</p>
                            </div>
                        </div>
                        <div className="flex items-center gap-3">
                            <button
                                onClick={handleRefresh}
                                disabled={isRefreshing}
                                className="p-2 text-slate-400 hover:text-slate-200 hover:bg-slate-800 rounded-lg transition disabled:opacity-50"
                            >
                                <RefreshCw className={`w-4.5 h-4.5 ${isRefreshing ? 'animate-spin' : ''}`} />
                            </button>
                        </div>
                    </div>

                    {/* Filtros de la Consola */}
                    <div className="bg-slate-900/50 border-b border-slate-800 px-6 py-3 flex gap-2 overflow-x-auto">
                        {['ALL', 'INFO', 'SUCCESS', 'WARNING', 'ERROR'].map((lvl) => (
                            <button
                                key={lvl}
                                onClick={() => setFilterLevel(lvl)}
                                className={`px-3 py-1.5 rounded-lg font-mono text-[10px] font-bold border transition ${
                                    filterLevel === lvl
                                        ? 'bg-indigo-500/20 text-indigo-400 border-indigo-500/40'
                                        : 'text-slate-400 border-transparent hover:bg-slate-800'
                                }`}
                            >
                                {lvl === 'ALL' ? 'TODOS' : lvl}
                            </button>
                        ))}
                    </div>

                    {/* Cuerpo de Logs */}
                    <div className="flex-1 p-6 overflow-y-auto font-mono text-xs space-y-3 bg-slate-950/80 custom-scrollbar select-text">
                        {filteredLogs.length === 0 ? (
                            <div className="text-center py-20 text-slate-600 font-mono italic">
                                --- No se han registrado logs con el nivel actual ---
                            </div>
                        ) : (
                            filteredLogs.map((log) => (
                                <div key={log.log_id} className="flex items-start gap-4 hover:bg-slate-900/40 p-1.5 rounded transition">
                                    <span className="text-slate-600 select-none">
                                        [{new Date(log.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })}]
                                    </span>
                                    <span className={`px-2 py-0.5 rounded text-[9px] font-bold uppercase select-none ${getLevelBadgeClass(log.level)}`}>
                                        {log.level}
                                    </span>
                                    {log.node_id && (
                                        <span className="text-indigo-400 font-bold select-none">
                                            [{log.node_id}]
                                        </span>
                                    )}
                                    <span className="text-slate-300 flex-1 break-all">
                                        {log.message}
                                    </span>
                                </div>
                            ))
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
}
