import { NextAuthOptions, DefaultSession } from "next-auth";
import CredentialsProvider from "next-auth/providers/credentials";
import { prisma } from "@/lib/prisma";
import bcrypt from "bcryptjs";

// Extender tipos incorporados para soportar roles e IDs personalizados
declare module "next-auth" {
    interface Session {
        user: {
            id: string;
            role: string;
        } & DefaultSession["user"];
    }
    interface User {
        id: string;
        role: string;
    }
}

declare module "next-auth/jwt" {
    interface JWT {
        id: string;
        role: string;
    }
}

/**
 * Configuración de Autenticación para NextAuth.js.
 * 
 * Define la estrategia de autenticación (JWT) y el proveedor de credenciales.
 * Utiliza Prisma para verificar credenciales contra la base de datos PostgreSQL
 * y bcrypt para comparar hashes de contraseñas de forma segura.
 * 
 * Los callbacks aseguran que el ID y Rol del usuario persistan en el token y la sesión.
 */
export const authOptions: NextAuthOptions = {
    session: {
        strategy: "jwt",
    },
    providers: [
        CredentialsProvider({
            name: "Credenciales", // Nombre mostrado en UI por defecto (aunque usamos form custom)
            credentials: {
                email: { label: "Email", type: "email" },
                password: { label: "Contraseña", type: "password" },
            },
            async authorize(credentials) {
                if (!credentials?.email || !credentials?.password) {
                    return null;
                }

                const user = await prisma.user.findUnique({
                    where: { email: credentials.email },
                });

                if (!user) {
                    return null;
                }

                // Verificar contraseña
                const isPasswordValid = await bcrypt.compare(
                    credentials.password,
                    user.password_hash
                );

                if (!isPasswordValid) {
                    return null;
                }

                // Retornar objeto de usuario (mapeado a la interfaz User extendida)
                return {
                    id: String(user.user_id),
                    name: user.full_name,
                    email: user.email,
                    role: user.role,
                };
            },
        }),
    ],
    callbacks: {
        async session({ session, token }) {
            if (token && session.user) {
                session.user.role = token.role;
                session.user.id = token.id;
            }
            return session;
        },
        async jwt({ token, user }) {
            if (user) {
                token.role = user.role;
                token.id = user.id;
            }
            return token;
        },
    },
};
