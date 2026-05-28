#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <RadioLib.h>
#include "secrets.h"
#include "lora_protocol.h"

// --- Hardware Definitions ---
#define MY_NODE_ID 0

#define VEXT_PIN 3        
#define BACKLIGHT_PIN 21  
#define GNSS_RX_PIN 33    
#define GNSS_TX_PIN 34    

// --- LoRa SPI Pins (Heltec Internal) ---
#define LORA_CS   8
#define LORA_SCK  9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14

// --- SD Card SPI Pins (Custom Secondary SPI) ---
#define SD_MISO 4
#define SD_MOSI 5
#define SD_SCK  6
#define SD_CS   7

// --- Objects ---
TinyGPSPlus gps;
TFT_eSPI tft = TFT_eSPI(); 
SPIClass* spiSD = nullptr; // Secondary SPI bus for SD

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// --- Variables ---
bool sdMounted = false;
File currentImgFile;
bool isImgFileOpen = false;

File activeFile;
bool isFileActive = false;
uint16_t expectedSeqNum = 0; // 0 significa que no hay sesión de imagen activa
unsigned long lastChunkReceivedTime = 0;
bool* chunkReceived = NULL;
uint16_t totalChunks = 0;
uint32_t imgSize = 0;

// Polling Demo Variables and Reliability State Machine
unsigned long lastPollTime = 0;
const unsigned long pollInterval = 30000; // Poll every 30s
uint8_t targetNode = 1; // Alternar entre nodos (ej. 1 al 4)
bool pollForImage = false; // Alternar entre telemetría e imagen
uint16_t packetSequence = 0;

enum PollingState {
    POLL_STATE_IDLE,
    POLL_STATE_WAITING_RESPONSE
};
PollingState pollState = POLL_STATE_IDLE;
uint8_t currentAttempt = 0;
unsigned long requestSentTime = 0;
unsigned long currentTimeout = 3000; // 3s para telemetría, 6s para imagen
volatile bool responseReceived = false;

// --- Custom Stream Class for Base64 Encoding on the fly ---
class ImagePayloadStream : public Stream {
private:
    File _file;
    String _nodeId;
    size_t _fileSize;
    size_t _base64Size;
    String _prefix;
    String _suffix;
    size_t _totalSize;
    size_t _position;
    
    // SD buffering
    uint8_t _sdBuf[512];
    int _sdBufLen;
    int _sdBufPos;

    uint8_t _buf[3];
    int _bufLen;
    char _b64[4];
    int _b64Pos;
    const char* _base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int readSDByte() {
        if (_sdBufPos >= _sdBufLen) {
            _sdBufLen = _file.read(_sdBuf, sizeof(_sdBuf));
            _sdBufPos = 0;
            if (_sdBufLen <= 0) return -1;
        }
        return _sdBuf[_sdBufPos++];
    }

    void encodeBlock() {
        _b64[0] = _base64_table[_buf[0] >> 2];
        _b64[1] = _base64_table[((_buf[0] & 0x03) << 4) | ((_bufLen > 1 ? _buf[1] : 0) >> 4)];
        _b64[2] = (_bufLen > 1) ? _base64_table[((_buf[1] & 0x0f) << 2) | ((_bufLen > 2 ? _buf[2] : 0) >> 6)] : '=';
        _b64[3] = (_bufLen > 2) ? _base64_table[_buf[2] & 0x3f] : '=';
        _b64Pos = 0;
    }

public:
    ImagePayloadStream(File file, String nodeId) : _file(file), _nodeId(nodeId) {
        _prefix = "{\"node_id\":\"" + _nodeId + "\",\"image_original_base64\":\"";
        _suffix = "\"}";
        _fileSize = _file.size();
        _base64Size = ((_fileSize + 2) / 3) * 4;
        _totalSize = _prefix.length() + _base64Size + _suffix.length();
        _position = 0;
        _bufLen = 0;
        _b64Pos = 4; 
        _sdBufLen = 0;
        _sdBufPos = 0;
    }

    size_t getTotalSize() { return _totalSize; }
    int available() override { return _totalSize - _position; }

