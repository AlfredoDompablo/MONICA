import { withAuth } from "next-auth/middleware";
import { NextResponse } from "next/server";

export default withAuth(
    function middleware(req) {
        const token = req.nextauth.token;
        const path = req.nextUrl.pathname;
        const method = req.method;

        console.log(`Middleware: ${method} ${path} - User Role: ${token?.role || 'Guest'}`);

        // Definir rutas que requieren privilegios de administrador para modificaciones (POST, PUT, DELETE).
        // Las peticiones GET a estas rutas (excepto usuarios) deben ser públicas.

        // Lógica de Autorización:
        // 1. Si el método es POST/PUT/DELETE -> Requiere rol de Administrador.
        // 2. Si el método es GET -> Acceso Público (implícito), EXCEPTO rutas sensibles.

        const isModification = method === "POST" || method === "PUT" || method === "DELETE";

        // Protección estricta para el endpoint de usuarios (/api/users)
        if (path.startsWith("/api/users")) {
            // Verificar si el usuario está autenticado
            if (!token) {
                return new NextResponse(
                    JSON.stringify({ error: "Unauthorized" }),
                    { status: 401, headers: { "content-type": "application/json" } }
                );
            }
            // Verificar si el usuario tiene rol de administrador para realizar modificaciones
            if (isModification && token.role !== "admin") {
                return new NextResponse(
                    JSON.stringify({ error: "Forbidden: Admin role required" }),
                    { status: 403, headers: { "content-type": "application/json" } }
                );
            }
        }

        // Rutas de datos públicos: Nodos, Lecturas de Sensores, Gráficas y Detecciones
        if (
            path.startsWith("/api/nodes") ||
            path.startsWith("/api/sensor-readings") ||
            path.startsWith("/api/readings") ||
            path.startsWith("/api/waste-detections")
        ) {
            // Permitir acceso GET para todos (público)

            // Bloquear modificaciones (escritura) si no es administrador
            if (isModification) {
                if (token?.role !== "admin") {
                    return new NextResponse(
                        JSON.stringify({ error: "Forbidden: Admin role required" }),
                        { status: 403, headers: { "content-type": "application/json" } }
                    );
                }
            }
        }
    },
    {
        callbacks: {
            // Devuelve true para permitir que la función middleware maneje la lógica de autorización.
            // Si devolvemos false, NextAuth redirigirá automáticamente al login.
            authorized: ({ token }) => true,
        },
    }
);

export const config = {
    matcher: [
        "/api/users/:path*",
        "/api/nodes/:path*",
        "/api/sensor-readings/:path*",
        "/api/readings/:path*",
        "/api/waste-detections/:path*"
    ],
};
