import { z } from 'zod';

/**
 * Esquema de validación para Nodos.
 * Se usa tanto para la creación como para la edición de nodos.
 * - node_id: Identificador único, alfanumérico, máx 20 caracteres.
 * - description: Descripción legible del nodo.
 * - latitude/longitude: Coordenadas geográficas validadas (-90..90, -180..180).
 */
export const nodeSchema = z.object({
    node_id: z.string().min(1, "Node ID is required").max(20, "Node ID too long"),
    description: z.string().min(1, "Description is required").max(100, "Description too long"),
    latitude: z.number().min(-90).max(90),
    longitude: z.number().min(-180).max(180),
    user_id: z.number().optional(), // Opcional: ID del usuario creador (si aplica)
});

/**
 * Esquema para la validación de lecturas de sensores (POST).
 * Valida los datos entrantes de sensores físicos o simulados.
 * Aplica restricciones físicas lógicas (ej. pH 0-14, Batería 0-100).
 */
export const sensorReadingSchema = z.object({
    node_id: z.string().min(1, "Node ID is required"),
    ph: z.number().min(0, "pH must be >= 0").max(14, "pH must be <= 14").optional().nullable(),
    dissolved_oxygen: z.number().min(0).optional().nullable(),
    turbidity: z.number().min(0).optional().nullable(),
    connectivity: z.number().min(0).optional().nullable(),
    temperature: z.number().min(-50).max(100).optional().nullable(),
    battery_level: z.number().min(0).max(100).optional().nullable(),
});

/**
 * Esquema para la actualización parcial de lecturas (PUT).
 * Permite modificar valores individuales de una lectura existente.
 * Mantiene las mismas restricciones de rango que sensorReadingSchema.
 */
export const readingUpdateSchema = z.object({
    ph: z.number().min(0).max(14).optional().nullable(),
    dissolved_oxygen: z.number().min(0).optional().nullable(),
    turbidity: z.number().min(0).optional().nullable(),
    connectivity: z.number().min(0).optional().nullable(),
    temperature: z.number().min(-50).max(100).optional().nullable(),
    battery_level: z.number().min(0).max(100).optional().nullable(),
});
