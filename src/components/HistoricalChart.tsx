"use client";

import { useState, useMemo, useEffect } from 'react';
import { 
    AreaChart, 
    Area, 
    XAxis, 
    YAxis, 
    CartesianGrid, 
    Tooltip, 
    ResponsiveContainer,
    ReferenceArea,
    Legend
} from 'recharts';
import { useNodeSelection } from '@/contexts/NodeContext';
import { format, subDays, addHours } from 'date-fns';
import { es } from 'date-fns/locale';

// Parameter definitions for ranges (synchronized with waterQuality.ts logic)
const PARAM_CONFIG: Record<string, { label: string, unit: string, min: number, max: number, green: [number, number], yellow: [number, number][] }> = {
    temperature: { 
        label: 'Temperatura', 
        unit: '°C', 
        min: 0, 
        max: 50,
        green: [10, 30],
        yellow: [[30, 35]] // Only upper yellow range visually useful for simpler chart
    },
    ph: { 
        label: 'pH', 
        unit: '', 
        min: 0, 
        max: 14,
        green: [6.5, 8.5],
        yellow: [[6.0, 6.5], [8.5, 9.0]]
    },
    turbidity: { 
        label: 'Turbidez', 
        unit: 'UNT', 
        min: 0, 
        max: 100, // Cap visuals
        green: [0, 5],
        yellow: [[5, 25]]
    },
    dissolved_oxygen: { 
        label: 'Oxígeno Disuelto', 
        unit: 'mg/L', 
        min: 0, 
        max: 15,
        green: [5, 15], // > 5
        yellow: [[3, 5]]
    },
    conductivity: { 
        label: 'Conductividad', 
        unit: 'µS/cm', 
        min: 0, 
        max: 2500,
        green: [0, 750],
        yellow: [[750, 1500]]
    }
};

// Mock Generator Removed


