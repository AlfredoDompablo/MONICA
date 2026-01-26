"use client";

import React, { useState } from 'react';
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

  /**
   * Lista de enlaces de navegación.
   * Cada objeto contiene el nombre visible y la ruta (href).
   */
  const links = [
    { name: 'Inicio', href: '/' },
    { name: 'Mapa', href: '/mapa' },
    { name: 'Estadísticas', href: '/estadisticas' },
    { name: 'Galería', href: '/galeria' },
    { name: 'Nosotros', href: '/nosotros' },
  ];

  return (
    <nav className="bg-[#f8f8f8] backdrop-blur-md shadow-sm fixed w-full z-50 transition-all duration-300">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex justify-between h-16">
          {/* Logo Section */}
          <div className="flex-shrink-0 flex items-center">
            <Link href="/" className="flex items-center gap-2">
              <div className="relative h-16 w-40">
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
              const isActive = pathname === link.href;
              return (
                <Link
                  key={link.name}
                  href={link.href}
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
             const isActive = pathname === link.href;
             return (
              <Link
                key={link.name}
                href={link.href}
                onClick={() => setIsOpen(false)}
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
