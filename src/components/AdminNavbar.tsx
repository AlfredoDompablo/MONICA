"use client";

import { signOut, useSession } from 'next-auth/react';
import Link from 'next/link';
import Image from 'next/image';

export default function AdminNavbar() {
    const { data: session } = useSession();

    return (
        <nav className="bg-white shadow border-b border-gray-200 fixed w-full z-50">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
                <div className="flex justify-between h-16">
                    <div className="flex">
                        <div className="flex-shrink-0 flex items-center">
                             <div className="relative h-10 w-24">
                                <Image
                                    src="/LogoMonica.svg"
                                    alt="Logo Monica"
                                    fill
                                    className="object-contain"
                                    priority
                                />
                            </div>
                            <span className="ml-2 text-sm font-semibold text-gray-500 border-l pl-2 border-gray-300">Admin Panel</span>
                        </div>
                        <div className="hidden sm:ml-6 sm:flex sm:space-x-8">
                            <Link href="/dashboard" className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                                Dashboard
                            </Link>
                            <Link href="/dashboard/nodes" className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                                Nodos
                            </Link>
                             <Link href="/dashboard/data" className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                                Datos
                            </Link>
                             <Link href="/dashboard/settings" className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium">
                                Configuración
                            </Link>
                        </div>
                    </div>
                    <div className="flex items-center">
                        <div className="flex-shrink-0">
                            <span className="mr-4 text-sm text-gray-600">{session?.user?.name || 'Administrador'}</span>
                            <button
                                onClick={() => signOut({ callbackUrl: '/' })}
                                className="relative inline-flex items-center gap-x-1.5 rounded-md bg-red-50 py-2 px-3 text-sm font-semibold text-red-600 shadow-sm hover:bg-red-100"
                            >
                                Cerrar Sesión
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        </nav>
    );
}
