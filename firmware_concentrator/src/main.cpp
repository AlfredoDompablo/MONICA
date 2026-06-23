/**
 * @file main.cpp
 * @brief Firmware de Producción para el Nodo Concentrador de MONICA (ID: 0).
 * 
 * Este programa actúa como la pasarela (Gateway) central y coordinadora de la red de sensores.
 * Administra el polling periódico (muestreo) de los nodos sensores de la red mediante una máquina
 * de estados confiable con lógica de reintentos, recibe la telemetría ambiental e imágenes de cámaras,
 * reensambla y consolida fragmentos de imágenes JPG directamente en una tarjeta SD por SPI dedicado,
 * codifica los datos binarios en Base64 en tiempo real mediante un flujo de transmisión dinámico,
 * y sube la información a los servidores API y servicios de Inteligencia Artificial (IA) mediante WiFi.
 * 
 * Arquitectura de Red:
 *   [Concentrador 0] <-> [Nodo 1] <-> [Nodo 2] <-> [Nodo 3] <-> [Nodo 4]
 * 
 * Diseñado con aislamiento SPI estricto (HSPI para SD, FSPI para pantalla/LoRa) para corregir conflictos 
 * de hardware del ESP32-S3 y asignación dinámica de constructores en setup() para evitar bootloops globales.
 */

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

// ============================================================================
// --- DEFINICIONES DE HARDWARE DEL CONCENTRADOR ---
// ============================================================================
#define MY_NODE_ID 0          ///< Identificador único e inalterable del Concentrador Central
#define VEXT_PIN 3            ///< Control del regulador de energía para periféricos y pantalla
#define BACKLIGHT_PIN 21      ///< Pin PWM/Digital para la retroiluminación de la pantalla TFT
#define GNSS_RX_PIN 33        ///< Receptor UART RX para el módulo GPS/GNSS
#define GNSS_TX_PIN 34        ///< Transmisor UART TX para el módulo GPS/GNSS

// ============================================================================
// --- CONFIGURACIÓN DE PINES SPI PARA EL RADIO LORA (Bus Interno Heltec) ---
// ============================================================================
#define LORA_CS   8           ///< Chip Select de LoRa SPI
#define LORA_SCK  9           ///< Reloj SCK de LoRa SPI
#define LORA_MOSI 10          ///< Salida de datos MOSI de LoRa SPI
#define LORA_MISO 11          ///< Entrada de datos MISO de LoRa SPI
#define LORA_RST  12          ///< Pin físico de reinicio del transceptor LoRa
#define LORA_BUSY 13          ///< Pin busy de estado del SX1262
#define LORA_DIO1 14          ///< Interrupción física de eventos del SX1262 (DIO1)

// ============================================================================
// --- CONFIGURACIÓN DE PINES SPI PARA LA TARJETA SD (Puerto Secundario HSPI) ---
// ============================================================================
// La reubicación de la SD al bus HSPI resolvió la colisión y bloqueo físico que causaba
// usar el mismo bus compartido del transceptor LoRa con pines diferentes.
#define SD_MISO 4             ///< Entrada de datos MISO para tarjeta SD
#define SD_MOSI 5             ///< Salida de datos MOSI para tarjeta SD
#define SD_SCK  6             ///< Reloj SPI para tarjeta SD
#define SD_CS   7             ///< Chip Select (Habilitador) para tarjeta SD

// ============================================================================
// --- DECLARACIÓN DE PROTOTIPOS DE FUNCIONES ---
// ============================================================================
void sendNetworkLog(String level, String message, String nodeId = "NODE_C");
void updateCommandStatus(int commandId, String status, String response);
void checkPendingCommands();

// ============================================================================
// --- INSTANCIACIÓN DE OBJETOS GLOBALES ---
// ============================================================================
TinyGPSPlus gps;              ///< Decodificador de sentencias GPS NMEA
TFT_eSPI tft = TFT_eSPI();    ///< Controlador de visualización de la pantalla TFT

/**
 * @brief Puntero al bus SPI secundario para la tarjeta SD.
 * Declarado como puntero global para evitar la ejecución prematura del constructor de SPIClass
 * antes de que termine el bootloader del ESP32-S3, solucionando un bootloop silencioso de hardware
 * por inicializaciones estáticas duplicadas de puertos con la librería TFT_eSPI.
 */
SPIClass* spiSD = nullptr; 

/// Transceptor LoRa SX1262 asociado al bus SPI FSPI predeterminado
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ============================================================================
// --- VARIABLES DE ESTADO Y FLUJO ---
// ============================================================================
bool sdMounted = false;       ///< Indica si la tarjeta SD fue montada con éxito
File currentImgFile;          ///< Descriptor de archivo de imagen para lectura
bool isImgFileOpen = false;   ///< Estado de apertura del archivo de imagen

File activeFile;              ///< Descriptor de archivo para escritura de imagen en progreso
bool isFileActive = false;    ///< Estado de sesión de descarga de imagen

