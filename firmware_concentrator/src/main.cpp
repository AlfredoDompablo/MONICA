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
SPIClass spiSD(HSPI); // Secondary SPI bus for SD

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// --- Variables ---
unsigned long lastSensorTime = 0;
unsigned long lastImageTime = 0;
const unsigned long sensorInterval = 10000; // 10 seconds
const unsigned long imageInterval = 30000;  // 60 seconds
bool sdMounted = false;
File currentImgFile;
bool isImgFileOpen = false;

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
            if (_sdBufLen == 0) return -1;
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

    // Override readBytes to avoid Arduino's timedRead() delay which causes false timeouts
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
   // void flush() override {}
    size_t write(uint8_t) override { return 0; }
};

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
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
  tft.println("Iniciando LoRa...");
  int state = radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 1.6, false); // 915MHz, SF9
  if (state == RADIOLIB_ERR_NONE) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("LoRa OK!");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Error LoRa: "); tft.println(state);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // SD Card
  tft.setTextSize(1);
  tft.println("Montando SD...");
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
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
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("=== MONICA CONCENTRADOR ===");
  
  // Estado de Hardware (Fila 1)
  tft.setCursor(0, 10);
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
  tft.println("OK");
  
  // IP (Fila 2)
  tft.setCursor(0, 20);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  
  // Separador (Fila 3)
  tft.setCursor(0, 30);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("---------------------------");

  // Iniciar la escucha LoRa en segundo plano
  radio.startReceive();
  Serial.println("Concentrador listo y escuchando por LoRa...");
}

void sendSensorData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println(String("CONCENTRADOR: ") + NODE_ID);

  if (!gps.location.isValid()) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("\nSin Fix GPS");
      return;
  }

  StaticJsonDocument<400> doc;
  doc["node_id"] = NODE_ID;
  doc["latitude"] = gps.location.lat();
  doc["longitude"] = gps.location.lng();
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Enviando Sensores...");

  // Aquí iría la lógica para recibir datos de los nodos por LoRa.
  // Por ahora, como piloto, enviamos una lectura simulada general.
  doc["ph"] = random(60, 80) / 10.0;
  doc["dissolved_oxygen"] = random(70, 90) / 10.0;
  doc["turbidity"] = random(0, 50);
  doc["conductivity"] = random(400, 600);
  doc["temperature"] = random(200, 300) / 10.0;
  doc["battery_level"] = random(80, 100);

  String jsonOutput;
  serializeJson(doc, jsonOutput);

  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  
  int httpResponseCode = http.POST(jsonOutput);
  
  if (httpResponseCode > 0) {
    tft.println(String("Sensores OK: ") + httpResponseCode);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println(String("Error API: ") + httpResponseCode);
  }
  http.end();
}

void forwardSensorData(uint8_t nodeId, SensorPayload* sp) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  
  StaticJsonDocument<400> doc;
  char nodeFormatted[20];
  sprintf(nodeFormatted, "NODE_%03d", nodeId);
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

  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  
  tft.fillRect(0, 60, 160, 20, TFT_BLACK); // Zona de acción (Fila inferior)
  tft.setCursor(0, 60);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Subiendo HTTP...");
  
  int httpResponseCode = http.POST(jsonOutput);
  if (httpResponseCode == 200 || httpResponseCode == 201) {
    Serial.println(String("Sensor Reenviado OK: ") + httpResponseCode);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("POST OK!");
  } else {
    Serial.println(String("Error reenviando sensor: ") + httpResponseCode);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("POST ERR!");
  }
  http.end();
}

