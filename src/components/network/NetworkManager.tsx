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
    Trash2,
    Sliders,
    ChevronDown,
    Calendar,
    Clock,
    Plus,
    XCircle
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
    parameters: string | null;
}

export default function NetworkManager({ nodes }: { nodes: NodeInfo[] }) {
    const [selectedNode, setSelectedNode] = useState<string>(nodes[0]?.node_id || '');
    const [commandType, setCommandType] = useState<string>('POLL_TELEMETRY');
    const [logs, setLogs] = useState<NetworkLog[]>([]);
    const [commands, setCommands] = useState<NetworkCommand[]>([]);
    const [filterLevel, setFilterLevel] = useState<string>('ALL');
    const [isSending, setIsSending] = useState(false);
    const [isRefreshing, setIsRefreshing] = useState(false);

    // Parámetros de Cámara Remota
    const [cameraResolution, setCameraResolution] = useState<number>(10);
    const [cameraBrightness, setCameraBrightness] = useState<number>(0);
    const [cameraContrast, setCameraContrast] = useState<number>(1);
    const [cameraQuality, setCameraQuality] = useState<number>(14);
    const [cameraSaturation, setCameraSaturation] = useState<number>(0);
    const [cameraSpecialEffect, setCameraSpecialEffect] = useState<number>(0);
    const [cameraWhitebal, setCameraWhitebal] = useState<number>(1);
    const [cameraAwbGain, setCameraAwbGain] = useState<number>(1);
    const [cameraWbMode, setCameraWbMode] = useState<number>(0);
    const [cameraExposureCtrl, setCameraExposureCtrl] = useState<number>(1);
    const [cameraAec2, setCameraAec2] = useState<number>(0);
    const [cameraAeLevel, setCameraAeLevel] = useState<number>(0);
    const [cameraAecValue, setCameraAecValue] = useState<number>(300);
    const [cameraGainCtrl, setCameraGainCtrl] = useState<number>(1);
    const [cameraAgcGain, setCameraAgcGain] = useState<number>(0);
    const [cameraGainceiling, setCameraGainceiling] = useState<number>(0);
    const [cameraBpc, setCameraBpc] = useState<number>(0);
    const [cameraWpc, setCameraWpc] = useState<number>(1);
    const [cameraRawGma, setCameraRawGma] = useState<number>(1);
    const [cameraLenc, setCameraLenc] = useState<number>(1);
    const [cameraHmirror, setCameraHmirror] = useState<number>(0);
    const [cameraVflip, setCameraVflip] = useState<number>(0);
    const [cameraDcw, setCameraDcw] = useState<number>(1);
    const [cameraColorbar, setCameraColorbar] = useState<number>(0);
    const [showAdvanced, setShowAdvanced] = useState<boolean>(false);
    const [isSynced, setIsSynced] = useState<boolean>(false);

    // --- ESTADOS Y CONTROL DE RUTINAS PROGRAMADAS ---
    const [activeTab, setActiveTab] = useState<'realtime' | 'routines'>('realtime');
    const [routines, setRoutines] = useState<any[]>([]);
    const [editingRoutineId, setEditingRoutineId] = useState<number | null>(null);
    const [newRoutineName, setNewRoutineName] = useState('');
    const [newRoutineDesc, setNewRoutineDesc] = useState('');
    const [selectedDays, setSelectedDays] = useState<number[]>([1, 2, 3, 4, 5]); // Lunes a Viernes por defecto
    const [newRoutineHour, setNewRoutineHour] = useState<number>(12);
    const [newRoutineMin, setNewRoutineMin] = useState<number>(0);
    const [newRoutineSteps, setNewRoutineSteps] = useState<{ type: string; target_node_id: string }[]>([]);
    const [stepNode, setStepNode] = useState<string>('');
    const [stepType, setStepType] = useState<string>('POLL_TELEMETRY');
    const [isSavingRoutine, setIsSavingRoutine] = useState(false);

    // Seleccionar primer nodo por defecto
    useEffect(() => {
        if (nodes.length > 0 && !stepNode) {
            setStepNode(nodes[0].node_id);
        }
    }, [nodes, stepNode]);

    // Cargar Logs, Comandos y Rutinas desde la API
    const fetchData = useCallback(async () => {
        try {
            const logsRes = await fetch('/api/network/logs?limit=50');
            const commandsRes = await fetch('/api/network/commands?limit=10');
            const routinesRes = await fetch('/api/network/routines');

            if (logsRes.ok && commandsRes.ok) {
                const logsData = await logsRes.json();
                const commandsData = await commandsRes.json();
                setLogs(logsData);
                setCommands(commandsData);
            }
            if (routinesRes.ok) {
                const routinesData = await routinesRes.json();
                setRoutines(routinesData);
            }
        } catch (error) {
            console.error('Error fetching network data:', error);
        }
    }, []);

    // Handlers para la gestión de rutinas
    const handleAddStepToNewRoutine = () => {
        if (!stepNode || !stepType) return;
        setNewRoutineSteps(prev => [...prev, { type: stepType, target_node_id: stepNode }]);
    };

    const handleRemoveStepFromNewRoutine = (index: number) => {
        setNewRoutineSteps(prev => prev.filter((_, i) => i !== index));
    };

    const toggleDay = (dayNum: number) => {
        setSelectedDays(prev => 
            prev.includes(dayNum) 
                ? prev.filter(d => d !== dayNum) 
                : [...prev, dayNum]
        );
    };

    const handleEditRoutine = (routine: any) => {
        setEditingRoutineId(routine.routine_id);
        setNewRoutineName(routine.name);
        setNewRoutineDesc(routine.description || '');
        setSelectedDays(routine.days_of_week.split(',').map(Number));
        setNewRoutineHour(routine.hour);
        setNewRoutineMin(routine.minute);
        setNewRoutineSteps(routine.steps.map((s: any) => ({ type: s.type, target_node_id: s.target_node_id })));
    };

    const handleCancelEdit = () => {
        setEditingRoutineId(null);
        setNewRoutineName('');
        setNewRoutineDesc('');
        setSelectedDays([1, 2, 3, 4, 5]);
        setNewRoutineHour(12);
        setNewRoutineMin(0);
        setNewRoutineSteps([]);
    };

    const handleSaveRoutine = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!newRoutineName.trim()) return;
        if (newRoutineSteps.length === 0) {
            alert('Agrega al menos un paso a la rutina antes de guardar.');
            return;
        }
        if (selectedDays.length === 0) {
            alert('Selecciona al menos un día para programar la rutina.');
            return;
        }

        setIsSavingRoutine(true);
        try {
            const url = editingRoutineId 
                ? `/api/network/routines/${editingRoutineId}`
                : '/api/network/routines';
            const method = editingRoutineId ? 'PUT' : 'POST';

            const res = await fetch(url, {
                method,
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    name: newRoutineName,
                    description: newRoutineDesc,
                    days_of_week: selectedDays.sort((a, b) => a - b).join(','),
                    hour: newRoutineHour,
                    minute: newRoutineMin,
                    steps: newRoutineSteps
                })
            });

            if (res.ok) {
                handleCancelEdit();
                fetchData();
            } else {
                let errorMsg = 'Error desconocido';
                try {
                    const data = await res.json();
                    errorMsg = data.error || errorMsg;
                } catch (e) {
                    errorMsg = res.statusText || errorMsg;
                }
                alert('Error al guardar rutina: ' + errorMsg);
            }
        } catch (error) {
            console.error('Error saving routine:', error);
        } finally {
            setIsSavingRoutine(false);
        }
    };

    const handleDeleteRoutine = async (routineId: number) => {
        if (!confirm('¿Estás seguro de que deseas eliminar esta rutina programada?')) return;
        try {
            const res = await fetch(`/api/network/routines/${routineId}`, {
                method: 'DELETE'
            });
            if (res.ok) {
                // Si estábamos editando la que se borró, cancelar edición
                if (editingRoutineId === routineId) {
                    handleCancelEdit();
                }
                fetchData();
            }
        } catch (error) {
            console.error('Error deleting routine:', error);
        }
    };

    const handleRunRoutineNow = async (routineId: number) => {
        try {
            const res = await fetch(`/api/network/routines/${routineId}`, {
                method: 'POST'
            });
            if (res.ok) {
                alert('Rutina enviada a la cola de ejecución manual. Se iniciará el PING de comprobación en los nodos.');
                fetchData();
            }
        } catch (error) {
            console.error('Error triggering routine:', error);
        }
    };

    const handleAbortExecution = async (executionId: number) => {
        if (!confirm('¿Estás seguro de que deseas abortar esta ejecución en curso?')) return;
        try {
            const res = await fetch(`/api/network/routines/executions/${executionId}/abort`, {
                method: 'POST'
            });
            if (res.ok) {
                fetchData();
            } else {
                let errorMsg = 'Error al abortar';
                try {
                    const data = await res.json();
                    errorMsg = data.error || errorMsg;
                } catch (e) {}
                alert(errorMsg);
            }
        } catch (error) {
            console.error('Error aborting execution:', error);
        }
    };

    // Configurar polling automático cada 3 segundos
    useEffect(() => {
        fetchData();
        const interval = setInterval(fetchData, 3000);
        return () => clearInterval(interval);
    }, [fetchData]);

    // Cargar la última configuración de cámara del nodo seleccionado al cambiar de nodo
    useEffect(() => {
        if (!selectedNode) return;
        setIsSynced(false); // Bloquear al cambiar de nodo
        
        const fetchLastConfig = async () => {
            try {
                const resConfig = await fetch(`/api/network/commands?limit=1&node_id=${selectedNode}&type=CONFIG_CAMERA`);
                const resGet = await fetch(`/api/network/commands?limit=1&node_id=${selectedNode}&type=GET_CAMERA_CONFIG`);
                
                let latestCmd = null;
                
                if (resConfig.ok) {
                    const data = await resConfig.json();
                    if (data && data.length > 0 && data[0].status === 'COMPLETED') {
                        latestCmd = data[0];
                    }
                }
                
                if (resGet.ok) {
                    const data = await resGet.json();
                    if (data && data.length > 0 && data[0].status === 'COMPLETED') {
                        const getCmd = data[0];
                        if (!latestCmd || new Date(getCmd.timestamp) > new Date(latestCmd.timestamp)) {
                            latestCmd = getCmd;
                        }
                    }
                }

                if (latestCmd && latestCmd.parameters) {
                    const params = typeof latestCmd.parameters === 'string' ? JSON.parse(latestCmd.parameters) : latestCmd.parameters;
                    if (params.resolution !== undefined) setCameraResolution(Number(params.resolution));
                    if (params.brightness !== undefined) setCameraBrightness(Number(params.brightness));
                    if (params.contrast !== undefined) setCameraContrast(Number(params.contrast));
                    if (params.quality !== undefined) setCameraQuality(Number(params.quality));
                    if (params.saturation !== undefined) setCameraSaturation(Number(params.saturation));
                    if (params.special_effect !== undefined) setCameraSpecialEffect(Number(params.special_effect));
                    if (params.whitebal !== undefined) setCameraWhitebal(Number(params.whitebal));
                    if (params.awb_gain !== undefined) setCameraAwbGain(Number(params.awb_gain));
                    if (params.wb_mode !== undefined) setCameraWbMode(Number(params.wb_mode));
                    if (params.exposure_ctrl !== undefined) setCameraExposureCtrl(Number(params.exposure_ctrl));
                    if (params.aec2 !== undefined) setCameraAec2(Number(params.aec2));
                    if (params.ae_level !== undefined) setCameraAeLevel(Number(params.ae_level));
                    if (params.aec_value !== undefined) setCameraAecValue(Number(params.aec_value));
                    if (params.gain_ctrl !== undefined) setCameraGainCtrl(Number(params.gain_ctrl));
                    if (params.agc_gain !== undefined) setCameraAgcGain(Number(params.agc_gain));
                    if (params.gainceiling !== undefined) setCameraGainceiling(Number(params.gainceiling));
                    if (params.bpc !== undefined) setCameraBpc(Number(params.bpc));
                    if (params.wpc !== undefined) setCameraWpc(Number(params.wpc));
                    if (params.raw_gma !== undefined) setCameraRawGma(Number(params.raw_gma));
                    if (params.lenc !== undefined) setCameraLenc(Number(params.lenc));
                    if (params.hmirror !== undefined) setCameraHmirror(Number(params.hmirror));
                    if (params.vflip !== undefined) setCameraVflip(Number(params.vflip));
                    if (params.dcw !== undefined) setCameraDcw(Number(params.dcw));
                    if (params.colorbar !== undefined) setCameraColorbar(Number(params.colorbar));
                }
            } catch (error) {
                console.error('Error fetching last camera config:', error);
            }
        };

        fetchLastConfig();
    }, [selectedNode]);

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
                    parameters: commandType === 'CONFIG_CAMERA' ? {
                        resolution: cameraResolution,
                        brightness: cameraBrightness,
                        contrast: cameraContrast,
                        quality: cameraQuality,
                        saturation: cameraSaturation,
                        special_effect: cameraSpecialEffect,
                        whitebal: cameraWhitebal,
                        awb_gain: cameraAwbGain,
                        wb_mode: cameraWbMode,
                        exposure_ctrl: cameraExposureCtrl,
                        aec2: cameraAec2,
                        ae_level: cameraAeLevel,
                        aec_value: cameraAecValue,
                        gain_ctrl: cameraGainCtrl,
                        agc_gain: cameraAgcGain,
                        gainceiling: cameraGainceiling,
                        bpc: cameraBpc,
                        wpc: cameraWpc,
                        raw_gma: cameraRawGma,
                        lenc: cameraLenc,
                        hmirror: cameraHmirror,
                        vflip: cameraVflip,
                        dcw: cameraDcw,
                        colorbar: cameraColorbar
                    } : undefined
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

    const handleSyncCameraConfig = async () => {
        if (!selectedNode) return;
        setIsSending(true);
        setIsSynced(false);
        try {
            const response = await fetch('/api/network/commands', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    type: 'GET_CAMERA_CONFIG',
                    target_node_id: selectedNode,
                }),
            });

            if (response.ok) {
                const newCmd = await response.json();
                const commandId = newCmd.command_id;
                
                // Polling del comando hasta que se complete o falle (máximo 15 segundos)
                const startTime = Date.now();
                const pollInterval = setInterval(async () => {
                    try {
                        const statusRes = await fetch(`/api/network/commands/${commandId}`);
                        if (statusRes.ok) {
                            const cmd = await statusRes.json();
                            if (cmd.status === 'COMPLETED') {
                                clearInterval(pollInterval);
                                setIsSending(false);
                                
                                if (cmd.parameters) {
                                    const params = typeof cmd.parameters === 'string' ? JSON.parse(cmd.parameters) : cmd.parameters;
                                    if (params.resolution !== undefined) setCameraResolution(Number(params.resolution));
                                    if (params.brightness !== undefined) setCameraBrightness(Number(params.brightness));
                                    if (params.contrast !== undefined) setCameraContrast(Number(params.contrast));
                                    if (params.quality !== undefined) setCameraQuality(Number(params.quality));
                                    if (params.saturation !== undefined) setCameraSaturation(Number(params.saturation));
                                    if (params.special_effect !== undefined) setCameraSpecialEffect(Number(params.special_effect));
                                    if (params.whitebal !== undefined) setCameraWhitebal(Number(params.whitebal));
                                    if (params.awb_gain !== undefined) setCameraAwbGain(Number(params.awb_gain));
                                    if (params.wb_mode !== undefined) setCameraWbMode(Number(params.wb_mode));
                                    if (params.exposure_ctrl !== undefined) setCameraExposureCtrl(Number(params.exposure_ctrl));
                                    if (params.aec2 !== undefined) setCameraAec2(Number(params.aec2));
                                    if (params.ae_level !== undefined) setCameraAeLevel(Number(params.ae_level));
                                    if (params.aec_value !== undefined) setCameraAecValue(Number(params.aec_value));
                                    if (params.gain_ctrl !== undefined) setCameraGainCtrl(Number(params.gain_ctrl));
                                    if (params.agc_gain !== undefined) setCameraAgcGain(Number(params.agc_gain));
                                    if (params.gainceiling !== undefined) setCameraGainceiling(Number(params.gainceiling));
                                    if (params.bpc !== undefined) setCameraBpc(Number(params.bpc));
                                    if (params.wpc !== undefined) setCameraWpc(Number(params.wpc));
                                    if (params.raw_gma !== undefined) setCameraRawGma(Number(params.raw_gma));
                                    if (params.lenc !== undefined) setCameraLenc(Number(params.lenc));
                                    if (params.hmirror !== undefined) setCameraHmirror(Number(params.hmirror));
                                    if (params.vflip !== undefined) setCameraVflip(Number(params.vflip));
                                    if (params.dcw !== undefined) setCameraDcw(Number(params.dcw));
                                    if (params.colorbar !== undefined) setCameraColorbar(Number(params.colorbar));
                                    
                                    setIsSynced(true);
                                }
                                fetchData();
                            } else if (cmd.status === 'FAILED') {
                                clearInterval(pollInterval);
                                setIsSending(false);
                                alert(`Error al sincronizar: ${cmd.response || 'El concentrador reportó un fallo.'}`);
                                fetchData();
                            }
                        }
                    } catch (pollErr) {
                        console.error('Error polling command status:', pollErr);
                    }

                    // Timeout después de 15 segundos
                    if (Date.now() - startTime > 15000) {
                        clearInterval(pollInterval);
                        setIsSending(false);
                        alert('Timeout: La cámara o el nodo tardaron demasiado en responder.');
                        fetchData();
                    }
                }, 1500);

            } else {
                const err = await response.json();
                alert(`Error: ${err.error || 'No se pudo enviar la solicitud de sincronización'}`);
                setIsSending(false);
            }
        } catch (error) {
            console.error('Error sending sync command:', error);
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

    const getDayName = (day: number) => {
        const days = ['Domingo', 'Lunes', 'Martes', 'Miércoles', 'Jueves', 'Viernes', 'Sábado', 'Todos los días'];
        return days[day] || '';
    };

    const formatDaysOfWeek = (daysStr: string) => {
        if (!daysStr) return '';
        const daysMap = ['Dom', 'Lun', 'Mar', 'Mié', 'Jue', 'Vie', 'Sáb'];
        const days = daysStr.split(',').map(Number);
        if (days.length === 7) return 'Todos los días';
        return days.map(d => daysMap[d]).join(', ');
    };

    return (
        <div className="space-y-6 w-full animate-fadeIn">
            {/* Control de Pestañas Superior */}
            <div className="flex border-b border-gray-200">
                <button
                    onClick={() => setActiveTab('realtime')}
                    className={`py-3.5 px-6 font-bold text-sm border-b-2 transition flex items-center gap-2 ${
                        activeTab === 'realtime'
                            ? 'border-indigo-600 text-indigo-600'
                            : 'border-transparent text-gray-500 hover:text-gray-700'
                    }`}
                >
                    <Terminal className="w-4 h-4" />
                    Consola Real-Time y Logs
                </button>
                <button
                    onClick={() => setActiveTab('routines')}
                    className={`py-3.5 px-6 font-bold text-sm border-b-2 transition flex items-center gap-2 ${
                        activeTab === 'routines'
                            ? 'border-indigo-600 text-indigo-600'
                            : 'border-transparent text-gray-500 hover:text-gray-700'
                    }`}
                >
                    <Calendar className="w-4 h-4" />
                    Rutinas Programadas
                </button>
            </div>

            {activeTab === 'realtime' ? (
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

                                <label className={`flex items-center gap-3 p-4 rounded-xl border cursor-pointer transition ${
                                    commandType === 'CONFIG_CAMERA'
                                        ? 'border-indigo-600 bg-indigo-50/55'
                                        : 'border-gray-200 hover:bg-gray-50'
                                }`}>
                                    <input
                                        type="radio"
                                        name="command"
                                        value="CONFIG_CAMERA"
                                        checked={commandType === 'CONFIG_CAMERA'}
                                        onChange={() => setCommandType('CONFIG_CAMERA')}
                                        className="sr-only"
                                    />
                                    <Sliders className={`w-5 h-5 ${commandType === 'CONFIG_CAMERA' ? 'text-indigo-600' : 'text-gray-500'}`} />
                                    <div>
                                        <p className="text-sm font-bold text-gray-900">Configurar Cámara</p>
                                        <p className="text-xs text-gray-500">Resolución, brillo y contraste</p>
                                    </div>
                                </label>
                            </div>
                        </div>

                        {commandType === 'CONFIG_CAMERA' && (
                            <div className="bg-gray-50 border border-gray-100 rounded-xl p-4 space-y-4 animate-fadeIn">
                                <div className="flex justify-between items-center mb-1">
                                    <h3 className="text-xs font-bold text-gray-700 uppercase tracking-wider">Parámetros de la Cámara</h3>
                                    <button
                                        type="button"
                                        disabled={isSending || !selectedNode}
                                        onClick={handleSyncCameraConfig}
                                        className="inline-flex items-center gap-1.5 px-2.5 py-1 text-xs font-semibold rounded-lg text-indigo-750 bg-indigo-50 border border-indigo-200 hover:bg-indigo-100 transition disabled:opacity-50"
                                    >
                                        <RefreshCw className={`w-3 h-3 ${isSending ? 'animate-spin' : ''}`} />
                                        Sincronizar Ajustes
                                    </button>
                                </div>
                                {!isSynced && (
                                    <p className="text-[10px] text-amber-750 font-bold bg-amber-50/80 border border-amber-200/70 rounded-lg p-2.5 animate-fadeIn flex items-center gap-1.5">
                                        ⚠️ Por favor, sincroniza los ajustes para cargar y habilitar la edición de parámetros.
                                    </p>
                                )}
                                <fieldset disabled={!isSynced || isSending} className="space-y-4 border-0 p-0 m-0">
                                <div>
                                    <label className="block text-[11px] font-bold text-gray-500 mb-1">Resolución</label>
                                    <select
                                        value={cameraResolution}
                                        onChange={(e) => setCameraResolution(Number(e.target.value))}
                                        className="w-full rounded-lg border border-gray-300 py-2 px-3 text-sm focus:border-indigo-500 focus:ring-indigo-550 text-gray-900 font-medium"
                                    >
                                        <option value={0}>96x96</option>
                                        <option value={1}>QQVGA (160x120)</option>
                                        <option value={2}>QCIF (176x144)</option>
                                        <option value={3}>HQVGA (240x176)</option>
                                        <option value={4}>240x240</option>
                                        <option value={5}>QVGA (320x240)</option>
                                        <option value={6}>CIF (400x296)</option>
                                        <option value={7}>HVGA (480x320)</option>
                                        <option value={8}>VGA (640x480)</option>
                                        <option value={9}>SVGA (800x600)</option>
                                        <option value={10}>XGA (1024x768)</option>
                                        <option value={11}>HD (1280x720)</option>
                                        <option value={12}>SXGA (1280x1024)</option>
                                        <option value={13}>UXGA (1600x1200)</option>
                                        <option value={14}>FHD (1920x1080)</option>
                                        <option value={15}>P_HD (720x1280)</option>
                                        <option value={16}>P_3MP (864x1536)</option>
                                        <option value={17}>QXGA (2048x1536)</option>
                                        <option value={18}>QHD (2560x1440)</option>
                                        <option value={19}>WQXGA (2560x1600)</option>
                                        <option value={20}>P_FHD (1080x1920)</option>
                                        <option value={21}>QSXGA (2560x1920)</option>
                                    </select>
                                </div>
                                <div className="grid grid-cols-2 gap-3">
                                    <div>
                                        <label className="block text-[11px] font-bold text-gray-500 mb-1">Brillo</label>
                                        <select
                                            value={cameraBrightness}
                                            onChange={(e) => setCameraBrightness(Number(e.target.value))}
                                            className="w-full rounded-lg border border-gray-300 py-2 px-3 text-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                        >
                                            <option value={-2}>-2 (Bajo)</option>
                                            <option value={-1}>-1</option>
                                            <option value={0}>0 (Normal)</option>
                                            <option value={1}>+1</option>
                                            <option value={2}>+2 (Alto)</option>
                                        </select>
                                    </div>
                                    <div>
                                        <label className="block text-[11px] font-bold text-gray-500 mb-1">Contraste</label>
                                        <select
                                            value={cameraContrast}
                                            onChange={(e) => setCameraContrast(Number(e.target.value))}
                                            className="w-full rounded-lg border border-gray-300 py-2 px-3 text-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                        >
                                            <option value={-2}>-2 (Bajo)</option>
                                            <option value={-1}>-1</option>
                                            <option value={0}>0 (Normal)</option>
                                            <option value={1}>+1</option>
                                            <option value={2}>+2 (Alto)</option>
                                        </select>
                                    </div>
                                </div>

                                {/* Ajustes Avanzados */}
                                <div className="pt-3 border-t border-gray-200/70">
                                    <button
                                        type="button"
                                        onClick={() => setShowAdvanced(!showAdvanced)}
                                        className="flex items-center justify-between w-full text-[10px] font-bold text-gray-500 hover:text-gray-700 uppercase tracking-wider transition"
                                    >
                                        <span className="flex items-center gap-1.5">
                                            <Sliders className="w-3.5 h-3.5" />
                                            Ajustes Avanzados
                                        </span>
                                        <ChevronDown className={`w-3.5 h-3.5 transform transition-transform duration-200 ${showAdvanced ? 'rotate-180 text-indigo-650' : ''}`} />
                                    </button>
                                    
                                    {showAdvanced && (
                                        <div className="mt-3 space-y-4 pt-3 border-t border-dashed border-gray-200 animate-fadeIn max-h-[380px] overflow-y-auto pr-1 select-none">
                                            {/* Calidad JPG */}
                                            <div>
                                                <div className="flex justify-between items-center mb-1">
                                                    <label className="block text-[10px] font-bold text-gray-500">Calidad JPG (Compresión)</label>
                                                    <span className="text-[11px] font-mono font-bold text-indigo-600 bg-indigo-50 px-2 py-0.5 rounded-md">{cameraQuality}</span>
                                                </div>
                                                <input
                                                    type="range"
                                                    min="10"
                                                    max="63"
                                                    value={cameraQuality}
                                                    onChange={(e) => setCameraQuality(Number(e.target.value))}
                                                    className="w-full h-1.5 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-indigo-600 focus:outline-none"
                                                />
                                            </div>

                                            {/* Imagen Básica */}
                                            <div className="space-y-2.5">
                                                <h4 className="text-[9px] font-black text-indigo-500 uppercase tracking-widest border-b pb-0.5">Ajustes de Imagen</h4>
                                                <div className="grid grid-cols-2 gap-3">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Saturación</label>
                                                        <select
                                                            value={cameraSaturation}
                                                            onChange={(e) => setCameraSaturation(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={-2}>-2 (Baja)</option>
                                                            <option value={-1}>-1</option>
                                                            <option value={0}>0 (Normal)</option>
                                                            <option value={1}>+1</option>
                                                            <option value={2}>+2 (Alta)</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Efecto Especial</label>
                                                        <select
                                                            value={cameraSpecialEffect}
                                                            onChange={(e) => setCameraSpecialEffect(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>Ninguno</option>
                                                            <option value={1}>Negativo</option>
                                                            <option value={2}>B&N</option>
                                                            <option value={3}>Rojizo</option>
                                                            <option value={4}>Verdoso</option>
                                                            <option value={5}>Azulado</option>
                                                            <option value={6}>Sepia</option>
                                                        </select>
                                                    </div>
                                                </div>
                                            </div>

                                            {/* Balance de Blancos */}
                                            <div className="space-y-2.5 pt-1">
                                                <h4 className="text-[9px] font-black text-indigo-500 uppercase tracking-widest border-b pb-0.5">Balance de Blancos</h4>
                                                <div className="grid grid-cols-3 gap-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">W. Balance</label>
                                                        <select
                                                            value={cameraWhitebal}
                                                            onChange={(e) => setCameraWhitebal(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-1.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">AWB Gain</label>
                                                        <select
                                                            value={cameraAwbGain}
                                                            onChange={(e) => setCameraAwbGain(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-1.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Modo WB</label>
                                                        <select
                                                            value={cameraWbMode}
                                                            disabled={cameraWhitebal === 0}
                                                            onChange={(e) => setCameraWbMode(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-1.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium disabled:opacity-50"
                                                        >
                                                            <option value={0}>Auto</option>
                                                            <option value={1}>Soleado</option>
                                                            <option value={2}>Nublado</option>
                                                            <option value={3}>Oficina</option>
                                                            <option value={4}>Casa</option>
                                                        </select>
                                                    </div>
                                                </div>
                                            </div>

                                            {/* Exposición y Ganancia */}
                                            <div className="space-y-2.5 pt-1">
                                                <h4 className="text-[9px] font-black text-indigo-500 uppercase tracking-widest border-b pb-0.5">Exposición y Ganancia</h4>
                                                <div className="grid grid-cols-2 gap-3 mb-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Exp Ctrl (AEC)</label>
                                                        <select
                                                            value={cameraExposureCtrl}
                                                            onChange={(e) => setCameraExposureCtrl(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>Manual</option>
                                                            <option value={1}>Auto</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">AEC2 DSP</label>
                                                        <select
                                                            value={cameraAec2}
                                                            onChange={(e) => setCameraAec2(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                </div>
                                                <div className="grid grid-cols-2 gap-3 mb-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Nivel AE</label>
                                                        <select
                                                            value={cameraAeLevel}
                                                            onChange={(e) => setCameraAeLevel(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={-2}>-2</option>
                                                            <option value={-1}>-1</option>
                                                            <option value={0}>0</option>
                                                            <option value={1}>+1</option>
                                                            <option value={2}>+2</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Valor AEC (Manual)</label>
                                                        <input
                                                            type="number"
                                                            min="0"
                                                            max="1200"
                                                            value={cameraAecValue}
                                                            disabled={cameraExposureCtrl === 1}
                                                            onChange={(e) => setCameraAecValue(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1 px-2 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium disabled:opacity-50 outline-none"
                                                        />
                                                    </div>
                                                </div>
                                                <div className="grid grid-cols-3 gap-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">AGC Ctrl</label>
                                                        <select
                                                            value={cameraGainCtrl}
                                                            onChange={(e) => setCameraGainCtrl(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-1 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>Manual</option>
                                                            <option value={1}>Auto</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Ganancia AGC</label>
                                                        <input
                                                            type="number"
                                                            min="0"
                                                            max="30"
                                                            value={cameraAgcGain}
                                                            disabled={cameraGainCtrl === 1}
                                                            onChange={(e) => setCameraAgcGain(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1 px-1 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium disabled:opacity-50 outline-none"
                                                        />
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Ceiling</label>
                                                        <select
                                                            value={cameraGainceiling}
                                                            onChange={(e) => setCameraGainceiling(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-1 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>2X</option>
                                                            <option value={1}>4X</option>
                                                            <option value={2}>8X</option>
                                                            <option value={3}>16X</option>
                                                            <option value={4}>32X</option>
                                                            <option value={5}>64X</option>
                                                            <option value={6}>128X</option>
                                                        </select>
                                                    </div>
                                                </div>
                                            </div>

                                            {/* Procesamiento y Filtros */}
                                            <div className="space-y-2.5 pt-1">
                                                <h4 className="text-[9px] font-black text-indigo-500 uppercase tracking-widest border-b pb-0.5">Filtros y Orientación</h4>
                                                <div className="grid grid-cols-2 gap-3 mb-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">BPC (Pixel Muerto)</label>
                                                        <select
                                                            value={cameraBpc}
                                                            onChange={(e) => setCameraBpc(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">WPC (Pix. Ruidoso)</label>
                                                        <select
                                                            value={cameraWpc}
                                                            onChange={(e) => setCameraWpc(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                </div>
                                                <div className="grid grid-cols-2 gap-3 mb-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Raw Gamma</label>
                                                        <select
                                                            value={cameraRawGma}
                                                            onChange={(e) => setCameraRawGma(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Corrección Lente</label>
                                                        <select
                                                            value={cameraLenc}
                                                            onChange={(e) => setCameraLenc(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                </div>
                                                <div className="grid grid-cols-2 gap-3 mb-2">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Espejo (H-Mirror)</label>
                                                        <select
                                                            value={cameraHmirror}
                                                            onChange={(e) => setCameraHmirror(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Volteo (V-Flip)</label>
                                                        <select
                                                            value={cameraVflip}
                                                            onChange={(e) => setCameraVflip(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                </div>
                                                <div className="grid grid-cols-2 gap-3">
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">DCW (Downsample)</label>
                                                        <select
                                                            value={cameraDcw}
                                                            onChange={(e) => setCameraDcw(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                    <div>
                                                        <label className="block text-[10px] font-bold text-gray-500 mb-1">Barra de Color</label>
                                                        <select
                                                            value={cameraColorbar}
                                                            onChange={(e) => setCameraColorbar(Number(e.target.value))}
                                                            className="w-full rounded-lg border border-gray-300 py-1.5 px-2.5 text-xs focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                                        >
                                                            <option value={0}>OFF</option>
                                                            <option value={1}>ON</option>
                                                        </select>
                                                    </div>
                                                </div>
                                            </div>
                                        </div>
                                    )}
                                </div>
                                </fieldset>
                            </div>
                        )}

                        <button
                            type="submit"
                            disabled={isSending || !selectedNode || (commandType === 'CONFIG_CAMERA' && !isSynced)}
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
                                            {cmd.type === 'POLL_TELEMETRY' 
                                                ? 'Telemetría' 
                                                : cmd.type === 'POLL_IMAGE' 
                                                ? 'Imagen' 
                                                : cmd.type === 'PING' 
                                                ? 'PING' 
                                                : 'Configurar Cámara'}
                                        </p>
                                        <p className="text-gray-500 font-medium">Nodo: {cmd.target_node_id}</p>
                                        {cmd.parameters && cmd.type === 'CONFIG_CAMERA' && (
                                            <p className="text-[10px] text-gray-500 font-medium">
                                                Parámetros: {(() => {
                                                    try {
                                                        const p = JSON.parse(cmd.parameters);
                                                        const getResLabel = (val: number) => {
                                                            switch(val) {
                                                                case 0: return "96x96";
                                                                case 1: return "QQVGA";
                                                                case 2: return "QCIF";
                                                                case 3: return "HQVGA";
                                                                case 4: return "240x240";
                                                                case 5: return "QVGA";
                                                                case 6: return "CIF";
                                                                case 7: return "HVGA";
                                                                case 8: return "VGA";
                                                                case 9: return "SVGA";
                                                                case 10: return "XGA";
                                                                case 11: return "HD";
                                                                case 12: return "SXGA";
                                                                case 13: return "UXGA";
                                                                case 14: return "FHD";
                                                                case 15: return "P_HD";
                                                                case 16: return "P_3MP";
                                                                case 17: return "QXGA";
                                                                case 18: return "QHD";
                                                                case 19: return "WQXGA";
                                                                case 20: return "P_FHD";
                                                                case 21: return "QSXGA";
                                                                default: return `Res ${val}`;
                                                            }
                                                        };
                                                        return `Res: ${getResLabel(p.resolution)}, Br: ${p.brightness}, Co: ${p.contrast}, Q: ${p.quality || 14}`;
                                                    } catch {
                                                        return cmd.parameters;
                                                    }
                                                })()}
                                            </p>
                                        )}
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
            ) : (
                <div className="grid grid-cols-1 lg:grid-cols-3 gap-8 animate-fadeIn w-full">
                    {/* Columna Izquierda: Lista e Historial */}
                    <div className="lg:col-span-2 space-y-6">
                        {/* Lista de Rutinas */}
                        <div className="bg-white rounded-2xl border border-gray-200 p-6 shadow-sm">
                            <h2 className="text-xl font-bold text-gray-900 mb-4 flex items-center gap-2">
                                <Clock className="w-5 h-5 text-indigo-600 animate-pulse" />
                                Rutinas de Monitoreo Activas
                            </h2>
                            
                            {routines.length === 0 ? (
                                <div className="text-center py-10 border border-dashed border-gray-200 rounded-xl text-gray-400 italic">
                                    No hay rutinas programadas configuradas.
                                </div>
                            ) : (
                                <div className="space-y-4">
                                    {routines.map((routine) => (
                                        <div key={routine.routine_id} className="p-4 rounded-xl border border-gray-150 bg-gray-50/50 flex flex-col md:flex-row md:items-center justify-between gap-4">
                                            <div>
                                                <h3 className="font-bold text-gray-900 text-lg">{routine.name}</h3>
                                                {routine.description && <p className="text-gray-500 text-sm mb-1">{routine.description}</p>}
                                                <div className="flex flex-wrap items-center gap-x-4 gap-y-1 mt-1 text-sm text-gray-600 font-medium">
                                                    <span className="flex items-center gap-1">
                                                        <Calendar className="w-4 h-4 text-indigo-500" />
                                                        {formatDaysOfWeek(routine.days_of_week)}
                                                    </span>
                                                    <span className="flex items-center gap-1">
                                                        <Clock className="w-4 h-4 text-indigo-500" />
                                                        {String(routine.hour).padStart(2, '0')}:{String(routine.minute).padStart(2, '0')} hrs
                                                    </span>
                                                    <span className="bg-indigo-50 text-indigo-600 px-2 py-0.5 rounded-lg text-xs font-bold">
                                                        {routine.steps?.length || 0} comandos
                                                    </span>
                                                </div>
                                            </div>
                                            <div className="flex items-center gap-2 shrink-0">
                                                <button
                                                    onClick={() => handleRunRoutineNow(routine.routine_id)}
                                                    className="px-3 py-2 bg-emerald-50 text-emerald-600 hover:bg-emerald-100 rounded-xl text-sm font-bold transition flex items-center gap-1.5"
                                                    title="Ejecutar ahora mismo"
                                                >
                                                    <Play className="w-4 h-4" />
                                                    Lanzar ya
                                                </button>
                                                <button
                                                    onClick={() => handleEditRoutine(routine)}
                                                    className="p-2 bg-indigo-50 text-indigo-600 hover:bg-indigo-100 rounded-xl transition"
                                                    title="Editar rutina"
                                                >
                                                    <Sliders className="w-4 h-4" />
                                                </button>
                                                <button
                                                    onClick={() => handleDeleteRoutine(routine.routine_id)}
                                                    className="p-2 bg-rose-50 text-rose-600 hover:bg-rose-100 rounded-xl transition"
                                                    title="Eliminar rutina"
                                                >
                                                    <Trash2 className="w-4 h-4" />
                                                </button>
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            )}
                        </div>

                        {/* Historial de Ejecuciones */}
                        <div className="bg-white rounded-2xl border border-gray-200 p-6 shadow-sm">
                            <h2 className="text-xl font-bold text-gray-900 mb-4 flex items-center gap-2">
                                <Activity className="w-5 h-5 text-indigo-600" />
                                Historial de Ejecuciones Recientes
                            </h2>

                            <div className="overflow-x-auto">
                                <table className="w-full text-left border-collapse">
                                    <thead>
                                        <tr className="border-b border-gray-200 text-sm font-bold text-gray-500 bg-gray-50/50">
                                            <th className="p-4">Rutina</th>
                                            <th className="p-4">Inicio</th>
                                            <th className="p-4">Fin</th>
                                            <th className="p-4">Estado / Cola de Pasos</th>
                                        </tr>
                                    </thead>
                                    <tbody className="divide-y divide-gray-100">
                                        {routines.flatMap(r => r.executions || []).length === 0 ? (
                                            <tr>
                                                <td colSpan={4} className="p-8 text-center text-gray-400 italic">
                                                    No se registran ejecuciones de rutinas en la sesión.
                                                </td>
                                            </tr>
                                        ) : (
                                            routines
                                                .flatMap(r => (r.executions || []).map((exec: any) => ({ ...exec, routineName: r.name })))
                                                .sort((a, b) => new Date(b.started_at).getTime() - new Date(a.started_at).getTime())
                                                .slice(0, 10)
                                                .map((exec: any) => (
                                                    <tr key={exec.execution_id} className="hover:bg-gray-50/40 text-sm text-gray-700">
                                                        <td className="p-4 font-bold text-gray-900">{exec.routineName}</td>
                                                        <td className="p-4 whitespace-nowrap">
                                                            {new Date(exec.started_at).toLocaleString([], { dateStyle: 'short', timeStyle: 'short' })}
                                                        </td>
                                                        <td className="p-4 whitespace-nowrap">
                                                            {exec.finished_at 
                                                                ? new Date(exec.finished_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }) 
                                                                : 'En curso...'}
                                                        </td>
                                                        <td className="p-4">
                                                            <div className="flex flex-col gap-2">
                                                                <div className="flex items-center gap-2">
                                                                    {exec.status === 'RUNNING' && <RefreshCw className="w-4 h-4 text-amber-500 animate-spin" />}
                                                                    {exec.status === 'COMPLETED' && <CheckCircle2 className="w-4 h-4 text-emerald-500" />}
                                                                    {exec.status === 'FAILED' && <AlertTriangle className="w-4 h-4 text-rose-500" />}
                                                                    <span className={`text-xs font-bold uppercase ${
                                                                        exec.status === 'COMPLETED' ? 'text-emerald-600' : exec.status === 'FAILED' ? 'text-rose-600' : 'text-amber-600'
                                                                    }`}>
                                                                        {exec.status}
                                                                    </span>
                                                                    {exec.status === 'RUNNING' && (
                                                                        <button
                                                                            onClick={() => handleAbortExecution(exec.execution_id)}
                                                                            title="Abortar Ejecución"
                                                                            className="ml-3 inline-flex items-center gap-1 px-2.5 py-1 text-xs font-bold rounded-lg bg-rose-50 border border-rose-200 text-rose-700 hover:bg-rose-100 transition shadow-sm"
                                                                        >
                                                                            <XCircle className="w-3.5 h-3.5" />
                                                                            Abortar
                                                                        </button>
                                                                    )}
                                                                </div>
                                                                <div className="flex flex-wrap gap-1.5 mt-1">
                                                                    {exec.steps?.map((step: any, sIdx: number) => {
                                                                        let stepLabel = '';
                                                                        if (step.type === 'PRE_PING') {
                                                                            stepLabel = `Ping ${step.node_id}`;
                                                                            if (step.attempt > 1) stepLabel += ` (Intento ${step.attempt})`;
                                                                        } else if (step.type === 'POLL_TELEMETRY') {
                                                                            stepLabel = `Tlm ${step.node_id}`;
                                                                        } else if (step.type === 'POLL_IMAGE') {
                                                                            stepLabel = `Foto ${step.node_id}`;
                                                                        }

                                                                        return (
                                                                            <span 
                                                                                key={step.exec_step_id} 
                                                                                className={`px-2 py-0.5 rounded text-[10px] font-bold border transition ${
                                                                                    step.status === 'COMPLETED' 
                                                                                        ? 'bg-emerald-50 text-emerald-700 border-emerald-200' 
                                                                                        : step.status === 'FAILED' 
                                                                                        ? 'bg-rose-50 text-rose-700 border-rose-200' 
                                                                                        : step.status === 'RUNNING' 
                                                                                        ? 'bg-amber-50 text-amber-700 border-amber-200 animate-pulse' 
                                                                                        : 'bg-gray-50 text-gray-400 border-gray-200'
                                                                                }`}
                                                                                title={`Paso de ejecución: ${step.type} en ${step.node_id} (Estado: ${step.status})`}
                                                                            >
                                                                                {stepLabel}
                                                                            </span>
                                                                        );
                                                                    })}
                                                                </div>
                                                            </div>
                                                        </td>
                                                    </tr>
                                                ))
                                        )}
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    </div>

                    {/* Columna Derecha: Formulario de Creación / Edición */}
                    <div className="lg:col-span-1">
                        <div className="bg-white rounded-2xl border border-gray-200 p-6 shadow-sm sticky top-6">
                            <h2 className="text-xl font-bold text-gray-900 mb-6 flex items-center gap-2">
                                <Plus className="w-5 h-5 text-indigo-600" />
                                {editingRoutineId !== null ? 'Editar Rutina' : 'Nueva Rutina'}
                            </h2>

                            <form onSubmit={handleSaveRoutine} className="space-y-5">
                                <div>
                                    <label className="block text-sm font-semibold text-gray-700 mb-2">Nombre de Rutina</label>
                                    <input
                                        type="text"
                                        value={newRoutineName}
                                        onChange={(e) => setNewRoutineName(e.target.value)}
                                        placeholder="Ej. Monitoreo Semanal"
                                        className="w-full rounded-xl border border-gray-300 py-3 px-4 shadow-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium placeholder-gray-400"
                                        required
                                    />
                                </div>

                                <div>
                                    <label className="block text-sm font-semibold text-gray-700 mb-2">Descripción</label>
                                    <textarea
                                        value={newRoutineDesc}
                                        onChange={(e) => setNewRoutineDesc(e.target.value)}
                                        placeholder="Breve propósito de la rutina"
                                        className="w-full rounded-xl border border-gray-300 py-3 px-4 shadow-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium placeholder-gray-400"
                                        rows={2}
                                    />
                                </div>

                                <div>
                                    <label className="block text-sm font-semibold text-gray-700 mb-2">Días de ejecución</label>
                                    <div className="flex gap-2 flex-wrap">
                                        {[
                                            { label: 'D', value: 0, title: 'Domingo' },
                                            { label: 'L', value: 1, title: 'Lunes' },
                                            { label: 'M', value: 2, title: 'Martes' },
                                            { label: 'M', value: 3, title: 'Miércoles' },
                                            { label: 'J', value: 4, title: 'Jueves' },
                                            { label: 'V', value: 5, title: 'Viernes' },
                                            { label: 'S', value: 6, title: 'Sábado' }
                                        ].map((day) => {
                                            const isSelected = selectedDays.includes(day.value);
                                            return (
                                                <button
                                                    key={day.value}
                                                    type="button"
                                                    onClick={() => toggleDay(day.value)}
                                                    title={day.title}
                                                    className={`w-10 h-10 rounded-full font-bold text-xs flex items-center justify-center border transition-all ${
                                                        isSelected
                                                            ? 'bg-indigo-600 text-white border-indigo-600 shadow-md shadow-indigo-600/20'
                                                            : 'bg-white text-gray-600 border-gray-200 hover:bg-gray-50'
                                                    }`}
                                                >
                                                    {day.label}
                                                </button>
                                            );
                                        })}
                                    </div>
                                </div>

                                <div className="grid grid-cols-2 gap-3">
                                    <div>
                                        <label className="block text-sm font-semibold text-gray-700 mb-2">Hora</label>
                                        <select
                                            value={newRoutineHour}
                                            onChange={(e) => setNewRoutineHour(Number(e.target.value))}
                                            className="w-full rounded-xl border border-gray-300 py-3 px-4 shadow-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                        >
                                            {Array.from({ length: 24 }).map((_, i) => (
                                                <option key={i} value={i}>{String(i).padStart(2, '0')}</option>
                                            ))}
                                        </select>
                                    </div>
                                    <div>
                                        <label className="block text-sm font-semibold text-gray-700 mb-2">Minuto</label>
                                        <select
                                            value={newRoutineMin}
                                            onChange={(e) => setNewRoutineMin(Number(e.target.value))}
                                            className="w-full rounded-xl border border-gray-300 py-3 px-4 shadow-sm focus:border-indigo-500 focus:ring-indigo-500 text-gray-900 font-medium"
                                        >
                                            {Array.from({ length: 60 }).map((_, i) => (
                                                <option key={i} value={i}>{String(i).padStart(2, '0')}</option>
                                            ))}
                                        </select>
                                    </div>
                                </div>

                                <div className="border-t border-gray-200 pt-4">
                                    <label className="block text-sm font-bold text-gray-850 mb-3">
                                        Pasos de Rutina ({newRoutineSteps.length})
                                    </label>
                                    
                                    {newRoutineSteps.length === 0 ? (
                                        <p className="text-sm text-gray-400 italic mb-4">No hay comandos agregados. Agrega uno abajo.</p>
                                    ) : (
                                        <div className="space-y-2 mb-4 max-h-40 overflow-y-auto">
                                            {newRoutineSteps.map((step, idx) => (
                                                <div key={idx} className="flex items-center justify-between p-2.5 bg-gray-50 border border-gray-200 rounded-lg text-xs font-bold text-gray-750">
                                                    <span>
                                                        Paso {idx + 1}: {step.type === 'POLL_TELEMETRY' ? 'Telemetría' : 'Foto'} → {step.target_node_id}
                                                    </span>
                                                    <button
                                                        type="button"
                                                        onClick={() => handleRemoveStepFromNewRoutine(idx)}
                                                        className="text-rose-500 hover:text-rose-700"
                                                    >
                                                        Quitar
                                                    </button>
                                                </div>
                                            ))}
                                        </div>
                                    )}

                                    {/* Configurar Paso a Añadir */}
                                    <div className="bg-indigo-50/45 border border-indigo-100 p-3.5 rounded-xl space-y-3">
                                        <div>
                                            <label className="block text-[11px] font-bold text-indigo-900 mb-1 uppercase tracking-wider">Nodo Destino</label>
                                            <select
                                                value={stepNode}
                                                onChange={(e) => setStepNode(e.target.value)}
                                                className="w-full rounded-lg border border-gray-300 py-1.5 px-3 text-xs bg-white text-gray-850 font-bold"
                                            >
                                                {nodes.map(n => (
                                                    <option key={n.node_id} value={n.node_id}>{n.node_id}</option>
                                                ))}
                                            </select>
                                        </div>
                                        <div>
                                            <label className="block text-[11px] font-bold text-indigo-900 mb-1 uppercase tracking-wider">Acción</label>
                                            <select
                                                value={stepType}
                                                onChange={(e) => setStepType(e.target.value)}
                                                className="w-full rounded-lg border border-gray-300 py-1.5 px-3 text-xs bg-white text-gray-850 font-bold"
                                            >
                                                <option value="POLL_TELEMETRY">Pedir Telemetría</option>
                                                <option value="POLL_IMAGE">Pedir Imagen</option>
                                            </select>
                                        </div>
                                        <button
                                            type="button"
                                            onClick={handleAddStepToNewRoutine}
                                            className="w-full py-2 bg-indigo-600 hover:bg-indigo-700 text-white rounded-lg text-xs font-bold transition flex items-center justify-center gap-1.5"
                                        >
                                            <Plus className="w-3.5 h-3.5" />
                                            Añadir a la Cola
                                        </button>
                                    </div>
                                </div>

                                <div className="flex gap-3 w-full">
                                    {editingRoutineId !== null && (
                                        <button
                                            type="button"
                                            onClick={handleCancelEdit}
                                            className="flex-1 py-3.5 bg-gray-100 hover:bg-gray-200 text-gray-700 font-bold rounded-xl text-sm transition shadow-sm"
                                        >
                                            Cancelar
                                        </button>
                                    )}
                                    <button
                                        type="submit"
                                        disabled={isSavingRoutine || newRoutineSteps.length === 0}
                                        className="flex-grow flex-1 py-3.5 bg-indigo-600 hover:bg-indigo-700 disabled:bg-gray-200 disabled:text-gray-400 text-white rounded-xl text-sm font-bold transition flex items-center justify-center gap-2 shadow-md shadow-indigo-600/10"
                                    >
                                        {editingRoutineId !== null ? 'Guardar Cambios' : 'Guardar Rutina'}
                                    </button>
                                </div>
                            </form>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
