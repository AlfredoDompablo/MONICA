"use client";

import dynamic from 'next/dynamic';

const Map = dynamic(() => import('@/components/Map'), { 
    ssr: false,
    loading: () => <div className="h-full w-full flex items-center justify-center bg-gray-100 rounded-xl animate-pulse">Cargando mapa interactivo...</div>
});

const MapSection = () => {
    return (
        <section id="mapa" className="w-full py-20 px-4 bg-gray-50">
            <div className="max-w-7xl mx-auto">
                <div className="text-center mb-12">
                    <h2 className="text-4xl font-bold text-[#1e3570] mb-4">Ubicación de Sensores</h2>
                    <p className="text-lg text-gray-600 max-w-2xl mx-auto">
                        Explora nuestra red de monitoreo en tiempo real a lo largo del Río Magdalena.
                    </p>
                </div>
                
                <div className="h-[600px] w-full rounded-2xl overflow-hidden shadow-xl border border-gray-200">
                    <Map />
                </div>
            </div>
        </section>
    );
};

export default MapSection;