uint16_t expectedSeqNum = 0;             ///< Fragmento de secuencia esperado. 0 significa sesión inactiva.
unsigned long lastChunkReceivedTime = 0;  ///< Último milisegundo en que se recibió un fragmento (Timeout check)
bool* chunkReceived = NULL;              ///< Bitmap dinámico en RAM para control de retransmisión selectiva (ARQ)
uint16_t totalChunks = 0;                ///< Número total de fragmentos de la imagen activa
uint8_t missingAttempts = 0;             ///< Contador de intentos de solicitud de fragmentos perdidos
uint32_t imgSize = 0;                    ///< Tamaño neto en bytes de la imagen capturada
int activeImageCommandId = 0;            ///< ID del comando web que solicitó la foto activa
volatile bool isGpsNotFixed = false;     ///< Indica si el nodo sensor respondió que no tiene fix de GPS

unsigned long lastPollTime = 0;          ///< Última vez que se sondeó un nodo
const unsigned long pollInterval = 30000;///< Intervalo de sondeo (30 segundos)
uint8_t targetNode = 1;                  ///< Nodo actual sondeado
bool pollForImage = false;               ///< Bandera alternante para solicitar telemetría o imagen
uint16_t packetSequence = 0;             ///< Secuenciador incremental de paquetes LoRa

enum PollingState {
    POLL_STATE_IDLE,                     ///< Concentrador en reposo
    POLL_STATE_WAITING_RESPONSE          ///< Concentrador esperando ACK o datos del nodo sensor
};
PollingState pollState = POLL_STATE_IDLE;

uint8_t currentAttempt = 0;              ///< Intento actual para la solicitud de red (máximo 3)
unsigned long requestSentTime = 0;       ///< Momento exacto de envío de la solicitud
unsigned long currentTimeout = 3000;     ///< Límite de tiempo dinámico de respuesta
volatile bool responseReceived = false;   ///< Bandera lógica que indica respuesta correcta

// ============================================================================
// --- CLASE OPTIMIZADA: ImagePayloadStream ---
// ============================================================================
/**
 * @class ImagePayloadStream
 * @brief Flujo de transmisión (Stream) para codificar archivos a Base64 en caliente.
 * 
 * Diseñado bajo arquitectura de "Zero-Memory Overhead". En lugar de leer toda la imagen JPG
 * de la SD a la memoria RAM (lo que provocaría un desbordamiento del Heap en el ESP32-S3),
 * esta clase lee bloques secuenciales de la SD de 512 bytes, codifica de 3 en 3 bytes a Base64
 * sobre la marcha a medida que el cliente HTTPClient lee del Stream, e inyecta dinámicamente
 * las cabeceras y cierres del formato JSON.
 */
class ImagePayloadStream : public Stream {
private:
    File _file;              ///< Archivo binario JPG de origen
    String _nodeId;          ///< Identificador formateado del nodo sensor
    size_t _fileSize;        ///< Tamaño real de la imagen en disco
    size_t _base64Size;      ///< Tamaño esperado resultante del cuerpo Base64
    String _prefix;          ///< Cabecera JSON: {"node_id":"...","image_original_base64":"
    String _suffix;          ///< Cierre JSON: "}
    size_t _totalSize;       ///< Tamaño total de la carga HTTP (Content-Length)
    size_t _position;        ///< Posición de lectura del Stream virtual
    
    uint8_t _sdBuf[512];     ///< Búfer intermedio para lecturas eficientes de la SD
    int _sdBufLen;           ///< Datos válidos en el búfer de SD
    int _sdBufPos;           ///< Posición del cursor en el búfer de SD

    uint8_t _buf[3];         ///< Almacenamiento temporal binario de 3 bytes para convertir a Base64
    int _bufLen;             ///< Longitud útil actual de _buf (1 a 3)
    char _b64[4];            ///< Salida codificada de 4 caracteres Base64
    int _b64Pos;             ///< Posición de lectura del bloque de caracteres Base64
    const char* _base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    /**
     * @brief Lee un solo byte del búfer de la SD. Si está agotado, lee un nuevo bloque de 512 bytes.
     * @return Byte leído, o -1 si se llegó al fin de archivo.
     */
    int readSDByte() {
        if (_sdBufPos >= _sdBufLen) {
            _sdBufLen = _file.read(_sdBuf, sizeof(_sdBuf));
            _sdBufPos = 0;
            if (_sdBufLen <= 0) return -1;
        }
        return _sdBuf[_sdBufPos++];
    }

