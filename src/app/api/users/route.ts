import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import bcrypt from 'bcryptjs';

/**
 * GET /api/users
 * 
 * Recupera la lista de todos los usuarios registrados en el sistema.
 * 
 * @returns {Promise<NextResponse>} Respuesta JSON con el array de usuarios (sin incluir contraseñas).
 */
export async function GET() {
  try {
    const users = await prisma.user.findMany({
      orderBy: { user_id: 'asc' },
      select: {
        user_id: true,
        full_name: true,
        email: true,
        role: true,
        is_active: true,
        // Excluir password_hash
      }
    });
    return NextResponse.json(users);
  } catch (error) {
    console.error('Error fetching users:', error);
    return NextResponse.json(
      { error: 'Error al obtener usuarios', details: String(error) },
      { status: 500 }
    );
  }
}

/**
 * POST /api/users
 * 
 * Crea un nuevo usuario en el sistema.
 * Valida los campos obligatorios y hashea la contraseña antes de guardarla.
 * 
 * @param {Request} request - La petición HTTP con los datos del usuario.
 * @returns {Promise<NextResponse>} Respuesta JSON con el usuario creado o un mensaje de error.
 */
export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { full_name, email, passwordRaw, role, is_active } = body;

    if (!full_name || !email || !passwordRaw || !role) {
      return NextResponse.json(
        { error: 'Faltan campos obligatorios' },
        { status: 400 }
      );
    }

    // Hash password
    const hashedPassword = await bcrypt.hash(passwordRaw, 10);

    const newUser = await prisma.user.create({
      data: {
        full_name,
        email,
        password_hash: hashedPassword,
        role,
        is_active: is_active ?? true,
      },
      select: {
        user_id: true,
        full_name: true,
        email: true,
        role: true,
        is_active: true,
      }
    });

    return NextResponse.json(newUser, { status: 201 });
  } catch (error) {
    console.error('Error creating user:', error);
    // Verificar si es error de unicidad (email duplicado)
    if (String(error).includes('Unique constraint')) {
      return NextResponse.json(
        { error: 'El email ya está registrado' },
        { status: 409 }
      );
    }
    return NextResponse.json(
      { error: 'Error al crear usuario' },
      { status: 500 }
    );
  }
}