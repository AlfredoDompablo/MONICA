import { ApiReference } from '@scalar/nextjs-api-reference'

const config = {
    url: '/openapi.json',
}

/**
 * Referencia de API (Scalar)
 * 
 * Genera la interfaz gráfica de documentación de la API.
 * Lee la especificación OpenAPI desde /openapi.json.
 */
export const GET = ApiReference(config)