    int read() override {
        if (_position >= _totalSize) return -1;
        int b = -1;
        if (_position < _prefix.length()) {
            b = _prefix[_position];
        } else if (_position < _prefix.length() + _base64Size) {
            if (_b64Pos >= 4) {
                _bufLen = 0;
                while (_bufLen < 3) {
                    int c = readSDByte();
                    if (c < 0) break;
                    _buf[_bufLen++] = c;
                }
                if (_bufLen > 0) encodeBlock();
            }
            if (_b64Pos < 4) b = _b64[_b64Pos++];
        } else {
            size_t suffixPos = _position - (_prefix.length() + _base64Size);
            if (suffixPos < _suffix.length()) b = _suffix[suffixPos];
        }
        if (b != -1) _position++;
        return b;
    }

    size_t readBytes(uint8_t *buffer, size_t length) override {
        size_t count = 0;
        while (count < length) {
            int c = read();
            if (c < 0) break;
            buffer[count++] = (uint8_t)c;
        }
        return count;
    }
    
    size_t readBytes(char *buffer, size_t length) override {
        return readBytes((uint8_t*)buffer, length);
    }

    int peek() override { return -1; }
    size_t write(uint8_t) override { return 0; }
};

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    int maxBytes = 50; // Evitar lazos infinitos por ruido en pin RX flotante
    while (Serial1.available() && maxBytes-- > 0)
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

void drawHeader(const char* title) {
    tft.fillRect(0, 0, 160, 15, tft.color565(0, 51, 102)); // Navy Blue bar
    tft.drawFastHLine(0, 15, 160, tft.color565(200, 200, 200)); // Silver line
    tft.setTextColor(TFT_WHITE, tft.color565(0, 51, 102));
    tft.setTextSize(1);
    tft.setCursor((160 - strlen(title) * 6) / 2, 4); // Centrado
    tft.print(title);
}

void setup() {
  Serial.begin(115200);
  
  // Power Systems
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  delay(100);

  // TFT Display
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Concentrador Init...");

  // GPS
  Serial1.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  // LoRa SPI & Init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Serial.println("Iniciando transceptor LoRa SX1262 del Concentrador...");
  tft.println("Iniciando LoRa...");
  int state = radio.begin(
    LORA_FREQUENCY,
    LORA_BANDWIDTH,
    LORA_SF,
    LORA_CR,
    LORA_SYNC_WORD,
    LORA_POWER,
    LORA_PREAMBLE_LEN,
    LORA_TCXO_VOLTAGE,
    LORA_USE_REGULATOR
  );
  if (state == RADIOLIB_ERR_NONE) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("LoRa OK!");
    Serial.println("LoRa SX1262: INICIALIZADO CON EXITO [OK]");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Error LoRa: "); tft.println(state);
    Serial.printf("[ERROR] Fallo al iniciar LoRa SX1262. Codigo: %d\n", state);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // SD Card
  tft.setTextSize(1);
  spiSD = new SPIClass(HSPI);
  spiSD->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, *spiSD)) {
    Serial.println("Fallo al montar SD!");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Error: SD Card!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sdMounted = false;
  } else {
    Serial.println("SD Inicializada.");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("SD OK!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sdMounted = true;
  }

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  
  // Dibujar el Dashboard Permanente (Mitad Superior)
  tft.fillScreen(TFT_BLACK);
  drawHeader("MONICA CONCENTRADOR");
  
  // Estado de Hardware (Fila 1 en y = 18)
  tft.setCursor(0, 18);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("SD: ");
  if (sdMounted) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("ERR");
  }
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("  |  WiFi: ");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print("OK");
  
  // IP (Fila 2 en y = 28)
  tft.setCursor(0, 28);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("IP: ");
  tft.print(WiFi.localIP());
  
  // Separador (en y = 39)
  tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
  
  // Separador inferior del Footer (en y = 63)
  tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));

  // Iniciar la escucha LoRa en segundo plano
  radio.startReceive();
  Serial.println("Concentrador listo y escuchando por LoRa...");
}

