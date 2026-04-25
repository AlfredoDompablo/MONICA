# MONICA - Sistema de Monitoreo Ambiental e Inteligencia Artificial

> **Plataforma de vigilancia inteligente y visión artificial para la gestión de recursos hídricos.**

**MONICA** (Monitoreo e Inteligencia de Cuerpos de Agua) integra sensores IoT y **Visión Artificial (YOLOv11)** para ofrecer una supervisión continua del estado de los cuerpos de agua, facilitando la toma de decisiones informadas mediante un dashboard interactivo y análisis de segmentación de residuos.

---

## 🛠️ Stack Tecnológico

| Componente | Tecnología | Rol |
| :--- | :--- | :--- |
| **Frontend/API** | Next.js 15+ (React 19) | Interfaz de usuario y orquestación de datos. |
| **IA Service** | FastAPI (Python 3.11) | Inferencia YOLOv11-seg para detección de residuos. |
| **Storage** | MinIO (S3 Compatible) | Almacenamiento local de imágenes originales y máscaras. |
| **Database** | PostgreSQL 16 | Persistencia de lecturas de sensores y metadatos de IA. |
| **ORM** | Prisma 6 | Gestión de base de datos tipada y migraciones. |

---

## 🔧 Guía de Despliegue Completo (Debian/Linux)

Sigue estos pasos en orden para desplegar el sistema completo tal cual está configurado actualmente.

### 1. Preparación del Sistema Operativo
Instala las dependencias necesarias para Node.js, Python y el procesamiento de imágenes de OpenCV:

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y git curl python3-venv libgl1-mesa-glx libglib2.0-0 build-essential
```

### 2. Infraestructura con Docker (Base de Datos y Almacenamiento)
Asegúrate de tener Docker instalado. Ejecuta los servicios base:

```bash
# Levantar PostgreSQL y MinIO
docker compose up -d

# Verificar que estén corriendo
docker ps
```
*Acceso MinIO Console: `http://tu-ip:9001` (User: `admin`, Pass: `monica_secret_123`)*
*⚠️ IMPORTANTE: Crea manualmente un bucket llamado `detecciones` en el panel de MinIO.*

### 3. Configuración del Servicio de IA (Python)
Configura el microservicio que procesa las imágenes de los nodos:

```bash
cd ai_service

# Crear y activar entorno virtual
python3 -m venv venv
source venv/bin/activate

# Instalar dependencias de IA
pip install -r requirements.txt

# Colocar los pesos del modelo
# Asegúrate de que tu archivo 'best.pt' esté en: ai_service/model/best.pt

# Ejecutar servicio en segundo plano con PM2 (recomendado)
pm2 start "python main.py" --name monica-ai
```

### 4. Configuración del Dashboard (Next.js)
Regresa a la raíz del proyecto para configurar la interfaz y la API:

```bash
cd ..

# Instalar dependencias de Node
npm install --legacy-peer-deps

# Configurar variables de entorno
# Copia el ejemplo y edita según tus credenciales
cp .env.example .env 
nano .env
```

#### Inicializar Base de Datos:
```bash
# Sincronizar esquema de base de datos
npx prisma db push

# (Opcional) Cargar datos iniciales y usuario administrador
npx tsx src/prisma/seed.ts
```

#### Despliegue de Producción:
```bash
# Construir la aplicación
npm run build

# Ejecutar con PM2 para persistencia
pm2 start npm --name "monica-dashboard" -- start
```

---

## 📋 Variables de Entorno Requeridas (.env)

| Variable | Descripción | Ejemplo |
| :--- | :--- | :--- |
| `DATABASE_URL` | Conexión a PostgreSQL | `postgresql://admin:password@localhost:5432/rio_db` |
| `NEXTAUTH_SECRET` | Secreto para tokens de sesión | `openssl rand -base64 32` |
| `NEXTAUTH_URL` | URL base de la aplicación | `http://localhost:3000` |
| `S3_ENDPOINT` | Punto de acceso a MinIO | `http://localhost:9000` |
| `S3_ACCESS_KEY` | Usuario de MinIO | `admin` |
| `S3_SECRET_KEY` | Password de MinIO | `monica_secret_123` |
| `S3_BUCKET_NAME` | Nombre del bucket | `detecciones` |
| `AI_SERVICE_URL` | URL del servicio de Python | `http://localhost:8000` |

---

## 📡 Gestión de Procesos (PM2)
Comandos útiles para monitorear el sistema:

```bash
pm2 status          # Ver estado de los servicios
pm2 logs            # Ver logs en tiempo real
pm2 restart all     # Reiniciar todo el sistema
```

---

## 📂 Estructura de Rutas
*   **Página Principal (`/`)**: Landing page con mapa y galería integrada.
*   **Gestión de IA (`/dashboard/galeria`)**: Panel administrativo para depurar detecciones.
*   **API de Detecciones (`/api/waste-detections`)**: Punto de entrada para los nodos hardware.

---
© 2026 Proyecto MONICA. Implementación de Alta Disponibilidad.
