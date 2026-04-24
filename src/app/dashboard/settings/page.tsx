"use client";

import React, { useState } from 'react';
import { useSession } from 'next-auth/react';
import { UserCircle, Users } from 'lucide-react';
import ProfileTab from '@/components/admin/settings/ProfileTab';
import UsersTab from '@/components/admin/settings/UsersTab';

export default function SettingsPage() {
    const { data: session } = useSession();
    const [activeTab, setActiveTab] = useState('profile');

    const isSuper = session?.user?.role === 'super';

    return (
        <div className="max-w-7xl mx-auto py-8 px-4 sm:px-6 lg:px-8">
            <div className="mb-8">
                <h1 className="text-3xl font-bold text-gray-900">Configuración</h1>
                <p className="mt-2 text-sm text-gray-500">Administra tu perfil {isSuper && 'y los usuarios del sistema'}.</p>
            </div>
            
            <div className="grid grid-cols-1 md:grid-cols-12 gap-8 items-start">
                {/* Sidebar Menu */}
                <div className="md:col-span-4 lg:col-span-3">
                    <div className="bg-white rounded-xl shadow-sm border border-gray-200 overflow-hidden sticky top-24">
                        <nav className="flex flex-col">
                            <button 
                                onClick={() => setActiveTab('profile')}
                                className={`flex items-center px-6 py-4 text-left transition-colors border-b border-gray-100 ${activeTab === 'profile' ? 'bg-blue-50/50 text-[#1e3570] font-semibold border-l-4 border-l-[#1e3570]' : 'text-gray-600 hover:bg-gray-50 border-l-4 border-transparent'}`}
                            >
                                <UserCircle className="w-5 h-5 mr-3" />
                                Mi Perfil
                            </button>
                            {isSuper && (
                                <button 
                                    onClick={() => setActiveTab('users')}
                                    className={`flex items-center px-6 py-4 text-left transition-colors border-b border-gray-100 ${activeTab === 'users' ? 'bg-blue-50/50 text-[#1e3570] font-semibold border-l-4 border-l-[#1e3570]' : 'text-gray-600 hover:bg-gray-50 border-l-4 border-transparent'}`}
                                >
                                    <Users className="w-5 h-5 mr-3" />
                                    Gestión de Usuarios
                                </button>
                            )}
                        </nav>
                    </div>
                </div>

                {/* Main Content Area */}
                <div className="md:col-span-8 lg:col-span-9 bg-white rounded-xl shadow-sm border border-gray-200 min-h-[500px]">
                    {activeTab === 'profile' && <ProfileTab />}
                    {activeTab === 'users' && isSuper && <UsersTab />}
                </div>
            </div>
        </div>
    );
}
