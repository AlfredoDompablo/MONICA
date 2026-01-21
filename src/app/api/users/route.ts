import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';

// GET /api/users - Obtener todos los usuarios
export async function GET() {
  try {
    const users = await prisma.user.findMany({
      orderBy: { user_id: 'asc' }
    });
    return NextResponse.json(users);
  } catch (error) {
    console.error('Error fetching users:', error);
    return NextResponse.json(
      { error: 'Error al obtener usuarios' },
      { status: 500 }
    );
  }
}

// POST /api/users - Crear un nuevo usuario
export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { full_name, email, password_hash, role, is_active } = body;

    // Validación básica
    if (!full_name || !email || !password_hash || !role) {
      return NextResponse.json(
        { error: 'Faltan campos obligatorios' },
        { status: 400 }
      );
    }

    const newUser = await prisma.user.create({
      data: {
        full_name,
        email,
        password_hash,
        role,
        is_active: is_active ?? true,
      },
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