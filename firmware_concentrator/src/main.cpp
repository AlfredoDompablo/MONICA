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

// --- Definiciones de Hardware del Concentrador ---
#define MY_NODE_ID 0          // Identificador fijo e inalterable del Concentrador Central.

#define VEXT_PIN 3            // Control del suministro de energía a la pantalla y periféricos.
#define BACKLIGHT_PIN 21      // Pin de control del brillo de la pantalla TFT.
#define GNSS_RX_PIN 33        // Pin de recepción UART para el receptor GNSS/GPS.
#define GNSS_TX_PIN 34        // Pin de transmisión UART para el receptor GNSS/GPS.

// --- Pines del Bus SPI para el Transceptor LoRa (Internos de Heltec) ---
#define LORA_CS   8           // Chip Select de LoRa.
#define LORA_SCK  9           // Reloj de bus LoRa SPI.
#define LORA_MOSI 10          // Salida de datos LoRa SPI.
#define LORA_MISO 11          // Entrada de datos LoRa SPI.
#define LORA_RST  12          // Pin de reinicio LoRa.
#define LORA_BUSY 13          // Pin ocupado LoRa.
#define LORA_DIO1 14          // Pin de interrupción de eventos LoRa.

// --- Pines del Bus SPI Dedicado para Tarjeta SD (Puerto Secundario HSPI) ---
// La reubicación de la SD al bus HSPI resolvió la colisión y bloqueo físico que causaba
// usar el mismo bus compartido del transceptor LoRa con pines diferentes.
#define SD_MISO 4             // Master In Slave Out para SD.
#define SD_MOSI 5             // Master Out Slave In para SD.
#define SD_SCK  6             // Reloj SPI para SD.
#define SD_CS   7             // Chip Select para SD.

// --- Instanciación de Objetos y Punteros de Buses Dinámicos ---
TinyGPSPlus gps;             // Decodificador de ráfagas GPS.
TFT_eSPI tft = TFT_eSPI();   // Controlador gráfico del display LCD.

/**
 * Puntero al bus SPI secundario para la tarjeta SD.
 * Declarado como puntero global para evitar la ejecución prematura del constructor de SPIClass
 * antes de que termine el bootloader del ESP32-S3, solucionando un bootloop silencioso de hardware
 * por inicializaciones estáticas duplicadas de puertos con la librería TFT_eSPI.
 */
SPIClass* spiSD = nullptr; 

// Controlador de radio LoRa
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// --- Variables de Estado para la Tarjeta SD ---
bool sdMounted = false;       // Bandera de control de montaje exitoso de la tarjeta SD.
File currentImgFile;          // Descriptor de archivo para lectura.
bool isImgFileOpen = false;   // Indicador de estado de apertura de archivo de lectura.

File activeFile;              // Descriptor de archivo activo en el que se graban los chunks de imagen.
bool isFileActive = false;    // Indicador de sesión de escritura de streaming en progreso.

// --- Variables del Búfer de Streaming y Fragmentación ---
uint16_t expectedSeqNum = 0;           // 0 indica inactividad. >= 1 indica índice del fragmento esperado.
unsigned long lastChunkReceivedTime = 0; // Marca de tiempo (millis) para la prevención y timeouts de sesión colgada.
bool* chunkReceived = NULL;            // Bitmap en RAM dinámico para el control selectivo de pérdidas (NACK).
uint16_t totalChunks = 0;              // Cantidad total esperada de fragmentos de imagen.
uint32_t imgSize = 0;                  // Tamaño total en bytes de la imagen capturada.

// --- Máquina de Estados del Polling Periódico y Fiabilidad ---
unsigned long lastPollTime = 0;        // Marca de tiempo del último ciclo de muestreo.
const unsigned long pollInterval = 30000; // Intervalo de polling por defecto (30 segundos).
uint8_t targetNode = 1;                // Nodo objetivo actual de la red lineal (1 al 4).
bool pollForImage = false;             // Bandera alternante para solicitar telemetría o disparo de cámara.
uint16_t packetSequence = 0;           // Secuencia incremental de paquetes de comando salientes.

enum PollingState {
    POLL_STATE_IDLE,                   // Estado de espera pasiva entre intervalos.
    POLL_STATE_WAITING_RESPONSE        // Esperando respuesta LoRa o ACK del nodo sensor.
};
PollingState pollState = POLL_STATE_IDLE;