    /**
     * @brief Convierte 3 bytes binarios del búfer a 4 caracteres de texto Base64.
     */
    void encodeBlock() {
        _b64[0] = _base64_table[_buf[0] >> 2];
        _b64[1] = _base64_table[((_buf[0] & 0x03) << 4) | ((_bufLen > 1 ? _buf[1] : 0) >> 4)];
        _b64[2] = (_bufLen > 1) ? _base64_table[((_buf[1] & 0x0f) << 2) | ((_bufLen > 2 ? _buf[2] : 0) >> 6)] : '=';
        _b64[3] = (_bufLen > 2) ? _base64_table[_buf[2] & 0x3f] : '=';
        _b64Pos = 0;
    }

public:
    /**
     * @brief Constructor del Stream.
     * @param file Archivo abierto para lectura.
     * @param nodeId ID del nodo que generó la foto.
     */
    ImagePayloadStream(File file, String nodeId) : _file(file), _nodeId(nodeId) {
        _prefix = "{\"node_id\":\"" + _nodeId + "\",\"image_original_base64\":\"";
        _suffix = "\"}";
        _fileSize = _file.size();
        _base64Size = ((_fileSize + 2) / 3) * 4; 
        _totalSize = _prefix.length() + _base64Size + _suffix.length();
        _position = 0;
        _bufLen = 0;
        _b64Pos = 4; ///< Forzar codificación del bloque inicial
        _sdBufLen = 0;
        _sdBufPos = 0;
    }

    size_t getTotalSize() { return _totalSize; }
    int available() override { return _totalSize - _position; }

    /**
     * @brief Proporciona dinámicamente cada carácter del payload HTTP.
     */
    int read() override {
        if (_position >= _totalSize) return -1;
        int b = -1;
        
        // Cabecera JSON
        if (_position < _prefix.length()) {
            b = _prefix[_position];
        } 
        // Conversión y entrega Base64
        else if (_position < _prefix.length() + _base64Size) {
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
        } 
        // Cierre JSON
        else {
            size_t suffixPos = _position - (_prefix.length() + _base64Size);
            if (suffixPos < _suffix.length()) b = _suffix[suffixPos];
        }
        if (b != -1) _position++;
        return b;
    }

    /**
     * @brief Optimización de lectura en bloque para HTTPClient.
     */
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

/**
 * @brief Retardo inteligente para procesamiento del decodificador GPS.
 */
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    int maxBytes = 50; 
    while (Serial1.available() && maxBytes-- > 0)
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

/**
 * @brief Dibuja la cabecera azul marino del dashboard.
 */
void drawHeader(const char* title) {
    tft.fillRect(0, 0, 160, 15, tft.color565(0, 51, 102)); 
    tft.drawFastHLine(0, 15, 160, tft.color565(200, 200, 200)); 
    tft.setTextColor(TFT_WHITE, tft.color565(0, 51, 102));
    tft.setTextSize(1);
    tft.setCursor((160 - strlen(title) * 6) / 2, 4); 
    tft.print(title);
}

/**
 * @brief Configuración del sistema de hardware y enlace inalámbrico.
 */
void setup() {
  Serial.begin(115200);
  
  // Encendido físico de periféricos
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  delay(100);

  // Inicializar pantalla TFT
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Concentrador Init...");

  // Inicializar receptor GPS
  Serial1.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  // Iniciar bus SPI dedicado y transceptor LoRa
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

  // --- TARJETA SD: CONFIGURACIÓN POR BUS SPI ALTERNATIVO HSPI ---
  // Se instancia dinámicamente para prevenir interferencias con la pantalla durante el arranque
  tft.setTextSize(1);
  spiSD = new SPIClass(HSPI);
  spiSD->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS, *spiSD)) {
    Serial.println("[ERROR] Fallo al montar la tarjeta SD!");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Error: SD Card!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sdMounted = false;
  } else {
    Serial.println("[INFO] Tarjeta SD inicializada correctamente.");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("SD OK!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sdMounted = true;
  }

  // Conexión WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  
  // Dashboard Estático
  tft.fillScreen(TFT_BLACK);
  drawHeader("MONICA CONCENTRADOR");
  
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
  
  tft.setCursor(0, 28);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("IP: ");
  tft.print(WiFi.localIP());
  
  tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
  tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));

  // Entrar en modo escucha de LoRa
  radio.startReceive();
  Serial.println("Concentrador listo y escuchando por LoRa...");
}

/**
 * @brief Envía logs de diagnóstico de red a la API web de MONICA.
 */
void sendNetworkLog(String level, String message, String nodeId) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  
  // Reemplazar la ruta base para apuntar al endpoint de logs de red
  String url = String(API_URL);
  url.replace("/api/sensor-readings", "/api/network/logs");
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  
  StaticJsonDocument<300> doc;
  doc["level"] = level;
  doc["message"] = message;
  doc["node_id"] = nodeId;
  
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  
  http.POST(jsonOutput);
  http.end();
}

/**
 * @brief Envía lecturas de telemetría a la API ambiental en formato JSON.
 */
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

/**
 * @brief Gestiona el flujo de reensamblado e integridad de imágenes recibidas por fragmentos.
 * 
 * Lógica ARQ (Confirmación Selectiva):
 *  1. DATA_IMG_START: Borra archivos antiguos, reserva el bitmap de fragmentos en RAM
 *     y abre el archivo JPG en SD pre-asignando su tamaño total para optimizar la escritura SPI.
 *  2. DATA_IMG_CHUNK: Escribe el fragmento en la ubicación de disco exacta `(seq-1) * 232` y marca el bitmap.
 *  3. DATA_IMG_END: Revisa si hay fragmentos perdidos. Si los hay, responde enviando una lista
 *     de fragmentos faltantes (`CMD_REQ_MISSING`). Si no los hay, cierra el archivo y lo envía a la API.
 */
