import { withAuth } from "next-auth/middleware";
import { NextResponse } from "next/server";

export default withAuth(
    function middleware(req) {
        const token = req.nextauth.token;
        const path = req.nextUrl.pathname;
        const method = req.method;

        console.log(`Middleware: ${method} ${path} - User Role: ${token?.role || 'Guest'}`);

        // 0. BYPASS TOTAL para peticiones de Hardware/Script (POST a endpoints de datos)
        if (
            (path.startsWith("/api/sensor-readings") || path.startsWith("/api/waste-detections")) && 
            method === "POST"
        ) {
            console.log("Middleware: Bypass detectado para Hardware POST");
            return NextResponse.next();
        }

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
