"use client";

import AdminNavbar from '@/components/AdminNavbar';
import { useSession } from 'next-auth/react';
import { useRouter } from 'next/navigation';
import { useEffect } from 'react';

export default function DashboardLayout({ children }: { children: React.ReactNode }) {
    const { data: session, status } = useSession();
    const router = useRouter();

    useEffect(() => {
        if (status === 'unauthenticated') {
            router.push('/admin/login');
        }
    }, [status, router]);

    if (status === 'loading') {
        return <div className="min-h-screen flex items-center justify-center">Cargando...</div>;
    }

    if (!session) {
        return null;
    }

    return (
        <div className="min-h-screen bg-gray-100">
            <AdminNavbar />
            <main className="pt-16 pb-10">
                <div className="max-w-7xl mx-auto sm:px-6 lg:px-8 mt-6">
                    {children}
                </div>
            </main>
        </div>
    );
}
