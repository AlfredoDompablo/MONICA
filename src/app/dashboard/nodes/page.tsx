"use client";

import { useEffect, useState } from 'react';
import { Pencil, Trash2, Plus } from 'lucide-react';
import NodeModal from '@/components/NodeModal';

interface Node {
    node_id: string;
    description: string;
    latitude: number;
    longitude: number;
    status?: string;
    last_seen?: string;
}

/**
 * Página de Gestión de Nodos
 * 
 * Permite a los administradores:
 * - Listar todos los nodos registrados.
 * - Agregar nuevos nodos.
 * - Editar la ubicación y descripción de nodos existentes.
 * - Eliminar nodos.
 */
export default function NodesPage() {
    const [nodes, setNodes] = useState<Node[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [searchTerm, setSearchTerm] = useState('');
    
    // Modal state
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingNode, setEditingNode] = useState<Node | null>(null);

    const fetchNodes = async () => {
        setIsLoading(true);
        try {
            const response = await fetch('/api/nodes');
            if (response.ok) {
                const data = await response.json();
                setNodes(data);
            }
        } catch (error) {
            console.error("Error fetching nodes:", error);
        } finally {
            setIsLoading(false);
        }
    };

    useEffect(() => {
        fetchNodes();
    }, []);

    const handleEdit = (node: Node) => {
        setEditingNode(node);
        setIsModalOpen(true);
    };

    const handleCreate = () => {
        setEditingNode(null);
        setIsModalOpen(true);
    };

    const handleDelete = async (nodeId: string) => {
        if (!confirm('¿Estás seguro de que deseas eliminar este nodo? Esta acción no se puede deshacer.')) return;

        try {
            const response = await fetch(`/api/nodes/${nodeId}`, {
                method: 'DELETE',
            });

            if (response.ok) {
                setNodes(nodes.filter(n => n.node_id !== nodeId));
                alert('Nodo eliminado correctamente');
            } else {
                const errorData = await response.json();
                alert(`Error al eliminar: ${errorData.error}`);
            }
        } catch (error) {
            console.error("Error deleting node:", error);
            alert('Error al eliminar el nodo');
        }
    };

    const handleSave = async (nodeData: Node) => {
        const isEditing = !!editingNode;
        const url = isEditing ? `/api/nodes/${nodeData.node_id}` : '/api/nodes';
        const method = isEditing ? 'PUT' : 'POST';

        const response = await fetch(url, {
            method,
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(nodeData),
        });

        if (response.ok) {
            await fetchNodes(); // Refresh list
            setIsModalOpen(false);
        } else {
            const error = await response.json();
            throw new Error(error.error || 'Error al guardar');
        }
    };

    const filteredNodes = nodes.filter(node => 
        node.node_id.toLowerCase().includes(searchTerm.toLowerCase()) ||
        node.description.toLowerCase().includes(searchTerm.toLowerCase())
    );

    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <h1 className="text-2xl font-bold text-gray-900">Gestión de Nodos</h1>
                <button
                    onClick={handleCreate}
                    className="inline-flex items-center px-4 py-2 border border-transparent text-sm font-medium rounded-md shadow-sm text-white bg-[#1e3570] hover:bg-[#162a5a] focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-[#1e3570]"
                >
                    <Plus className="h-4 w-4 mr-2" />
                    Nuevo Nodo
                </button>
            </div>

            <div className="bg-white shadow rounded-lg overflow-hidden">
                <div className="p-4 border-b border-gray-200 bg-gray-50 flex items-center gap-4">
                    <input
                        type="text"
                        placeholder="Buscar por ID o descripción..."
                        className="block w-full max-w-sm rounded-md border-gray-300 shadow-sm focus:border-[#1e3570] focus:ring-[#1e3570] sm:text-sm p-2 border"
                        value={searchTerm}
                        onChange={(e) => setSearchTerm(e.target.value)}
                    />
                </div>
                
                <div className="overflow-x-auto">
                    <table className="min-w-full divide-y divide-gray-200">
                        <thead className="bg-gray-50">
                            <tr>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">ID</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Descripción</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Latitud</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Longitud</th>
                                <th scope="col" className="px-6 py-3 text-right text-xs font-medium text-gray-500 uppercase tracking-wider">Acciones</th>
                            </tr>
                        </thead>
                        <tbody className="bg-white divide-y divide-gray-200">
                            {isLoading ? (
                                <tr>
                                    <td colSpan={5} className="px-6 py-4 text-center text-gray-500">Cargando nodos...</td>
                                </tr>
                            ) : filteredNodes.length === 0 ? (
                                <tr>
                                    <td colSpan={5} className="px-6 py-4 text-center text-gray-500">No se encontraron nodos.</td>
                                </tr>
                            ) : (
                                filteredNodes.map((node) => (
                                    <tr key={node.node_id} className="hover:bg-gray-50">
                                        <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">{node.node_id}</td>
                                        <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">{node.description}</td>
                                        <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">{Number(node.latitude).toFixed(6)}</td>
                                        <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">{Number(node.longitude).toFixed(6)}</td>
                                        <td className="px-6 py-4 whitespace-nowrap text-right text-sm font-medium">
                                            <button
                                                onClick={() => handleEdit(node)}
                                                className="text-indigo-600 hover:text-indigo-900 mr-4"
                                                title="Editar"
                                            >
                                                <Pencil className="h-4 w-4" />
                                            </button>
                                            <button
                                                onClick={() => handleDelete(node.node_id)}
                                                className="text-red-600 hover:text-red-900"
                                                title="Eliminar"
                                            >
                                                <Trash2 className="h-4 w-4" />
                                            </button>
                                        </td>
                                    </tr>
                                ))
                            )}
                        </tbody>
                    </table>
                </div>
            </div>

            <NodeModal
                isOpen={isModalOpen}
                onClose={() => setIsModalOpen(false)}
                onSave={handleSave}
                node={editingNode}
            />
        </div>
    );
}