void handleImageFragment(LoRaPacket* packet, size_t payloadLen) {
    String filename = String("/img_node_") + packet->header.srcId + ".jpg";
    
    tft.fillRect(0, 53, 160, 10, TFT_BLACK);
    tft.setCursor(0, 53);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    
    // --- INICIO DE TRANSMISIÓN DE FOTO ---
    if (packet->header.type == DATA_IMG_START) {
        lastChunkReceivedTime = millis();
        if (isFileActive) {
            activeFile.close();
            isFileActive = false;
        }

        if (payloadLen >= 6) {
            memcpy(&imgSize, packet->payload, 4);
            memcpy(&totalChunks, packet->payload + 4, 2);
            Serial.printf("LoRa RX: DATA_IMG_START recibido. Tamaño: %u bytes, Total Chunks: %u\n", imgSize, totalChunks);
        } else {
            imgSize = 0;
            totalChunks = 0;
            Serial.println("LoRa RX: DATA_IMG_START recibido (sin payload de tamaño)");
        }

        // Alojar bitmap dinámico de control de fragmentos recibidos (máx 2048 chunks)
        if (chunkReceived != NULL) {
            free(chunkReceived);
            chunkReceived = NULL;
        }
        chunkReceived = (bool*)calloc(2048, sizeof(bool));
        expectedSeqNum = 1;
        missingAttempts = 0;
        
        if (SD.exists(filename)) {
            SD.remove(filename);
        }
        
        activeFile = SD.open(filename, FILE_WRITE);
        if (activeFile) {
            isFileActive = true;
            // Pre-asignar espacio contiguo para optimizar velocidad del bus SPI al escribir
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
    // --- RECIBIR FRAGMENTO ---
    else if (packet->header.type == DATA_IMG_CHUNK) {
        uint16_t seq = packet->header.seqNum;
        lastChunkReceivedTime = millis();
        
        // Inicialización de emergencia por si se perdió el paquete de inicio
        if (!isFileActive) {
            Serial.printf("LoRa RX: DATA_IMG_CHUNK seq %d recibido sin START. Inicializando sesión de emergencia...\n", seq);
            if (chunkReceived != NULL) {
                free(chunkReceived);
                chunkReceived = NULL;
            }
            chunkReceived = (bool*)calloc(2048, sizeof(bool));
            totalChunks = 2048; 
            expectedSeqNum = 1;
            missingAttempts = 0;
            
            if (SD.exists(filename)) {
                SD.remove(filename);
            }
            activeFile = SD.open(filename, FILE_WRITE);
            if (activeFile) {
                isFileActive = true;
            }
        }

        // Ignorar duplicados
        if (chunkReceived != NULL && seq < 2048 && chunkReceived[seq]) {
            Serial.printf("LoRa RX: Descartando chunk duplicado seq %d (ya recibido)\n", seq);
            return;
        }

        // Escribir el fragmento en la ubicación de disco exacta (offset)
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
        }
        
        if (seq % 10 == 0 || seq == totalChunks) {
            tft.fillRect(0, 65, 160, 12, TFT_BLACK);
            tft.setCursor(0, 65);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.printf("%u / %u Chunks", (uint32_t)seq, totalChunks);
        }
    }
    // --- FIN DE LA IMAGEN ---
    else if (packet->header.type == DATA_IMG_END) {
        lastChunkReceivedTime = millis();
        Serial.println("LoRa RX: DATA_IMG_END recibido. Verificando integridad de chunks...");

        // Sesión fantasma (ACK duplicado en el aire)
        if (expectedSeqNum == 0) {
            Serial.println("LoRa RX: DATA_IMG_END recibido fuera de sesión activa. Re-enviando ACK final...");
            LoRaPacket ackPacket;
            memset(&ackPacket, 0, sizeof(LoRaHeader));
            ackPacket.header.syncWord[0] = LORA_SYNC_0;
            ackPacket.header.syncWord[1] = LORA_SYNC_1;
            ackPacket.header.srcId = MY_NODE_ID; 
            ackPacket.header.destId = packet->header.srcId;
            ackPacket.header.nextHopId = 1; 
            ackPacket.header.type = ACK;
            ackPacket.header.seqNum = packetSequence++;
            ackPacket.header.ttl = 5;
            
            delay(80); 
            radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
            radio.startReceive();
            return;
        }

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

        // Buscar fragmentos ausentes en el bitmap
        uint16_t missingCount = 0;
        uint16_t firstMissing[98]; 
        
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

        // Solicitar retransmisión (NACK) si faltan fragmentos
        if (missingCount > 0) {
            if (missingAttempts >= 3) {
                Serial.printf("LoRa Integrity: ¡Perdidos %u chunks, pero se alcanzó el límite de 3 intentos! Consolidando imagen con faltantes...\n", missingCount);
                tft.fillRect(0, 53, 160, 10, TFT_BLACK);
                tft.setCursor(0, 53);
                tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                tft.print("Img Incompleta!");
                
                if (activeImageCommandId > 0) {
                    updateCommandStatus(activeImageCommandId, "FAILED", "Imagen incompleta después de 3 reintentos LoRa.");
                    activeImageCommandId = 0;
                }
                
                missingCount = 0; // Forzar flujo de finalización
            } else {
                missingAttempts++;
                Serial.printf("LoRa Integrity: ¡Perdidos %u chunks! Enviando petición de retransmisión CMD_REQ_MISSING (Intento %d/3)...\n", missingCount, missingAttempts);
                
                tft.fillRect(0, 53, 160, 10, TFT_BLACK);
                tft.setCursor(0, 53);
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.printf("NACK: %u (Int %d)", missingCount, missingAttempts);
                
                LoRaPacket nackPacket;
                memset(&nackPacket, 0, sizeof(LoRaHeader));
                nackPacket.header.syncWord[0] = LORA_SYNC_0;
                nackPacket.header.syncWord[1] = LORA_SYNC_1;
                nackPacket.header.srcId = MY_NODE_ID; 
                nackPacket.header.destId = packet->header.srcId;
                nackPacket.header.nextHopId = 1; 
                nackPacket.header.type = CMD_REQ_MISSING;
                nackPacket.header.seqNum = packetSequence++;
                nackPacket.header.ttl = 5;
                
                memcpy(nackPacket.payload, &missingCount, 2);
                memcpy(nackPacket.payload + 2, firstMissing, missingCount * 2);
                
                delay(80); 
                radio.transmit((uint8_t*)&nackPacket, sizeof(LoRaHeader) + 2 + (missingCount * 2));
                radio.startReceive();
                return;
            }
        }

        // Éxito: 100% de la imagen reensamblada correctamente
        Serial.println("LoRa Integrity: ¡100% de fragmentos recibidos con éxito! Enviando ACK final...");
        
        if (activeImageCommandId > 0) {
            updateCommandStatus(activeImageCommandId, "COMPLETED", "Imagen recibida al 100% con éxito.");
            activeImageCommandId = 0;
        }
        
        tft.fillRect(0, 53, 160, 10, TFT_BLACK);
        tft.setCursor(0, 53);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("Reensamblado OK!");

        // Responder confirmación final de recepción
        LoRaPacket ackPacket;
        memset(&ackPacket, 0, sizeof(LoRaHeader));
        ackPacket.header.syncWord[0] = LORA_SYNC_0;
        ackPacket.header.syncWord[1] = LORA_SYNC_1;
        ackPacket.header.srcId = MY_NODE_ID; 
        ackPacket.header.destId = packet->header.srcId;
        ackPacket.header.nextHopId = 1; 
        ackPacket.header.type = ACK;
        ackPacket.header.seqNum = packetSequence++;
        ackPacket.header.ttl = 5;
        
        delay(80); 
        radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
        radio.startReceive();

        expectedSeqNum = 0; 
        
        if (chunkReceived != NULL) {
            free(chunkReceived);
            chunkReceived = NULL;
        }

        if (isFileActive) {
            activeFile.close();
            isFileActive = false;
            Serial.println("Archivo de imagen consolidado y cerrado en SD con éxito.");
        }
        
        // --- SUBIDA HTTP POST CON RETRIES A LA API DE INTELIGENCIA ARTIFICIAL ---
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
                
                // Formatear payload de streaming Base64 en caliente
                char imgNodeFormatted[20];
                sprintf(imgNodeFormatted, "NODE_%03d", packet->header.srcId);
                ImagePayloadStream payloadStream(imgFile, String(imgNodeFormatted));
                
                HTTPClient http;
                http.setTimeout(60000); ///< 60 segundos por si el servidor ejecuta inferencia pesada en CPU
                http.begin(AI_API_URL);
                http.addHeader("Content-Type", "application/json");
                http.addHeader("x-api-key", API_KEY);
                
                // Enviar usando el puntero a la clase Stream personalizada
                httpResponseCode = http.sendRequest("POST", &payloadStream, payloadStream.getTotalSize());
                Serial.printf("Intento %d completado. Código HTTP: %d\n", attempt, httpResponseCode);
                
                http.end(); 
                imgFile.close();
                
                if (httpResponseCode == 200 || httpResponseCode == 201) {
                    tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                    tft.setCursor(0, 60);
                    tft.setTextColor(TFT_GREEN, TFT_BLACK);
                    tft.println("Foto API OK!");
                    break;
                } else {
                    tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                    tft.setCursor(0, 60);
                    tft.setTextColor(TFT_RED, TFT_BLACK);
                    tft.printf("ERR %d (Intentando...)", httpResponseCode);
                }
                
                if (attempt < attempts) {
                    delay(2000); 
                }
            } else {
                Serial.println("Error: No se pudo abrir el archivo guardado para enviar a la API");
                break;
            }
        }
        
        lastPollTime = millis(); 
    }
}

