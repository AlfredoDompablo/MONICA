"use client";

import React, { useState, useEffect } from 'react';
import { useSession } from 'next-auth/react';
import { Users, Plus, Check, X, ShieldAlert, CheckCircle2 } from 'lucide-react';

interface User {
    user_id: number;
    full_name: string;
    email: string;
    role: string;
    is_active: boolean;
}

export default function UsersTab() {
    const { data: session } = useSession();
    const currentUserId = parseInt(session?.user?.id as string);

    const [users, setUsers] = useState<User[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState('');
    const [isAddingUser, setIsAddingUser] = useState(false);

    // Form state
    const [newUser, setNewUser] = useState({
        full_name: '',
        email: '',
        passwordRaw: '',
        role: 'admin'
    });

    const fetchUsers = async () => {
        try {
            setLoading(true);
            const res = await fetch('/api/users');
            if (!res.ok) throw new Error('Error al cargar usuarios');
            const data = await res.json();
            setUsers(data);
        } catch (err: any) {
            setError(err.message);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        fetchUsers();
    }, []);

    const handleAddUser = async (e: React.FormEvent) => {
        e.preventDefault();
        setError('');
        try {
            const res = await fetch('/api/users', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(newUser)
            });

            if (!res.ok) {
                const data = await res.json();
                throw new Error(data.error || 'Error al crear usuario');
            }

            setIsAddingUser(false);
            setNewUser({ full_name: '', email: '', passwordRaw: '', role: 'admin' });
            fetchUsers();
        } catch (err: any) {
            setError(err.message);
        }
    };

    const toggleUserStatus = async (userId: number, isActive: boolean) => {
        try {
            const res = await fetch(`/api/users/${userId}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ is_active: !isActive })
            });

            if (!res.ok) throw new Error('Error al cambiar estado');
            fetchUsers();
        } catch (err: any) {
            alert(err.message);
        }
    };

    const handleDeleteUser = async (userId: number) => {
        if (!confirm('¿Estás seguro de que deseas eliminar este usuario permanentemente?')) return;
        
        try {
            const res = await fetch(`/api/users/${userId}`, {
                method: 'DELETE',
            });

            const data = await res.json().catch(() => ({}));
            if (!res.ok) throw new Error(data.error || data.details || 'Error al eliminar usuario');
            
            fetchUsers();
        } catch (err: any) {
            alert(err.message);
        }
    };

    return (
        <div className="p-8">
            <div className="flex justify-between items-center mb-6 border-b border-gray-200 pb-4">
                <div>
                    <h2 className="text-2xl font-semibold text-gray-800">Gestión de Usuarios</h2>
                    <p className="text-sm text-gray-500 mt-1">Administra los accesos al panel de control.</p>
                </div>
                <button
                    onClick={() => setIsAddingUser(!isAddingUser)}
                    className="inline-flex items-center justify-center rounded-md border border-transparent bg-[#1e3570] px-4 py-2 text-sm font-medium text-white shadow-sm hover:bg-[#1e3570]/90 transition-colors"
                >
                    {isAddingUser ? <X className="w-4 h-4 mr-2" /> : <Plus className="w-4 h-4 mr-2" />}
                    {isAddingUser ? 'Cancelar' : 'Nuevo Usuario'}
                </button>
            </div>

            {error && (
                <div className="mb-4 p-4 rounded-md bg-red-50 text-red-700 text-sm">
                    {error}
                </div>
            )}

            {isAddingUser && (
                <form onSubmit={handleAddUser} className="mb-8 bg-gray-50 p-6 rounded-lg border border-gray-200">
                    <h3 className="text-lg font-medium text-gray-900 mb-4">Crear Nuevo Usuario</h3>
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <div>
                            <label className="block text-sm font-medium text-gray-700">Nombre Completo</label>
                            <input
                                type="text"
                                required
                                value={newUser.full_name}
                                onChange={(e) => setNewUser({...newUser, full_name: e.target.value})}
                                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                            />
                        </div>
                        <div>
                            <label className="block text-sm font-medium text-gray-700">Correo Electrónico</label>
                            <input
                                type="email"
                                required
                                value={newUser.email}
                                onChange={(e) => setNewUser({...newUser, email: e.target.value})}
                                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                            />
                        </div>
                        <div>
                            <label className="block text-sm font-medium text-gray-700">Contraseña Temporal</label>
                            <input
                                type="text"
                                required
                                value={newUser.passwordRaw}
                                onChange={(e) => setNewUser({...newUser, passwordRaw: e.target.value})}
                                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border text-gray-900 bg-white"
                            />
                        </div>
                        <div>
                            <label className="block text-sm font-medium text-gray-700">Rol</label>
                            <select
                                value={newUser.role}
                                onChange={(e) => setNewUser({...newUser, role: e.target.value})}
                                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500 sm:text-sm px-4 py-2 border bg-white text-gray-900"
                            >
                                <option value="super">Super (Acceso Total)</option>
                                <option value="admin">Administrador (Limitado)</option>
                            </select>
                        </div>
                    </div>
                    <div className="mt-4 flex justify-end">
                        <button type="submit" className="bg-[#1e3570] text-white px-4 py-2 rounded-md text-sm font-medium hover:bg-[#1e3570]/90">
                            Guardar Usuario
                        </button>
                    </div>
                </form>
            )}

            {loading ? (
                <div className="text-center py-10 text-gray-500">Cargando usuarios...</div>
            ) : (
                <div className="overflow-x-auto rounded-lg border border-gray-200">
                    <table className="min-w-full divide-y divide-gray-200">
                        <thead className="bg-gray-50">
                            <tr>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Usuario</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Rol</th>
                                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Estado</th>
                                <th scope="col" className="px-6 py-3 text-right text-xs font-medium text-gray-500 uppercase tracking-wider">Acciones</th>
                            </tr>
                        </thead>
                        <tbody className="bg-white divide-y divide-gray-200">
                            {users.map((user) => (
                                <tr key={user.user_id}>
                                    <td className="px-6 py-4 whitespace-nowrap">
                                        <div className="flex items-center">
                                            <div className="flex-shrink-0 h-10 w-10 bg-gray-100 rounded-full flex items-center justify-center">
                                                <Users className="h-5 w-5 text-gray-400" />
                                            </div>
                                            <div className="ml-4">
                                                <div className="text-sm font-medium text-gray-900">{user.full_name}</div>
                                                <div className="text-sm text-gray-500">{user.email}</div>
                                            </div>
                                        </div>
                                    </td>
                                    <td className="px-6 py-4 whitespace-nowrap">
                                        <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${user.role === 'super' ? 'bg-indigo-100 text-indigo-800' : user.role === 'admin' ? 'bg-purple-100 text-purple-800' : 'bg-gray-100 text-gray-800'}`}>
                                            {user.role}
                                        </span>
                                    </td>
                                    <td className="px-6 py-4 whitespace-nowrap">
                                        {user.is_active ? (
                                            <span className="flex items-center text-sm text-green-600 font-medium">
                                                <CheckCircle2 className="w-4 h-4 mr-1" /> Activo
                                            </span>
                                        ) : (
                                            <span className="flex items-center text-sm text-red-600 font-medium">
                                                <ShieldAlert className="w-4 h-4 mr-1" /> Inactivo
                                            </span>
                                        )}
                                    </td>
                                    <td className="px-6 py-4 whitespace-nowrap text-right text-sm font-medium space-x-3">
                                        {user.user_id === currentUserId ? (
                                            <span className="text-gray-400 italic">Tú (Sesión Actual)</span>
                                        ) : (
                                            <>
                                                <button
                                                    onClick={() => toggleUserStatus(user.user_id, user.is_active)}
                                                    className={`${user.is_active ? 'text-orange-600 hover:text-orange-900' : 'text-green-600 hover:text-green-900'}`}
                                                >
                                                    {user.is_active ? 'Desactivar' : 'Activar'}
                                                </button>
                                                {!user.is_active && (
                                                    <button
                                                        onClick={() => handleDeleteUser(user.user_id)}
                                                        className="text-red-600 hover:text-red-900"
                                                    >
                                                        Eliminar
                                                    </button>
                                                )}
                                            </>
                                        )}
                                    </td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            )}
        </div>
    );
}
