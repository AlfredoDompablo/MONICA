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
            // Permitir que CUALQUIER usuario autenticado modifique su propio perfil
            if (path === "/api/users/profile" && method === "PUT") {
                if (!token) {
                    return new NextResponse(
                        JSON.stringify({ error: "Unauthorized" }),
                        { status: 401, headers: { "content-type": "application/json" } }
                    );
                }
                return NextResponse.next(); // Sale del middleware y permite la petición
            }

            // Verificar si el usuario está autenticado
            if (!token) {
                return new NextResponse(
                    JSON.stringify({ error: "Unauthorized" }),
                    { status: 401, headers: { "content-type": "application/json" } }
                );
            }
            
            // Requerir rol 'super' para gestionar usuarios (GET, POST, PUT, DELETE)
            if (token.role !== "super") {
                return new NextResponse(
                    JSON.stringify({ error: "Forbidden: Super role required" }),
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

            // Caso especial: POST /api/sensor-readings (Hardware Auth)
            // Permitimos que pase el middleware sin verificar token de usuario admin,
            // ya que la autenticación real se hará con x-api-key en el route handler.
            if (path.startsWith("/api/sensor-readings") && method === "POST") {
                return NextResponse.next();
            }

            // Bloquear modificaciones (escritura) si no es super o admin
            if (isModification) {
                if (token?.role !== "admin" && token?.role !== "super") {
                    return new NextResponse(
                        JSON.stringify({ error: "Forbidden: Admin or Super role required" }),
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