uint8_t currentAttempt = 0;            // Contador de intentos del comando actual (máximo 3).
unsigned long requestSentTime = 0;     // Registro del momento en que se transmitió la solicitud.
unsigned long currentTimeout = 3000;   // Timeout dinámico de respuesta (3.5s para telemetría, 6s para fotos).
volatile bool responseReceived = false; // Bandera de interrupción lógica que indica recepción exitosa.

/**
 * @class ImagePayloadStream
 * @brief Clase de flujo personalizada (Stream) para codificar imágenes a Base64 en caliente.
 * 
 * Esta clase es una de las mayores optimizaciones del Concentrador. Hereda de Stream, lo que permite
 * al cliente HTTPClient de ESP32 leer datos de ella bajo demanda para la petición POST.
 * 
 * En lugar de leer toda la imagen JPG desde la SD a la RAM, codificarla en Base64 en RAM (lo cual
 * consumiría hasta 3 veces el tamaño del archivo y provocaría un colapso por falta de memoria Heap),
 * esta clase lee bloques secuenciales de la SD de 512 bytes, los codifica a Base64 bajo demanda de 3 en 3 bytes,
 * e inyecta dinámicamente los metadatos JSON al inicio (`prefix`) y fin (`suffix`) del stream.
 */
class ImagePayloadStream : public Stream {
private:
    File _file;              // Archivo JPG físico en SD a leer.
    String _nodeId;          // ID del nodo formateado para el campo JSON.
    size_t _fileSize;        // Tamaño original del archivo binario.
    size_t _base64Size;      // Tamaño resultante calculado de la sección Base64.
    String _prefix;          // Estructura JSON inicial: {"node_id":"NODE_X","image_original_base64":"
    String _suffix;          // Cierre del JSON: "}
    size_t _totalSize;       // Tamaño total de la carga a reportar a HTTP (Content-Length).
    size_t _position;        // Posición actual leída del stream general.
    
    // Búfer intermedio para evitar lecturas lentas byte a byte de la SD
    uint8_t _sdBuf[512];     
    int _sdBufLen;           
    int _sdBufPos;           

    uint8_t _buf[3];         // Búfer temporal de 3 bytes binarios para la codificación Base64.
    int _bufLen;             
    char _b64[4];            // Bloque codificado de 4 caracteres Base64 resultantes.
    int _b64Pos;             
    const char* _base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    /**
     * @brief Lee un único byte del búfer interno de la tarjeta SD. Si el búfer está vacío,
     * lee un nuevo bloque de 512 bytes desde el disco.
     * @return El byte leído o -1 si llegó al fin del archivo.
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
     * @brief Codifica los 3 bytes binarios en _buf a 4 caracteres de texto Base64
     * y los almacena en el búfer _b64.
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
     * @brief Constructor del Stream de Imagen en Base64.
     * @param file Archivo abierto en modo lectura en SD.
     * @param nodeId ID formateado del nodo de origen.
     */
    ImagePayloadStream(File file, String nodeId) : _file(file), _nodeId(nodeId) {
        _prefix = "{\"node_id\":\"" + _nodeId + "\",\"image_original_base64\":\"";
        _suffix = "\"}";
        _fileSize = _file.size();
        _base64Size = ((_fileSize + 2) / 3) * 4; // Cálculo de la longitud final matemática de Base64
        _totalSize = _prefix.length() + _base64Size + _suffix.length();
        _position = 0;
        _bufLen = 0;
        _b64Pos = 4; // Forzar codificación inicial del primer bloque
        _sdBufLen = 0;
        _sdBufPos = 0;
    }

    size_t getTotalSize() { return _totalSize; }
    int available() override { return _totalSize - _position; }

    /**
     * @brief Proporciona secuencialmente cada carácter del stream bajo demanda del motor de red HTTP.
     * @return El byte/carácter leído del JSON virtual en Base64, o -1 al finalizar.
     */
    int read() override {
        if (_position >= _totalSize) return -1;
        int b = -1;
        
        // Etapa 1: Inyectar la cabecera JSON
        if (_position < _prefix.length()) {
            b = _prefix[_position];
        } 
        // Etapa 2: Codificar y proveer el cuerpo en Base64 dinámicamente
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
        // Etapa 3: Inyectar el cierre JSON
        else {
            size_t suffixPos = _position - (_prefix.length() + _base64Size);
            if (suffixPos < _suffix.length()) b = _suffix[suffixPos];
        }
        if (b != -1) _position++;
        return b;
    }

