import React from 'react';
import Image from 'next/image';

export default function NosotrosPage() {
  const teamMembers = [
    {
      name: 'Alfredo Dompablo',
      role: 'Lead Developer & Architect',
      description: 'Especialista en sistemas distribuidos y arquitecturas en tiempo real con más de 10 años de experiencia.',
      image: 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?q=80&w=400&auto=format&fit=crop'
    },
    {
      name: 'Elena Rodríguez',
      role: 'Environmental Scientist',
      description: 'Investigadora experta en ecosistemas acuáticos y dinámica de fluidos en ríos altoandinos.',
      image: 'https://images.unsplash.com/photo-1494790108377-be9c29b29330?q=80&w=400&auto=format&fit=crop'
    },
    {
      name: 'Carlos Mendoza',
      role: 'IoT Hardware Engineer',
      description: 'Ingeniero electrónico apasionado por la creación de sensores robustos de ultra-bajo consumo.',
      image: 'https://images.unsplash.com/photo-1519085360753-af0119f7cbe7?q=80&w=400&auto=format&fit=crop'
    }
  ];

  return (
    <div className="min-h-screen bg-[#fafafa]">
      {/* Hero Section */}
      <section className="relative h-[60vh] flex items-center justify-center overflow-hidden">
        <div className="absolute inset-0 w-full h-full bg-gradient-to-br from-[#1e3570]/90 to-[#0d1b3e]/90 z-10" />
        <div className="absolute inset-0 w-full h-full">
          <Image
            src="https://images.unsplash.com/photo-1437482078695-73f5ca6c96e2?q=80&w=2070&auto=format&fit=crop"
            alt="Río fluyendo"
            fill
            className="object-cover"
            priority
          />
        </div>
        <div className="relative z-20 text-center px-4 max-w-4xl mx-auto mt-16">
          <h1 className="text-5xl md:text-7xl font-bold text-white mb-6 tracking-tight drop-shadow-lg">
            Nuestra Misión
          </h1>
          <p className="text-xl md:text-2xl text-blue-100 font-light leading-relaxed">
            Proteger y preservar nuestros recursos hídricos mediante inteligencia, monitoreo continuo y tecnología de vanguardia.
          </p>
        </div>
      </section>

      {/* Visión y Valores */}
      <section className="py-24 px-4 sm:px-6 lg:px-8 max-w-7xl mx-auto">
        <div className="grid grid-cols-1 md:grid-cols-2 gap-16 items-center">
          <div className="space-y-8">
            <div className="inline-block px-4 py-1 rounded-full bg-blue-100 text-[#1e3570] font-semibold text-sm tracking-wide">
              EL PROYECTO MONICA
            </div>
            <h2 className="text-4xl font-extrabold text-gray-900 leading-tight">
              Tecnología al servicio de la naturaleza
            </h2>
            <p className="text-lg text-gray-600 leading-relaxed">
              MONICA (Monitoreo e Inteligencia de Cuerpos de Agua) nace de la necesidad crítica de entender 
              la salud de nuestros ríos y lagos en tiempo real. Combinamos hardware IoT resistente, 
              conectividad satelital y análisis de datos avanzado para proporcionar una visión completa y predictiva.
            </p>
            <div className="flex gap-4 pt-4">
              <div className="w-12 h-1 bg-[#1e3570] rounded-full" />
              <div className="w-12 h-1 bg-blue-300 rounded-full" />
            </div>
          </div>
          <div className="relative h-[400px] rounded-3xl overflow-hidden shadow-2xl group">
            <Image
              src="https://images.unsplash.com/photo-1581091226825-a6a2a5aee158?q=80&w=1000&auto=format&fit=crop"
              alt="Desarrollo de sensores"
              fill
              className="object-cover transition-transform duration-700 group-hover:scale-110"
            />
            <div className="absolute inset-0 bg-gradient-to-t from-black/60 to-transparent" />
            <div className="absolute bottom-6 left-6 right-6">
              <h3 className="text-white font-semibold text-xl">Innovación Continua</h3>
              <p className="text-gray-200 text-sm mt-2">Diseñando sensores para entornos extremos</p>
            </div>
          </div>
        </div>
      </section>

      {/* Equipo */}
      <section className="py-24 bg-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="text-center max-w-3xl mx-auto mb-16">
            <h2 className="text-3xl md:text-4xl font-bold text-gray-900 mb-4">Conoce al Equipo</h2>
            <p className="text-lg text-gray-500">
              Un grupo multidisciplinario de ingenieros, científicos y soñadores comprometidos con el futuro del agua.
            </p>
          </div>
          
          <div className="grid grid-cols-1 md:grid-cols-3 gap-12">
            {teamMembers.map((member, idx) => (
              <div key={idx} className="group flex flex-col items-center">
                <div className="relative w-48 h-48 mb-6 rounded-full overflow-hidden shadow-lg transition-transform duration-500 group-hover:-translate-y-2 group-hover:shadow-xl ring-4 ring-transparent group-hover:ring-blue-100">
                  <Image
                    src={member.image}
                    alt={member.name}
                    fill
                    className="object-cover"
                  />
                </div>
                <h3 className="text-xl font-bold text-gray-900 group-hover:text-[#1e3570] transition-colors">{member.name}</h3>
                <span className="text-sm font-semibold text-blue-600 mb-3">{member.role}</span>
                <p className="text-center text-gray-500 text-sm leading-relaxed px-4">
                  {member.description}
                </p>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* CTA */}
      <section className="py-20 relative overflow-hidden">
        <div className="absolute inset-0 bg-[#1e3570]" />
        {/* Background Pattern */}
        <div className="absolute inset-0 opacity-10" style={{ backgroundImage: 'radial-gradient(circle at 2px 2px, white 1px, transparent 0)', backgroundSize: '24px 24px' }}></div>
        
        <div className="relative z-10 max-w-4xl mx-auto text-center px-4">
          <h2 className="text-3xl md:text-5xl font-bold text-white mb-6">¿Interesado en colaborar?</h2>
          <p className="text-xl text-blue-100 mb-10 font-light">
            Estamos abiertos a alianzas con instituciones gubernamentales, universidades y ONGs que compartan nuestra visión.
          </p>
          <a href="mailto:contacto@monica.project" className="inline-block bg-white text-[#1e3570] font-bold text-lg py-4 px-10 rounded-full shadow-lg hover:bg-blue-50 hover:shadow-xl transition-all duration-300 transform hover:-translate-y-1">
            Contáctanos
          </a>
        </div>
      </section>
    </div>
  );
}