void forwardSensorData(uint8_t srcId, SensorPayload* sp) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  
  StaticJsonDocument<400> doc;
  char nodeFormatted[20];
  sprintf(nodeFormatted, "NODE_%03d", srcId);
  doc["node_id"] = String(nodeFormatted);
  doc["latitude"] = sp->latitude;
  doc["longitude"] = sp->longitude;
  doc["ph"] = sp->ph;
  doc["dissolved_oxygen"] = sp->dissolved_oxygen;
  doc["turbidity"] = sp->turbidity;
  doc["conductivity"] = sp->conductivity;
  doc["temperature"] = sp->temperature;
  doc["battery_level"] = sp->battery_level;

  String jsonOutput;
  serializeJson(doc, jsonOutput);

  http.setTimeout(15000);
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  
  tft.fillRect(0, 53, 160, 10, TFT_BLACK);
  tft.setCursor(0, 53);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Subiendo HTTP...");
  
  int httpResponseCode = http.POST(jsonOutput);
  tft.fillRect(0, 53, 160, 10, TFT_BLACK);
  tft.setCursor(0, 53);
  if (httpResponseCode == 200 || httpResponseCode == 201) {
    Serial.println(String("Sensor Reenviado OK: ") + httpResponseCode);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("HTTP: POST OK!");
  } else {
    Serial.println(String("Error reenviando sensor: ") + httpResponseCode);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.printf("HTTP: ERR %d", httpResponseCode);
  }
  http.end();
}

