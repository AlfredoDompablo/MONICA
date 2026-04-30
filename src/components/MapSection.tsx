"use client";

import dynamic from 'next/dynamic';

const Map = dynamic(() => import('@/components/Map'), { 
    ssr: false,
    loading: () => <div className="h-full w-full flex items-center justify-center bg-gray-100 rounded-xl animate-pulse">Cargando mapa interactivo...</div>
});

import { motion } from 'framer-motion';

/**
 * Componente MapSection
 * 
 * Sección de la página de inicio que presenta el mapa interactivo de monitoreo.
 * Sirve como contenedor visual para el componente `Map` cargado dinámicamente.
 * 
 * @returns {JSX.Element} Sección del mapa.
 */
const MapSection = () => {
    return (
        <section id="mapa" className="w-full py-24 px-4 bg-gray-50 overflow-hidden">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
                
                {/* Header de Sección Estilo Premium */}
                <div className="flex flex-col md:flex-row md:items-end justify-between mb-12 gap-6">
                    <motion.div 
                        initial={{ opacity: 0, x: -20 }}
                        whileInView={{ opacity: 1, x: 0 }}
                        viewport={{ once: true }}
                        className="max-w-2xl"
                    >
                        <h2 className="text-sm font-bold text-[#1e3570] uppercase tracking-[0.3em] mb-3">Geolocalización</h2>
                        <h3 className="text-4xl md:text-5xl font-black text-gray-900 tracking-tight">
                            Ubicación de <span className="text-blue-600">Sensores</span>
                        </h3>
                        <p className="mt-4 text-gray-600 text-lg">
                            Explora nuestra red de monitoreo en tiempo real a lo largo del Río Magdalena para un seguimiento preciso.
                        </p>
                    </motion.div>
                </div>
                
                <div className="h-[600px] w-full rounded-2xl overflow-hidden shadow-xl border border-gray-200">
                    <Map />
                </div>
            </div>
        </section>
    );
};

export default MapSection;
