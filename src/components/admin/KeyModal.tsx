'use client';

import { useState } from 'react';
import { X, Copy, Check } from 'lucide-react';

interface KeyModalProps {
  isOpen: boolean;
  onClose: () => void;
  apiKey: string | null;
  nodeId: string;
}

export default function KeyModal({ isOpen, onClose, apiKey, nodeId }: KeyModalProps) {
  const [copied, setCopied] = useState(false);

  if (!isOpen || !apiKey) return null;

  const handleCopy = () => {
    navigator.clipboard.writeText(apiKey);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm">
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow-xl w-full max-w-md p-6 relative animate-in fade-in zoom-in duration-200">
        <button 
          onClick={onClose}
          className="absolute top-4 right-4 text-gray-500 hover:text-gray-700 dark:hover:text-gray-300 transition-colors"
        >
          <X size={20} />
        </button>

        <div className="mb-6">
          <h3 className="text-xl font-bold text-gray-900 dark:text-white mb-2">
            Nueva API Key Generada
          </h3>
          <p className="text-sm text-yellow-600 dark:text-yellow-400 bg-yellow-50 dark:bg-yellow-900/20 p-3 rounded-md border border-yellow-200 dark:border-yellow-800">
            ⚠️ <strong>Importante:</strong> Esta clave solo se mostrará una vez. Cópiala y guárdala en un lugar seguro (secrets.h).
          </p>
        </div>

        <div className="space-y-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              Nodo ID
            </label>
            <input 
              type="text" 
              readOnly 
              value={nodeId}
              className="w-full bg-gray-100 dark:bg-gray-700 border-none rounded-md px-3 py-2 text-gray-600 dark:text-gray-400 font-mono text-sm"
            />
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              API Key (Token Seguro)
            </label>
            <div className="flex gap-2">
              <input 
                type="text" 
                readOnly 
                value={apiKey}
                className="flex-1 bg-white dark:bg-gray-900 border border-gray-300 dark:border-gray-600 rounded-md px-3 py-2 text-green-600 dark:text-green-400 font-mono text-sm font-bold shadow-sm focus:ring-2 focus:ring-green-500 outline-none"
              />
              <button
                onClick={handleCopy}
                className="bg-gray-100 hover:bg-gray-200 dark:bg-gray-700 dark:hover:bg-gray-600 text-gray-700 dark:text-gray-200 p-2 rounded-md transition-colors flex items-center justify-center min-w-[44px]"
                title="Copiar al portapapeles"
              >
                {copied ? <Check size={20} className="text-green-500" /> : <Copy size={20} />}
              </button>
            </div>
          </div>
        </div>

        <div className="mt-8 flex justify-end">
          <button
            onClick={onClose}
            className="px-4 py-2 bg-gray-900 dark:bg-white text-white dark:text-gray-900 rounded-md hover:opacity-90 transition-opacity font-medium text-sm"
          >
            Entendido, cerrar
          </button>
        </div>
      </div>
    </div>
  );
}