void handleImageFragment(LoRaPacket* packet, size_t payloadLen) {
    String filename = String("/img_node_") + packet->header.srcId + ".jpg";
    
    tft.fillRect(0, 53, 160, 10, TFT_BLACK);
    tft.setCursor(0, 53);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    
    if (packet->header.type == DATA_IMG_START) {
        lastChunkReceivedTime = millis();
        // Asegurarse de cerrar de forma segura cualquier descriptor de archivo anterior
        if (isFileActive) {
            activeFile.close();
            isFileActive = false;
        }

        // Obtener tamaño total y chunks de los datos del payload si existen (6 bytes)
        if (payloadLen >= 6) {
            memcpy(&imgSize, packet->payload, 4);
            memcpy(&totalChunks, packet->payload + 4, 2);
            Serial.printf("LoRa RX: DATA_IMG_START recibido. Tamaño: %u bytes, Total Chunks: %u\n", imgSize, totalChunks);
        } else {
            // Fallback si no viene en el payload
            imgSize = 0;
            totalChunks = 0;
            Serial.println("LoRa RX: DATA_IMG_START recibido (sin payload de tamaño)");
        }

        // Inicializar el bitmap en RAM para control de fragmentos perdidos
        if (chunkReceived != NULL) {
            free(chunkReceived);
            chunkReceived = NULL;
        }
        // Reservar un tamaño base seguro de 2048 elementos (suficiente para imágenes de hasta 409KB)
        chunkReceived = (bool*)calloc(2048, sizeof(bool));

        expectedSeqNum = 1; // Esperamos iniciar con el chunk 1
        
        // Recrear archivo en SD de forma limpia para empezar la grabación secuencial
        if (SD.exists(filename)) {
            SD.remove(filename);
        }
        
        // Abrir y mantener el descriptor del archivo abierto para streaming rápido
        activeFile = SD.open(filename, FILE_WRITE);
        if (activeFile) {
            isFileActive = true;
            // Pre-asignar tamaño contiguo para evitar fragmentación y latencias de asignación
            if (imgSize > 0) {
                activeFile.seek(imgSize - 1);
                activeFile.write(0);
                activeFile.seek(0);
                Serial.printf("SD: Espacio pre-asignado contiguamente para %u bytes\n", imgSize);
            }
            Serial.printf("IMG_START de Nodo %d (Descriptor de archivo SD abierto para streaming)\n", packet->header.srcId);
        } else {
            Serial.println("Error: Falló al inicializar y abrir el archivo en la SD para streaming");
        }
        tft.print("Recibiendo Foto...");
    } 
    else if (packet->header.type == DATA_IMG_CHUNK) {
        uint16_t seq = packet->header.seqNum;
        lastChunkReceivedTime = millis();
        
        // Inicializar sesión y archivo de forma dinámica si se perdió el START
        if (!isFileActive) {
            Serial.printf("LoRa RX: DATA_IMG_CHUNK seq %d recibido sin START. Inicializando sesión de emergencia...\n", seq);
            
            if (chunkReceived != NULL) {
                free(chunkReceived);
                chunkReceived = NULL;
            }
            chunkReceived = (bool*)calloc(2048, sizeof(bool));
            totalChunks = 2048; // Límite temporal máximo
            expectedSeqNum = 1;
            
            if (SD.exists(filename)) {
                SD.remove(filename);
            }
            activeFile = SD.open(filename, FILE_WRITE);
            if (activeFile) {
                isFileActive = true;
                Serial.println("SD: Archivo abierto dinámicamente en modo de emergencia.");
            } else {
                Serial.println("Error: Falló al abrir archivo en SD para sesión dinámica");
            }
        }

        // Descartar de forma segura fragmentos duplicados (retransmisiones)
        if (chunkReceived != NULL && seq < 2048 && chunkReceived[seq]) {
            Serial.printf("LoRa RX: Descartando chunk duplicado seq %d (ya recibido)\n", seq);
            return;
        }

        // Escribir el fragmento directamente en el offset físico correspondiente en el archivo
        if (isFileActive && payloadLen > 0) {
            uint32_t offset = (seq - 1) * LORA_MAX_PAYLOAD;
            activeFile.seek(offset);
            activeFile.write(packet->payload, payloadLen);
            
            if (chunkReceived != NULL && seq < 2048) {
                chunkReceived[seq] = true;
            }
            
            if (seq == expectedSeqNum) {
                expectedSeqNum++;
            }
            Serial.printf("IMG_CHUNK seq %d (%u bytes guardados en SD en offset %u)\n", seq, payloadLen, offset);
        } else {
            Serial.println("Error: Archivo inactivo en volcado de chunk");
        }
        
        tft.fillRect(0, 65, 160, 12, TFT_BLACK);
        tft.setCursor(0, 65);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("%u / %u Chunks", (uint32_t)seq, totalChunks);
    }
    else if (packet->header.type == DATA_IMG_END) {
        lastChunkReceivedTime = millis();
        Serial.println("LoRa RX: DATA_IMG_END recibido. Verificando integridad de chunks...");

        // Si recibimos DATA_IMG_END pero no hay sesión activa (expectedSeqNum == 0), 
        // significa que enviamos ACK antes pero se perdió, y el nodo está reintentando.
        if (expectedSeqNum == 0) {
            Serial.println("LoRa RX: DATA_IMG_END recibido fuera de sesión activa. Re-enviando ACK final...");
            LoRaPacket ackPacket;
            memset(&ackPacket, 0, sizeof(LoRaHeader));
            ackPacket.header.syncWord[0] = LORA_SYNC_0;
            ackPacket.header.syncWord[1] = LORA_SYNC_1;
            ackPacket.header.srcId = MY_NODE_ID; // 0
            ackPacket.header.destId = packet->header.srcId;
            ackPacket.header.nextHopId = 1; // Siguiente salto es el Nodo 1
            ackPacket.header.type = ACK;
            ackPacket.header.seqNum = packetSequence++;
            ackPacket.header.ttl = 5;
            delay(80); // Retardo mínimo para batería (gracia de RF)
            radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
            radio.startReceive();
            return;
        }

        // Obtener tamaño real y cantidad de chunks reales de los datos del payload de DATA_IMG_END si existen
        if (payloadLen >= 6) {
            uint32_t actualImgSize = 0;
            uint16_t actualTotalChunks = 0;
            memcpy(&actualImgSize, packet->payload, 4);
            memcpy(&actualTotalChunks, packet->payload + 4, 2);
            
            if (actualTotalChunks > 0 && actualTotalChunks < 2048) {
                imgSize = actualImgSize;
                totalChunks = actualTotalChunks;
                Serial.printf("LoRa RX: Info extraída de DATA_IMG_END. Tamaño: %u bytes, Total Chunks: %u\n", imgSize, totalChunks);
                

            }
        }

        // Verificar fragmentos perdidos
        uint16_t missingCount = 0;
        uint16_t firstMissing[98]; // Máximo 98 por paquete LoRa
        
        if (chunkReceived != NULL && totalChunks > 0) {
            for (uint16_t i = 1; i <= totalChunks; i++) {
                if (!chunkReceived[i]) {
                    if (missingCount < 98) {
                        firstMissing[missingCount++] = i;
                    } else {
                        break;
                    }
                }
            }
        }

        if (missingCount > 0) {
            Serial.printf("LoRa Integrity: ¡Perdidos %u chunks! Enviando petición de retransmisión CMD_REQ_MISSING...\n", missingCount);
            
            tft.fillRect(0, 53, 160, 10, TFT_BLACK);
            tft.setCursor(0, 53);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.printf("NACK: %u Chunks", missingCount);
            
            LoRaPacket nackPacket;
            memset(&nackPacket, 0, sizeof(LoRaHeader));
            nackPacket.header.syncWord[0] = LORA_SYNC_0;
            nackPacket.header.syncWord[1] = LORA_SYNC_1;
            nackPacket.header.srcId = MY_NODE_ID; // 0
            nackPacket.header.destId = packet->header.srcId;
            nackPacket.header.nextHopId = 1; // Siguiente salto es el Nodo 1
            nackPacket.header.type = CMD_REQ_MISSING;
            nackPacket.header.seqNum = packetSequence++;
            nackPacket.header.ttl = 5;
            
            memcpy(nackPacket.payload, &missingCount, 2);
            memcpy(nackPacket.payload + 2, firstMissing, missingCount * 2);
            
            delay(80); // Retardo mínimo para batería (gracia de RF)
            radio.transmit((uint8_t*)&nackPacket, sizeof(LoRaHeader) + 2 + (missingCount * 2));
            radio.startReceive();
            return;
        }

        // Si no hay fragmentos perdidos: ¡Éxito absoluto de reensamblado!
        Serial.println("LoRa Integrity: ¡100% de fragmentos recibidos con éxito! Enviando ACK final...");
        
        tft.fillRect(0, 53, 160, 10, TFT_BLACK);
        tft.setCursor(0, 53);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("Reensamblado OK!");

        // Enviar ACK final al nodo para que libere su buffer
        LoRaPacket ackPacket;
        memset(&ackPacket, 0, sizeof(LoRaHeader));
        ackPacket.header.syncWord[0] = LORA_SYNC_0;
        ackPacket.header.syncWord[1] = LORA_SYNC_1;
        ackPacket.header.srcId = MY_NODE_ID; // 0
        ackPacket.header.destId = packet->header.srcId;
        ackPacket.header.nextHopId = 1; // Siguiente salto es el Nodo 1
        ackPacket.header.type = ACK;
        ackPacket.header.seqNum = packetSequence++;
        ackPacket.header.ttl = 5;
        delay(80); // Retardo mínimo para batería (gracia de RF)
        radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
        radio.startReceive();

        expectedSeqNum = 0; // Finalizar sesión activa de imagen
        
        // Liberar el bitmap
        if (chunkReceived != NULL) {
            free(chunkReceived);
            chunkReceived = NULL;
        }

        // Cerrar formalmente el archivo consolidándolo en la SD
        if (isFileActive) {
            activeFile.close();
            isFileActive = false;
            Serial.println("Archivo de imagen consolidado y cerrado en SD con éxito.");
        }
        
        // Proceder con la lectura limpia del archivo guardado en SD para subir a la API de IA (con hasta 3 intentos)
        int attempts = 3;
        int httpResponseCode = -1;
        
        for (int attempt = 1; attempt <= attempts; attempt++) {
            File imgFile = SD.open(filename, FILE_READ);
            if (imgFile) {
                size_t finalSize = imgFile.size();
                if (attempt == 1) {
                    Serial.printf("Reensamblado completo! Tamaño final en SD: %u bytes (%.2f KB). Enviando a API...\n", 
                                  finalSize, finalSize / 1024.0);
                }
                
                Serial.printf("Subida API - Intento %d de %d...\n", attempt, attempts);
                tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                tft.setCursor(0, 60);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.printf("Subiendo %d/%d...", attempt, attempts);
                
                char imgNodeFormatted[20];
                sprintf(imgNodeFormatted, "NODE_%03d", packet->header.srcId);
                ImagePayloadStream payloadStream(imgFile, String(imgNodeFormatted));
                
                HTTPClient http;
                http.setTimeout(60000); // 60s timeout para dar tiempo a la inferencia de IA lenta en CPU
                http.begin(AI_API_URL);
                http.addHeader("Content-Type", "application/json");
                http.addHeader("x-api-key", API_KEY);
                
                httpResponseCode = http.sendRequest("POST", &payloadStream, payloadStream.getTotalSize());
                Serial.printf("Intento %d completado. Código HTTP: %d\n", attempt, httpResponseCode);
                
                http.end(); // Limpiar la conexión HTTP
                imgFile.close();
                
                if (httpResponseCode == 200 || httpResponseCode == 201) {
                    tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                    tft.setCursor(0, 60);
                    tft.setTextColor(TFT_GREEN, TFT_BLACK);
                    tft.println("Foto API OK!");
                    break; // Éxito! Salir del bucle
                } else {
                    tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                    tft.setCursor(0, 60);
                    tft.setTextColor(TFT_RED, TFT_BLACK);
                    tft.printf("ERR %d (Intentando...)", httpResponseCode);
                }
                
                if (attempt < attempts) {
                    delay(2000); // Esperar 2 segundos antes del siguiente intento
                }
            } else {
                Serial.println("Error: No se pudo abrir el archivo guardado para enviar a la API");
                break;
            }
        }
        
        lastPollTime = millis(); // Resetear el temporizador de polling tras la subida para dar tiempo de procesamiento y evitar colisiones inmediatas
    }
}