/**
 * @brief Envía comando LoRa para interrogar a un nodo sensor.
 */
void pollNode(uint8_t nodeToPoll, PacketType cmd) {
    LoRaPacket packet;
    memset(&packet, 0, sizeof(LoRaHeader));
    packet.header.syncWord[0] = LORA_SYNC_0;
    packet.header.syncWord[1] = LORA_SYNC_1;
    packet.header.srcId = MY_NODE_ID; 
    packet.header.destId = nodeToPoll;
    packet.header.nextHopId = 1; ///< Enrutamiento lineal: concentrador siempre salta al Nodo 1
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

/**
 * @brief Gestiona la lectura e interpretación de datos recibidos a través de LoRa.
 */
void handleLoRa() {
    if (digitalRead(LORA_DIO1)) { 
        LoRaPacket packet;
        int state = radio.readData((uint8_t*)&packet, sizeof(packet));
        
        if (state == RADIOLIB_ERR_NONE) {
            if (packet.header.syncWord[0] == LORA_SYNC_0 && packet.header.syncWord[1] == LORA_SYNC_1) {
                
                // Ignorar si no va dirigido a este nodo físico como el siguiente salto
                if (packet.header.nextHopId != MY_NODE_ID) {
                    radio.startReceive();
                    return;
                }
                
                if (packet.header.destId == MY_NODE_ID) {
                    Serial.printf("LoRa RX: Src %d, Tipo 0x%02X, Seq %d\n", packet.header.srcId, packet.header.type, packet.header.seqNum);
                    
                    size_t packetLen = radio.getPacketLength();
                    size_t payloadLen = packetLen - sizeof(LoRaHeader);

                    // Telemetría ambiental
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
                    // Fragmento de imagen
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
                    // ACK de toma de foto exitosa
                    else if (packet.header.type == ACK) {
                        tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                        tft.setCursor(0, 42);
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.printf("ACK Nodo %d", packet.header.srcId);
                        
                        if (packet.header.srcId == targetNode) {
                            responseReceived = true; 
                        }
                    }
                    // NACK / Respuesta de error del nodo
                    else if (packet.header.type == NACK) {
                        char payloadBuf[LORA_MAX_PAYLOAD + 1];
                        size_t safeLen = min(payloadLen, (size_t)LORA_MAX_PAYLOAD);
                        memcpy(payloadBuf, packet.payload, safeLen);
                        payloadBuf[safeLen] = '\0';
                        String payloadMsg = String(payloadBuf);
                        
                        Serial.printf("LoRa RX: NACK recibido de Nodo %d. Mensaje: %s\n", packet.header.srcId, payloadMsg.c_str());
                        
                        if (packet.header.srcId == targetNode) {
                            if (payloadMsg == "GPS_NO_FIX") {
                                isGpsNotFixed = true;
                            }
                            responseReceived = true; // Liberar polling de red
                        }
                    }
                }
            }
        }
        radio.startReceive(); 
    }
}

/**
 * @brief Escucha y procesa solicitudes manuales enviadas a través del monitor serie del puerto USB.
 * Comando esperado: "POLL <nodo> <tipo>" (Ej: "POLL 2 T")
 */
void handleSerial() {
  if (Serial.available()) {
    String cmdStr = Serial.readStringUntil('\n');
    cmdStr.trim();
    
    if (cmdStr.startsWith("POLL")) {
      if (expectedSeqNum > 0) {
        Serial.println("[ERROR] Recepción de imagen en progreso. Comando ignorado.");
        return;
      }
      if (pollState != POLL_STATE_IDLE) {
        Serial.println("[ERROR] Concentrador ocupado esperando red. Comando ignorado.");
        return;
      }
      
      int firstSpace = cmdStr.indexOf(' ');
      int secondSpace = cmdStr.indexOf(' ', firstSpace + 1);
      if (firstSpace != -1 && secondSpace != -1) {
        int node = cmdStr.substring(firstSpace + 1, secondSpace).toInt();
        String typeChar = cmdStr.substring(secondSpace + 1);
        typeChar.trim();
        
        if (node >= 1 && node <= 4) {
          bool isImg = (typeChar.equalsIgnoreCase("I"));
          bool isTelem = (typeChar.equalsIgnoreCase("T"));
          
          if (isImg || isTelem) {
            targetNode = node;
            pollForImage = isImg;
            responseReceived = false;
            currentAttempt = 1;
            requestSentTime = millis();
            currentTimeout = pollForImage ? 6000 : 3500;
            pollState = POLL_STATE_WAITING_RESPONSE;
            
            PacketType cmd = pollForImage ? CMD_REQ_IMAGE : CMD_REQ_TELEMETRY;
            Serial.printf("[MANUAL] Iniciando sondeo forzado: Nodo %d, Tipo %s\n", targetNode, pollForImage ? "IMAGEN" : "TELEMETRIA");
            pollNode(targetNode, cmd);
          } else {
            Serial.println("[ERROR] Tipo inválido. Use 'T' para telemetría o 'I' para imagen.");
          }
        } else {
          Serial.println("[ERROR] Nodo inválido (Rango esperado: 1 a 4).");
        }
      } else {
        Serial.println("[ERROR] Formato incorrecto. Formato: POLL <nodo> <tipo> (Ej: POLL 2 T)");
      }
    }
  }
}

/**
 * @brief Actualiza el estado de un comando en la base de datos de la web.
 */
void updateCommandStatus(int commandId, String status, String response) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  
  String url = String(API_URL);
  url.replace("/api/sensor-readings", "/api/network/commands/" + String(commandId));
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  
  StaticJsonDocument<300> doc;
  doc["status"] = status;
  doc["response"] = response;
  
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  
  http.PUT(jsonOutput);
  http.end();
}

