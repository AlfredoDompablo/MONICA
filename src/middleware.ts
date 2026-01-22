import { withAuth } from "next-auth/middleware";
import { NextResponse } from "next/server";

export default withAuth(
    function middleware(req) {
        const token = req.nextauth.token;
        const path = req.nextUrl.pathname;
        const method = req.method;

        console.log(`Middleware: ${method} ${path} - User Role: ${token?.role || 'Guest'}`);

        // Define routes that require admin for modification (POST, PUT, DELETE)
        // GET requests to these (except users) should be public.
        // Users route usually stays protected or at least admin-only for listing, 
        // but explicit request was about nodes, sensor-readings, waste-detections.

        // Logic:
        // 1. If method is POST/PUT/DELETE -> Require Admin
        // 2. If method is GET -> Public (implicit fallthrough), EXCEPT for sensitive routes if any.

        // Note: We need to handle /api/users specifically. 
        // If users listing should be public? Unlikely. Assuming /api/users listing requires admin too for safety,
        // or authenticated. But based on prompt "consultas de nodos, sensores, waste sin autenticarse", 
        // it implies these SPECIFICALLY are public. 
        // We will Protect /api/users entirely for now, allowing admin access.

        const isModification = method === "POST" || method === "PUT" || method === "DELETE";

        // Protect /api/users fully (or at least modifications + listing if desired, but let's stick to modification rules first)
        // Actually, let's keep it safe: /api/users requires auth always in previous logic. 
        // To allow simple logic:

        if (path.startsWith("/api/users")) {
            // For users, maybe we still want to ensure at least some auth?
            // But the user prompt emphasized public GET for nodes/sensors/waste.
            // If I follow the previous `authorized: ({ token }) => !!token`, everything needed auth.
            // Now I am relaxing `authorized`.

            // If I want to keep /api/users protected (as it was implicitly by the callback), 
            // I must check here.
            if (!token) {
                return new NextResponse(
                    JSON.stringify({ error: "Unauthorized" }),
                    { status: 401, headers: { "content-type": "application/json" } }
                );
            }
            // Be admin to modify users
            if (isModification && token.role !== "admin") {
                return new NextResponse(
                    JSON.stringify({ error: "Forbidden: Admin role required" }),
                    { status: 403, headers: { "content-type": "application/json" } }
                );
            }
        }

        // For public-read data routes: Nodes, Sensor Readings, Waste Detections
        if (
            path.startsWith("/api/nodes") ||
            path.startsWith("/api/sensor-readings") ||
            path.startsWith("/api/waste-detections")
        ) {
            // Allow GET for everyone (even no token)
            // Block modifications if not admin
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
            // Return true to let the middleware function handle the logic.
            // If we return false, it redirects to login immediately.
            authorized: ({ token }) => true,
        },
    }
);

export const config = {
    matcher: [
        "/api/users/:path*",
        "/api/nodes/:path*",
        "/api/sensor-readings/:path*",
        "/api/waste-detections/:path*"
    ],
};
