import { NextResponse } from 'next/server';
import { prisma } from '@/lib/prisma';
import { getServerSession } from 'next-auth';
import { authOptions } from '@/lib/auth';

export async function GET(request: Request) {
    // Public endpoint for charts
    // const session = await getServerSession(authOptions);
    // if (!session) { return NextResponse.json({ error: 'Unauthorized' }, { status: 401 }); }

    const { searchParams } = new URL(request.url);
    const nodeId = searchParams.get('node_id');
    const startDate = searchParams.get('start_date');
    const endDate = searchParams.get('end_date');
    const page = parseInt(searchParams.get('page') || '1');
    const limit = parseInt(searchParams.get('limit') || '50');
    const skip = (page - 1) * limit;

    // Build the where clause dynamically
    const where: any = {};

    if (nodeId) {
        where.node_id = nodeId;
    }

    if (startDate || endDate) {
        where.timestamp = {};
        if (startDate) {
            where.timestamp.gte = new Date(startDate);
        }
        if (endDate) {
            // Set end date to the end of the day if just a date string is provided, 
            // or use directly if it's a full ISO string. 
            // Assuming valid ISO strings or date strings YYYY-MM-DD
            const end = new Date(endDate);
            // If the time is 00:00:00, we generally want to include the whole day, so set to 23:59:59
            // However, usually filters send specific timestamps or we handle it here.
            // For simplicity, let's assume the frontend sends the correct end timestamp or we just use 'lte'
            // Use 'lte' (less than or equal)
            where.timestamp.lte = end;
        }
    }

    try {
        const [readings, total] = await Promise.all([
            prisma.sensorReading.findMany({
                where,
                orderBy: {
                    timestamp: 'desc',
                },
                take: limit,
                skip: skip,
                include: {
                    node: {    // Include node details just in case, or at least description
                        select: {
                            description: true
                        }
                    }
                }
            }),
            prisma.sensorReading.count({ where }),
        ]);

        return NextResponse.json({
            data: readings,
            pagination: {
                total,
                page,
                limit,
                totalPages: Math.ceil(total / limit),
            },
        });
    } catch (error) {
        console.error('Error fetching sensor readings:', error);
        return NextResponse.json(
            { error: 'Error fetching readings' },
            { status: 500 }
        );
    }
}