/**
 * @brief Consulta la API web por comandos LoRa pendientes y los ejecuta.
 */
void checkPendingCommands() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (expectedSeqNum > 0) return; // No hacer polling de comandos si se está descargando una foto
  
  HTTPClient http;
  String url = String(API_URL);
  url.replace("/api/sensor-readings", "/api/network/commands");
  
  http.begin(url);
  http.addHeader("x-api-key", API_KEY);
  
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc.is<JsonArray>()) {
      JsonArray array = doc.as<JsonArray>();
      if (array.size() > 0) {
        JsonObject cmdObj = array[0];
        int commandId = cmdObj["command_id"];
        String type = cmdObj["type"];
        String targetNodeId = cmdObj["target_node_id"];
        
        Serial.printf("[COMMAND] Recibido de la web: ID %d, Tipo %s, Destino %s\n", commandId, type.c_str(), targetNodeId.c_str());
        sendNetworkLog("INFO", "Recibido comando " + type + " para " + targetNodeId + " desde la web. Ejecutando...", "NODE_C");
        
        updateCommandStatus(commandId, "PROCESSING", "Iniciando transmisión LoRa...");
        
        int nodeNum = 1;
        if (targetNodeId.startsWith("NODE_00")) {
            nodeNum = targetNodeId.substring(7).toInt();
        } else if (targetNodeId.startsWith("NODE_")) {
            nodeNum = targetNodeId.substring(5).toInt();
        }
        
        bool success = false;
        String responseMsg = "";
        
        if (type == "POLL_TELEMETRY") {
            responseReceived = false;
            isGpsNotFixed = false;
            currentAttempt = 1;
            requestSentTime = millis();
            currentTimeout = 3500;
            pollState = POLL_STATE_WAITING_RESPONSE;
            targetNode = nodeNum;
            pollForImage = false;
            
            pollNode(nodeNum, CMD_REQ_TELEMETRY);
            
            unsigned long waitStart = millis();
            while (millis() - waitStart < 4000) {
                smartDelay(10);
                handleLoRa();
                if (responseReceived) {
                    if (isGpsNotFixed) {
                        success = false;
                        responseMsg = "El nodo no tiene señal fija de GPS. Re-intentar más tarde.";
                    } else {
                        success = true;
                        responseMsg = "Telemetría recibida con éxito.";
                    }
                    break;
                }
            }
            if (!success && responseMsg == "") {
                responseMsg = "Timeout: No se recibió respuesta de telemetría LoRa.";
            }
            
            updateCommandStatus(commandId, success ? "COMPLETED" : "FAILED", responseMsg);
            if (success) {
                sendNetworkLog("SUCCESS", "Comando " + type + " ejecutado con éxito: " + responseMsg, targetNodeId);
            } else {
                sendNetworkLog("WARNING", "Fallo al ejecutar comando " + type + ": " + responseMsg, targetNodeId);
            }
        } 
        else if (type == "POLL_IMAGE") {
            responseReceived = false;
            currentAttempt = 1;
            requestSentTime = millis();
            currentTimeout = 6000;
            pollState = POLL_STATE_WAITING_RESPONSE;
            targetNode = nodeNum;
            pollForImage = true;
            
            // Registrar el ID del comando activo para actualizarlo cuando finalice la transferencia del archivo
            activeImageCommandId = commandId;
            
            pollNode(nodeNum, CMD_REQ_IMAGE);
            
            unsigned long waitStart = millis();
            while (millis() - waitStart < 7000) {
                smartDelay(10);
                handleLoRa();
                if (expectedSeqNum > 0 || responseReceived) {
                    success = true;
                    responseMsg = "Disparo exitoso. Descargando imagen...";
                    break;
                }
            }
            if (!success) {
                responseMsg = "Timeout: No se recibió respuesta del nodo para disparar foto.";
                updateCommandStatus(commandId, "FAILED", responseMsg);
                sendNetworkLog("ERROR", "Error ejecutando comando " + type + ": " + responseMsg, targetNodeId);
                activeImageCommandId = 0; // Cancelar comando activo
            } else {
                // Si inició correctamente, se mantiene en PROCESSING y se actualiza al final de la descarga (DATA_IMG_END)
                updateCommandStatus(commandId, "PROCESSING", "Iniciando recepción de fragmentos de imagen...");
                sendNetworkLog("INFO", "Comando " + type + " iniciado con éxito. Descargando chunks...", targetNodeId);
            }
        }
        else if (type == "PING") {
            LoRaPacket packet;
            memset(&packet, 0, sizeof(LoRaHeader));
            packet.header.syncWord[0] = LORA_SYNC_0;
            packet.header.syncWord[1] = LORA_SYNC_1;
            packet.header.srcId = MY_NODE_ID; 
            packet.header.destId = nodeNum;
            packet.header.nextHopId = 1; 
            packet.header.type = CMD_PING;
            packet.header.seqNum = packetSequence++;
            packet.header.ttl = 5;
            
            responseReceived = false;
            targetNode = nodeNum;
            radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader));
            radio.startReceive();
            
            unsigned long waitStart = millis();
            while (millis() - waitStart < 3000) {
                smartDelay(10);
                handleLoRa();
                if (responseReceived) {
                    success = true;
                    responseMsg = "Ping LoRa exitoso. Nodo activo.";
                    break;
                }
            }
            if (!success) {
                responseMsg = "Fallo de conexión: Sin respuesta de ping LoRa.";
            }
            
            updateCommandStatus(commandId, success ? "COMPLETED" : "FAILED", responseMsg);
            if (success) {
                sendNetworkLog("SUCCESS", "Comando " + type + " ejecutado con éxito: " + responseMsg, targetNodeId);
            } else {
                sendNetworkLog("ERROR", "Error ejecutando comando " + type + ": " + responseMsg, targetNodeId);
            }
        }
        else {
            responseMsg = "Comando desconocido o no soportado.";
            updateCommandStatus(commandId, "FAILED", responseMsg);
            sendNetworkLog("ERROR", "Error ejecutando comando " + type + ": " + responseMsg, targetNodeId);
        }
        
        pollState = POLL_STATE_IDLE;
        lastPollTime = millis();
      }
    }
  }
  http.end();
}