void handleImageFragment(LoRaPacket* packet, size_t payloadLen) {
    String filename = String("/img_node_") + packet->header.nodeId + ".jpg";
    
    tft.fillRect(0, 60, 160, 20, TFT_BLACK); // Zona de acción
    tft.setCursor(0, 60);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    
    if (packet->header.dataType == LORA_TYPE_IMG_START) {
        if (isImgFileOpen) {
            currentImgFile.close();
            isImgFileOpen = false;
        }
        if (SD.exists(filename)) SD.remove(filename);
        currentImgFile = SD.open(filename, FILE_WRITE);
        if (currentImgFile) {
            isImgFileOpen = true;
            if (payloadLen > 0) {
                currentImgFile.write(packet->payload, payloadLen);
            }
        }
        Serial.printf("IMG_START de Nodo %d (Archivo SD Abierto)\n", packet->header.nodeId);
        tft.println("Recibiendo Foto...");
    } 
    else if (packet->header.dataType == LORA_TYPE_IMG_CHUNK) {
        // Recuperación o resincronización limpia en Chunk 1
        if (packet->header.pktIndex == 1) {
            if (isImgFileOpen) {
                currentImgFile.close();
                isImgFileOpen = false;
            }
            if (SD.exists(filename)) SD.remove(filename);
            currentImgFile = SD.open(filename, FILE_WRITE);
            if (currentImgFile) {
                isImgFileOpen = true;
            }
            Serial.println("Sincronia: Archivo recreado en Chunk 1.");
        }
        
        if (isImgFileOpen) {
            currentImgFile.write(packet->payload, payloadLen);
        } else {
            currentImgFile = SD.open(filename, FILE_APPEND);
            if (currentImgFile) {
                isImgFileOpen = true;
                currentImgFile.write(packet->payload, payloadLen);
            }
        }
        Serial.printf("IMG_CHUNK seq %d\n", packet->header.pktIndex);
        tft.print("Chunk: "); tft.println(packet->header.pktIndex);
    }
    else if (packet->header.dataType == LORA_TYPE_IMG_END) {
        if (isImgFileOpen) {
            if (payloadLen > 0) {
                currentImgFile.write(packet->payload, payloadLen);
            }
            currentImgFile.close();
            isImgFileOpen = false;
            Serial.println("Cerrando archivo de imagen en la SD.");
        } else {
            File file = SD.open(filename, FILE_APPEND);
            if (file) {
                file.write(packet->payload, payloadLen);
                file.close();
            }
        }
        
        File imgFile = SD.open(filename, FILE_READ);
        if (imgFile) {
            size_t finalSize = imgFile.size();
            Serial.printf("IMG_END. Reensamblado completo! Tamaño SD: %u bytes. Enviando a API...\n", finalSize);
            
            // DEBUG: Imprimir los primeros 32 bytes de la SD en HEX
            uint8_t temp[32];
            size_t bytesRead = imgFile.read(temp, 32);
            Serial.printf("DEBUG HEX SD FILE: ");
            for (size_t i = 0; i < bytesRead; i++) {
                Serial.printf("%02X ", temp[i]);
            }
            Serial.println();
            imgFile.seek(0); // CRITICO: Regresar el cursor al inicio para la transmisión
            
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.println("Subiendo IA API...");
            char imgNodeFormatted[20];
            sprintf(imgNodeFormatted, "NODE_%03d", packet->header.nodeId);
            ImagePayloadStream payloadStream(imgFile, String(imgNodeFormatted));
            HTTPClient http;
            http.setTimeout(30000);
            http.begin(AI_API_URL);
            http.addHeader("Content-Type", "application/json");
            http.addHeader("x-api-key", API_KEY);
            int httpResponseCode = http.sendRequest("POST", &payloadStream, payloadStream.getTotalSize());
            Serial.printf("Envio a IA API completado. Codigo: %d\n", httpResponseCode);
            
            tft.fillRect(0, 60, 160, 20, TFT_BLACK);
            tft.setCursor(0, 60);
            if(httpResponseCode == 200 || httpResponseCode == 201) {
              tft.setTextColor(TFT_GREEN, TFT_BLACK);
              tft.println("Foto API OK!");
            } else {
              tft.setTextColor(TFT_RED, TFT_BLACK);
              tft.printf("ERR API: %d\n", httpResponseCode);
            }
            imgFile.close();
        }
    }
}

void handleLoRa() {
    if (digitalRead(LORA_DIO1)) { 
        LoRaPacket packet;
        int state = radio.readData((uint8_t*)&packet, sizeof(packet));
        
        if (state == RADIOLIB_ERR_NONE) {
            if (packet.header.syncWord[0] == LORA_SYNC_0 && packet.header.syncWord[1] == LORA_SYNC_1) {
                Serial.printf("LoRa RX: Nodo %d, Tipo 0x%02X, Seq %d\n", packet.header.nodeId, packet.header.dataType, packet.header.pktIndex);
                
                size_t packetLen = radio.getPacketLength();
                size_t payloadLen = packetLen - sizeof(LoRaHeader);

                if (packet.header.dataType == LORA_TYPE_SENSOR) {
                    tft.fillRect(0, 40, 160, 15, TFT_BLACK); // Zona de Eventos
                    tft.setCursor(0, 40);
                    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                    tft.printf("RX: SENSOR (%d)\n", packet.header.nodeId);
                    
                    SensorPayload* sp = (SensorPayload*)packet.payload;
                    forwardSensorData(packet.header.nodeId, sp);
                }
                else if (packet.header.dataType >= LORA_TYPE_IMG_START && packet.header.dataType <= LORA_TYPE_IMG_END) {
                    if(packet.header.dataType == LORA_TYPE_IMG_START) {
                       tft.fillRect(0, 40, 160, 15, TFT_BLACK); // Zona de Eventos
                       tft.setCursor(0, 40);
                       tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                       tft.printf("RX: IMG (%d)\n", packet.header.nodeId);
                    }
                    handleImageFragment(&packet, payloadLen);
                }
            }
        }
        radio.startReceive(); 
    }
}

void loop() {
  smartDelay(10); 
  handleLoRa();
}
