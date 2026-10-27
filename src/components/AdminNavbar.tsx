"use client";

import { useState } from 'react';
import { signOut, useSession } from 'next-auth/react';
import Link from 'next/link';
import Image from 'next/image';
import { usePathname } from 'next/navigation';
import { Menu, X, User } from 'lucide-react';

/**
 * Componente AdminNavbar
 * 
 * Barra de navegación superior exclusiva para usuarios autenticados con rol de administrador.
 * Proporciona acceso rápido a las secciones de gestión del dashboard con resaltado de pestaña activa.
 * 
 * @returns {JSX.Element} Elemento JSX que renderiza la barra de navegación administrativa.
 */
export default function AdminNavbar() {
    // Obtener sesión actual para mostrar nombre de usuario
    const { data: session } = useSession();
    const pathname = usePathname();
    const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

    const navItems = [
        { name: 'Dashboard', href: '/dashboard', exact: true },
        { name: 'Nodos', href: '/dashboard/nodes', exact: false },
        { name: 'Galería', href: '/dashboard/galeria', exact: false },
        { name: 'Red', href: '/dashboard/red', exact: false },
        { name: 'Datos', href: '/dashboard/data', exact: false },
        { name: 'Configuración', href: '/dashboard/settings', exact: false },
    ];

    const isLinkActive = (item: { href: string; exact: boolean }) => {
        if (!pathname) return false;
        if (item.exact) {
            return pathname === item.href;
        }
        return pathname === item.href || pathname.startsWith(`${item.href}/`);
    };

    return (
        <nav className="bg-white shadow border-b border-gray-200 fixed w-full z-50">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
                <div className="flex justify-between h-16">
                    <div className="flex items-center">
                        {/* Botón menú móvil */}
                        <div className="sm:hidden mr-2">
                            <button
                                onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
                                className="p-2 rounded-md text-gray-500 hover:text-gray-700 hover:bg-gray-100 transition-colors"
                                aria-label="Abrir menú"
                            >
                                {mobileMenuOpen ? <X className="w-6 h-6" /> : <Menu className="w-6 h-6" />}
                            </button>
                        </div>

                        {/* Logo y Cápsula con Nombre de Usuario (sin función de botón) */}
                        <div className="flex-shrink-0 inline-flex items-center gap-3 select-none">
                            <Image
                                src="/LogoMonica.svg"
                                alt="Logo Monica"
                                width={125}
                                height={30}
                                className="h-8 w-auto object-contain"
                                priority
                            />
                            <span className="inline-flex items-center gap-1.5 px-2.5 py-1 text-xs font-semibold bg-slate-100 text-slate-700 rounded-full border border-slate-200">
                                <User className="w-3.5 h-3.5 text-slate-500" />
                                <span>{session?.user?.name || 'Administrador'}</span>
                            </span>
                        </div>

                        {/* Enlaces de escritorio con mayor holgura (gap-8 a gap-12) */}
                        <nav className="hidden sm:flex sm:items-stretch sm:gap-8 md:gap-10 lg:gap-12 h-16 sm:ml-10 md:ml-14 lg:ml-16">
                            {navItems.map((item) => {
                                const active = isLinkActive(item);
                                return (
                                    <Link 
                                        key={item.href}
                                        href={item.href} 
                                        className={`inline-flex items-center px-2 border-b-2 text-sm whitespace-nowrap transition-all duration-150 ${
                                            active
                                                ? 'border-[#1e3570] text-[#1e3570] font-bold'
                                                : 'border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 font-medium'
                                        }`}
                                    >
                                        {item.name}
                                    </Link>
                                );
                            })}
                        </nav>
                    </div>

                    <div className="flex items-center">
                        <div className="flex-shrink-0 flex items-center">
                            <button
                                onClick={async () => {
                                    await signOut({ redirect: false });
                                    window.location.href = '/admin/login';
                                }}
                                className="relative inline-flex items-center gap-x-1.5 rounded-lg bg-red-50 py-1.5 px-3 text-xs sm:text-sm font-semibold text-red-600 shadow-xs hover:bg-red-100 transition-colors"
                            >
                                Cerrar Sesión
                            </button>
                        </div>
                    </div>
                </div>
            </div>

            {/* Menú desplegable móvil */}
            {mobileMenuOpen && (
                <div className="sm:hidden border-t border-gray-200 bg-white shadow-lg animate-fade-in">
                    <div className="pt-2 pb-3 space-y-1 px-4">
                        {navItems.map((item) => {
                            const active = isLinkActive(item);
                            return (
                                <Link
                                    key={item.href}
                                    href={item.href}
                                    onClick={() => setMobileMenuOpen(false)}
                                    className={`block px-3 py-2 rounded-lg text-base font-medium transition-colors ${
                                        active
                                            ? 'bg-blue-50 text-[#1e3570] font-bold border-l-4 border-[#1e3570]'
                                            : 'text-gray-600 hover:bg-gray-50 hover:text-gray-900'
                                    }`}
                                >
                                    {item.name}
                                </Link>
                            );
                        })}
                    </div>
                </div>
            )}
        </nav>
    );
}
