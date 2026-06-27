"use client";

import React, { useState, useEffect } from 'react';
import { Camera, RefreshCcw, Calendar, Filter, ChevronRight } from 'lucide-react';
import DetectionModal from '@/components/admin/gallery/DetectionModal';
import { motion } from 'framer-motion';

/**
 * Componente GallerySection
 * 
 * Sección integrada en la página principal para mostrar las detecciones de residuos.
 */
export default function GallerySection() {
    const [detections, setDetections] = useState<any[]>([]);
    const [nodes, setNodes] = useState<any[]>([]);
    const [loading, setLoading] = useState(true);
    const [selectedDetection, setSelectedDetection] = useState(null);
    const [filterNode, setFilterNode] = useState('');
    const [orderBy, setOrderBy] = useState('date_desc');
    
    const fetchNodes = async () => {
        try {
            const res = await fetch('/api/nodes');
            const data = await res.json();
            if (Array.isArray(data)) {
                setNodes(data.filter((n: any) => n.node_id !== 'NODE_C'));
            }
        } catch (error) {
            console.error('Error loading nodes:', error);
        }
    };

    const fetchDetections = async (silent = false) => {
        if (!silent) setLoading(true);
        try {
            let url = `/api/waste-detections?orderBy=${orderBy}`;
            if (filterNode) {
                url += `&node_id=${filterNode}`;
            }
            const res = await fetch(url);
            const data = await res.json();
            setDetections(Array.isArray(data) ? data : []);
        } catch (error) {
            console.error('Error loading detections:', error);
        } finally {
            if (!silent) setLoading(false);
        }
    };

    useEffect(() => {
        fetchNodes();
    }, []);

    useEffect(() => {
        fetchDetections();
        
        // Polling silencioso cada 10 segundos
        const interval = setInterval(() => fetchDetections(true), 10000);
        return () => clearInterval(interval);
    }, [filterNode, orderBy]);

    // Calcular estadísticas en tiempo real
    const totalDetections = detections.length;
    const avgCoverage = totalDetections > 0
        ? (detections.reduce((acc, curr) => acc + (parseFloat(curr.coverage_percent) || 0), 0) / totalDetections).toFixed(1)
        : '0.0';
    const criticalDetections = detections.filter((d: any) => (parseFloat(d.coverage_percent) || 0) > 40).length;

    return (
        <section id="galeria" className="py-24 bg-gray-50 overflow-hidden">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
                
                {/* Header de Sección */}
                <div className="flex flex-col md:flex-row md:items-end justify-between mb-10 gap-6">
                    <motion.div 
                        initial={{ opacity: 0, x: -20 }}
                        whileInView={{ opacity: 1, x: 0 }}
                        viewport={{ once: true }}
                        className="max-w-2xl"
                    >
                        <h2 className="text-sm font-bold text-[#1e3570] uppercase tracking-[0.3em] mb-3">Evidencia Visual</h2>
                        <h3 className="text-4xl md:text-5xl font-black text-gray-900 tracking-tight">
                            Registro de <span className="text-blue-600">Vigilancia</span>
                        </h3>
                        <p className="mt-4 text-gray-600 text-lg">
                            Análisis visual de residuos capturado durante las jornadas de monitoreo de nuestros nodos sensores.
                        </p>
                    </motion.div>

                    <div className="flex flex-wrap items-center gap-3 bg-white p-2 rounded-2xl shadow-sm border border-gray-100">
                        {/* Filtro de Nodos */}
                        <div className="relative">
                            <Filter className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-400" />
                            <select 
                                value={filterNode}
                                onChange={(e) => setFilterNode(e.target.value)}
                                className="pl-10 pr-8 py-2 bg-transparent text-sm font-bold text-gray-700 outline-none appearance-none cursor-pointer"
                            >
                                <option value="">Todos los Nodos</option>
                                {nodes.map((node) => (
                                    <option key={node.node_id} value={node.node_id}>
                                        {node.node_id} ({node.description || 'Sin descripción'})
                                    </option>
                                ))}
                            </select>
                        </div>

                        {/* Selector de Orden */}
                        <div className="h-6 w-px bg-gray-200"></div>

                        <div className="relative">
                            <select 
                                value={orderBy}
                                onChange={(e) => setOrderBy(e.target.value)}
                                className="px-3 py-2 bg-transparent text-sm font-bold text-gray-700 outline-none appearance-none cursor-pointer"
                            >
                                <option value="date_desc">Más recientes primero</option>
                                <option value="date_asc">Más antiguos primero</option>
                                <option value="coverage_desc">Mayor contaminación primero</option>
                                <option value="coverage_asc">Menor contaminación primero</option>
                            </select>
                        </div>

                        <button 
                            onClick={() => fetchDetections()}
                            className="p-2 text-gray-400 hover:text-[#1e3570] transition-colors"
                        >
                            <RefreshCcw className={`w-5 h-5 ${loading ? 'animate-spin' : ''}`} />
                        </button>
                    </div>
                </div>

                {/* Tarjetas de Estadísticas Rápidas */}
                <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-12">
                    <div className="bg-white p-6 rounded-3xl border border-gray-100 shadow-sm flex items-center justify-between">
                        <div>
                            <p className="text-xs font-bold text-gray-400 uppercase tracking-wider mb-1">Muestras Capturadas</p>
                            <h4 className="text-3xl font-black text-gray-900">{totalDetections}</h4>
                        </div>
                        <div className="w-12 h-12 bg-blue-50 text-blue-600 rounded-2xl flex items-center justify-center font-bold text-xl">
                            #
                        </div>
                    </div>
                    <div className="bg-white p-6 rounded-3xl border border-gray-100 shadow-sm flex items-center justify-between">
                        <div>
                            <p className="text-xs font-bold text-gray-400 uppercase tracking-wider mb-1">Cobertura Promedio</p>
                            <h4 className="text-3xl font-black text-blue-600">{avgCoverage}%</h4>
                        </div>
                        <div className="w-12 h-12 bg-blue-50 text-blue-600 rounded-2xl flex items-center justify-center font-bold text-xl">
                            %
                        </div>
                    </div>
                    <div className="bg-white p-6 rounded-3xl border border-gray-100 shadow-sm flex items-center justify-between">
                        <div>
                            <p className="text-xs font-bold text-gray-400 uppercase tracking-wider mb-1">Alertas Críticas (&gt;40%)</p>
                            <h4 className="text-3xl font-black text-red-600">{criticalDetections}</h4>
                        </div>
                        <div className="w-12 h-12 bg-red-50 text-red-600 rounded-2xl flex items-center justify-center font-bold text-xl">
                            ⚠
                        </div>
                    </div>
                </div>

                {/* Grid */}
                {loading ? (
                    <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6">
                        {[1, 2, 3, 4].map((n) => (
                            <div key={n} className="aspect-[4/3] bg-gray-200 rounded-3xl animate-pulse"></div>
                        ))}
                    </div>
                ) : detections.length === 0 ? (
                    <div className="bg-white rounded-3xl p-20 text-center border border-gray-100">
                        <Camera className="w-12 h-12 text-gray-300 mx-auto mb-4" />
                        <p className="text-gray-500 font-medium">No se han registrado detecciones recientemente.</p>
                    </div>
                ) : (
                    <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6">
                        {detections.map((detection: any, idx) => (
                            <motion.div 
                                key={detection.detection_id}
                                initial={{ opacity: 0, y: 20 }}
                                whileInView={{ opacity: 1, y: 0 }}
                                viewport={{ once: true }}
                                transition={{ delay: idx * 0.1 }}
                                onClick={() => setSelectedDetection(detection)}
                                className="group relative bg-white rounded-3xl overflow-hidden shadow-sm hover:shadow-2xl transition-all duration-500 cursor-pointer border border-gray-100"
                            >
                                <div className="aspect-[4/3] relative overflow-hidden">
                                    <img 
                                        src={`/api/waste-detections/${detection.detection_id}/image?type=original`} 
                                        alt="Detección" 
                                        className="w-full h-full object-cover transition-transform duration-700 group-hover:scale-110"
                                    />
                                    <div className="absolute inset-0 bg-gradient-to-t from-black/80 via-black/20 to-transparent opacity-60 group-hover:opacity-80 transition-opacity"></div>
                                    
                                    <div className="absolute top-4 left-4">
                                        <span className="px-3 py-1 bg-white/20 backdrop-blur-md text-white text-[10px] font-black rounded-full uppercase tracking-widest border border-white/30">
                                            {detection.node_id}
                                        </span>
                                    </div>

                                    <div className="absolute bottom-4 left-4 right-4">
                                        <div className="flex justify-between items-end text-white">
                                            <div>
                                                <p className="text-[10px] font-bold text-blue-300 uppercase leading-none mb-1">Residuos</p>
                                                <p className="text-xl font-black">{detection.coverage_percent}%</p>
                                            </div>
                                            <div className="w-8 h-8 bg-white/20 backdrop-blur-md rounded-full flex items-center justify-center group-hover:bg-blue-600 transition-colors">
                                                <ChevronRight className="w-5 h-5 text-white" />
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </motion.div>
                        ))}
                    </div>
                )}
            </div>

            {/* Modal */}
            {selectedDetection && (
                <DetectionModal 
                    detection={selectedDetection} 
                    onClose={() => setSelectedDetection(null)} 
                />
            )}
        </section>
    );
}
