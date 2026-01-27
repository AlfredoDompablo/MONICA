"use client";

import { usePathname } from 'next/navigation';

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
