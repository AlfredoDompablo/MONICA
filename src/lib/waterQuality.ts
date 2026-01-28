export type ParameterStatus = 'Bueno' | 'Regular' | 'Malo' | '...';
export type TrafficLightColor = 'green' | 'yellow' | 'red';

export interface TrafficLightResult {
    color: TrafficLightColor;
    status: ParameterStatus;
}

export interface ICAResult {
    value: number;
    classification: 'Excelente' | 'Buena' | 'Regular' | 'Mala' | 'Muy Mala';
    color: string; // Color hex for UI
}

// --- Capa 1: Semaforización Individual ---

/**
 * Evalúa el estado de la Temperatura.
 * Rango ideal: 10-30°C.
 */
export const getTemperatureStatus = (value: number): TrafficLightResult => {
    if (value >= 10 && value <= 30) return { color: 'green', status: 'Bueno' };
    if (value > 30 && value <= 35) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

/**
 * Evalúa el estado del pH.
 * Rango ideal: 6.5 - 8.5.
 */
export const getPHStatus = (value: number): TrafficLightResult => {
    if (value >= 6.5 && value <= 8.5) return { color: 'green', status: 'Bueno' };
    if ((value >= 6.0 && value < 6.5) || (value > 8.5 && value <= 9.0)) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

/**
 * Evalúa el estado de la Turbidez (UNT).
 * Ideal: <= 5 UNT.
 */
export const getTurbidityStatus = (value: number): TrafficLightResult => {
    if (value <= 5) return { color: 'green', status: 'Bueno' };
    if (value > 5 && value <= 25) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

/**
 * Evalúa el estado del Oxígeno Disuelto (mg/L).
 * Ideal: >= 5 mg/L.
 */
export const getDOStatus = (value: number): TrafficLightResult => {
    if (value >= 5) return { color: 'green', status: 'Bueno' };
    if (value >= 3 && value < 5) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

/**
 * Evalúa el estado de la Conductividad (µS/cm).
 * Ideal: < 750 µS/cm.
 */
export const getConductivityStatus = (value: number): TrafficLightResult => {
    if (value < 750) return { color: 'green', status: 'Bueno' };
    if (value >= 750 && value <= 1500) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

// --- Capa 2: Cálculo del ICA Global ---

/**
 * Función auxiliar para calcular subíndices lineales.
 * Retorna 100 si value <= min, 0 si value >= max, e interpola linealmente entre ellos.
 *
 * @param value Valor actual del parámetro.
 * @param min Límite inferior para obtener puntaje máximo (100).
 * @param max Límite superior que resulta en puntaje mínimo (0).
 */
const calculateSubIndex = (value: number, min: number, max: number): number => {
    if (value <= min) return 100;
    if (value >= max) return 0;
    return 100 * (1 - (value - min) / (max - min));
};

/**
 * Calcula el Índice de Calidad del Agua (ICA) basado en un sistema de subíndices ponderados.
 * Utiliza los parámetros: pH, Oxígeno Disuelto, Turbidez y Conductividad.
 * 
 * @param {number} ph - Valor de pH actual.
 * @param {number} do_mgL - Oxígeno Disuelto en mg/L.
 * @param {number} turbidity - Turbidez en UNT.
 * @param {number} conductivity - Conductividad en µS/cm.
 * @returns {ICAResult} Objeto con valor numérico (0-100), clasificación textual y color.
 */
export const calculateICA = (
    ph: number,
    do_mgL: number,
    turbidity: number,
    conductivity: number
): ICAResult => {
    // Calculo de subíndices (q_i) - Normalización lineal de 0 a 100

    // q_pH: Óptimo 7. Penaliza desviación.
    const q_pH = calculateSubIndex(Math.abs(ph - 7), 0, 2);

    // q_OD: Oxígeno Disuelto. Entre más alto mejor.
    // Lógica invertida: Si OD <= 2 -> 0. Si OD >= 8 -> 100.
    let q_OD = 0;
    if (do_mgL >= 8) q_OD = 100;
    else if (do_mgL <= 2) q_OD = 0;
    else q_OD = 100 * ((do_mgL - 2) / (8 - 2));

    // q_Turbidez: Entre más bajo mejor.
    const q_Turb = calculateSubIndex(turbidity, 5, 50);

    // q_CE: Conductividad. Entre más bajo mejor.
    const q_CE = calculateSubIndex(conductivity, 250, 1500);

    // Pesos definidos para cada parámetro
    const W_PH = 0.15;
    const W_OD = 0.17;
    const W_CE = 0.17;
    const W_TURB = 0.17;

    // Normalización por suma de pesos para asegurar escala 0-100
    const sumWeights = W_PH + W_OD + W_CE + W_TURB;

    let icaValue = (
        W_PH * q_pH +
        W_OD * q_OD +
        W_CE * q_CE +
        W_TURB * q_Turb
    ) / sumWeights;

    // Asegurar 0-100
    icaValue = Math.min(100, Math.max(0, icaValue));

    // Clasificación
    let classification: ICAResult['classification'];
    let color: string;

    if (icaValue >= 90) {
        classification = 'Excelente';
        color = '#22c55e'; // Green-500
    } else if (icaValue >= 70) {
        classification = 'Buena';
        color = '#84cc16'; // Lime-500
    } else if (icaValue >= 50) {
        classification = 'Regular';
        color = '#eab308'; // Yellow-500
    } else if (icaValue >= 25) {
        classification = 'Mala';
        color = '#f97316'; // Orange-500
    } else {
        classification = 'Muy Mala';
        color = '#ef4444'; // Red-500
    }

    return {
        value: Math.round(icaValue),
        classification,
        color
    };
};
