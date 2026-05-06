#include <Arduino.h>
#include <TinyGPS++.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <RadioLib.h>
#include "lora_protocol.h"

// --- Identificador Único de este Nodo Sensor ---
#define MY_NODE_ID 1

// --- Hardware Definitions ---
#define VEXT_PIN 3        
#define BACKLIGHT_PIN 21  
#define GNSS_RX_PIN 33    
#define GNSS_TX_PIN 34    

// --- LoRa SPI Pins ---
#define LORA_CS   8
#define LORA_SCK  9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14

// --- Pines para ESP32-CAM (Borde) ---
#define CAM_UART_TX 17
#define CAM_UART_RX 18
#define CAM_EN      6

// --- Objects ---
TinyGPSPlus gps;
TFT_eSPI tft = TFT_eSPI(); 
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
HardwareSerial camSerial(2); // Usar UART2 (UART1 ya lo usa el GPS)

// --- Variables ---
unsigned long lastSensorTime = 0;
const unsigned long sensorInterval = 30000; // 30s
uint16_t packetSequence = 0;

// --- Cache para Repetidor en Topología Lineal ---
struct SeenPacket {
    uint8_t nodeId;
    uint8_t dataType;
    uint16_t pktIndex;
};
SeenPacket seenBuffer[10];
int seenCount = 0;



static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

void setup() {
  Serial.begin(115200);
  camSerial.begin(115200, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);
  
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(CAM_EN, OUTPUT);
  digitalWrite(CAM_EN, LOW); // Cámara apagada por defecto

  delay(100);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Sensor Node Init");

  Serial1.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  tft.println("Iniciando LoRa...");
  int state = radio.begin(915.0, 125.0, 9, 7, 0x12, 10, 8, 1.6, false);
  if (state == RADIOLIB_ERR_NONE) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("LoRa OK!");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.printf("LoRa Error: %d\n", state);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  radio.startReceive(); // Inicializar modo de recepción continua para escuchar/repetir
}

void sendSensorTelemetry() {
  LoRaPacket packet;
  memset(&packet, 0, sizeof(LoRaPacket));
  
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.nodeId = MY_NODE_ID;
  packet.header.dataType = LORA_TYPE_SENSOR;
  packet.header.pktIndex = packetSequence++;

  SensorPayload* sp = (SensorPayload*)packet.payload;
  sp->latitude = gps.location.isValid() ? gps.location.lat() : 0.0;
  sp->longitude = gps.location.isValid() ? gps.location.lng() : 0.0;
  sp->ph = random(60, 80) / 10.0;
  sp->dissolved_oxygen = random(70, 90) / 10.0;
  sp->turbidity = random(0, 50);
  sp->conductivity = random(400, 600);
  sp->temperature = random(200, 300) / 10.0;
  sp->battery_level = random(80, 100);

  size_t packetSize = sizeof(LoRaHeader) + sizeof(SensorPayload);
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Enviando Telemetria...");
  
  int state = radio.transmit((uint8_t*)&packet, packetSize);
  
  if (state == RADIOLIB_ERR_NONE) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("TX OK");
    Serial.println("Telemetria enviada con exito");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.printf("TX Fail: %d\n", state);
  }
  radio.startReceive(); // CRITICO: Volver a escuchar después de transmitir
}

