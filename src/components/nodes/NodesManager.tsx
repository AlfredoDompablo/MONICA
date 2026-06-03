'use client';

import { useState, useRef } from 'react';
import NodeTable from './NodeTable';
import { createNode, updateNode } from '@/actions/node-actions';
import { Pencil, Plus } from 'lucide-react';

type Node = {
  node_id: string;
  description: string;
  latitude: number | string;
  longitude: number | string;
  is_active: boolean;
  last_seen: Date | null;
  key_hash: string | null;
};

// Initial state for form actions (unused if using direct async handler wrapper)
// const initialState = { message: '', success: false };

export default function NodesManager({ nodes }: { nodes: any[] }) {
  const [editingNode, setEditingNode] = useState<Node | null>(null);
  const [idError, setIdError] = useState<string | null>(null);
  const [descriptionError, setDescriptionError] = useState<string | null>(null);
  const formRef = useRef<HTMLFormElement>(null);

  const handleEdit = (node: any) => {
    // Ensures proper typing when setting state
    setEditingNode({
        ...node,
        latitude: node.latitude?.toString() || '0', 
        longitude: node.longitude?.toString() || '0'
    });
    setIdError(null); // Clear error when switching to edit
    setDescriptionError(null);
    window.scrollTo({ top: 0, behavior: 'smooth' });
  };

  const handleCancelEdit = () => {
    setEditingNode(null);
    setIdError(null);
    setDescriptionError(null);
    formRef.current?.reset();
  };
  
  const handleIdChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const value = e.target.value;
    // Case-insensitive check
    if (nodes.some(n => n.node_id.toLowerCase() === value.toLowerCase())) {
      setIdError('Este ID ya está en uso.');
    } else {
      setIdError(null);
    }
  };

  const handleDescriptionChange = (e: React.ChangeEvent<HTMLInputElement>) => {
      const value = e.target.value;
      if (value.length < 5) {
          setDescriptionError('La descripción debe tener al menos 5 caracteres.');
      } else if (value.length > 100) {
          setDescriptionError('La descripción no puede exceder 100 caracteres.');
      } else {
          setDescriptionError(null);
      }
  };

  const handleSubmit = async (formData: FormData) => {
      // Prevent submission if ID error exists (double check)
      if (idError && !editingNode) {
          return; // Allow UI to show error, don't alert
      }
      
      const desc = formData.get('description') as string;
      if (desc.length < 5 || desc.length > 100) {
          setDescriptionError('La descripción debe tener entre 5 y 100 caracteres.');
          return;
      }

      let result;
      if (editingNode) {
          result = await updateNode(null, formData);
      } else {
          result = await createNode(null, formData);
      }
      
      if (result.success) {
          formRef.current?.reset();
          setEditingNode(null);
      } else {
          alert(result.message);
      }
  };

  const concentrators = nodes.filter(n => 
    n.node_id.toUpperCase().startsWith('NODE_C') || 
    n.node_id.toUpperCase().startsWith('CONC')
  );

  const sensors = nodes.filter(n => 
    !n.node_id.toUpperCase().startsWith('NODE_C') && 
    !n.node_id.toUpperCase().startsWith('CONC')
  );

  return (
    <div className="flex flex-col lg:flex-row gap-8">
      {/* Formulario (Izquierda/Arriba) - Ancho fijo en desktop */}
      <div className="w-full lg:w-80 flex-shrink-0">
        <div className={`bg-white dark:bg-gray-800 p-6 rounded-lg shadow border sticky top-6 transition-all duration-300 ${editingNode ? 'border-yellow-400 dark:border-yellow-600 ring-2 ring-yellow-400/20' : 'border-gray-200 dark:border-gray-700'}`}>
          <h2 className="text-lg font-semibold text-gray-900 dark:text-white mb-4 flex items-center gap-2">
            {editingNode ? (
                <>
                    <span className="p-2 bg-yellow-100 dark:bg-yellow-900/30 text-yellow-600 rounded-md"><Pencil size={18}/></span>
                    Editar Nodo
                </>
            ) : (
                <>
                    <span className="p-2 bg-blue-100 dark:bg-blue-900/30 text-blue-600 rounded-md"><Plus size={18}/></span>
                    Registrar Nuevo Nodo
                </>
            )}
          </h2>
          
          <form action={handleSubmit} ref={formRef} className="space-y-4">
             {/* Hidden ID for updates */}
             {editingNode && <input type="hidden" name="node_id" value={editingNode.node_id} />}

            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">ID del Nodo</label>
              <input 
                name="node_id" 
                placeholder="Ej. CONC_NORTE o NODE_005" 
                required
                readOnly={!!editingNode}
                defaultValue={editingNode?.node_id || ''}
                onChange={!editingNode ? handleIdChange : undefined}
                className={`w-full rounded-md border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-900 text-gray-900 dark:text-white shadow-sm focus:border-blue-500 focus:ring-blue-500 py-2 px-3 border ${editingNode ? 'opacity-60 cursor-not-allowed bg-gray-100 dark:bg-gray-800' : ''} ${idError ? 'border-red-500 focus:border-red-500 focus:ring-red-500' : ''}`}
              />
              {idError && !editingNode && (
                <p className="text-xs text-red-600 font-bold mt-1 animate-pulse">
                  ⚠️ {idError}
                </p>
              )}
              {!editingNode && !idError && (
                <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
                  Usa el prefijo <span className="font-semibold text-blue-600">CONC_</span> o <span className="font-semibold text-blue-600">NODE_C</span> para registrar un concentrador. De lo contrario, se registrará como un nodo sensor.
                </p>
              )}
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">Descripción</label>
              <input 
                name="description" 
                placeholder="Ubicación o propósito" 
                required
                key={editingNode ? `desc-${editingNode.node_id}` : 'desc-new'}
                defaultValue={editingNode?.description || ''}
                onChange={handleDescriptionChange}
                className={`w-full rounded-md border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-900 text-gray-900 dark:text-white shadow-sm focus:border-blue-500 focus:ring-blue-500 py-2 px-3 border ${descriptionError ? 'border-red-500 focus:border-red-500 focus:ring-red-500' : ''}`}
              />
              {descriptionError && (
                <p className="text-xs text-red-600 font-bold mt-1 animate-pulse">
                  ⚠️ {descriptionError}
                </p>
              )}
            </div>

            <div className="flex gap-2 pt-2">
                {editingNode && (
                    <button 
                        type="button"
                        onClick={handleCancelEdit}
                        className="w-1/3 py-2 px-4 border border-gray-300 rounded-md shadow-sm text-sm font-medium text-gray-700 bg-white hover:bg-gray-50 dark:bg-gray-700 dark:text-gray-200 dark:border-gray-600 dark:hover:bg-gray-600 transition-colors"
                    >
                        Cancelar
                    </button>
                )}
                <button 
                type="submit"
                className={`flex-1 flex justify-center py-2 px-4 border border-transparent rounded-md shadow-sm text-sm font-medium text-white focus:outline-none focus:ring-2 focus:ring-offset-2 transition-colors ${editingNode ? 'bg-yellow-600 hover:bg-yellow-700 focus:ring-yellow-500' : 'bg-blue-600 hover:bg-blue-700 focus:ring-blue-500'}`}
                >
                {editingNode ? 'Guardar Cambios' : 'Crear Nodo'}
                </button>
            </div>
            
          </form>
        </div>
      </div>

      {/* Tablas (Derecha) - Ocupa el resto */}
      <div className="flex-1 min-w-0 space-y-8">
        <div>
          <h2 className="text-xl font-bold text-gray-800 dark:text-white mb-3 flex items-center gap-2">
            <span className="w-2 h-2 rounded-full bg-blue-600"></span>
            Concentradores
          </h2>
          <NodeTable nodes={concentrators} onEdit={handleEdit} isConcentratorTable={true} />
        </div>

        <div>
          <h2 className="text-xl font-bold text-gray-800 dark:text-white mb-3 flex items-center gap-2">
            <span className="w-2 h-2 rounded-full bg-indigo-600"></span>
            Nodos Sensores
          </h2>
          <NodeTable nodes={sensors} onEdit={handleEdit} isConcentratorTable={false} />
        </div>
      </div>
    </div>
  );
}
