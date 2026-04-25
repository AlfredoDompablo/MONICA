"use client";

import React, { useState, useEffect } from 'react';
import { Camera, Filter, RefreshCcw, Trash2 } from 'lucide-react';
import DetectionModal from '@/components/admin/gallery/DetectionModal';

export default function AdminGalleryPage() {
    const [detections, setDetections] = useState([]);
    const [loading, setLoading] = useState(true);
    const [selectedDetection, setSelectedDetection] = useState(null);
    const [filterNode, setFilterNode] = useState('');
    
    const fetchDetections = async () => {
        setLoading(true);
        try {
            const url = filterNode ? `/api/waste-detections?node_id=${filterNode}` : '/api/waste-detections';
            const res = await fetch(url);
            const data = await res.json();
            setDetections(data);
        } catch (error) {
            console.error('Error loading detections:', error);
        } finally {
            setLoading(false);
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

    useEffect(() => {
        fetchDetections();
    }, [filterNode]);

    return (
        <div className="p-8 max-w-7xl mx-auto min-h-screen">
            {/* Header */}
            <div className="flex flex-col md:flex-row md:items-center justify-between mb-8 gap-4">
                <div>
                    <h1 className="text-3xl font-bold text-gray-900">Gestión de Vigilancia</h1>
                    <p className="text-gray-500 mt-1">Administra los registros visuales y depura detecciones erróneas.</p>
                </div>
                <div className="flex items-center gap-3">
                    <button 
                        onClick={fetchDetections}
                        className="p-2 text-gray-400 hover:text-[#1e3570] hover:bg-gray-100 rounded-lg transition-all"
                        title="Refrescar"
                    >
                        <RefreshCcw className={`w-5 h-5 ${loading ? 'animate-spin' : ''}`} />
                    </button>
                    <div className="relative">
                        <Filter className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-400" />
                        <select 
                            value={filterNode}
                            onChange={(e) => setFilterNode(e.target.value)}
                            className="pl-10 pr-4 py-2 bg-white border border-gray-200 rounded-xl text-sm font-medium text-gray-700 shadow-sm focus:ring-2 focus:ring-[#1e3570] outline-none appearance-none cursor-pointer hover:border-gray-300 transition-all"
                        >
                            <option value="">Todos los Nodos</option>
                            <option value="NODO-01">NODO-01</option>
                            <option value="NODO-02">NODO-02</option>
                        </select>
                    </div>
                </div>
            </div>

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
                    {detections.map((detection: any) => (
                        <div 
                            key={detection.detection_id}
                            onClick={() => setSelectedDetection(detection)}
                            className="group relative bg-white rounded-2xl overflow-hidden border border-gray-100 shadow-sm hover:shadow-xl transition-all duration-300 cursor-pointer"
                        >
                            {/* Image Preview */}
                            <div className="aspect-[4/3] bg-gray-100 relative overflow-hidden">
                                <img 
                                    src={`/api/waste-detections/${detection.detection_id}/image?type=original`} 
                                    alt="Detection" 
                                    className="w-full h-full object-cover transition-transform duration-500 group-hover:scale-110"
                                    loading="lazy"
                                />
                                
                                {/* Admin Actions Overlay */}
                                <div className="absolute inset-0 bg-black/0 group-hover:bg-black/20 transition-all">
                                    <button 
                                        onClick={(e) => handleDelete(e, detection.detection_id)}
                                        className="absolute top-3 right-3 p-2 bg-red-500 text-white rounded-lg opacity-0 group-hover:opacity-100 transition-all hover:bg-red-600 shadow-lg"
                                        title="Eliminar registro"
                                    >
                                        <Trash2 className="w-4 h-4" />
                                    </button>
                                </div>

                                <div className="absolute top-3 left-3 flex gap-2">
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
                    ))}
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

