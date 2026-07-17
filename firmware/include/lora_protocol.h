#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <stdint.h>

// --- CONFIGURACIÓN DE RADIO LORA (RadioLib SX1262) ---
#define LORA_FREQUENCY      915.0  // Frecuencia en MHz (ej: 915.0)
#define LORA_BANDWIDTH      500.0  // Ancho de banda en kHz (125.0, 250.0, 500.0)
#define LORA_SF             7      // Spreading Factor (SF7 para velocidad, SF9 por defecto)
#define LORA_CR             5      // Coding Rate (5 significa 4/5 para menor Time-on-Air)
#define LORA_SYNC_WORD      0x12   // Sync Word para filtrar redes ajenas
#define LORA_POWER          10     // Potencia en dBm (hasta 22 dBm)
#define LORA_PREAMBLE_LEN   8      // Longitud de preámbulo
#define LORA_TCXO_VOLTAGE   1.6    // Voltaje TCXO
#define LORA_USE_REGULATOR  false  // false = usa DC-DC (más eficiente), true = usa LDO

// --- CONFIGURACIÓN DE BAJO CONSUMO (CAD & Sleep) ---
// CAD_SLEEP_MS: Tiempo que el nodo sensor duerme entre escaneos CAD.
#define CAD_SLEEP_MS            2500

// CAD_PREAMBLE_MS: Duración mínima del preámbulo largo para despertar nodos.
// DEBE ser > CAD_SLEEP_MS + latencia de arranque TCXO (~50ms) + tiempo CAD (~3ms) + margen (250ms).
// Con CAD_SLEEP_MS=2500: 2500 + 50 + 3 + 250 = 2803ms → redondeamos a 2850ms.
#define CAD_PREAMBLE_MS         2850

// CAD_PREAMBLE_SYMBOLS: Número de símbolos del preámbulo largo, pre-calculado en tiempo de
// compilación para evitar errores de aritmética flotante en tiempo de ejecución.
// Fórmula: symbols = CAD_PREAMBLE_MS / symbol_time_ms
//          symbol_time_ms = (2^LORA_SF) / LORA_BANDWIDTH  →  2^7 / 500.0 = 0.256ms
//          symbols = 2850 / 0.256 = 11132 (redondeado hacia arriba para seguridad)
// Verificación: 11132 síms × 0.256ms/sím = 2849.8ms  ✓  Cubre el ciclo de sleep completo.
// Límite SX1262: máx 65535 símbolos.  11132 << 65535  ✓
#define CAD_PREAMBLE_SYMBOLS    ((uint16_t)11200)

// CAD_RX_TIMEOUT_MS: Tiempo máximo de espera de paquete después de que CAD detecta señal.
// CRÍTICO: debe cubrir el peor caso: el nodo despierta justo cuando el concentrador
// EMPIEZA a transmitir el preámbulo largo. En ese caso el nodo debe esperar casi todo
// el preámbulo completo (2849ms) + header + payload antes de recibir RX_DONE.
// Cálculo: CAD_PREAMBLE_MS + margen = 2850 + 650 = 3500ms.
// Pasos RTC: 3500ms × 64 = 224,000 = 0x036B00 (3 bytes big-endian).
#define CAD_RX_TIMEOUT_MS       3500

// IDLE_TIMEOUT_MS: Tiempo de inactividad antes de entrar en modo sleep (3 minutos).
#define IDLE_TIMEOUT_MS         180000

#pragma pack(push, 1)

// Tipos de Mensajes
enum PacketType : uint8_t {
    // Comandos (Concentrador -> Nodo)
    CMD_PING          = 0x10,
    CMD_REQ_TELEMETRY = 0x11,
    CMD_REQ_IMAGE     = 0x12,
    CMD_REQ_MISSING   = 0x13, // Concentrador -> Nodo (Petición de fragmentos perdidos)
    CMD_CONFIG_CAM    = 0x14, // Concentrador -> Nodo (Configuración de la cámara)
    CMD_GET_CAM_CONFIG = 0x15, // Concentrador -> Nodo (Obtener configuración de la cámara)
    CMD_GO_TO_SLEEP    = 0x16, // Concentrador -> Nodo (Comando de apagado en cascada)
    CMD_WAKEUP         = 0x17, // Concentrador -> Nodo (Comando de despertar secuencial)
    
    // Respuestas (Nodo -> Concentrador)
    ACK               = 0x20,
    NACK              = 0x21,
    ACK_PROCESSING    = 0x22, // Nodo -> Concentrador (Capturando y procesando imagen de cámara)
    ACK_WAKEUP        = 0x23, // Nodo -> Concentrador (Confirmación de despertar)
    DATA_TELEMETRY    = 0x30,
    DATA_IMG_START    = 0x31,
    DATA_IMG_CHUNK    = 0x32,
    DATA_IMG_END      = 0x33,
    DATA_CAM_CONFIG   = 0x34  // Nodo -> Concentrador (Configuración devuelta)
};

// Secuencia de Sincronización para filtrar ruido
#define LORA_SYNC_0 0xAA
#define LORA_SYNC_1 0xBB

// Estructura de cabecera direccional
struct LoRaHeader {
    uint8_t syncWord[2]; // 0xAA 0xBB
    uint8_t srcId;       // Origen (0 = Concentrador)
    uint8_t destId;      // Destino (0 = Concentrador)
    uint8_t nextHopId;   // Siguiente salto físico inmediato
    uint8_t type;        // PacketType
    uint16_t seqNum;     // Secuencia o Chunk
    uint8_t ttl;         // Time-To-Live
};

// Payload de Sensores (25 Bytes)
struct SensorPayload {
    float latitude;
    float longitude;
    float ph;
    float dissolved_oxygen;
    uint16_t turbidity;
    uint16_t conductivity;
    float temperature;
    uint8_t battery_level;
};

// Payload de Configuración de Cámara (24 Bytes)
struct CameraConfigPayload {
    uint8_t resolution;      // framesize_t (0-21)
    int8_t brightness;       // -2 a 2
    int8_t contrast;         // -2 a 2
    uint8_t quality;         // Calidad JPEG (10-63)
    int8_t saturation;       // -2 a 2
    uint8_t special_effect;  // 0 a 6
    uint8_t whitebal;        // 0 o 1
    uint8_t awb_gain;        // 0 o 1
    uint8_t wb_mode;         // 0 a 4
    uint8_t exposure_ctrl;   // 0 o 1
    uint8_t aec2;            // 0 o 1
    int8_t ae_level;         // -2 a 2
    uint16_t aec_value;      // 0 a 1200
    uint8_t gain_ctrl;       // 0 o 1
    uint8_t agc_gain;        // 0 a 30
    uint8_t gainceiling;     // 0 a 6
    uint8_t bpc;             // 0 o 1
    uint8_t wpc;             // 0 o 1
    uint8_t raw_gma;         // 0 o 1
    uint8_t lenc;            // 0 o 1
    uint8_t hmirror;         // 0 o 1
    uint8_t vflip;           // 0 o 1
    uint8_t dcw;             // 0 o 1
    uint8_t colorbar;        // 0 o 1
};

// MTU Seguro para LoRa
#define LORA_MAX_PAYLOAD 240

// Paquete genérico de transmisión
struct LoRaPacket {
    LoRaHeader header;
    uint8_t payload[LORA_MAX_PAYLOAD];
};

#pragma pack(pop)

#endif