void requestAndSendImage() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("Tomando Foto...");
  
  // 1. Despertar cámara
  digitalWrite(CAM_EN, HIGH);
  delay(2000); // Dar tiempo a que bootee
  
  // Limpiar buffer UART
  while(camSerial.available()) camSerial.read();
  
  // 2. Enviar comando "TAKE_PIC"
  camSerial.println("TAKE_PIC");
  
  // Esperar el tamaño binario de 4 bytes (uint32_t)
  unsigned long waitStart = millis();
  bool imgIncoming = false;
  uint32_t imgSize = 0;
  while(millis() - waitStart < 5000) {
    if(camSerial.available() >= 4) {
      camSerial.readBytes((uint8_t*)&imgSize, 4);
      imgIncoming = true;
      break;
    }
  }

  if (!imgIncoming || imgSize <= 0 || imgSize > 100000) { // Max 100KB safe limit
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Error: No camara / Size");
    digitalWrite(CAM_EN, LOW);
    radio.startReceive(); // Volver a escuchar
    return;
  }

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.printf("Recibiendo %d bytes\n", imgSize);

  // Almacenar toda la imagen en RAM para evitar desbordamiento de UART (Overflow)
  uint8_t* imgBuffer = (uint8_t*)malloc(imgSize);
  if (!imgBuffer) {
     tft.println("Error Malloc RAM!");
     radio.startReceive(); // Volver a escuchar
     return;
  }

  int bytesRead = 0;
  unsigned long readStart = millis();
  while(bytesRead < imgSize && millis() - readStart < 10000) {
      if (camSerial.available()) {
          imgBuffer[bytesRead++] = camSerial.read();
      }
  }

  if (bytesRead < imgSize) {
     tft.println("Error UART Timeout");
     free(imgBuffer);
     radio.startReceive(); // Volver a escuchar
     return;
  }

  tft.println("Enviando por LoRa...");

  // 3. Enviar Paquete de Inicio
  LoRaPacket packet;
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.nodeId = MY_NODE_ID;
  packet.header.dataType = LORA_TYPE_IMG_START;
  packet.header.pktIndex = 0;
  radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader));

  // CRITICO: Darle tiempo al Concentrador para crear el archivo en su tarjeta SD
  // y volver a activar su receptor antes de que le caiga el primer Chunk de datos.
  delay(1000);

  // 4. Transmitir en fragmentos desde la RAM
  packet.header.dataType = LORA_TYPE_IMG_CHUNK;
  uint16_t chunkIndex = 1;
  uint32_t bytesSent = 0;
  
  // Calcular paquetes totales
  uint32_t totalChunks = (imgSize + LORA_MAX_PAYLOAD - 1) / LORA_MAX_PAYLOAD;

  tft.fillRect(0, 60, 160, 20, TFT_BLACK);
  tft.setCursor(0, 60);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf("0 / %u Chunks", totalChunks);

  while(bytesSent < imgSize) {
      uint32_t chunkLen = min((uint32_t)LORA_MAX_PAYLOAD, imgSize - bytesSent);
      memcpy(packet.payload, imgBuffer + bytesSent, chunkLen);
      packet.header.pktIndex = chunkIndex++;
      
      radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + chunkLen);
      bytesSent += chunkLen;
      
      // Mostrar progreso en Chunks en tiempo real absoluto (en cada chunk)
      tft.fillRect(0, 60, 160, 20, TFT_BLACK);
      tft.setCursor(0, 60);
      tft.printf("%u / %u Chunks", (uint32_t)(chunkIndex - 1), totalChunks);
      
      // CRITICO: Dar tiempo suficiente al Concentrador para guardar el chunk en su SD
      // y volver a activar su modo recepción. Si es muy rápido, el concentrador se queda sordo.
      delay(150); 
      Serial.printf("Chunk %d enviado (%d / %d)\n", chunkIndex - 1, bytesSent, imgSize);
  }

  // 5. Enviar Fin
  // Pausa MUY larga antes del paquete final para asegurar que la SD terminó de escribir el último chunk
  delay(500); 
  
  packet.header.dataType = LORA_TYPE_IMG_END;
  packet.header.pktIndex = chunkIndex;
  radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader));

  free(imgBuffer); // Liberar memoria RAM

  tft.fillRect(0, 60, 160, 20, TFT_BLACK);
  tft.setCursor(0, 60);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Foto Enviada OK");
  
  // Apagar cámara
  digitalWrite(CAM_EN, LOW);
  
  radio.startReceive(); // CRITICO: Volver a modo recepción para escuchar/repetir otros nodos
}

void handleRelay() {
  if (digitalRead(LORA_DIO1)) {
    LoRaPacket rxPacket;
    int state = radio.readData((uint8_t*)&rxPacket, sizeof(LoRaPacket));
    
    if (state == RADIOLIB_ERR_NONE) {
      if (rxPacket.header.syncWord[0] == LORA_SYNC_0 && rxPacket.header.syncWord[1] == LORA_SYNC_1) {
        // Solo repetir nodos que estén más lejos (ID mayor que el nuestro)
        if (rxPacket.header.nodeId > MY_NODE_ID) {
          // Verificar si ya lo hemos procesado y reenviado para evitar bucles infinitos
          bool alreadyForwarded = false;
          for (int i = 0; i < seenCount; i++) {
            if (seenBuffer[i].nodeId == rxPacket.header.nodeId &&
                seenBuffer[i].dataType == rxPacket.header.dataType &&
                seenBuffer[i].pktIndex == rxPacket.header.pktIndex) {
              alreadyForwarded = true;
              break;
            }
          }
          
          if (!alreadyForwarded) {
            // Guardar en caché de vistos
            if (seenCount < 10) {
              seenBuffer[seenCount++] = {rxPacket.header.nodeId, rxPacket.header.dataType, rxPacket.header.pktIndex};
            } else {
              for (int i = 0; i < 9; i++) seenBuffer[i] = seenBuffer[i+1];
              seenBuffer[9] = {rxPacket.header.nodeId, rxPacket.header.dataType, rxPacket.header.pktIndex};
            }
            
            // Obtener la longitud del paquete recibido
            size_t packetLen = radio.getPacketLength();
            
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(0, 0);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
            tft.printf("Relay Node %d\n", rxPacket.header.nodeId);
            tft.printf("Type 0x%02X Seq %d\n", rxPacket.header.dataType, rxPacket.header.pktIndex);
            
            // Transmitir (repetir) el paquete tal cual
            radio.transmit((uint8_t*)&rxPacket, packetLen);
            
            // Volver de inmediato a modo recepción continua
            radio.startReceive();
            
            Serial.printf("Relay OK: Nodo %d, Tipo 0x%02X, Seq %d\n", rxPacket.header.nodeId, rxPacket.header.dataType, rxPacket.header.pktIndex);
          }
        }
      }
    }
    // Asegurar que el receptor sigue activo pase lo que pase
    radio.startReceive();
  }
}

void loop() {
  smartDelay(50); // Delay más pequeño para capturar interrupciones rápido
  
  handleRelay(); // Escuchar y repetir paquetes de nodos hijos en topología lineal
  
  if (millis() - lastSensorTime > sensorInterval) {
    sendSensorTelemetry();
    
    // Para propósitos de demostración, enviamos la imagen justo después de los sensores
    delay(2000);
    requestAndSendImage();
    
    lastSensorTime = millis();
  }
}