    /**
     * @brief Optimización para lecturas por bloques por parte de HTTPClient.
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
 * @brief Retardo inteligente que procesa y decodifica las ráfagas GPS en segundo plano.
 * @param ms Tiempo en milisegundos.
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
 * @brief Dibuja la barra de título premium del concentrador.
 * @param title Título a renderizar.
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
 * @brief Configuración inicial del concentrador.
 */
void setup() {
  Serial.begin(115200);
  
  // Encendido de sistemas de alimentación regulados de la placa
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  delay(100);

  // Inicialización de la Pantalla TFT
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Concentrador Init...");

  // GPS
  Serial1.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  // Iniciar bus SPI dedicado a LoRa SX1262
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
  // Se instancia dinámicamente con `new` dentro de `setup()` para prevenir interferencias
  // con la configuración inicial de pantalla del bootloader, eliminando el bootloop de hardware.
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

  // Configuración de enlace WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  
  // Renderizado del Dashboard estático
  tft.fillScreen(TFT_BLACK);
  drawHeader("MONICA CONCENTRADOR");
  
  // Mostrar estado de la SD y conexión (Y = 18)
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
  
  // Mostrar IP local obtenida por DHCP (Y = 28)
  tft.setCursor(0, 28);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("IP: ");
  tft.print(WiFi.localIP());
  
  // Separadores fijos de la interfaz premium anti-solapamiento
  tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
  tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));

  // Poner el transceptor en modo recepción continua en segundo plano
  radio.startReceive();
  Serial.println("Concentrador listo y escuchando por LoRa...");
}

/**
 * @brief Formatea la telemetría recibida a JSON y la envía por HTTP POST a la API final.
 * @param srcId Nodo emisor de origen.
 * @param sp Puntero al bloque estructurado de lecturas en RAM.
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

  http.setTimeout(15000); // 15s de gracia
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
 * @brief Administrador y procesador del flujo de imágenes LoRa por fragmentos.
 * @param packet Puntero al paquete LoRa recibido.
 * @param payloadLen Longitud neta del payload.
 * 
 * Lógica ARQ / NACK Selectivo:
 *  1. Al recibir DATA_IMG_START: Abre/recrea un archivo físico en la SD y reserva un bitmap dinámico
 *     de 2048 booleanos en RAM para controlar qué fragmentos han sido escritos. Pre-asigna espacio físico
 *     en disco para evitar retardos de asignación en tiempo de escritura.
 *  2. Al recibir DATA_IMG_CHUNK: Escribe el payload en el offset exacto `(seq - 1) * LORA_MAX_PAYLOAD` 
 *     en la SD y marca el bitmap.
 *  3. Al recibir DATA_IMG_END: Recorre el bitmap para identificar si faltan bloques.
 *     - Si faltan: Envía CMD_REQ_MISSING con la lista de índices perdidos (NACK).
 *     - Si está completo: Cierra el archivo y ejecuta una subida robusta a la API de Inteligencia Artificial.
 */
