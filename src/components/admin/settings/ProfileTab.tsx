"use client";

import React, { useState, useEffect } from 'react';
import { useSession } from 'next-auth/react';
import { Save, Lock, User } from 'lucide-react';

export default function ProfileTab() {
    const { data: session, update } = useSession();
    const [name, setName] = useState('');
    const [currentPassword, setCurrentPassword] = useState('');
    const [newPassword, setNewPassword] = useState('');
    const [confirmPassword, setConfirmPassword] = useState('');
    
    const [loading, setLoading] = useState(false);
    const [message, setMessage] = useState({ text: '', type: '' });
    const [showPasswordFields, setShowPasswordFields] = useState(false);

    useEffect(() => {
        if (session?.user?.name) {
            setName(session.user.name);
        }
    }, [session]);

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setMessage({ text: '', type: '' });

        if (newPassword && newPassword !== confirmPassword) {
            setMessage({ text: 'Las nuevas contraseñas no coinciden', type: 'error' });
            return;
        }

        setLoading(true);
        try {
            const response = await fetch('/api/users/profile', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    full_name: name,
                    current_password: currentPassword,
                    new_password: newPassword
                })
            });

            const data = await response.json();

            if (!response.ok) {
                throw new Error(data.error || 'Error al actualizar');
            }

            setMessage({ text: 'Perfil actualizado exitosamente', type: 'success' });
            
            // Actualizar la sesión en NextAuth
            if (name !== session?.user?.name) {
                await update({ name });
            }

            // Limpiar contraseñas
            setCurrentPassword('');
            setNewPassword('');
            setConfirmPassword('');
        } catch (error: any) {
            setMessage({ text: error.message, type: 'error' });
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="p-8">
            <div className="mb-6 border-b border-gray-200 pb-4">
                <h2 className="text-2xl font-semibold text-gray-800">Mi Perfil</h2>
                <p className="text-sm text-gray-500 mt-1">Actualiza tu información personal y contraseña.</p>
            </div>

            <form onSubmit={handleSubmit} className="max-w-xl space-y-6">
                {message.text && (
                    <div className={`p-4 rounded-md text-sm ${message.type === 'error' ? 'bg-red-50 text-red-700' : 'bg-green-50 text-green-700'}`}>
                        {message.text}
                    </div>
                )}

                <div className="space-y-4">
                    <h3 className="text-lg font-medium text-gray-900 flex items-center gap-2">
                        <User className="w-5 h-5 text-gray-400" /> Datos Personales
                    </h3>
                    
                    <div>
                        <label className="block text-sm font-medium text-gray-700">Nombre Completo</label>
                        <input 
                            type="text" 
                            value={name}
                            onChange={(e) => setName(e.target.value)}
                            className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                            required
                        />
                    </div>
                    
                    <div>
                        <label className="block text-sm font-medium text-gray-700">Correo Electrónico (No editable)</label>
                        <input 
                            type="email" 
                            value={session?.user?.email || ''}
                            disabled
                            className="mt-1 block w-full rounded-md border-gray-200 bg-gray-50 text-gray-500 shadow-sm sm:text-sm px-4 py-2 border cursor-not-allowed"
                        />
                    </div>
                </div>

                <div className="space-y-4 pt-6 border-t border-gray-200">
                    <h3 className="text-lg font-medium text-gray-900 flex items-center gap-2">
                        <Lock className="w-5 h-5 text-gray-400" /> Cambiar Contraseña
                    </h3>
                    
                    {!showPasswordFields ? (
                        <button
                            type="button"
                            onClick={() => setShowPasswordFields(true)}
                            className="inline-flex items-center px-4 py-2 border border-gray-300 shadow-sm text-sm font-medium rounded-md text-gray-700 bg-white hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-[#1e3570] transition-colors"
                        >
                            {/*<Lock className="w-4 h-4 mr-2" />*/}
                            Modificar Contraseña
                        </button>
                    ) : (
                        <div className="space-y-4 animate-in fade-in slide-in-from-top-4 duration-300">
                            {/*<p className="text-sm text-gray-500">Deja estos campos en blanco si decides no cambiarla.</p>*/}
                            
                            <div>
                                <label className="block text-sm font-medium text-gray-700">Contraseña Actual</label>
                                <input 
                                    type="password" 
                                    value={currentPassword}
                                    onChange={(e) => setCurrentPassword(e.target.value)}
                                    className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                                />
                            </div>

                            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                                <div>
                                    <label className="block text-sm font-medium text-gray-700">Nueva Contraseña</label>
                                    <input 
                                        type="password" 
                                        value={newPassword}
                                        onChange={(e) => setNewPassword(e.target.value)}
                                        className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                                    />
                                </div>
                                <div>
                                    <label className="block text-sm font-medium text-gray-700">Confirmar Nueva Contraseña</label>
                                    <input 
                                        type="password" 
                                        value={confirmPassword}
                                        onChange={(e) => setConfirmPassword(e.target.value)}
                                        className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                                    />
                                </div>
                            </div>

                            <div>
                                {/* <button
                                    type="button"
                                    onClick={() => {
                                        setShowPasswordFields(false);
                                        setCurrentPassword('');
                                        setNewPassword('');
                                        setConfirmPassword('');
                                    }}
                                    className="text-sm text-red-600 hover:text-red-800 font-medium transition-colors"
                                >
                                    Cancelar cambio de contraseña
                                </button>*/}
                            </div>
                        </div>
                    )}
                </div>

                <div className="pt-4 flex justify-end">
                    <button
                        type="submit"
                        disabled={loading}
                        className="inline-flex items-center justify-center rounded-md border border-transparent bg-[#1e3570] px-4 py-2 text-sm font-medium text-white shadow-sm hover:bg-[#1e3570]/90 focus:outline-none focus:ring-2 focus:ring-[#1e3570] focus:ring-offset-2 disabled:opacity-50 transition-colors"
                    >
                        <Save className="w-4 h-4 mr-2" />
                        {loading ? 'Guardando...' : 'Guardar Cambios'}
                    </button>
                </div>
            </form>
        </div>
    );
}
