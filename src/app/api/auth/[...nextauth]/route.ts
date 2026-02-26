import NextAuth from "next-auth";
import { authOptions } from "@/lib/auth";

/**
 * Manejador de Autenticación (NextAuth)
 * 
 * Gestiona los procesos de inicio y cierre de sesión.
 * Configuración definida en @/lib/auth.
 * Soporta credenciales (admin) y gestión de sesiones JWT.
 */
const handler = NextAuth(authOptions);

export { handler as GET, handler as POST };
