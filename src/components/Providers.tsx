"use client";

import { SessionProvider } from "next-auth/react";

/**
 * Componente Providers
 * 
 * Envoltorio global para proveedores de contexto como `SessionProvider` de NextAuth.
 * Permite que el estado de la sesión y otros contextos globales sean accesibles en toda la aplicación.
 * 
 * @param {children} children - Componentes hijos de la aplicación.
 */
export function Providers({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}
