"use client";

import { useState, useEffect } from 'react';

interface Node {
    node_id: string;
    description: string;
    latitude: number;
    longitude: number;
}

interface NodeModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: (node: Node) => Promise<void>;
    node: Node | null;
}

export default function NodeModal({ isOpen, onClose, onSave, node }: NodeModalProps) {
    const [formData, setFormData] = useState<{
        node_id: string;
        description: string;
        latitude: string | number;
        longitude: string | number;
    }>({
        node_id: '',
        description: '',
        latitude: '',
        longitude: '',
    });
    const [isSaving, setIsSaving] = useState(false);

    useEffect(() => {
        if (node) {
            setFormData(node);
        } else {
            setFormData({
                node_id: '',
                description: '',
                latitude: '',
                longitude: '',
            });
        }
    }, [node, isOpen]);

    if (!isOpen) return null;

    // Manejador del envío del formulario
    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setIsSaving(true);
        try {
            await onSave({
                ...formData,
                // Convertimos las cadenas a números flotantes antes de enviar
                // Esto es necesario porque el estado local permite cadenas intermedias para la edición
                latitude: typeof formData.latitude === 'string' ? parseFloat(formData.latitude) : formData.latitude,
                longitude: typeof formData.longitude === 'string' ? parseFloat(formData.longitude) : formData.longitude,
            });
            onClose();
        } catch (error) {
            console.error("Error saving node:", error);
        } finally {
            setIsSaving(false);
        }
    };

    /**
     * Maneja los cambios en los inputs numéricos.
     * Permite escribir signos negativos y valida dinámicamente rangos (min/max).
     * Si el usuario escribe un valor fuera de rango, lo ajusta (clamping) o lo ignora.
     */
    const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const { name, value, min, max } = e.target;

        // Permitimos borrar el campo o escribir solo "-" para empezar un negativo
        if (value === '' || value === '-') {
            setFormData(prev => ({ ...prev, [name]: value }));
            return;
        }

        const numValue = parseFloat(value);
        if (isNaN(numValue)) return; // Ignoramos si no es número

        // Si excede el máximo, forzamos el valor máximo
        if (max && numValue > parseFloat(max)) {
            setFormData(prev => ({ ...prev, [name]: max }));
            return;
        }

        // Si es menor al mínimo (y ya es un número completo válido), forzamos el mínimo
        if (min && numValue < parseFloat(min)) {
            setFormData(prev => ({ ...prev, [name]: min }));
            return;
        }

        setFormData(prev => ({ ...prev, [name]: value }));
    };

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm">
            <div className="bg-white rounded-lg shadow-xl w-full max-w-md mx-4 overflow-hidden">
                <div className="px-6 py-4 border-b border-gray-200 flex justify-between items-center">
                    <h3 className="text-lg font-medium text-gray-900">
                        {node ? 'Editar Nodo' : 'Nuevo Nodo'}
                    </h3>
                    <button onClick={onClose} className="text-gray-400 hover:text-gray-500">
                        <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                        </svg>
                    </button>
                </div>
                
                <form onSubmit={handleSubmit} className="p-6 space-y-4">
                    <div>
                        <label className="block text-sm font-medium text-gray-700">ID del Nodo</label>
                        <input
                            type="text"
                            required
                            disabled={!!node} // ID cannot be changed if editing
                            value={formData.node_id}
                            onChange={(e) => setFormData({ ...formData, node_id: e.target.value })}
                            className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-[#1e3570] focus:ring-[#1e3570] sm:text-sm p-2 border disabled:bg-gray-100 disabled:text-gray-500"
                        />
                    </div>
                    
                    <div>
                        <label className="block text-sm font-medium text-gray-700">Descripción</label>
                        <input
                            type="text"
                            required
                            value={formData.description}
                            onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                            className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-[#1e3570] focus:ring-[#1e3570] sm:text-sm p-2 border"
                        />
                    </div>
                    
                    <div className="grid grid-cols-2 gap-4">
                        <div>
                            <label className="block text-sm font-medium text-gray-700">Latitud</label>
                            <input
                                type="number"
                                step="any"
                                min="-90"
                                max="90"
                                required
                                name="latitude"
                                value={formData.latitude}
                                onChange={handleInputChange}
                                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-[#1e3570] focus:ring-[#1e3570] sm:text-sm p-2 border"
                            />
                        </div>
                        <div>
                            <label className="block text-sm font-medium text-gray-700">Longitud</label>
                            <input
                                type="number"
                                step="any"
                                min="-180"
                                max="180"
                                required
                                name="longitude"
                                value={formData.longitude}
                                onChange={handleInputChange}
                                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-[#1e3570] focus:ring-[#1e3570] sm:text-sm p-2 border"
                            />
                        </div>
                    </div>

                    <div className="flex justify-end gap-3 mt-6 pt-2">
                        <button
                            type="button"
                            onClick={onClose}
                            className="px-4 py-2 text-sm font-medium text-gray-700 bg-white border border-gray-300 rounded-md hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-[#1e3570]"
                        >
                            Cancelar
                        </button>
                        <button
                            type="submit"
                            disabled={isSaving}
                            className="px-4 py-2 text-sm font-medium text-white bg-[#1e3570] border border-transparent rounded-md hover:bg-[#162a5a] focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-[#1e3570] disabled:opacity-70 flex items-center"
                        >
                            {isSaving ? 'Guardando...' : 'Guardar'}
                        </button>
                    </div>
                </form>
            </div>
        </div>
    );
}
