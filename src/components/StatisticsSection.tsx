"use client";

import { useEffect, useState } from 'react';
import { 
    getTemperatureStatus, 
    getPHStatus, 
    getTurbidityStatus, 
    getDOStatus, 
    getConductivityStatus, 
    calculateICA,
    type TrafficLightResult 
} from '@/lib/waterQuality';
import { motion } from 'framer-motion';
import { Activity, Droplets, Thermometer, Zap, Waves, Info } from 'lucide-react';
import { useNodeSelection } from '@/contexts/NodeContext';
import HistoricalChart from './HistoricalChart';

type NodeKey = 'general' | string;

export default function StatisticsSection() {
    const { selectedNodeId, setSelectedNodeId } = useNodeSelection();
    // Local state for UI selection (defaults to general, syncs with context)
    const [selectedNode, setSelectedNode] = useState<NodeKey>('general');
    const [nodes, setNodes] = useState<any[]>([]);
    const [loading, setLoading] = useState(true);

    // Fetch real nodes
    useEffect(() => {
        const fetchNodes = async () => {
            try {
                const res = await fetch('/api/nodes');
                if (res.ok) {
                    const data = await res.json();
                    setNodes(data);
                }
            } catch (err) {
                console.error("Failed to fetch nodes", err);
            } finally {
                setLoading(false);
            }
        };
        fetchNodes();
    }, []);

    // Sync context change to local state
    useEffect(() => {
        if (selectedNodeId) {
            setSelectedNode(selectedNodeId);
        } else {
            setSelectedNode('general');
        }
    }, [selectedNodeId]);

    // Handle local click -> update context
    const handleNodeChange = (key: NodeKey) => {
        setSelectedNode(key);
        if (key === 'general') setSelectedNodeId(null);
        else setSelectedNodeId(key);
    };

    const [currentStats, setCurrentStats] = useState({
        temp: { color: 'green', status: '...' } as TrafficLightResult,
        ph: { color: 'green', status: '...' } as TrafficLightResult,
        turb: { color: 'green', status: '...' } as TrafficLightResult,
        do: { color: 'green', status: '...' } as TrafficLightResult,
        cond: { color: 'green', status: '...' } as TrafficLightResult,
        ica: { value: 0, classification: '...', color: '#ccc' }
    });
    
    // Calcular valores a mostrar basado en la selección
    const getDisplayValues = () => {
        if (selectedNode !== 'general') {
            const node = nodes.find(n => n.node_id === selectedNode);
            if (node && node.sensor_readings && node.sensor_readings.length > 0) {
                const reading = node.sensor_readings[0];
                return {
                    name: node.description,
                    temperature: Number(reading.temperature) || 0,
                    ph: Number(reading.ph) || 0,
                    turbidity: Number(reading.turbidity) || 0,
                    dissolved_oxygen: Number(reading.dissolved_oxygen) || 0,
                    conductivity: Number(reading.conductivity) || 0,
                };
            }
            return { 
                name: selectedNode, temperature: 0, ph: 0, turbidity: 0, dissolved_oxygen: 0, conductivity: 0 
            };
        }

        // Calcular Promedio General de los datos reales
        if (nodes.length === 0) return { temperature: 0, ph: 0, turbidity: 0, dissolved_oxygen: 0, conductivity: 0 };

        let count = 0;
        const acc = nodes.reduce((acc, node) => {
            if (node.sensor_readings && node.sensor_readings.length > 0) {
                const r = node.sensor_readings[0];
                acc.temperature += Number(r.temperature) || 0;
                acc.ph += Number(r.ph) || 0;
                acc.turbidity += Number(r.turbidity) || 0;
                acc.dissolved_oxygen += Number(r.dissolved_oxygen) || 0;
                acc.conductivity += Number(r.conductivity) || 0; // Note: 'conductivity' in DB vs 'conductivity' in UI
                count++;
            }
            return acc;
        }, { temperature: 0, ph: 0, turbidity: 0, dissolved_oxygen: 0, conductivity: 0 });

        if (count === 0) return acc;

        return {
            temperature: parseFloat((acc.temperature / count).toFixed(1)),
            ph: parseFloat((acc.ph / count).toFixed(1)),
            turbidity: parseFloat((acc.turbidity / count).toFixed(1)),
            dissolved_oxygen: parseFloat((acc.dissolved_oxygen / count).toFixed(1)),
            conductivity: Math.round(acc.conductivity / count),
        };
    };

    useEffect(() => {
        const values = getDisplayValues();
        
        const tempStat = getTemperatureStatus(values.temperature);
        const phStat = getPHStatus(values.ph);
        const turbStat = getTurbidityStatus(values.turbidity);
        const doStat = getDOStatus(values.dissolved_oxygen);
        const condStat = getConductivityStatus(values.conductivity);

        const icaStat = calculateICA(values.ph, values.dissolved_oxygen, values.turbidity, values.conductivity);

        setCurrentStats({
            temp: tempStat,
            ph: phStat,
            turb: turbStat,
            do: doStat,
            cond: condStat,
            ica: icaStat as any
        });
    }, [selectedNode, nodes]);

    const getColorClass = (color: string) => {
        switch(color) {
            case 'green': return 'bg-emerald-50 text-emerald-900 border-emerald-200'; // Lighter bg for better contrast
            case 'yellow': return 'bg-yellow-50 text-yellow-900 border-yellow-200';
            case 'red': return 'bg-red-50 text-red-900 border-red-200';
            default: return 'bg-gray-50 text-gray-900 border-gray-200';
        }
    };
    
    const getDotColor = (color: string) => {
         switch(color) {
            case 'green': return 'bg-emerald-500 shadow-emerald-200 shadow-lg';
            case 'yellow': return 'bg-yellow-500 shadow-yellow-200 shadow-lg';
            case 'red': return 'bg-red-500 shadow-red-200 shadow-lg';
            default: return 'bg-gray-500';
        }
    };

    const ParameterCard = ({ title, value, unit, statusResult, icon: Icon }: any) => (
        <motion.div 
            layout
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -20 }}
            transition={{ duration: 0.3 }}
            className={`p-6 rounded-2xl border backdrop-blur-md shadow-sm hover:shadow-xl transition-all duration-300 ${getColorClass(statusResult.color)}`}
        >
            <div className="flex justify-between items-start mb-4">
                <div className={`p-3 rounded-xl bg-white/60 shadow-sm`}>
                    <Icon className="w-6 h-6" />
                </div>
                <div className={`w-3 h-3 rounded-full ${getDotColor(statusResult.color)} animate-pulse`} />
            </div>
            <div>
                <p className="text-xs font-bold opacity-70 uppercase tracking-widest">{title}</p>
                <div className="flex items-baseline gap-1 mt-2">
                    <h3 className="text-4xl font-extrabold tracking-tight">{value}</h3>
                    <span className="text-sm font-semibold opacity-60">{unit}</span>
                </div>
                <div className="mt-4 inline-flex items-center px-3 py-1 rounded-full text-xs font-bold bg-white/60 backdrop-blur-sm">
                    {statusResult.status}
                </div>
            </div>
        </motion.div>
    );

    const displayValues = getDisplayValues();

    return (
        <section id="estadisticas" className="py-24 relative overflow-hidden bg-slate-50">
            {/* Background decoration */}
            <div className="absolute top-0 left-0 w-full h-[500px] bg-gradient-to-b from-blue-100/40 to-transparent -z-10" />
            <div className="absolute right-0 top-1/4 w-96 h-96 bg-blue-200/20 rounded-full blur-3xl -z-10" />
            <div className="absolute left-0 bottom-0 w-64 h-64 bg-emerald-200/20 rounded-full blur-3xl -z-10" />

            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
                <div className="text-center mb-12">
                    <h2 className="text-3xl md:text-5xl font-black text-[#1e3570] mb-6 tracking-tight">
                        Análisis de Calidad del Agua
                    </h2>
                    <p className="text-lg text-slate-600 max-w-2xl mx-auto leading-relaxed">
                        Evaluación en tiempo real mediante nuestra red de sensores distribuidos.
                        Selecciona un nodo para ver detalles específicos o visualiza el promedio general.
                    </p>
                </div>

                {/* Node Selector Pills */}
                <div className="flex flex-wrap justify-center gap-2 mb-16">
                     <button
                        onClick={() => handleNodeChange('general')}
                        className={`px-6 py-2.5 rounded-full text-sm font-bold transition-all duration-200 shadow-sm ${
                            selectedNode === 'general' 
                            ? 'bg-[#1e3570] text-white shadow-blue-900/20 scale-105' 
                            : 'bg-white text-slate-600 hover:bg-slate-100 border border-slate-200'
                        }`}
                    >
                        🌐 Promedio General
                    </button>
                    {nodes.map((node) => (
                        <button
                            key={node.node_id}
                            onClick={() => handleNodeChange(node.node_id)}
                            className={`px-6 py-2.5 rounded-full text-sm font-bold transition-all duration-200 shadow-sm ${
                                selectedNode === node.node_id
                                ? 'bg-[#1e3570] text-white shadow-blue-900/20 scale-105' 
                                : 'bg-white text-slate-600 hover:bg-slate-100 border border-slate-200'
                            }`}
                        >
                            📍 {node.description}
                        </button>
                    ))}
                </div>

                {/* ICA Score Main Card */}
                <div className="mb-12">
                     <motion.div 
                        key={selectedNode} // Animate on change
                        initial={{ opacity: 0, scale: 0.98 }}
                        animate={{ opacity: 1, scale: 1 }}
                        transition={{ duration: 0.4 }}
                        className="relative rounded-3xl overflow-hidden shadow-2xl bg-white border border-slate-100 max-w-5xl mx-auto"
                    >
                        <div className="absolute top-0 left-0 w-full h-1.5" style={{ backgroundColor: currentStats.ica.color }} />
                        <div className="p-8 md:p-12 flex flex-col md:flex-row items-center justify-between gap-10">
                            <div className="flex-1 text-center md:text-left space-y-2">
                                <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-slate-100 text-slate-600 text-xs font-bold uppercase tracking-wider mb-2">
                                    {/* Nombre Dinamico */}
                                    {selectedNode === 'general' 
                                        ? 'Red Completa' 
                                        : (nodes.find(n => n.node_id === selectedNode)?.description || `Nodo ${selectedNode}`)
                                    }
                                </div>
                                <h3 className="text-3xl md:text-4xl font-bold text-slate-900">Índice de Calidad (ICA)</h3>
                                <p className="text-slate-500 text-lg">Cálculo ponderado basado en normas internacionales.</p>
                            </div>
                            
                            <div className="flex items-center gap-8 md:gap-12">
                                <div className="text-right hidden md:block">
                                    <p className="text-xs text-slate-400 font-bold uppercase tracking-widest mb-1">Clasificación</p>
                                    <p className="text-4xl font-black tracking-tight" style={{ color: currentStats.ica.color }}>
                                        {currentStats.ica.classification}
                                    </p>
                                </div>
                                <div className="relative w-40 h-40 flex items-center justify-center">
                                    <svg className="w-full h-full transform -rotate-90 drop-shadow-lg">
                                        <circle cx="80" cy="80" r="70" stroke="#f1f5f9" strokeWidth="12" fill="none" />
                                        <circle 
                                            cx="80" cy="80" r="70" 
                                            stroke={currentStats.ica.color} 
                                            strokeWidth="12" 
                                            fill="none" 
                                            strokeDasharray={440}
                                            strokeDashoffset={440 - (440 * currentStats.ica.value) / 100}
                                            strokeLinecap="round"
                                            className="transition-all duration-1000 ease-in-out"
                                        />
                                    </svg>
                                    <div className="absolute inset-0 flex items-center justify-center flex-col">
                                        <span className="text-5xl font-black text-slate-900 tracking-tighter">{currentStats.ica.value}</span>
                                        <span className="text-xs font-bold text-slate-400">PUNTOS</span>
                                    </div>
                                </div>
                                <div className="text-center md:hidden">
                                     <p className="text-2xl font-black mt-2" style={{ color: currentStats.ica.color }}>
                                        {currentStats.ica.classification}
                                    </p>
                                </div>
                            </div>
                        </div>
                    </motion.div>
                </div>

                {/* Parameters Grid */}
                <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-6 mb-16">
                    <ParameterCard 
                        title="Temperatura" 
                        value={displayValues.temperature} 
                        unit="°C" 
                        statusResult={currentStats.temp} 
                        icon={Thermometer} 
                    />
                    <ParameterCard 
                        title="pH" 
                        value={displayValues.ph} 
                        unit="pH" 
                        statusResult={currentStats.ph} 
                        icon={Activity} 
                    />
                    <ParameterCard 
                        title="Turbidez" 
                        value={displayValues.turbidity} 
                        unit="UNT" 
                        statusResult={currentStats.turb} 
                        icon={Waves} 
                    />
                    <ParameterCard 
                        title="Oxígeno Disuelto" 
                        value={displayValues.dissolved_oxygen} 
                        unit="mg/L" 
                        statusResult={currentStats.do} 
                        icon={Droplets} 
                    />
                    <ParameterCard 
                        title="Conductividad" 
                        value={displayValues.conductivity} 
                        unit="µS/cm" 
                        statusResult={currentStats.cond} 
                        icon={Zap} 
                    />
                </div>

                {/* Historical Chart Section */}
                <motion.div
                    initial={{ opacity: 0, y: 30 }}
                    whileInView={{ opacity: 1, y: 0 }}
                    transition={{ delay: 0.2 }}
                >
                    <HistoricalChart />
                </motion.div>
            </div>
        </section>
    );
}
