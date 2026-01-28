"use client";

import React, { useState, useEffect } from 'react';
import Link from 'next/link';
import Image from 'next/image';
import { usePathname } from 'next/navigation';

/**
 * Componente Navbar
 * 
 * Barra de navegación principal de la aplicación.
 * Incluye el logo de la marca, enlaces de navegación y un menú responsivo para dispositivos móviles.
 * Utiliza Tailwind CSS para los estilos y efectos visuales (glassmorphism).
 * 
 * @returns {JSX.Element} El elemento JSX del Navbar.
 */
export default function Navbar() {
  const [isOpen, setIsOpen] = useState(false);
  const pathname = usePathname();
  
  /* eslint-disable react-hooks/exhaustive-deps */
  const [activeSection, setActiveSection] = useState('inicio');

  useEffect(() => {
    const handleScroll = () => {
      const sections = ['inicio', 'mapa', 'estadisticas', 'galeria', 'nosotros'];
      const scrollPosition = window.scrollY + 100; // Compensación para la barra de navegación

      for (const section of sections) {
        const element = document.getElementById(section);
        if (element) {
          const offsetTop = element.offsetTop;
          const offsetBottom = offsetTop + element.offsetHeight;

          if (scrollPosition >= offsetTop && scrollPosition < offsetBottom) {
            setActiveSection(section);
          }
        }
      }
    };

    window.addEventListener('scroll', handleScroll);
    
    // Verificar posición inicial
    handleScroll();
    
    return () => window.removeEventListener('scroll', handleScroll);
  }, []);

  /**
   * Lista de enlaces de navegación.
   * Cada objeto contiene el nombre visible, la ruta (href) y el ID de sección asociado.
   */
  const links = [
    { name: 'Inicio', href: '/', sectionId: 'inicio' },
    { name: 'Mapa', href: '/#mapa', sectionId: 'mapa' },
    { name: 'Estadísticas', href: '/#estadisticas', sectionId: 'estadisticas' },
    { name: 'Galería', href: '/galeria', sectionId: 'galeria' },
    { name: 'Nosotros', href: '/nosotros', sectionId: 'nosotros' },
  ];

  /* No renderizar Navbar en página de login o dashboard */
  if (pathname?.startsWith('/admin/login') || pathname?.startsWith('/dashboard')) {
      return null;
  }

  const isLinkActive = (sectionId: string) => {
    return activeSection === sectionId;
  };

  return (
    <nav className="bg-[#f8f8f8] backdrop-blur-md shadow-sm fixed top-0 w-full z-50 transition-all duration-300">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex justify-between h-20">
          {/* Sección de Logo */}
          <div className="flex-shrink-0 flex items-center">
            <Link href="/" className="flex items-center gap-2" onClick={() => setActiveSection('inicio')}>
              <div className="relative h-20 w-48">
                 <Image
                  src="/LogoMonica.svg"
                  alt="Logo Monica"
                  fill
                  className="object-contain"
                  priority
                />
              </div>
            </Link>
          </div>

          {/* Desktop Menu */}
          <div className="hidden md:flex md:items-center md:space-x-8">
            {links.map((link) => {
              const isActive = isLinkActive(link.sectionId);
              return (
                <Link
                  key={link.name}
                  href={link.href}
                  onClick={() => setActiveSection(link.sectionId)}
                  className={`relative px-3 py-2 text-sm font-medium transition-colors duration-200 
                    ${isActive ? 'text-[#1e3570]' : 'text-gray-600 hover:text-[#1e3570]'}
                    group
                  `}
                >
                  {link.name}
                  <span className={`absolute bottom-0 left-0 w-full h-0.5 bg-[#1e3570] transform transition-transform duration-300 origin-left
                    ${isActive ? 'scale-x-100' : 'scale-x-0 group-hover:scale-x-100'}
                  `} />
                </Link>
              );
            })}
          </div>

          {/* Mobile Menu Button */}
          <div className="flex items-center md:hidden">
            <button
              onClick={() => setIsOpen(!isOpen)}
              className="inline-flex items-center justify-center p-2 rounded-md text-gray-600 hover:text-[#1e3570] focus:outline-none transition-colors"
              aria-expanded="false"
            >
              <span className="sr-only">Open main menu</span>
              {!isOpen ? (
                <svg className="block h-6 w-6" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M4 6h16M4 12h16M4 18h16" />
                </svg>
              ) : (
                <svg className="block h-6 w-6" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M6 18L18 6M6 6l12 12" />
                </svg>
              )}
            </button>
          </div>
        </div>
      </div>

      {/* Mobile Menu */}
      <div className={`md:hidden transition-all duration-300 ease-in-out ${isOpen ? 'max-h-64 opacity-100 shadow-md' : 'max-h-0 opacity-0 overflow-hidden'}`}>
        <div className="px-2 pt-2 pb-3 space-y-1 bg-[#f8f8f8] sm:px-3">
          {links.map((link) => {
             const isActive = isLinkActive(link.sectionId);
             return (
              <Link
                key={link.name}
                href={link.href}
                onClick={() => {
                   setIsOpen(false);
                   setActiveSection(link.sectionId);
                }}
                className={`block px-3 py-2 rounded-md text-base font-medium transition-colors
                  ${isActive ? 'text-[#1e3570] bg-[#1e3570]/10' : 'text-gray-600 hover:text-[#1e3570] hover:bg-gray-50'}
                `}
              >
                {link.name}
              </Link>
            )
          })}
        </div>
      </div>
    </nav>
  );
}
