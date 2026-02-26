'use client';

import { useState, useEffect } from 'react';
import { generateNodeKey, toggleNodeStatus, deleteNode } from '@/actions/node-actions';
import { Key, Trash2, Power, PowerOff, RefreshCw, Pencil } from 'lucide-react';
import KeyModal from '@/components/admin/KeyModal';

type Node = {
  node_id: string;
  description: string;
  is_active: boolean;
  last_seen: Date | null;
  key_hash: string | null;
  latitude: number | string; // Handle potentially serializableDecimal
  longitude: number | string;
};

export default function NodeTable({ nodes, onEdit }: { nodes: Node[], onEdit: (node: Node) => void }) {
  const [loading, setLoading] = useState<string | null>(null);
  const [modalData, setModalData] = useState<{ isOpen: boolean; apiKey: string | null; nodeId: string }>({
    isOpen: false,
    apiKey: null,
    nodeId: '',
  });
  
  // Hydration fix: Ensure client matches server by only rendering formatted dates after mount
  const [mounted, setMounted] = useState(false);
  useEffect(() => {
    setMounted(true);
  }, []);

  const handleGenerateKey = async (nodeId: string) => {
    if (!confirm('¿Generar una nueva API Key invalidará la anterior. ¿Deseas continuar?')) return;
    
    setLoading(nodeId);
    try {
      const result = await generateNodeKey(nodeId);
      if (result.success && result.apiKey) {
        setModalData({ isOpen: true, apiKey: result.apiKey, nodeId });
      } else {
        alert('Error: ' + result.message);
      }
    } catch (err) {
      alert('Error inesperado');
    } finally {
      setLoading(null);
    }
  };

  const handleToggleStatus = async (nodeId: string, currentStatus: boolean) => {
    setLoading(nodeId);
    await toggleNodeStatus(nodeId, !currentStatus);
    setLoading(null);
  };

  const handleDelete = async (nodeId: string) => {
    if (!confirm(`¿Estás seguro de ELIMINAR el nodo ${nodeId}? Esta acción no se puede deshacer.`)) return;
    setLoading(nodeId);
    await deleteNode(nodeId);
    setLoading(null);
  };

  return (
    <>
      <div className="overflow-x-auto bg-white dark:bg-gray-800 rounded-lg shadow border border-gray-200 dark:border-gray-700">
        <table className="min-w-full divide-y divide-gray-200 dark:divide-gray-700">
          <thead className="bg-gray-50 dark:bg-gray-700/50">
            <tr>
              <th className="px-3 py-3 text-left text-xs font-medium text-gray-500 dark:text-gray-400 uppercase tracking-wider">ID del Nodo</th>
              <th className="px-3 py-3 text-left text-xs font-medium text-gray-500 dark:text-gray-400 uppercase tracking-wider">Descripción</th>
              <th className="px-3 py-3 text-left text-xs font-medium text-gray-500 dark:text-gray-400 uppercase tracking-wider">Estado</th>
              <th className="px-3 py-3 text-left text-xs font-medium text-gray-500 dark:text-gray-400 uppercase tracking-wider">Última Conexión</th>
              <th className="px-3 py-3 text-left text-xs font-medium text-gray-500 dark:text-gray-400 uppercase tracking-wider">Seguridad</th>
              <th className="px-3 py-3 text-right text-xs font-medium text-gray-500 dark:text-gray-400 uppercase tracking-wider">Acciones</th>
            </tr>
          </thead>
          <tbody className="bg-white dark:bg-gray-800 divide-y divide-gray-200 dark:divide-gray-700">
            {nodes.map((node) => (
              <tr key={node.node_id} className="hover:bg-gray-50 dark:hover:bg-gray-700/50 transition-colors">
                <td className="px-3 py-4 whitespace-nowrap text-sm font-medium text-gray-900 dark:text-white">
                  {node.node_id}
                </td>
                <td className="px-3 py-4 whitespace-nowrap text-sm text-gray-500 dark:text-gray-400">
                  {node.description}
                </td>
                <td className="px-3 py-4 whitespace-nowrap text-sm">
                  <span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium ${
                    node.is_active 
                      ? 'bg-green-100 text-green-800 dark:bg-green-900/30 dark:text-green-400' 
                      : 'bg-red-100 text-red-800 dark:bg-red-900/30 dark:text-red-400'
                  }`}>
                    {node.is_active ? 'Activo' : 'Inactivo'}
                  </span>
                </td>
                <td className="px-3 py-4 whitespace-nowrap text-sm text-gray-500 dark:text-gray-400">
                  {mounted ? (
                    node.last_seen ? new Date(node.last_seen).toLocaleString() : 'Nunca'
                  ) : (
                    node.last_seen ? '...' : 'Nunca'
                  )}
                </td>
                <td className="px-3 py-4 whitespace-nowrap text-sm text-gray-500 dark:text-gray-400">
                  {node.key_hash ? (
                    <span className="text-green-600 dark:text-green-400 flex items-center gap-1 text-xs">
                       <Key size={14} /> Configurado
                    </span>
                  ) : (
                    <span className="text-yellow-600 dark:text-yellow-400 flex items-center gap-1 text-xs">
                       ⚠️ Sin configurar
                    </span>
                  )}
                </td>
                <td className="px-3 py-4 whitespace-nowrap text-right text-sm font-medium flex justify-end gap-2">
                  <button
                    onClick={() => onEdit(node)}
                    disabled={!!loading}
                    className="text-blue-600 hover:text-blue-900 dark:text-blue-400 dark:hover:text-blue-300 p-1 rounded hover:bg-blue-50 dark:hover:bg-blue-900/30"
                    title="Editar Nodo"
                  >
                    <Pencil size={18} />
                  </button>

                  <button
                    onClick={() => handleGenerateKey(node.node_id)}
                    disabled={loading === node.node_id}
                    className="text-indigo-600 hover:text-indigo-900 dark:text-indigo-400 dark:hover:text-indigo-300 p-1 rounded hover:bg-indigo-50 dark:hover:bg-indigo-900/30"
                    title="Generar nueva API Key"
                  >
                    {loading === node.node_id ? <RefreshCw size={18} className="animate-spin" /> : <Key size={18} />}
                  </button>
                  
                  <button
                    onClick={() => handleToggleStatus(node.node_id, node.is_active)}
                    disabled={loading === node.node_id}
                    className={`${node.is_active ? 'text-orange-600 hover:text-orange-900' : 'text-green-600 hover:text-green-900'} p-1 rounded hover:bg-gray-100 dark:hover:bg-gray-700`}
                    title={node.is_active ? "Desactivar nodo" : "Activar nodo"}
                  >
                    {node.is_active ? <PowerOff size={18} /> : <Power size={18} />}
                  </button>

                  <button
                    onClick={() => handleDelete(node.node_id)}
                    disabled={loading === node.node_id}
                    className="text-red-600 hover:text-red-900 dark:text-red-400 dark:hover:text-red-300 p-1 rounded hover:bg-red-50 dark:hover:bg-red-900/30"
                    title="Eliminar nodo"
                  >
                    <Trash2 size={18} />
                  </button>
                </td>
              </tr>
            ))}
            {nodes.length === 0 && (
              <tr>
                <td colSpan={6} className="px-6 py-8 text-center text-gray-500 dark:text-gray-400">
                  No hay nodos registrados. Crea uno nuevo para comenzar.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      <KeyModal 
        isOpen={modalData.isOpen} 
        onClose={() => setModalData(prev => ({ ...prev, isOpen: false }))}
        apiKey={modalData.apiKey}
        nodeId={modalData.nodeId}
      />
    </>
  );
}
