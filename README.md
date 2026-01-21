# WEB_MONICA - Sistema de Monitoreo Ambiental

**WEB_MONICA** es una plataforma web integral diseñada para el monitoreo y gestión de variables ambientales en ríos y cuerpos de agua. El sistema permite la recolección, visualización y análisis de datos provenientes de nodos de sensores distribuidos, así como la detección de residuos mediante análisis de imágenes.

## 🚀 Características Principales

*   **Gestión de Usuarios y Roles**: Control de acceso y administración de usuarios.
*   **Gestión de Nodos**: Registro y seguimiento de estaciones de monitoreo (nodos).
*   **Monitoreo de Sensores**: Registro histórico de variables críticas como pH, oxígeno disuelto, turbidez, etc.
*   **Detección de Residuos**: Registro de eventos de contaminación detectados por modelos de visión artificial.
*   **API RESTful**: Interfaz completa para la integración con dispositivos IoT y clientes frontend.

## 🛠️ Tecnologías Utilizadas

Este proyecto utiliza un stack moderno y robusto:

*   **[Next.js 15+](https://nextjs.org/)**: Framework de React para producción (App Router).
*   **[React](https://react.dev/)**: Biblioteca para interfaces de usuario.
*   **[Prisma ORM](https://www.prisma.io/)**: Acceso a base de datos tipo-seguro.
*   **[PostgreSQL](https://www.postgresql.org/)**: Base de datos relacional potente y fiable.
*   **[Docker](https://www.docker.com/)**: Contenedorización para un despliegue consistente.

## 📋 Requisitos Previos

Asegúrate de tener instalado lo siguiente:

*   **Node.js** (v18 o superior)
*   **Docker** y **Docker Compose**
*   **Git**

## 🔧 Instalación y Configuración

Sigue estos pasos para levantar el entorno de desarrollo:

1.  **Clonar el repositorio:**

    ```bash
    git clone https://github.com/AlfredoDompablo/WEB_MONICA.git
    cd WEB_MONICA/app-web
    ```

2.  **Instalar dependencias:**

    ```bash
    npm install
    ```

3.  **Configurar variables de entorno:**
    Crea un archivo `.env` en la raíz de `app-web` (o usa el existente) y configura la URL de la base de datos:

    ```env
    DATABASE_URL="postgresql://admin:password123@localhost:5432/rio_db?schema=public"
    ```

4.  **Levantar la base de datos:**
    Utiliza Docker Compose para iniciar PostgreSQL:

    ```bash
    docker compose up -d
    ```

5.  **Sincronizar la base de datos:**
    Ejecuta las migraciones de Prisma para crear las tablas:

    ```bash
    npx prisma db push
    ```

## ▶️ Ejecución

### Modo Desarrollo
Para iniciar el servidor con recarga en caliente (hot-reload):

```bash
npm run dev
```
La aplicación estará disponible en `http://localhost:3000`.

### Modo Producción
Para construir y ejecutar la versión optimizada:

```bash
npm run build
npm start
```

## 📡 Documentación de la API

La aplicación expone los siguientes endpoints REST:

### 👤 Usuarios (`/api/users`)
*   `GET /api/users`: Listar todos los usuarios.
*   `POST /api/users`: Registrar un nuevo usuario.
*   `GET /api/users/[id]`: Obtener detalle de un usuario (incluye nodos asignados).
*   `PUT /api/users/[id]`: Actualizar información de usuario.
*   `DELETE /api/users/[id]`: Desactivar usuario (eliminación lógica).

### 📡 Nodos (`/api/nodes`)
*   `GET /api/nodes`: Listar todos los nodos de monitoreo.
*   `POST /api/nodes`: Registrar un nuevo nodo.
*   `GET /api/nodes/[id]`: Obtener detalle de un nodo.

### 🧪 Lecturas de Sensores (`/api/sensor-readings`)
*   `GET /api/sensor-readings`: Consultar historial de lecturas.
    *   *Parámetros opcionales:* `node_id`, `startDate`, `endDate`, `limit`.
*   **POST** `/api/sensor-readings`: Registrar nueva lectura (usado por nodos IoT).

### ♻️ Detección de Residuos (`/api/waste-detections`)
*   `GET /api/waste-detections`: Consultar detecciones de residuos.
*   **POST** `/api/waste-detections`: Registrar evento de detección (soporta imágenes en Base64).

---