void pollNode(uint8_t nodeToPoll, PacketType cmd) {
    LoRaPacket packet;
    memset(&packet, 0, sizeof(LoRaHeader));
    packet.header.syncWord[0] = LORA_SYNC_0;
    packet.header.syncWord[1] = LORA_SYNC_1;
    packet.header.srcId = MY_NODE_ID; // 0
    packet.header.destId = nodeToPoll;
    packet.header.nextHopId = 1; // El Concentrador siempre habla directamente con el Nodo 1
    packet.header.type = cmd;
    packet.header.seqNum = packetSequence++;
    packet.header.ttl = 5;

    tft.fillRect(0, 80, 160, 20, TFT_BLACK);
    tft.setCursor(0, 80);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    if(cmd == CMD_REQ_TELEMETRY) tft.printf("Poll Sensor -> Nodo %d", nodeToPoll);
    else tft.printf("Poll Imagen -> Nodo %d", nodeToPoll);

    int txState = radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader));
    Serial.printf("Polling Node %d with CMD 0x%02X, TX State: %d\n", nodeToPoll, cmd, txState);
    radio.startReceive();
}

void handleLoRa() {
    if (digitalRead(LORA_DIO1)) { 
        LoRaPacket packet;
        int state = radio.readData((uint8_t*)&packet, sizeof(packet));
        
        if (state == RADIOLIB_ERR_NONE) {
            if (packet.header.syncWord[0] == LORA_SYNC_0 && packet.header.syncWord[1] == LORA_SYNC_1) {
                // Es físicamente para mí en este salto?
                if (packet.header.nextHopId != MY_NODE_ID) {
                    radio.startReceive();
                    return;
                }
                
                // Es para mí?
                if (packet.header.destId == MY_NODE_ID) {
                    Serial.printf("LoRa RX: Src %d, Tipo 0x%02X, Seq %d\n", packet.header.srcId, packet.header.type, packet.header.seqNum);
                    
                    size_t packetLen = radio.getPacketLength();
                    size_t payloadLen = packetLen - sizeof(LoRaHeader);

                    if (packet.header.type == DATA_TELEMETRY) {
                        tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                        tft.setCursor(0, 42);
                        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                        tft.printf("RX: SENSOR (%d)", packet.header.srcId);
                        
                        SensorPayload* sp = (SensorPayload*)packet.payload;
                        forwardSensorData(packet.header.srcId, sp);
                        
                        if (packet.header.srcId == targetNode && !pollForImage) {
                            responseReceived = true;
                        }
                    }
                    else if (packet.header.type >= DATA_IMG_START && packet.header.type <= DATA_IMG_END) {
                        if(packet.header.type == DATA_IMG_START) {
                           tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                           tft.setCursor(0, 42);
                           tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                           tft.printf("RX: IMG (%d)", packet.header.srcId);
                           
                           if (packet.header.srcId == targetNode && pollForImage) {
                               responseReceived = true;
                           }
                        }
                        handleImageFragment(&packet, payloadLen);
                    }
                    else if (packet.header.type == ACK) {
                        tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                        tft.setCursor(0, 42);
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.printf("ACK Nodo %d", packet.header.srcId);
                        
                        if (packet.header.srcId == targetNode && pollForImage) {
                            responseReceived = true;
                        }
                    }
                }
            }
        }
        radio.startReceive(); 
    }
}

