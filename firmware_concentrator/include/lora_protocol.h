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

#pragma pack(push, 1)

// Tipos de Mensajes
enum PacketType : uint8_t {
    // Comandos (Concentrador -> Nodo)
    CMD_PING          = 0x10,
    CMD_REQ_TELEMETRY = 0x11,
    CMD_REQ_IMAGE     = 0x12,
    CMD_REQ_MISSING   = 0x13, // Concentrador -> Nodo (Petición de fragmentos perdidos)
    CMD_CONFIG_CAM    = 0x14, // Concentrador -> Nodo (Configuración de la cámara)
    
    // Respuestas (Nodo -> Concentrador)
    ACK               = 0x20,
    NACK              = 0x21,
    DATA_TELEMETRY    = 0x30,
    DATA_IMG_START    = 0x31,
    DATA_IMG_CHUNK    = 0x32,
    DATA_IMG_END      = 0x33
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

// Payload de Configuración de Cámara (3 Bytes)
struct CameraConfigPayload {
    uint8_t resolution; // framesize_t (0-21)
    int8_t brightness;  // -2 a 2
    int8_t contrast;    // -2 a 2
};

// MTU Seguro para LoRa
#define LORA_MAX_PAYLOAD 200

// Paquete genérico de transmisión
struct LoRaPacket {
    LoRaHeader header;
    uint8_t payload[LORA_MAX_PAYLOAD];
};

#pragma pack(pop)

#endif