/**
 * @brief Bucle principal de ejecución del concentrador.
 */
void loop() {
  static unsigned long lastHeartbeat = 0;
  
  // Heartbeat local de estado en consola cada 5 segundos
  if (millis() - lastHeartbeat > 5000) {
    Serial.println("[HEARTBEAT] Concentrador ejecutando loop...");
    lastHeartbeat = millis();
  }
  
  if (expectedSeqNum == 0) {
    smartDelay(10); 
  } else {
    delay(1); // Evitar bloqueos de GPS para maximizar la velocidad y evitar pérdidas de paquetes LoRa
  }
  handleLoRa();
  handleSerial();

  // Consultar comandos pendientes cada 5 segundos de forma no-bloqueante
  static unsigned long lastCommandCheck = 0;
  if (expectedSeqNum == 0 && (millis() - lastCommandCheck > 5000)) {
      checkPendingCommands();
      lastCommandCheck = millis();
  }
  
  // --- CONTROL DE SEGURIDAD CONTRA SESIÓN COLGADA ---
  if (expectedSeqNum > 0 && (millis() - lastChunkReceivedTime > 60000)) {
      Serial.println("[TIMEOUT] Abortando descarga de imagen por inactividad de 60s.");
      if (isFileActive) {
          activeFile.close();
          isFileActive = false;
      }
      expectedSeqNum = 0;
      if (chunkReceived != NULL) {
          free(chunkReceived);
          chunkReceived = NULL;
      }
      lastPollTime = millis(); 
      
      if (activeImageCommandId > 0) {
          updateCommandStatus(activeImageCommandId, "FAILED", "Timeout de inactividad de 60s esperando fragmentos de imagen.");
          sendNetworkLog("ERROR", "Timeout de inactividad de 60s esperando fragmentos de imagen.", "NODE_C");
          activeImageCommandId = 0;
      }
      
      tft.fillRect(0, 53, 160, 10, TFT_BLACK);
      tft.setCursor(0, 53);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("Img Timeout!");
  }
  
  // --- CONTROL DE MÁQUINA DE ESTADOS DE POLLING ---
  if (expectedSeqNum == 0) {
      if (pollState == POLL_STATE_IDLE) {
          // El polling automático está desactivado por defecto (se realiza de forma manual vía puerto serie)
      }
      else if (pollState == POLL_STATE_WAITING_RESPONSE) {
          if (responseReceived) {
              Serial.printf("LoRa Flow: Respuesta exitosa recibida del Nodo %d en intento %d.\n", targetNode, currentAttempt);
              
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
          // Si expira el tiempo de respuesta, reintentar (máximo 3 veces)
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
                  // Fallar y saltar al siguiente nodo tras agotar intentos
                  Serial.printf("LoRa Flow: [FALLO] Sin respuesta del Nodo %d tras 3 intentos. Pasando al siguiente.\n", targetNode);
                  
                  tft.fillRect(0, 65, 160, 12, TFT_BLACK);
                  tft.setCursor(0, 65);
                  tft.setTextColor(TFT_RED, TFT_BLACK);
                  tft.printf("Err Nodo %d (3 int)", targetNode);
                  
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