export default function HistoricalChart() {
    const { selectedNodeId } = useNodeSelection();
    const [selectedParam, setSelectedParam] = useState('ph');

    const config = PARAM_CONFIG[selectedParam];

    // Check if we are viewing general average or specific node
    const isGeneral = !selectedNodeId;
    
    // State for chart data
    const [chartData, setChartData] = useState<any[]>([]);

    useEffect(() => {
        const fetchHistory = async () => {
            try {
                let url = `/api/readings?limit=50&node_id=${selectedNodeId}`;
                
                // If General (no node selected), fetch more data to aggregate
                // We assume 4 nodes, so 50 timestamps * 4 = 200 items needed approx
                if (!selectedNodeId) {
                    url = `/api/readings?limit=200`;
                } else {
                    // Specific node validation to avoid null string
                    url = `/api/readings?limit=50&node_id=${selectedNodeId}`;
                }

                const res = await fetch(url);
                if (res.ok) {
                    const result = await res.json();
                    const rawData = result.data || result;
                    
                    if (!Array.isArray(rawData)) {
                        setChartData([]);
                        return;
                    }

                    if (!selectedNodeId) {
                        // AGGREGATION LOGIC FOR GENERAL AVERAGE
                        // Group by timestamp
                        const grouped: Record<number, number[]> = {};
                        
                        rawData.forEach((r: any) => {
                            const ts = new Date(r.timestamp).getTime();
                            const val = Number(r[selectedParam]) || 0;
                            if (!grouped[ts]) grouped[ts] = [];
                            grouped[ts].push(val);
                        });

                        // Calculate average per timestamp
                        const aggregated = Object.entries(grouped).map(([ts, values]) => {
                            const avg = values.reduce((a, b) => a + b, 0) / values.length;
                            return {
                                timestamp: Number(ts), // Keep as number
                                value: Number(avg.toFixed(2))
                            };
                        });

                        // Sort by timestamp asc
                        aggregated.sort((a, b) => a.timestamp - b.timestamp);
                        
                        setChartData(aggregated);

                    } else {
                        // SINGLE NODE LOGIC
                        const formatted = rawData.map((r: any) => ({
                            timestamp: new Date(r.timestamp).getTime(), // Keep as number
                            value: Number(r[selectedParam]) || 0
                        })).sort((a: any, b: any) => a.timestamp - b.timestamp);
                        
                        setChartData(formatted);
                    }
                }
            } catch (err) {
                console.error("Error fetching historical data", err);
            }
        };

        fetchHistory();
    }, [selectedNodeId, selectedParam]);

    const data = chartData;

    const CustomTooltip = ({ active, payload, label }: any) => {
        if (active && payload && payload.length) {
            return (
                <div className="bg-white/95 backdrop-blur-sm p-3 border border-slate-200 rounded-lg shadow-lg text-sm">
                    <p className="font-bold text-slate-700 mb-1">
                        {format(label, "d MMM yy, HH:mm", { locale: es })}
                    </p>
                    <p className="font-semibold" style={{ color: '#1e3570' }}>
                        {config.label}: {payload[0].value} {config.unit}
                    </p>
                </div>
            );
        }
        return null;
    };

    return (
        <div className="bg-white rounded-3xl p-6 md:p-8 shadow-xl border border-slate-100">
            <div className="flex flex-col md:flex-row justify-between items-center mb-8 gap-4">
                <div>
                    <h3 className="text-xl font-bold text-slate-900">Histórico de Lecturas</h3>
                    <p className="text-sm text-slate-500">
                        {isGeneral ? "Promedio General de la Red" : `Datos del Sensor ${selectedNodeId}`} • Últimas 24 Horas
                    </p>
                </div>
                
                {/* Parameter Selector */}
                <div className="flex flex-wrap gap-2 justify-center">
                    {Object.entries(PARAM_CONFIG).map(([key, cfg]) => (
                        <button
                            key={key}
                            onClick={() => setSelectedParam(key)}
                            className={`px-3 py-1.5 rounded-lg text-xs font-bold transition-all ${
                                selectedParam === key
                                ? 'bg-[#1e3570] text-white shadow-md'
                                : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                            }`}
                        >
                            {cfg.label}
                        </button>
                    ))}
                </div>
            </div>

            <div className="h-[400px] w-full">
                <ResponsiveContainer width="100%" height="100%">
                    <AreaChart data={data} margin={{ top: 10, right: 30, left: 0, bottom: 0 }}>
                        <defs>
                            <linearGradient id="colorValue" x1="0" y1="0" x2="0" y2="1">
                                <stop offset="5%" stopColor="#2563eb" stopOpacity={0.3}/>
                                <stop offset="95%" stopColor="#2563eb" stopOpacity={0}/>
                            </linearGradient>
                        </defs>
                        <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#cbd5e1" strokeOpacity={0.6} />
                        <XAxis 
                            dataKey="timestamp" 
                            type="number"
                            domain={['dataMin', 'dataMax']}
                            scale="time"
                            tickFormatter={(unixTime) => format(unixTime, "d MMM yy", { locale: es })}
                            stroke="#64748b"
                            fontSize={12}
                            tickMargin={10}
                            fontWeight="bold"
                        />
                        <YAxis 
                            domain={[config.min, config.max]} 
                            stroke="#64748b"
                            fontSize={12}
                            unit={config.unit ? ` ${config.unit}` : ''}
                            fontWeight="bold"
                        />
                        <Tooltip content={<CustomTooltip />} cursor={{ stroke: '#3b82f6', strokeWidth: 2 }} />
                        
                        {/* Traffic Light Zones (Background) */}
                        
                        {/* Red Zone (Top) */}
                        {config.green[1] < config.max && (
                            <ReferenceArea 
                                y1={config.green[1]} 
                                y2={config.max} 
                                fill="#ef4444" 
                                fillOpacity={0.08}
                                stroke="none"
                            />
                        )}

                        {/* Green Zone (Desirable) */}
                        <ReferenceArea 
                            y1={config.green[0]} 
                            y2={config.green[1]} 
                            fill="#22c55e" 
                            fillOpacity={0.12}
                            stroke="#16a34a"
                            strokeWidth={1}
                            strokeDasharray="4 4"
                         />

                        {/* Red Zone (Bottom) */}
                        {config.green[0] > config.min && (
                            <ReferenceArea 
                                y1={config.min} 
                                y2={config.green[0]} 
                                fill="#ef4444" 
                                fillOpacity={0.08}
                                stroke="none"
                            />
                         )}

                        <Area 
                            type="monotone" 
                            dataKey="value" 
                            stroke="#2563eb" 
                            strokeWidth={3}
                            fillOpacity={1} 
                            fill="url(#colorValue)" 
                            activeDot={{ r: 6, strokeWidth: 0, fill: '#1e3a8a' }}
                            animationDuration={1500}
                        />
                    </AreaChart>
                </ResponsiveContainer>
            </div>
            
            <div className="flex justify-center gap-6 mt-6 text-sm font-bold text-slate-600">
                <div className="flex items-center gap-2">
                    <span className="w-4 h-4 rounded bg-green-500/20 border border-green-500 block"></span> Zona Segura
                </div>
                <div className="flex items-center gap-2">
                    <span className="w-4 h-4 rounded bg-red-500/15 border border-red-500/30 block"></span> Zona de Riesgo (Fuera de Rango)
                </div>
            </div>
        </div>
    );
}
