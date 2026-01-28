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

export const getTemperatureStatus = (value: number): TrafficLightResult => {
    if (value >= 10 && value <= 30) return { color: 'green', status: 'Bueno' };
    if (value > 30 && value <= 35) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

export const getPHStatus = (value: number): TrafficLightResult => {
    if (value >= 6.5 && value <= 8.5) return { color: 'green', status: 'Bueno' };
    if ((value >= 6.0 && value < 6.5) || (value > 8.5 && value <= 9.0)) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

export const getTurbidityStatus = (value: number): TrafficLightResult => {
    if (value <= 5) return { color: 'green', status: 'Bueno' };
    if (value > 5 && value <= 25) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

export const getDOStatus = (value: number): TrafficLightResult => {
    if (value >= 5) return { color: 'green', status: 'Bueno' };
    if (value >= 3 && value < 5) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

export const getConductivityStatus = (value: number): TrafficLightResult => {
    if (value < 750) return { color: 'green', status: 'Bueno' };
    if (value >= 750 && value <= 1500) return { color: 'yellow', status: 'Regular' };
    return { color: 'red', status: 'Malo' };
};

// --- Capa 2: Cálculo del ICA Global ---

// Función auxiliar para subíndices lineales
const calculateSubIndex = (value: number, min: number, max: number): number => {
    if (value <= min) return 100;
    if (value >= max) return 0;
    return 100 * (1 - (value - min) / (max - min));
};

export const calculateICA = (
    ph: number,
    do_mgL: number,
    turbidity: number,
    conductivity: number
): ICAResult => {
    // Calculo de subíndices (q_i)
    // Nota: Los rangos definidos en el prompt para subíndices fueron ejemplos simplificados.
    // Usaremos los valores del ejemplo para mantener consistencia con la solicitud, 
    // pero idealmente estos deberían ajustarse a normas oficiales como NSF o locales.

    // q_pH: Optimo 7. Distancia de 7. Rango de desviación aceptable +/- 2 aprox?
    // Usando ejemplo: q_pH = subindice_lineal(abs(pH - 7), 0, 2) --> Si dist es 0 (pH 7) -> 100. Si dist >= 2 (pH 5 o 9) -> 0.
    const q_pH = calculateSubIndex(Math.abs(ph - 7), 0, 2);

    // q_OD: Ejemplo: q_OD = subindice_lineal(OD, 2, 8) <-- Esto parece invertido logicamente en el ejemplo del prompt (si OD es bajo es malo).
    // Corrijamos la lógica para OD: Entre más alto mejor (hasta saturación).
    // Si OD >= 8 -> 100? Si OD <= 2 -> 0?
    // Usaremos lógica inversa para OD: valor ALTO es BUENO.
    // La función calculateSubIndex "baja" de 100 a 0 a medida que value va de min a max.
    // Para OD, queremos 0 si es <= 2, y 100 si es >= 8.
    // Reescribimos para OD especificamente o adaptamos.
    let q_OD = 0;
    if (do_mgL >= 8) q_OD = 100;
    else if (do_mgL <= 2) q_OD = 0;
    else q_OD = 100 * ((do_mgL - 2) / (8 - 2)); // Interpolación lineal positiva

    // q_Turbidez: Bajo es bueno.
    // Ejemplo: q_turbidez = subindice_lineal(turbidez, 5, 50)
    // Si <= 5 -> 100. Si >= 50 -> 0. Correcto.
    const q_Turb = calculateSubIndex(turbidity, 5, 50);

    // q_CE: Bajo es bueno.
    // Ejemplo: q_CE = subindice_lineal(CE, 250, 1500)
    // Si <= 250 -> 100. Si >= 1500 -> 0. Correcto.
    const q_CE = calculateSubIndex(conductivity, 250, 1500);

    // Pesos definidos
    const W_PH = 0.15;
    const W_OD = 0.17;
    const W_CE = 0.17;
    const W_TURB = 0.17;
    // Nota: Suma pesos = 0.66. El prompt dice "ICA = sum(wi * qi)". 
    // Si la suma de pesos no es 1, el ICA máximo no será 100.
    // Asumiremos que es un índice parcial o que faltan parámetros, pero seguiremos la fórmula ESTRICTA del usuario.
    // OJO: Si el usuario dio esos pesos especificos, los respetamos.

    // Re-leyendo prompt: "ICA = pesos.pH * q_pH + ...". 
    // Si todos q=100, ICA = 66. Esto podría ser confuso (max 66/100?).
    // Ajuste proactivo: Normalizar por la suma de pesos?
    // O tal vez hay otros params no listados?
    // "Parámetros de entrada: temperatura, pH, turbidez, oxigeno_disuelto, conductividad"
    // Temperatura no tiene peso en la fórmula del prompt.
    // Voy a normalizar el resultado final base 100 dividiendo por la suma de pesos (0.66) 
    // para que la escala 0-100 tenga sentido con la clasificación,
    // A MENOS que el usuario quiera estrictamente esa suma.
    // "El resultado se clasifica (Excelente >= 90...)". Si el max es 66, nunca será Excelente.
    // Por tanto, es IMPLÍCITO que se debe normalizar o ajustar pesos.
    // Ajustaré dividiendo por la suma de los pesos usados (0.66).

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
