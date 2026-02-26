# MONICA - Sistema de Monitoreo Ambiental

> **Plataforma de vigilancia inteligente para la gestión de recursos hídricos en tiempo real.**

**MONICA** (Monitoreo e Inteligencia de Cuerpos de Agua) es una solución tecnológica avanzada diseñada para la supervisión continua, análisis y gestión de variables ambientales críticas en ríos y lagos. Integra sensores IoT distribuidos y visión artificial para ofrecer una visión holística del estado de los cuerpos de agua, facilitando la toma de decisiones informadas.

---

## 🚀 Características Principales

### 📊 Visualización y Análisis
*   **Dashboard Interactivo**: Panel de control intuitivo con mapas de localización en tiempo real y estadísticas dinámicas.
*   **Gráficas Históricas Avanzadas**: Análisis temporal detallado de variables como conductividad, pH, oxígeno disuelto y turbidez.
*   **Detección de Anomalías**: Identificación visual de zonas de riesgo basada en parámetros configurables de calidad del agua.

### 📡 Gestión IoT y Red
*   **Gestión de Nodos**: Administración centralizada de estaciones de monitoreo distribuidas.
*   **Sincronización en Tiempo Real**: Ingesta continua de datos provenientes de múltiples sensores en campo.

### 🛡️ Seguridad y Administración
*   **Control de Acceso (RBAC)**: Sistema de roles y permisos (Administrador/Invitado) asegurado con `next-auth`. (Acceso a lecturas públicas, gestión protegida).
*   **API RESTful Segura**: Endpoints optimizados para la integración segura con dispositivos hardware y aplicaciones de terceros.

---

## 🛠️ Stack Tecnológico

La plataforma está construida sobre una arquitectura moderna, escalable y mantenible:

| Categoría | Tecnología | Descripción |
| :--- | :--- | :--- |
| **Frontend** | ![Next.js](https://img.shields.io/badge/Next.js-15+-black?style=flat-square&logo=next.js) ![React](https://img.shields.io/badge/React-19-blue?style=flat-square&logo=react) | Framework de alto rendimiento y servidor de renderizado. |
| **Backend** | ![Node.js](https://img.shields.io/badge/Node.js-LTS-green?style=flat-square&logo=node.js) | Lógica de servidor y API Routes. |
| **Base de Datos** | ![PostgreSQL](https://img.shields.io/badge/PostgreSQL-16-336791?style=flat-square&logo=postgresql) | Motor SQL relacional para integridad de datos. |
| **ORM** | ![Prisma](https://img.shields.io/badge/Prisma-6-darkblue?style=flat-square&logo=prisma) | Capa de acceso a datos segura y tipada. |
| **Infraestructura** | ![Docker](https://img.shields.io/badge/Docker-Enabled-2496ED?style=flat-square&logo=docker) | Contenedorización completa para despliegue. |

---

## 📋 Requisitos Previos

Para desplegar el entorno de desarrollo, asegúrate de contar con:

*   **Node.js** (v18 o superior)
*   **Docker** y **Docker Compose** (opcional, para BD local)
*   **Git**

---

## 🔧 Instalación y Configuración

1.  **Clonar el repositorio:**

    ```bash
    git clone https://github.com/AlfredoDompablo/MONICA.git
    cd MONICA
    ```

2.  **Instalar dependencias:**

    ```bash
    npm install
    # Si encuentras errores de dependencias (Ej: conflictos con @scalar/api-reference o Next.js 15), usa:
    npm install --legacy-peer-deps
    ```

3.  **Configurar entorno:**
    Crea un archivo `.env` en la raíz basándote en el ejemplo. Asegúrate de definir `DATABASE_URL` y generar un secreto seguro para `NEXTAUTH_SECRET`:

    ```env
    DATABASE_URL="postgresql://usuario:password@localhost:5432/nombre_db?schema=public"
    NEXTAUTH_SECRET="tu_secreto_super_seguro"
    NEXTAUTH_URL="http://localhost:3000"
    ```
    
    > **Nota:** Puedes generar un valor seguro para `NEXTAUTH_SECRET` ejecutando el siguiente comando en tu terminal:  
    > `openssl rand -base64 32`

4.  **Inicializar Base de Datos:**

    ```bash
    # Si usas Docker para la DB
    docker compose up -d

    # Aplicar migraciones y generar cliente
    npx prisma migrate dev
    ```

5.  **Cargar Datos de Prueba (Opcional):**
    
    ```bash
    # Sembrar usuario administrador base
    npm run seed

    # Cargar lecturas históricas adicionales desde archivo CSV (Asegúrate de tener el archivo CSV en la ruta correspondiente)
    npx tsx src/prisma/seed_csv.ts
    ```

---

## ▶️ Ejecución

### Modo Desarrollo
Inicia el servidor con recarga automática:

```bash
npm run dev
```
Accede a la plataforma en: [http://localhost:3000](http://localhost:3000)

### Modo Producción
Para construir y desplegar la versión optimizada:

```bash
npm run build
npm start
```

**Recomendación con PM2 (Para mantener el proceso activo):**
```bash
npm install -g pm2
pm2 start npm --name "monica-frontend" -- start
```

---

## 📡 Documentación API (Resumen)

El sistema expone una API REST organizada por recursos:

*   **`/api/nodes`**: Gestión de metadatos de las estaciones.
*   **`/api/sensor-readings`**: Ingesta de datos crudos (POST) y consultas (GET).
*   **`/api/readings`**: Endpoint optimizado para gráficas históricas (Público).
*   **`/api/waste-detections`**: Registro de incidentes de contaminación.
*   **`/api/users`**: Administración de usuarios (Solo Admin).

> **Nota de Seguridad**: Las operaciones de escritura (`POST`, `PUT`, `DELETE`) están estrictamente protegidas y requieren autenticación con rol de Administrador.

---

© 2026 Proyecto MONICA. Todos los derechos reservados.
