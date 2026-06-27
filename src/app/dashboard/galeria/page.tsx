"use client";

import React, { useState, useEffect } from 'react';
import { Camera, Filter, RefreshCcw, Trash2, CheckSquare, Square, List } from 'lucide-react';
import DetectionModal from '@/components/admin/gallery/DetectionModal';

export default function AdminGalleryPage() {
    const [detections, setDetections] = useState<any[]>([]);
    const [nodes, setNodes] = useState<any[]>([]);
    const [loading, setLoading] = useState(true);
    const [selectedDetection, setSelectedDetection] = useState(null);
    const [filterNode, setFilterNode] = useState('');
    const [orderBy, setOrderBy] = useState('date_desc');

    // Estados para la selección múltiple
    const [selectMode, setSelectMode] = useState(false);
    const [selectedIds, setSelectedIds] = useState<Set<number>>(new Set());
    const [isDeletingBulk, setIsDeletingBulk] = useState(false);

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

    const handleDelete = async (e: React.MouseEvent, id: number) => {
        e.stopPropagation(); // Evitar abrir el modal
        if (!confirm('¿Estás seguro de eliminar esta detección de forma permanente?')) return;

        try {
            const res = await fetch(`/api/waste-detections/${id}`, { method: 'DELETE' });
            if (res.ok) {
                setDetections(detections.filter((d: any) => d.detection_id !== id));
            } else {
                alert('Error al eliminar');
            }
        } catch (error) {
            console.error('Error deleting:', error);
        }
    };

    const handleBulkDelete = async () => {
        if (selectedIds.size === 0) return;
        if (!confirm(`¿Estás seguro de eliminar las ${selectedIds.size} detecciones seleccionadas de forma permanente?`)) return;

        setIsDeletingBulk(true);
        try {
            const deletePromises = Array.from(selectedIds).map(async (id) => {
                const res = await fetch(`/api/waste-detections/${id}`, { method: 'DELETE' });
                return { id, ok: res.ok };
            });
            const results = await Promise.all(deletePromises);
            const successfulIds = results.filter(r => r.ok).map(r => Number(r.id));
            
            setDetections(prev => prev.filter(d => !successfulIds.includes(Number(d.detection_id))));
            setSelectedIds(new Set());
            setSelectMode(false);
            
            // Refrescar silenciosamente desde el servidor para garantizar sincronía total con la DB
            await fetchDetections(true);
            
            if (successfulIds.length < results.length) {
                alert('Algunas detecciones no pudieron ser eliminadas.');
            }
        } catch (error) {
            console.error('Error in bulk delete:', error);
            alert('Error al realizar la eliminación masiva.');
        } finally {
            setIsDeletingBulk(false);
        }
    };

    const toggleSelect = (id: number, e?: React.MouseEvent) => {
        if (e) e.stopPropagation();
        const next = new Set(selectedIds);
        if (next.has(id)) {
            next.delete(id);
        } else {
            next.add(id);
        }
        setSelectedIds(next);
    };

    const toggleAll = () => {
        if (selectedIds.size === detections.length) {
            setSelectedIds(new Set());
        } else {
            setSelectedIds(new Set(detections.map(d => d.detection_id)));
        }
    };

    useEffect(() => {
        fetchNodes();
    }, []);

    useEffect(() => {
        fetchDetections();
        
        const interval = setInterval(() => fetchDetections(true), 10000);
        return () => clearInterval(interval);
    }, [filterNode, orderBy]);

    return (
        <div className="p-8 max-w-7xl mx-auto min-h-screen">
            {/* Header */}
            <div className="flex flex-col md:flex-row md:items-center justify-between mb-8 gap-4">
                <div>
                    <h1 className="text-3xl font-bold text-gray-900">Gestión de Vigilancia</h1>
                    <p className="text-gray-500 mt-1">Administra los registros visuales y depura detecciones erróneas.</p>
                </div>
                <div className="flex flex-wrap items-center gap-3">
                    {/* Botón Modo Selección */}
                    <button 
                        onClick={() => {
                            setSelectMode(!selectMode);
                            setSelectedIds(new Set());
                        }}
                        className={`flex items-center gap-2 px-4 py-2 rounded-xl text-sm font-bold border transition-all ${
                            selectMode 
                            ? 'bg-red-50 border-red-200 text-red-700 hover:bg-red-100' 
                            : 'bg-white border-gray-200 text-gray-700 hover:bg-gray-50 shadow-sm'
                        }`}
                        title="Selección múltiple"
                    >
                        <List className="w-4 h-4" />
                        {selectMode ? 'Cancelar Selección' : 'Selección Múltiple'}
                    </button>

                    <button 
                        onClick={() => fetchDetections()}
                        className="p-2 text-gray-400 hover:text-[#1e3570] hover:bg-gray-100 rounded-lg transition-all"
                        title="Refrescar"
                    >
                        <RefreshCcw className={`w-5 h-5 ${loading ? 'animate-spin' : ''}`} />
                    </button>

                    {/* Filtro de Nodos */}
                    <div className="relative">
                        <Filter className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-400" />
                        <select 
                            value={filterNode}
                            onChange={(e) => setFilterNode(e.target.value)}
                            className="pl-10 pr-8 py-2 bg-white border border-gray-200 rounded-xl text-sm font-medium text-gray-700 shadow-sm focus:ring-2 focus:ring-[#1e3570] outline-none appearance-none cursor-pointer hover:border-gray-300 transition-all"
                        >
                            <option value="">Todos los Nodos</option>
                            {nodes.map((node) => (
                                <option key={node.node_id} value={node.node_id}>
                                    {node.node_id} ({node.description || 'Sin descripción'})
                                </option>
                            ))}
                        </select>
                    </div>

                    {/* Ordenamiento */}
                    <div className="relative">
                        <select 
                            value={orderBy}
                            onChange={(e) => setOrderBy(e.target.value)}
                            className="px-3 py-2 bg-white border border-gray-200 rounded-xl text-sm font-medium text-gray-700 shadow-sm focus:ring-2 focus:ring-[#1e3570] outline-none appearance-none cursor-pointer hover:border-gray-300 transition-all"
                        >
                            <option value="date_desc">Más recientes primero</option>
                            <option value="date_asc">Más antiguos primero</option>
                            <option value="coverage_desc">Mayor contaminación primero</option>
                            <option value="coverage_asc">Menor contaminación primero</option>
                        </select>
                    </div>
                </div>
            </div>

            {/* Barra de Acciones de Selección Múltiple */}
            {selectMode && detections.length > 0 && (
                <div className="flex items-center justify-between bg-blue-50 border border-blue-100 rounded-2xl p-4 mb-6 animate-in slide-in-from-top-4 duration-300">
                    <div className="flex items-center gap-3">
                        <button 
                            onClick={toggleAll}
                            className="flex items-center gap-2 text-xs font-bold text-blue-700 bg-white px-3 py-1.5 rounded-lg border border-blue-200 hover:bg-blue-100/50 transition-all"
                        >
                            {selectedIds.size === detections.length ? 'Deseleccionar Todo' : 'Seleccionar Todo'}
                        </button>
                        <span className="text-sm font-bold text-blue-900">
                            {selectedIds.size} seleccionados de {detections.length}
                        </span>
                    </div>

                    <button 
                        onClick={handleBulkDelete}
                        disabled={selectedIds.size === 0 || isDeletingBulk}
                        style={{
                            backgroundColor: (selectedIds.size > 0 && !isDeletingBulk) ? '#dc2626' : '#fee2e2',
                            color: (selectedIds.size > 0 && !isDeletingBulk) ? '#ffffff' : '#f87171'
                        }}
                        className="flex items-center gap-2 px-4 py-2 rounded-xl text-sm font-bold shadow-md transition-all hover:bg-red-700 disabled:shadow-none disabled:cursor-not-allowed border border-red-500/20"
                    >
                        <Trash2 className="w-4 h-4" />
                        {isDeletingBulk ? 'Eliminando...' : 'Eliminar Lote'}
                    </button>
                </div>
            )}

            {/* Content Area */}
            {loading ? (
                <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-6">
                    {[1, 2, 3, 4, 5, 6, 7, 8].map((n) => (
                        <div key={n} className="aspect-[4/3] bg-gray-100 rounded-2xl animate-pulse"></div>
                    ))}
                </div>
            ) : detections.length === 0 ? (
                <div className="flex flex-col items-center justify-center py-32 bg-gray-50 rounded-3xl border-2 border-dashed border-gray-200">
                    <Camera className="w-16 h-16 text-gray-300 mb-4" />
                    <h3 className="text-xl font-bold text-gray-900">No hay detecciones</h3>
                    <p className="text-gray-500">No se encontraron registros para gestionar.</p>
                </div>
            ) : (
                <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-6">
                    {detections.map((detection: any) => {
                        const isSelected = selectedIds.has(detection.detection_id);
                        return (
                            <div 
                                key={detection.detection_id}
                                onClick={() => selectMode ? toggleSelect(detection.detection_id) : setSelectedDetection(detection)}
                                className={`group relative bg-white rounded-2xl overflow-hidden border transition-all duration-300 cursor-pointer ${
                                    isSelected 
                                    ? 'border-blue-500 ring-2 ring-blue-500/20 shadow-md bg-blue-50/10' 
                                    : 'border-gray-100 shadow-sm hover:shadow-xl'
                                }`}
                            >
                                {/* Image Preview */}
                                <div className="aspect-[4/3] bg-gray-100 relative overflow-hidden">
                                    <img 
                                        src={`/api/waste-detections/${detection.detection_id}/image?type=original`} 
                                        alt="Detection" 
                                        className="w-full h-full object-cover transition-transform duration-500 group-hover:scale-110"
                                        loading="lazy"
                                    />
                                    
                                    {/* Checkbox Overlay in Select Mode */}
                                    {selectMode ? (
                                        <div className="absolute inset-0 bg-black/10 flex items-start justify-end p-3">
                                            {isSelected ? (
                                                <CheckSquare className="w-6 h-6 text-blue-600 fill-white drop-shadow-md" />
                                            ) : (
                                                <Square className="w-6 h-6 text-white fill-black/20 drop-shadow-md" />
                                            )}
                                        </div>
                                    ) : (
                                        /* Admin Actions Overlay (Single delete) */
                                        <div className="absolute inset-0 bg-black/0 group-hover:bg-black/20 transition-all">
                                            <button 
                                                onClick={(e) => handleDelete(e, detection.detection_id)}
                                                className="absolute top-3 right-3 p-2 bg-red-500 text-white rounded-lg opacity-0 group-hover:opacity-100 transition-all hover:bg-red-600 shadow-lg"
                                                title="Eliminar registro"
                                            >
                                                <Trash2 className="w-4 h-4" />
                                            </button>
                                        </div>
                                    )}

                                    <div className="absolute top-3 left-3">
                                        <span className="px-2 py-1 bg-[#1e3570]/90 backdrop-blur-md text-white text-[10px] font-black rounded-lg shadow-sm">
                                            {detection.node_id}
                                        </span>
                                    </div>
                                </div>

                                {/* Info */}
                                <div className="p-4 flex items-center justify-between">
                                    <div>
                                        <p className="text-[10px] font-bold text-gray-400 uppercase leading-none mb-1">Cobertura</p>
                                        <h4 className="font-bold text-gray-900">{detection.coverage_percent}%</h4>
                                    </div>
                                    <div className="text-right">
                                        <p className="text-[10px] font-bold text-gray-400 uppercase leading-none mb-1">Confianza</p>
                                        <h4 className="font-bold text-gray-900">{(detection.confidence * 100).toFixed(0)}%</h4>
                                    </div>
                                </div>
                            </div>
                        );
                    })}
                </div>
            )}

            {/* Detail Modal */}
            {selectedDetection && (
                <DetectionModal 
                    detection={selectedDetection} 
                    onClose={() => setSelectedDetection(null)} 
                />
            )}
        </div>
    );
}