void loop() {
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    Serial.println("[HEARTBEAT] Concentrador ejecutando loop...");
    lastHeartbeat = millis();
  }
  smartDelay(10); 
  handleLoRa();
  
  // Control de Timeout para la sesión de imagen activa
  if (expectedSeqNum > 0 && (millis() - lastChunkReceivedTime > 60000)) {
      Serial.println("[TIMEOUT] No se recibieron chunks de imagen por 60s. Cancelando sesión de forma segura.");
      if (isFileActive) {
          activeFile.close();
          isFileActive = false;
      }
      expectedSeqNum = 0;
      if (chunkReceived != NULL) {
          free(chunkReceived);
          chunkReceived = NULL;
      }
      lastPollTime = millis(); // Resetear polling tras timeout para evitar ráfagas inmediatas
      tft.fillRect(0, 53, 160, 10, TFT_BLACK);
      tft.setCursor(0, 53);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("Img Timeout!");
  }
  
  // Lógica de Polling Automático Altamente Confiable con 3 Intentos de Retransmisión
  // Solo hacer polling si no estamos recibiendo activamente una imagen (expectedSeqNum == 0)
  if (expectedSeqNum == 0) {
      if (pollState == POLL_STATE_IDLE) {
          if (millis() - lastPollTime > pollInterval) {
              responseReceived = false;
              currentAttempt = 1;
              requestSentTime = millis();
              currentTimeout = pollForImage ? 6000 : 3500; // 6s para imagen (foto), 3.5s para telemetría
              pollState = POLL_STATE_WAITING_RESPONSE;
              
              PacketType cmd = pollForImage ? CMD_REQ_IMAGE : CMD_REQ_TELEMETRY;
              pollNode(targetNode, cmd);
          }
      }
      else if (pollState == POLL_STATE_WAITING_RESPONSE) {
          if (responseReceived) {
              Serial.printf("LoRa Flow: Respuesta exitosa recibida del Nodo %d en intento %d.\n", targetNode, currentAttempt);
              
              // Rotar lógica para el próximo ciclo
              if (pollForImage) {
                  targetNode++;
                  if (targetNode > 4) targetNode = 1;
                  pollForImage = false;
              } else {
                  pollForImage = true;
              }
              
              pollState = POLL_STATE_IDLE;
              lastPollTime = millis();
          }
          else if (millis() - requestSentTime > currentTimeout) {
              if (currentAttempt < 3) {
                  currentAttempt++;
                  Serial.printf("LoRa Flow: Timeout! Re-intentando peticion al Nodo %d (Intento %d/3)...\n", targetNode, currentAttempt);
                  
                  tft.fillRect(0, 65, 160, 12, TFT_BLACK);
                  tft.setCursor(0, 65);
                  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                  tft.printf("Reint %d... %d/3", targetNode, currentAttempt);
                  
                  PacketType cmd = pollForImage ? CMD_REQ_IMAGE : CMD_REQ_TELEMETRY;
                  pollNode(targetNode, cmd);
                  
                  requestSentTime = millis();
              } else {
                  Serial.printf("LoRa Flow: [FALLO] Sin respuesta del Nodo %d tras 3 intentos. Pasando al siguiente.\n", targetNode);
                  
                  tft.fillRect(0, 65, 160, 12, TFT_BLACK);
                  tft.setCursor(0, 65);
                  tft.setTextColor(TFT_RED, TFT_BLACK);
                  tft.printf("Err Nodo %d (3 int)", targetNode);
                  
                  // Rotar de todas formas para no quedar bloqueados
                  if (pollForImage) {
                      targetNode++;
                      if (targetNode > 4) targetNode = 1;
                      pollForImage = false;
                  } else {
                      pollForImage = true;
                  }
                  
                  pollState = POLL_STATE_IDLE;
                  lastPollTime = millis();
              }
          }
      }
  }
}
