"use client";

import { usePathname } from 'next/navigation';

/**
 * Componente MainLayoutWrapper
 * 
 * Envoltorio de diseño principal que ajusta dinámicamente el espaciado (padding) del contenido.
 * Añade un espaciado superior `pt-20` para compensar la altura del Navbar fijo en las páginas públicas,
 * mientras que lo elimina para el dashboard y la página de login que gestionan su propio diseño.
 * 
 * @param {children} children - El contenido de la página a renderizar.
 * @returns {JSX.Element} Elemento main con las clases de diseño apropiadas.
 */
export default function MainLayoutWrapper({ children }: { children: React.ReactNode }) {
  const pathname = usePathname();
  
  // Routes that manage their own layout/padding or don't need the default navbar spacing
  const isExcluded = pathname?.startsWith('/admin/login') || pathname?.startsWith('/dashboard');

  return (
    <main className={isExcluded ? '' : 'pt-20'}>
      {children}
    </main>
  );
}
