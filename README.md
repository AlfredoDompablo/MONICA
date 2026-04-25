# MONICA - Sistema de Monitoreo Ambiental e Inteligencia Artificial

> **Plataforma de vigilancia inteligente y visión artificial para la gestión de recursos hídricos.**

**MONICA** (Monitoreo e Inteligencia de Cuerpos de Agua) es una solución avanzada que integra sensores IoT y **Visión Artificial (YOLOv11)** para la supervisión continua del Río Magdalena.

---

## 🚀 Nuevas Características (v2.0)

### 🤖 Inteligencia Artificial (YOLOv11)
*   **Segmentación de Residuos**: Identificación precisa de plásticos y contaminantes flotantes usando YOLOv11-seg.
*   **Cálculo de Cobertura**: Estimación automática del porcentaje de área afectada en cada toma.
*   **Microservicio de Inferencia**: Servicio independiente en Python (FastAPI) para procesamiento de imágenes de alta velocidad.

### 📦 Almacenamiento Profesional (S3/MinIO)
*   **Gestión de Objetos**: Migración del almacenamiento binario de base de datos a un servidor **MinIO local**.
*   **Optimización de DB**: Base de datos ligera que solo almacena metadatos y rutas (keys) de S3.

### 🖼️ Galería Premium SPA
*   **Experiencia Single Page**: Galería integrada dinámicamente en la página principal con desplazamiento suave.
*   **Slider Comparativo**: Visualización interactiva "Before/After" (Original vs Máscara de IA).
*   **Gestión Administrativa**: Panel de control privado para depurar y gestionar el historial de vigilancia.

---

## 🛠️ Arquitectura del Sistema

| Componente | Tecnología | Rol |
| :--- | :--- | :--- |
| **Frontend/API** | Next.js 15+ | Interfaz de usuario y orquestación de datos. |
| **IA Service** | Python + YOLOv11 | Inferencia de visión artificial y segmentación. |
| **Storage** | MinIO (S3 Compatible) | Almacenamiento de imágenes originales y procesadas. |
| **Database** | PostgreSQL + Prisma | Persistencia de lecturas y metadatos de IA. |

---

## 🔧 Guía de Despliegue Completo

### 1. Infraestructura Base (MinIO)
Asegúrate de tener MinIO corriendo en tu servidor Debian:
```bash
docker run -d \
  -p 9000:9000 -p 9001:9001 \
  --name minio \
  -v ~/minio_data:/data \
  -e "MINIO_ROOT_USER=admin" \
  -e "MINIO_ROOT_PASSWORD=monica_secret_123" \
  minio/minio server /data --console-address ":9001"
```
*Crea un bucket llamado `detecciones` en la consola de MinIO (puerto 9001).*

### 2. Servicio de Inteligencia Artificial (Python)
Dentro de la carpeta `ai_service`:
```bash
# Instalación de dependencias de sistema
sudo apt update && sudo apt install -y python3-venv libgl1-mesa-glx libglib2.0-0

# Configurar entorno
python3 -m venv venv
./venv/bin/pip install -r requirements.txt

# Ejecutar servicio (Puerto 8000)
./venv/bin/python main.py
```

### 3. Aplicación Principal (Next.js)
```bash
# Instalar dependencias
npm install --legacy-peer-deps

# Sincronizar Base de Datos
npx prisma db push

# Ejecutar en modo producción
npm run build
pm2 start npm --name "monica-app" -- start
```

---

## 📋 Variables de Entorno (.env)
Asegúrate de tener estas claves configuradas:
```env
# Database
DATABASE_URL="postgresql://..."

# S3 / MinIO
S3_ENDPOINT="http://localhost:9000"
S3_ACCESS_KEY="admin"
S3_SECRET_KEY="monica_secret_123"
S3_BUCKET_NAME="detecciones"
S3_REGION="us-east-1"

# AI Service
AI_SERVICE_URL="http://localhost:8000"
```

---

## 📡 Endpoints de IA
*   `POST /process`: Recibe imagen del nodo y devuelve metadatos de segmentación.

---
© 2026 Proyecto MONICA. Implementación Profesional con Visión Artificial.
