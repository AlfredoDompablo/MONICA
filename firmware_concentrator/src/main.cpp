#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include "secrets.h"

// --- Hardware Definitions ---
#define VEXT_PIN 3        
#define BACKLIGHT_PIN 21  
#define GNSS_RX_PIN 33    
#define GNSS_TX_PIN 34    

// --- SD Card SPI Pins (Custom) ---
#define SD_MISO 4
#define SD_MOSI 5
#define SD_SCK  6
#define SD_CS   7

// --- Objects ---
TinyGPSPlus gps;
TFT_eSPI tft = TFT_eSPI(); 
SPIClass spiSD(FSPI); // Use an alternative SPI bus for SD

// --- Variables ---
unsigned long lastSensorTime = 0;
unsigned long lastImageTime = 0;
const unsigned long sensorInterval = 10000; // 10 seconds
const unsigned long imageInterval = 30000;  // 60 seconds

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

  // SD Card
  tft.setTextSize(1);
  tft.println("Montando SD...");
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("Fallo al montar SD!");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Error: SD Card!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    Serial.println("SD Inicializada.");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("SD OK!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("WiFi OK!");
  tft.println(WiFi.localIP());
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

void sendImageData() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  File file = SD.open("/test.jpeg", FILE_READ);
  if (!file) {
    Serial.println("No se encontro test.jpg en SD");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Sin imagen (test.jpeg)");
    return;
  }

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Enviando Imagen...");
  Serial.println("Enviando Imagen...");

  HTTPClient http;
  http.setTimeout(30000); // 30 segundos de timeout para imágenes
  http.begin(AI_API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);

  // Instanciar Stream con la imagen
  ImagePayloadStream payloadStream(file, NODE_ID);

  Serial.print("Enviando Payload de: ");
  Serial.print(payloadStream.getTotalSize());
  Serial.println(" bytes");
  tft.println(String(payloadStream.getTotalSize() / 1024) + " KB");

  // Enviar el Stream (Chunked Encoding transparente)
  int httpResponseCode = http.sendRequest("POST", &payloadStream, payloadStream.getTotalSize());

  if (httpResponseCode > 0) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println(String("Imagen OK: ") + httpResponseCode);
    Serial.println(String("Imagen enviada con exito. Codigo: ") + httpResponseCode);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println(String("Error Imagen: ") + httpResponseCode);
    Serial.println(String("Fallo envio imagen. Codigo: ") + httpResponseCode);
  }

  file.close();
  http.end();
}

void loop() {
  smartDelay(100); 
  
  if (millis() - lastSensorTime > sensorInterval) {
    sendSensorData();
    lastSensorTime = millis();
  }

  if (millis() - lastImageTime > imageInterval) {
    sendImageData();
    lastImageTime = millis();
  }
}
