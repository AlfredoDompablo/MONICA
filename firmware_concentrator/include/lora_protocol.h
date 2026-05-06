#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <stdint.h>

#pragma pack(push, 1)

// Tipos de Mensajes
#define LORA_TYPE_SENSOR    0x01
#define LORA_TYPE_IMG_START 0x02
#define LORA_TYPE_IMG_CHUNK 0x03
#define LORA_TYPE_IMG_END   0x04

// Secuencia de Sincronización para filtrar ruido
#define LORA_SYNC_0 0xAA
#define LORA_SYNC_1 0xBB

// Estructura de cabecera general (6 Bytes)
struct LoRaHeader {
    uint8_t syncWord[2]; 
    uint8_t nodeId;
    uint8_t dataType;
    uint16_t pktIndex;
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

// MTU Seguro para LoRa
#define LORA_MAX_PAYLOAD 200

// Paquete genérico de transmisión
struct LoRaPacket {
    LoRaHeader header;
    uint8_t payload[LORA_MAX_PAYLOAD];
};

#pragma pack(pop)

#endif
