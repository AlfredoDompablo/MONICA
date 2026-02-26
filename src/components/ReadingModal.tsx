import { useState, useEffect } from 'react';
import { X } from 'lucide-react';

interface ReadingModalProps {
  isOpen: boolean;
  onClose: () => void;
  onSave: (reading: any) => Promise<void>;
  reading: any;
}

/**
 * Componente ReadingModal
 * 
 * Ventana modal para la edición de lecturas de sensores.
 * Permite modificar manualmente los valores de parámetros físicos (pH, temperatura, etc.)
 * y la batería, validando los límites permitidos antes de guardar.
 * 
 * @param {ReadingModalProps} props - Propiedades del modal.
 */
export default function ReadingModal({ isOpen, onClose, onSave, reading }: ReadingModalProps) {
  const [formData, setFormData] = useState<any>({
    ph: '',
    dissolved_oxygen: '',
    turbidity: '',
    conductivity: '', // El estado interno usa el término correcto
    temperature: '',
    battery_level: ''
  });
  const [isSaving, setIsSaving] = useState(false);

  useEffect(() => {
    if (reading) {
      setFormData({
        ph: reading.ph ?? '',
        dissolved_oxygen: reading.dissolved_oxygen ?? '',
        turbidity: reading.turbidity ?? '',
        conductivity: reading.conductivity ?? '', // Mapeo directo ahora
        temperature: reading.temperature ?? '',
        battery_level: reading.battery_level ?? ''
      });
    }
  }, [reading]);

  /*
   * Maneja el cambio de valores en el formulario.
   * Implementa validación en tiempo real para asegurar que los valores (pH, Temp, etc.)
   * no excedan los límites físicos definidos en `min` y `max`.
   */
  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value, min, max } = e.target;
    
    // Permitir vacío o signo negativo temporal
    if (value === '' || value === '-') {
        setFormData((prev: any) => ({ ...prev, [name]: value }));
        return;
    }

    const numValue = parseFloat(value);
    
    if (isNaN(numValue)) return;

    // Validación de Máximo: Si se pasa, asignamos el máximo permitido.
    if (max && numValue > parseFloat(max)) {
        setFormData((prev: any) => ({ ...prev, [name]: max }));
        return;
    }
    
    // Validación de Mínimo: Igual que el máximo, clamp al mínimo.
    if (min && numValue < parseFloat(min)) {
         setFormData((prev: any) => ({ ...prev, [name]: min }));
         return;
    }

    // Si es válido, actualizamos el estado
    setFormData((prev: any) => ({
      ...prev,
      [name]: value
    }));
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsSaving(true);
    try {
      // Convert values to numbers or null before saving
      const dataToSave = {
        ...reading,
        ph: formData.ph !== '' ? parseFloat(formData.ph) : null,
        dissolved_oxygen: formData.dissolved_oxygen !== '' ? parseFloat(formData.dissolved_oxygen) : null,
        turbidity: formData.turbidity !== '' ? parseFloat(formData.turbidity) : null,
        conductivity: formData.conductivity !== '' ? parseFloat(formData.conductivity) : null, // Direct map
        temperature: formData.temperature !== '' ? parseFloat(formData.temperature) : null,
        battery_level: formData.battery_level !== '' ? parseFloat(formData.battery_level) : null,
      };
      
      await onSave(dataToSave);
      onClose();
    } catch (error) {
      console.error('Error saving reading:', error);
      alert('Error al guardar la lectura');
    } finally {
      setIsSaving(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 overflow-y-auto">
      <div className="flex items-center justify-center min-h-screen pt-4 px-4 pb-20 text-center sm:block sm:p-0">
        <div className="fixed inset-0 transition-opacity" aria-hidden="true">
          <div className="absolute inset-0 bg-gray-500 opacity-75" onClick={onClose}></div>
        </div>

        <span className="hidden sm:inline-block sm:align-middle sm:h-screen" aria-hidden="true">&#8203;</span>

        <div className="inline-block align-bottom bg-white rounded-lg text-left overflow-hidden shadow-xl transform transition-all sm:my-8 sm:align-middle sm:max-w-lg sm:w-full relative z-10">
          <div className="flex justify-between items-center px-6 py-4 border-b border-gray-200">
            <h3 className="text-lg font-medium text-gray-900">
              Editar Lectura
            </h3>
            <button
              onClick={onClose}
              className="text-gray-400 hover:text-gray-500 focus:outline-none"
            >
              <X className="h-6 w-6" />
            </button>
          </div>
          
          <form onSubmit={handleSubmit} className="p-6 space-y-4">
            <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700">pH</label>
                    <input
                    type="number"
                    step="0.01"
                    min="0"
                    max="14"
                    name="ph"
                    value={formData.ph}
                    onChange={handleChange}
                    className="mt-1 block w-full border-gray-300 rounded-md shadow-sm focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Temp (°C)</label>
                  <input
                    type="number"
                    step="0.01"
                    min="-50"
                    max="100"
                    name="temperature"
                    value={formData.temperature}
                    onChange={handleChange}
                    className="mt-1 block w-full border-gray-300 rounded-md shadow-sm focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Batería (%)</label>
                  <input
                    type="number"
                    step="0.01"
                    min="0"
                    max="100"
                    name="battery_level"
                    value={formData.battery_level}
                    onChange={handleChange}
                    className="mt-1 block w-full border-gray-300 rounded-md shadow-sm focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Turbidez</label>
                  <input
                    type="number"
                    step="0.01"
                    min="0"
                    name="turbidity"
                    value={formData.turbidity}
                    onChange={handleChange}
                    className="mt-1 block w-full border-gray-300 rounded-md shadow-sm focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Oxígeno Disuelto</label>
                  <input
                    type="number"
                    step="0.01"
                    min="0"
                    name="dissolved_oxygen"
                    value={formData.dissolved_oxygen}
                    onChange={handleChange}
                    className="mt-1 block w-full border-gray-300 rounded-md shadow-sm focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Conductividad</label>
                  <input
                    type="number"
                    step="0.01"
                    min="0"
                    name="conductivity" // Fixed input name
                    value={formData.conductivity} // Fixed value binding
                    onChange={handleChange}
                    className="mt-1 block w-full border-gray-300 rounded-md shadow-sm focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
                  />
                </div>
            </div>

            <div className="flex justify-end gap-3 mt-6 pt-2">
              <button
                type="button"
                onClick={onClose}
                className="px-4 py-2 border border-gray-300 rounded-md text-sm font-medium text-gray-700 hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500"
              >
                Cancelar
              </button>
              <button
                type="submit"
                disabled={isSaving}
                className="px-4 py-2 border border-transparent rounded-md shadow-sm text-sm font-medium text-white bg-blue-600 hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500 disabled:opacity-50"
              >
                {isSaving ? 'Guardando...' : 'Guardar'}
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
  );
}
