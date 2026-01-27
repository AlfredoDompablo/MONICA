"use client";

import Image from "next/image";

// ... constants ...
import { useRef, useState, useEffect } from "react";
import { motion, useMotionValue, useSpring, useMotionTemplate } from "framer-motion";
import { Eye, Activity, HeartHandshake } from "lucide-react";

const ROTATION_RANGE = 32.5;
const HALF_ROTATION_RANGE = 32.5 / 2;

/**
 * Componente Hero
 * 
 * Muestra la sección principal de la página de inicio con un título llamativo,
 * subtítulo y tarjetas interactivas 3D (Tilt Cards) que describen los pilares del proyecto.
 * Utiliza Framer Motion para las animaciones y efectos de mouse.
 */
const Hero = () => {
    return (
        <section id="inicio" className="relative w-full min-h-screen flex flex-col items-center justify-center overflow-hidden bg-gray-900 text-white py-20 px-4">
            
            {/* Background Image */}
            <div className="absolute inset-0 z-0">
                <Image
                    src="/rio magdalena.jpeg"
                    alt="Río Magdalena"
                    fill
                    className="object-cover"
                    priority
                />
                {/* Overlay Dark */}
                <div className="absolute inset-0 bg-black/50" />
            </div>

            {/* Background Elements - Subtle Gradients (with reduced opacity) */}
            <div className="absolute top-0 left-0 w-96 h-96 bg-blue-500/20 rounded-full blur-3xl -translate-x-1/2 -translate-y-1/2 z-0" />
            <div className="absolute bottom-0 right-0 w-96 h-96 bg-indigo-500/20 rounded-full blur-3xl translate-x-1/2 translate-y-1/2 z-0" />

            {/* Header Content */}
            <div className="relative z-10 text-center mb-16 max-w-4xl mx-auto">
                <motion.h1 
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.8, ease: "easeOut" }}
                    className="text-5xl md:text-7xl font-bold mb-6 tracking-tight text-white drop-shadow-lg"
                >
                    Protegiendo Nuestros Recursos Hídricos
                </motion.h1>
                <motion.p 
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.8, delay: 0.2, ease: "easeOut" }}
                    className="text-xl text-gray-200 md:text-2xl leading-relaxed max-w-2xl mx-auto drop-shadow-md"
                >
                    Monitoreo continuo de la calidad del agua en el Río Magdalena. Un proyecto para el futuro ambiental.
                </motion.p>
            </div>

            {/* Cards Container */}
            <div className="relative z-10 grid grid-cols-1 md:grid-cols-3 gap-8 w-full max-w-6xl mx-auto items-start">
                <TiltCard 
                    title="Visión" 
                    subtitle="¿Por Qué Monitoreamos?" 
                    description="Entender los cambios ambientales y la salud del ecosistema fluvial."
                    icon={Eye}
                    color="bg-emerald-400"
                />
                <TiltCard 
                    title="Tecnología" 
                    subtitle="¿Cómo lo Hacemos?" 
                    description="Recolección de datos automatizada y continua en cada ciclo semanal."
                    icon={Activity}
                    color="bg-blue-400"
                    className="md:mt-12" // Staggered layout
                />
                <TiltCard 
                    title="Impacto" 
                    subtitle="¿Cuál es el Beneficio?" 
                    description="Proporcionar información clave a gestores y la comunidad para la toma de decisiones."
                    icon={HeartHandshake}
                    color="bg-purple-400"
                />
            </div>
        </section>
    );
};

/**
 * Componente TiltCard
 * 
 * Tarjeta interactiva que rota en 3D siguiendo la posición del mouse.
 * 
 * @param {string} title - Título de la tarjeta.
 * @param {string} subtitle - Subtítulo (pregunta/contexto).
 * @param {string} description - Descripción breve.
 * @param {any} icon - Icono de Lucide-React.
 * @param {string} color - Clase de color de fondo para el icono (tailwind).
 * @param {string} className - Clases adicionales de estilo.
 */
const TiltCard = ({ title, subtitle, description, icon: Icon, color, className }: { 
    title: string; 
    subtitle: string; 
    description: string; 
    icon: any; 
    color: string;
    className?: string;
}) => {
    const ref = useRef<HTMLDivElement>(null);

    const x = useMotionValue(0);
    const y = useMotionValue(0);

    const xSpring = useSpring(x);
    const ySpring = useSpring(y);

    const transform = useMotionTemplate`rotateX(${xSpring}deg) rotateY(${ySpring}deg)`;

    const [isDesktop, setIsDesktop] = useState(false);

    useEffect(() => {
        const checkDesktop = () => {
            setIsDesktop(window.innerWidth >= 768);
        };
        
        checkDesktop();
        window.addEventListener("resize", checkDesktop);
        
        return () => window.removeEventListener("resize", checkDesktop);
    }, []);

    const handleMouseMove = (e: React.MouseEvent<HTMLDivElement, MouseEvent>) => {
        if (!isDesktop || !ref.current) return [0, 0];

        const rect = ref.current.getBoundingClientRect();
        // ... calculation
        const width = rect.width;
        const height = rect.height;

        const mouseX = (e.clientX - rect.left) * ROTATION_RANGE;
        const mouseY = (e.clientY - rect.top) * ROTATION_RANGE;

        const rX = (mouseY / height - HALF_ROTATION_RANGE) * -1;
        const rY = mouseX / width - HALF_ROTATION_RANGE;

        x.set(rX);
        y.set(rY);
    };

    const handleMouseLeave = () => {
        x.set(0);
        y.set(0);
    };

    return (
        <motion.div
            ref={ref}
            onMouseMove={handleMouseMove}
            onMouseLeave={handleMouseLeave}
            style={{ 
                transformStyle: "preserve-3d", 
                transform 
            }}
            className={`relative h-96 w-full rounded-xl bg-white/70 backdrop-blur-md border border-white/20 shadow-xl p-8 cursor-grab active:cursor-grabbing ${className}`}
            drag={isDesktop}
            dragConstraints={{ left: -50, right: 50, top: -50, bottom: 50 }}
            whileHover={isDesktop ? { scale: 1.02 } : {}}
            whileTap={isDesktop ? { scale: 0.98 } : {}}
        >
            <div 
                style={{ 
                    transform: "translateZ(75px)", 
                    transformStyle: "preserve-3d" 
                }}
                className="absolute inset-4 grid place-content-center rounded-xl bg-white shadow-lg"
            >
                <div 
                    className={`mx-auto mb-6 grid h-16 w-16 place-items-center rounded-full ${color} text-white shadow-md`}
                    style={{ transform: "translateZ(50px)" }}
                >
                    <Icon size={32} />
                </div>
                <h2 
                    style={{ transform: "translateZ(50px)" }}
                    className="text-center text-3xl font-bold mb-2 text-gray-800"
                >
                    {title}
                </h2>
                <h3 
                    style={{ transform: "translateZ(40px)" }}
                    className="text-center text-sm font-semibold text-gray-500 uppercase tracking-wider mb-4"
                >
                    {subtitle}
                </h3>
                <p 
                    style={{ transform: "translateZ(30px)" }}
                    className="text-center text-gray-600 px-4 leading-relaxed"
                >
                    {description}
                </p>
            </div>
        </motion.div>
    );
};

export default Hero;
