# 🌊 MONICA: Sistema Avanzado de Monitoreo Ambiental e Inteligencia Artificial

> **Plataforma Integral de Vigilancia Hídrica impulsada por IA y IoT.**  
> MONICA (Monitoreo e Inteligencia de Cuerpos de Agua) es una solución state-of-the-art que combina sensores de telemetría en tiempo real con algoritmos de visión artificial de última generación para la preservación de ecosistemas acuáticos.

![Next.js](https://img.shields.io/badge/Next.js-000000?style=flat&logo=next.js&logoColor=white)
![React](https://img.shields.io/badge/React-20232A?style=flat&logo=react&logoColor=61DAFB)
![TypeScript](https://img.shields.io/badge/TypeScript-007ACC?style=flat&logo=typescript&logoColor=white)
![Python](https://img.shields.io/badge/Python-3776AB?style=flat&logo=python&logoColor=white)
![FastAPI](https://img.shields.io/badge/FastAPI-05998B?style=flat&logo=fastapi&logoColor=white)
![PostgreSQL](https://img.shields.io/badge/PostgreSQL-336791?style=flat&logo=postgresql&logoColor=white)
![Docker](https://img.shields.io/badge/Docker-2496ED?style=flat&logo=docker&logoColor=white)
![Prisma](https://img.shields.io/badge/Prisma-2D3748?style=flat&logo=prisma&logoColor=white)
![MinIO](https://img.shields.io/badge/MinIO-C72E49?style=flat&logo=minio&logoColor=white)
![TailwindCSS](https://img.shields.io/badge/Tailwind_CSS-38B2AC?style=flat&logo=tailwind-css&logoColor=white)

---

## 🚀 Arquitectura del Sistema

MONICA está diseñada bajo una arquitectura de microservicios coordinados para garantizar escalabilidad y alta disponibilidad:

*   **Core Dashboard (Next.js 15 + React 19):** Interfaz de usuario dinámica con visualización de datos, mapas interactivos (Leaflet) y gestión administrativa.
*   **AI Microservice (Python + FastAPI):** Procesamiento asíncrono de imágenes utilizando **YOLOv11-seg** para segmentación de residuos sólidos.
*   **Persistencia (PostgreSQL 16):** Base de datos relacional para lecturas telemétricas y metadatos de IA.
*   **Almacenamiento de Objetos (MinIO/S3):** Repositorio persistente para imágenes originales y máscaras de inferencia.

---

## 🛠️ Stack Tecnológico Avanzado

| Capa | Tecnologías |
| :--- | :--- |
| **Frontend** | React 19, Next.js 15 (App Router), Tailwind CSS, Framer Motion (Animaciones), Recharts. |
| **Backend / API** | Next.js API Routes, Next-Auth (Seguridad RBAC), Prisma ORM 6. |
| **Inteligencia Artificial** | Python 3.11, Ultralytics YOLOv11-seg, OpenCV, FastAPI. |
| **Infraestructura** | Docker & Docker Compose, MinIO S3, PostgreSQL. |
| **DevOps** | PM2 Process Manager, TypeScript. |

---

## 📦 Guía de Instalación y Despliegue (Paso a Paso)

### 1. Requisitos Previos
Prepare el entorno Linux (Debian/Ubuntu recomendado):
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y git curl python3-venv libgl1-mesa-glx libglib2.0-0 build-essential
```

### 2. Clonación y Estructura
```bash
git clone https://github.com/AlfredoDompablo/MONICA.git
cd MONICA
```

### 3. Orquestación de Infraestructura (Docker)
Inicie los contenedores de PostgreSQL y MinIO:
```bash
docker compose up -d
# Verifique que los servicios estén activos
docker ps
```
*   **MinIO:** Acceda a `http://localhost:9001` con las credenciales `admin` / `monica_secret_123`.
*   **IMPORTANTE:** Debe crear manualmente un Bucket llamado `detecciones` y establecer su política de acceso como `Public` o `Custom` para visualización externa.

### 4. Servicio de Inteligencia Artificial (Python)
Configure el entorno de inferencia YOLOv11:
```bash
cd ai_service
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
# Coloque su modelo entrenado en ai_service/model/best.pt
```
Ejecute el servicio:
```bash
pm2 start "python main.py" --name monica-ai
```

### 5. Configuración del Dashboard y Base de Datos
Regrese a la raíz e instale dependencias:
```bash
cd ..
npm install --legacy-peer-deps
```
Configure el archivo `.env` basándose en `.env.example`:
```env
DATABASE_URL="postgresql://admin:password123@localhost:5432/rio_db?schema=public"
NEXTAUTH_SECRET="tu_secreto_aqui"
S3_ENDPOINT="http://localhost:9000"
S3_ACCESS_KEY="admin"
S3_SECRET_KEY="monica_secret_123"
S3_BUCKET_NAME="detecciones"
AI_SERVICE_URL="http://localhost:8000"
```

#### 🔄 Inicialización de Datos (Crucial)
Siga este orden para asegurar la integridad de los datos:
1.  **Push del Esquema:** `npx prisma db push`
2.  **Seed Base (Admin):** `npx tsx src/prisma/seed.ts` (Crea el usuario `admin@monica.com` / `admin123`).
3.  **Datos Históricos (CSV):** `npx tsx src/prisma/seed_csv.ts` (Carga lecturas históricas de sensores desde archivos CSV).

### 6. Lanzamiento a Producción
```bash
npm run build
pm2 start npm --name "monica-dashboard" -- start
```

---

## 📡 Documentación de la API (Endpoints Clave)

### 🌊 Calidad del Agua
*   `GET /api/nodes`: Obtiene todos los nodos activos y su ubicación.
*   `GET /api/readings`: Historial de lecturas telemétricas (pH, Oxígeno, etc.).
*   `POST /api/sensor-readings`: Endpoint para hardware IoT (requiere `X-API-Key`).

### 🤖 Inteligencia Artificial y Residuos
*   `GET /api/waste-detections`: Lista las detecciones visuales procesadas.
*   `POST /api/waste-detections`: **Pipeline Automatizado.**
    *   Envía imagen Base64 -> Microservicio IA -> MinIO -> Base de Datos.
*   `GET /api/waste-detections/[id]/image`: Recupera imágenes directamente desde el storage S3.

---

## 🛠️ Mantenimiento y Logs
Utilice PM2 para monitorear el estado de salud del sistema completo:
```bash
pm2 status          # Resumen de servicios
pm2 logs            # Depuración en tiempo real
pm2 monit           # Interfaz de monitoreo visual
```

---

## ✨ Estética y UX
El proyecto implementa un diseño **Moderno Dark/Light Glassmorphism**:
*   **Animaciones:** Todas las secciones utilizan `Framer Motion` para transiciones suaves al hacer scroll.
*   **Visualización:** Gráficas de alta precisión con `Recharts` para análisis de tendencias.
*   **Interactividad:** Mapa dinámico sincronizado con el contexto global de selección de nodos.

---
© 2026 MONICA Project. Desarrollado para la gestión inteligente de recursos hídricos.
