"use client";

import React from 'react';
import { X, Calendar, MapPin, Cpu, Target, Download } from 'lucide-react';
import ImageSlider from './ImageSlider';

interface DetectionModalProps {
    detection: any;
    onClose: () => void;
}

export default function DetectionModal({ detection, onClose }: DetectionModalProps) {
    if (!detection) return null;

    const originalUrl = `/api/waste-detections/${detection.detection_id}/image?type=original`;
    const maskedUrl = `/api/waste-detections/${detection.detection_id}/image?type=masked`;

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/60 backdrop-blur-md animate-in fade-in duration-300">
            <div className="bg-white rounded-2xl shadow-2xl w-full max-w-5xl max-h-[90vh] overflow-hidden flex flex-col md:flex-row animate-in zoom-in-95 duration-300">
                
                {/* Left Side: Interactive Gallery */}
                <div className="flex-1 bg-gray-900 p-4 flex items-center justify-center min-h-[300px]">
                    <div className="w-full max-w-3xl">
                        <ImageSlider 
                            originalUrl={originalUrl} 
                            maskedUrl={maskedUrl} 
                        />
                    </div>
                </div>

                {/* Right Side: Data & Actions */}
                <div className="w-full md:w-80 lg:w-96 p-6 flex flex-col border-l border-gray-100 bg-white">
                    <div className="flex justify-between items-start mb-6">
                        <div>
                            <h3 className="text-xl font-bold text-gray-900">Análisis de Detección</h3>
                            <p className="text-xs text-gray-500 uppercase tracking-wider font-semibold">ID: #{detection.detection_id}</p>
                        </div>
                        <button onClick={onClose} className="p-2 hover:bg-gray-100 rounded-full transition-colors">
                            <X className="w-5 h-5 text-gray-400" />
                        </button>
                    </div>

                    <div className="space-y-6 flex-1 overflow-y-auto pr-2">
                        {/* Coverage Score */}
                        <div className="p-4 bg-blue-50 rounded-xl border border-blue-100">
                            <div className="flex items-center justify-between mb-2">
                                <span className="text-sm font-medium text-blue-700">Cobertura de Residuos</span>
                                <span className="text-2xl font-black text-blue-900">{detection.coverage_percent}%</span>
                            </div>
                            <div className="w-full h-2 bg-blue-200 rounded-full overflow-hidden">
                                <div 
                                    className="h-full bg-blue-600 transition-all duration-1000" 
                                    style={{ width: `${detection.coverage_percent}%` }}
                                ></div>
                            </div>
                        </div>

                        {/* Metadata Grid */}
                        <div className="grid grid-cols-1 gap-4">
                            <div className="flex items-center gap-3">
                                <div className="p-2 bg-gray-100 rounded-lg">
                                    <MapPin className="w-4 h-4 text-gray-600" />
                                </div>
                                <div>
                                    <p className="text-[10px] text-gray-500 uppercase font-bold">Ubicación (Nodo)</p>
                                    <p className="text-sm font-semibold text-gray-900">{detection.node_id}</p>
                                </div>
                            </div>

                            <div className="flex items-center gap-3">
                                <div className="p-2 bg-gray-100 rounded-lg">
                                    <Calendar className="w-4 h-4 text-gray-600" />
                                </div>
                                <div>
                                    <p className="text-[10px] text-gray-500 uppercase font-bold">Fecha y Hora</p>
                                    <p className="text-sm font-semibold text-gray-900">
                                        {new Date(detection.timestamp).toLocaleString()}
                                    </p>
                                </div>
                            </div>

                            <div className="flex items-center gap-3">
                                <div className="p-2 bg-gray-100 rounded-lg">
                                    <Target className="w-4 h-4 text-gray-600" />
                                </div>
                                <div>
                                    <p className="text-[10px] text-gray-500 uppercase font-bold">Confianza de IA</p>
                                    <p className="text-sm font-semibold text-gray-900">{(detection.confidence * 100).toFixed(1)}%</p>
                                </div>
                            </div>

                            <div className="flex items-center gap-3">
                                <div className="p-2 bg-gray-100 rounded-lg">
                                    <Cpu className="w-4 h-4 text-gray-600" />
                                </div>
                                <div>
                                    <p className="text-[10px] text-gray-500 uppercase font-bold">Versión del Modelo</p>
                                    <p className="text-sm font-semibold text-gray-900">{detection.model_version || 'v1.0.0'}</p>
                                </div>
                            </div>
                        </div>
                    </div>

                    {/* Actions */}
                    <div className="pt-6 mt-6 border-t border-gray-100 space-y-3">
                        <a 
                            href={originalUrl} 
                            download 
                            className="flex items-center justify-center gap-2 w-full py-2.5 bg-[#1e3570] text-white rounded-xl text-sm font-bold hover:bg-[#1e3570]/90 transition-all shadow-md"
                        >
                            <Download className="w-4 h-4" /> Descargar Original
                        </a>
                        <a 
                            href={maskedUrl} 
                            download 
                            className="flex items-center justify-center gap-2 w-full py-2.5 bg-white border border-gray-200 text-gray-700 rounded-xl text-sm font-bold hover:bg-gray-50 transition-all"
                        >
                            <Download className="w-4 h-4" /> Descargar Máscara
                        </a>
                    </div>
                </div>
            </div>
        </div>
    );
}