void handleImageFragment(LoRaPacket* packet, size_t payloadLen) {
    String filename = String("/img_node_") + packet->header.srcId + ".jpg";
    
    tft.fillRect(0, 53, 160, 10, TFT_BLACK);
    tft.setCursor(0, 53);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    
    // --- LOTE INICIAL DE TRANSMISIÓN ---
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

        // Alojar dinámicamente el bitmap de control en RAM (límite base de 2048 fragmentos, ~409KB)
        if (chunkReceived != NULL) {
            free(chunkReceived);
            chunkReceived = NULL;
        }
        chunkReceived = (bool*)calloc(2048, sizeof(bool));
        expectedSeqNum = 1;
        
        if (SD.exists(filename)) {
            SD.remove(filename);
        }
        
        // Abrir archivo en SD para escritura rápida
        activeFile = SD.open(filename, FILE_WRITE);
        if (activeFile) {
            isFileActive = true;
            // Pre-asignar espacio contiguo en disco para optimizar velocidad de bus SPI
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
    // --- LOTE DE FRAGMENTOS ---
    else if (packet->header.type == DATA_IMG_CHUNK) {
        uint16_t seq = packet->header.seqNum;
        lastChunkReceivedTime = millis();
        
        // Inicialización de resguardo si se perdió el START inicial por interferencias
        if (!isFileActive) {
            Serial.printf("LoRa RX: DATA_IMG_CHUNK seq %d recibido sin START. Inicializando sesión de emergencia...\n", seq);
            if (chunkReceived != NULL) {
                free(chunkReceived);
                chunkReceived = NULL;
            }
            chunkReceived = (bool*)calloc(2048, sizeof(bool));
            totalChunks = 2048; 
            expectedSeqNum = 1;
            
            if (SD.exists(filename)) {
                SD.remove(filename);
            }
            activeFile = SD.open(filename, FILE_WRITE);
            if (activeFile) {
                isFileActive = true;
                Serial.println("SD: Archivo abierto dinámicamente en modo de emergencia.");
            }
        }

        // Descartar de forma segura fragmentos duplicados por retransmisiones tardías
        if (chunkReceived != NULL && seq < 2048 && chunkReceived[seq]) {
            Serial.printf("LoRa RX: Descartando chunk duplicado seq %d (ya recibido)\n", seq);
            return;
        }

        // Escribir el fragmento en la dirección de disco física exacta del archivo
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
        
        tft.fillRect(0, 65, 160, 12, TFT_BLACK);
        tft.setCursor(0, 65);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("%u / %u Chunks", (uint32_t)seq, totalChunks);
    }
    // --- LOTE DE CONFIRMACIÓN Y CIERRE ---
    else if (packet->header.type == DATA_IMG_END) {
        lastChunkReceivedTime = millis();
        Serial.println("LoRa RX: DATA_IMG_END recibido. Verificando integridad de chunks...");

        // Caso excepcional: El nodo retransmite el END porque el ACK final en el aire se perdió
        if (expectedSeqNum == 0) {
            Serial.println("LoRa RX: DATA_IMG_END recibido fuera de sesión activa. Re-enviando ACK final...");
            LoRaPacket ackPacket;
            memset(&ackPacket, 0, sizeof(LoRaHeader));
            ackPacket.header.syncWord[0] = LORA_SYNC_0;
            ackPacket.header.syncWord[1] = LORA_SYNC_1;
            ackPacket.header.srcId = MY_NODE_ID; // 0
            ackPacket.header.destId = packet->header.srcId;
            ackPacket.header.nextHopId = 1; // Primer salto es siempre el Nodo 1
            ackPacket.header.type = ACK;
            ackPacket.header.seqNum = packetSequence++;
            ackPacket.header.ttl = 5;
            
            delay(80); // Margen para batería
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

        // Escanear el bitmap buscando fragmentos perdidos
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

        // --- PROTOCOLO ARQ: SOLICITUD NACK DE RE-TRANSMISIÓN ---
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
            nackPacket.header.srcId = MY_NODE_ID; 
            nackPacket.header.destId = packet->header.srcId;
            nackPacket.header.nextHopId = 1; 
            nackPacket.header.type = CMD_REQ_MISSING;
            nackPacket.header.seqNum = packetSequence++;
            nackPacket.header.ttl = 5;
            
            memcpy(nackPacket.payload, &missingCount, 2);
            memcpy(nackPacket.payload + 2, firstMissing, missingCount * 2);
            
            delay(80); // Margen para transceptor receptor
            radio.transmit((uint8_t*)&nackPacket, sizeof(LoRaHeader) + 2 + (missingCount * 2));
            radio.startReceive();
            return;
        }

        // --- ÉXITO DE INTEGRIDAD: 100% REENSAMBLADO ---
        Serial.println("LoRa Integrity: ¡100% de fragmentos recibidos con éxito! Enviando ACK final...");
        
        tft.fillRect(0, 53, 160, 10, TFT_BLACK);
        tft.setCursor(0, 53);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("Reensamblado OK!");

        // Responder confirmación final
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

        // Guardar y consolidar archivo físico en la SD
        if (isFileActive) {
            activeFile.close();
            isFileActive = false;
            Serial.println("Archivo de imagen consolidado y cerrado en SD con éxito.");
        }
        
        // --- ENVÍO HTTP POST ROBUESTO A LA API DE IA (3 INTENTOS) ---
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
                
                // Inicializar flujo de codificación en Base64 en caliente
                char imgNodeFormatted[20];
                sprintf(imgNodeFormatted, "NODE_%03d", packet->header.srcId);
                ImagePayloadStream payloadStream(imgFile, String(imgNodeFormatted));
                
                HTTPClient http;
                http.setTimeout(60000); // 60 segundos (permite inferencias lentas de IA en CPU remota)
                http.begin(AI_API_URL);
                http.addHeader("Content-Type", "application/json");
                http.addHeader("x-api-key", API_KEY);
                
                // Enviar petición HTTP POST inyectando el Stream dinámico
                httpResponseCode = http.sendRequest("POST", &payloadStream, payloadStream.getTotalSize());
                Serial.printf("Intento %d completado. Código HTTP: %d\n", attempt, httpResponseCode);
                
                http.end(); 
                imgFile.close();
                
                if (httpResponseCode == 200 || httpResponseCode == 201) {
                    tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                    tft.setCursor(0, 60);
                    tft.setTextColor(TFT_GREEN, TFT_BLACK);
                    tft.println("Foto API OK!");
                    break; // Subida completada
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
        
        lastPollTime = millis(); // Reset de temporizador de polling
    }
}

/**
 * @brief Transmite una petición de comando LoRa estructurado a un nodo sensor remoto.
 * @param nodeToPoll Nodo a interrogar.
 * @param cmd Tipo de comando (solicitud de telemetría o imagen).
 */
void pollNode(uint8_t nodeToPoll, PacketType cmd) {
    LoRaPacket packet;
    memset(&packet, 0, sizeof(LoRaHeader));
    packet.header.syncWord[0] = LORA_SYNC_0;
    packet.header.syncWord[1] = LORA_SYNC_1;
    packet.header.srcId = MY_NODE_ID; 
    packet.header.destId = nodeToPoll;
    
    // Ruteo Lineal: El concentrador (0) siempre tiene como salto físico inicial al Nodo 1 directo.
    packet.header.nextHopId = 1; 
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
 * @brief Gestiona la escucha e interpretación del tráfico LoRa entrante para el Concentrador.
 */
void handleLoRa() {
    if (digitalRead(LORA_DIO1)) { 
        LoRaPacket packet;
        int state = radio.readData((uint8_t*)&packet, sizeof(packet));
        
        if (state == RADIOLIB_ERR_NONE) {
            if (packet.header.syncWord[0] == LORA_SYNC_0 && packet.header.syncWord[1] == LORA_SYNC_1) {
                
                // Filtro de salto físico crítico
                if (packet.header.nextHopId != MY_NODE_ID) {
                    radio.startReceive();
                    return;
                }
                
                if (packet.header.destId == MY_NODE_ID) {
                    Serial.printf("LoRa RX: Src %d, Tipo 0x%02X, Seq %d\n", packet.header.srcId, packet.header.type, packet.header.seqNum);
                    
                    size_t packetLen = radio.getPacketLength();
                    size_t payloadLen = packetLen - sizeof(LoRaHeader);

                    // Procesar telemetría ambiental
                    if (packet.header.type == DATA_TELEMETRY) {
                        tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                        tft.setCursor(0, 42);
                        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                        tft.printf("RX: SENSOR (%d)", packet.header.srcId);
                        
                        SensorPayload* sp = (SensorPayload*)packet.payload;
                        forwardSensorData(packet.header.srcId, sp);
                        
                        if (packet.header.srcId == targetNode && !pollForImage) {
                            responseReceived = true; // Liberar máquina de estados de polling
                        }
                    }
                    // Procesar fragmentos de cámara JPG
                    else if (packet.header.type >= DATA_IMG_START && packet.header.type <= DATA_IMG_END) {
                        if(packet.header.type == DATA_IMG_START) {
                           tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                           tft.setCursor(0, 42);
                           tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                           tft.printf("RX: IMG (%d)", packet.header.srcId);
                           
                           if (packet.header.srcId == targetNode && pollForImage) {
                               responseReceived = true; // Liberar máquina de estados de polling
                           }
                        }
                        handleImageFragment(&packet, payloadLen);
                    }
                    // Procesar confirmaciones (ACK) de disparo de cámara
                    else if (packet.header.type == ACK) {
                        tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                        tft.setCursor(0, 42);
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.printf("ACK Nodo %d", packet.header.srcId);
                        
                        if (packet.header.srcId == targetNode && pollForImage) {
                            responseReceived = true; // Liberar máquina de estados de polling
                        }
                    }
                }
            }
        }
        radio.startReceive(); 
    }
}
/**
 * @brief Escucha y procesa comandos manuales ingresados por el puerto serie USB.
 * 
 * Permite al usuario forzar el muestreo de un nodo y tipo de dato específico desde su PC.
 * Formato del comando esperado: "POLL <nodo> <tipo>"
 *   - <nodo>: ID de nodo (1 al 4).
 *   - <tipo>: 'T' para telemetría, 'I' para disparar cámara y obtener imagen.
 *   Ejemplo: "POLL 2 T" o "POLL 4 I"
 */
void handleSerial() {
  if (Serial.available()) {
    String cmdStr = Serial.readStringUntil('\n');
    cmdStr.trim();
    
    if (cmdStr.startsWith("POLL")) {
      if (expectedSeqNum > 0) {
        Serial.println("[ERROR MANUAL] Recepción de imagen en progreso. Comando omitido.");
        return;
      }
      if (pollState != POLL_STATE_IDLE) {
        Serial.println("[ERROR MANUAL] Concentrador ocupado esperando respuesta de red. Comando omitido.");
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
            Serial.println("[ERROR MANUAL] Tipo de dato inválido. Use 'T' para telemetría o 'I' para imagen.");
          }
        } else {
          Serial.println("[ERROR MANUAL] ID de nodo inválido. Rango permitido: 1 a 4.");
        }
      } else {
        Serial.println("[ERROR MANUAL] Formato de comando incorrecto. Use: POLL <nodo> <tipo> (Ej: POLL 2 T)");
      }
    }
  }
}

/**
 * @brief Loop de ejecución principal y control de máquinas de estado del Concentrador.
 */
void loop() {
  static unsigned long lastHeartbeat = 0;
  
  // Latido local cada 5 segundos
  if (millis() - lastHeartbeat > 5000) {
    Serial.println("[HEARTBEAT] Concentrador ejecutando loop...");
    lastHeartbeat = millis();
  }
  
  smartDelay(10); 
  handleLoRa();
  handleSerial();
  
  // --- CONTROL DE SEGURIDAD (TIMEOUT) PARA SESIÓN DE IMAGEN ---
  // Si la transmisión de una foto queda colgada por más de 60 segundos (p. ej., porque se apagó
  // el nodo emisor o perdió energía), abortar y liberar recursos de RAM de forma limpia.
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
      lastPollTime = millis(); // Reset de retardo de muestreo
      
      tft.fillRect(0, 53, 160, 10, TFT_BLACK);
      tft.setCursor(0, 53);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("Img Timeout!");
  }
  
  // --- MÁQUINA DE ESTADOS DE POLLING DE BAJO TIEMPO DE RESPUESTA CON 3 INTENTOS ---
  // Solo iniciar un nuevo muestreo si no nos encontramos en medio de una recepción activa de imagen
  if (expectedSeqNum == 0) {
      // Estado de espera completado: iniciar un nuevo muestreo (DESACTIVADO: ahora es 100% manual por puerto serie)
      if (pollState == POLL_STATE_IDLE) {
          /*
          if (millis() - lastPollTime > pollInterval) {
              responseReceived = false;
              currentAttempt = 1;
              requestSentTime = millis();
              currentTimeout = pollForImage ? 6000 : 3500; // Timeout dinámico
              pollState = POLL_STATE_WAITING_RESPONSE;
              
              PacketType cmd = pollForImage ? CMD_REQ_IMAGE : CMD_REQ_TELEMETRY;
              pollNode(targetNode, cmd);
          }
          */
      }
      // Estado en espera de respuesta: evaluar timeouts e intentos
      else if (pollState == POLL_STATE_WAITING_RESPONSE) {
          if (responseReceived) {
              Serial.printf("LoRa Flow: Respuesta exitosa recibida del Nodo %d en intento %d.\n", targetNode, currentAttempt);
              
              // Alternar la lógica de muestreo
              if (pollForImage) {
                  targetNode++;
                  if (targetNode > 4) targetNode = 1; // Regresar al Nodo 1 tras culminar Nodo 4
                  pollForImage = false;
              } else {
                  pollForImage = true;
              }
              
              pollState = POLL_STATE_IDLE;
              lastPollTime = millis();
          }
          // Timeout detectado en el intento actual
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
                  // Fallo total tras 3 intentos: saltar al siguiente nodo para no colgar el polling
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
