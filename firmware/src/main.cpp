#include <Arduino.h>
#include <TinyGPS++.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <RadioLib.h>
#include "lora_protocol.h"

// --- Identificador Único de este Nodo Sensor ---
#define MY_NODE_ID 3

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

#define DELAY_TX 200

// --- Pines para ESP32-CAM (Borde) ---
#define CAM_UART_TX 17
#define CAM_UART_RX 18
#define CAM_EN      6

// --- Objects ---
TinyGPSPlus gps;
TFT_eSPI tft = TFT_eSPI(); 
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
HardwareSerial camSerial(2);

// --- Variables ---
uint16_t packetSequence = 0;

// --- Cache para Repetidor ---
struct SeenPacket {
    uint8_t srcId;
    uint8_t type;
    uint16_t seqNum;
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
  camSerial.setRxBufferSize(4096);
  camSerial.begin(115200, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);
  
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(CAM_EN, OUTPUT);
  digitalWrite(CAM_EN, LOW);

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
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.printf("LoRa Error: %d\n", state);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  radio.startReceive();
}

void sendSensorTelemetry(uint8_t destId) {
  LoRaPacket packet;
  memset(&packet, 0, sizeof(LoRaPacket));
  
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.srcId = MY_NODE_ID;
  packet.header.destId = destId;
  packet.header.type = DATA_TELEMETRY;
  packet.header.seqNum = packetSequence++;
  packet.header.ttl = 5;

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
  radio.startReceive();
}

bool getChunkFromCam(uint32_t offset, uint32_t len, uint8_t* dest) {
  for (int retry = 0; retry < 3; retry++) {
      while(camSerial.available()) camSerial.read(); // Limpiar residuales
      
      camSerial.printf("GET_CHUNK %u %u\n", offset, len);
      
      unsigned long start = millis();
      uint32_t readBytes = 0;
      camSerial.setTimeout(500);
      
      while (readBytes < len && millis() - start < 800) {
          size_t readNow = camSerial.readBytes(dest + readBytes, len - readBytes);
          if (readNow > 0) {
              readBytes += readNow;
          } else {
              delay(2);
          }
      }
      
      if (readBytes == len) {
          return true; // ¡Éxito!
      }
      Serial.printf("Intento %d: getChunkFromCam falló (leído %u de %u)\n", retry + 1, readBytes, len);
      delay(50); // Breve espera antes del reintento
  }
  return false; // Falló tras 3 intentos
}

void requestAndSendImage(uint8_t destId) {
  // Enviar ACK primero para avisar que recibimos el comando y estamos tomando la foto
  LoRaPacket ackPacket;
  ackPacket.header.syncWord[0] = LORA_SYNC_0;
  ackPacket.header.syncWord[1] = LORA_SYNC_1;
  ackPacket.header.srcId = MY_NODE_ID;
  ackPacket.header.destId = destId;
  ackPacket.header.type = ACK;
  ackPacket.header.seqNum = packetSequence++;
  ackPacket.header.ttl = 5;
  radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
  delay(100);

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("Tomando Foto...");
  
  digitalWrite(CAM_EN, HIGH);
  delay(2000);
  
  while(camSerial.available()) camSerial.read();
  
  camSerial.println("TAKE_PIC");
  
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

  if (!imgIncoming || imgSize <= 0 || imgSize > 500000) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Error: No camara / Size");
    digitalWrite(CAM_EN, LOW);
    radio.startReceive();
    return;
  }

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.printf("Size: %u bytes\n", imgSize);
  tft.println("Enviando por LoRa...");

  uint16_t totalChunks = (imgSize + LORA_MAX_PAYLOAD - 1) / LORA_MAX_PAYLOAD;

  LoRaPacket packet;
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.srcId = MY_NODE_ID;
  packet.header.destId = destId;
  packet.header.type = DATA_IMG_START;
  packet.header.seqNum = 0;
  packet.header.ttl = 5;
  
  // Copiar tamaño total e información de fragmentación al inicio del streaming
  memcpy(packet.payload, &imgSize, 4);
  memcpy(packet.payload + 4, &totalChunks, 2);
  
  radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);

  delay(1000);

  packet.header.type = DATA_IMG_CHUNK;
  uint16_t chunkIndex = 1;
  uint32_t bytesSent = 0;

  tft.fillRect(0, 60, 160, 20, TFT_BLACK);
  tft.setCursor(0, 60);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  while(bytesSent < imgSize) {
      uint32_t chunkLen = min((uint32_t)LORA_MAX_PAYLOAD, imgSize - bytesSent);
      
      // Obtener el fragmento de la cámara dinámicamente en lugar de leer del heap
      if (getChunkFromCam(bytesSent, chunkLen, packet.payload)) {
          packet.header.seqNum = chunkIndex;
          radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + chunkLen);
          bytesSent += chunkLen;
          
          tft.fillRect(0, 60, 160, 20, TFT_BLACK);
          tft.setCursor(0, 60);
          tft.printf("%u / %u Chunks", (uint32_t)(chunkIndex - 1), totalChunks);
          
          delay(DELAY_TX); 
      } else {
          tft.fillRect(0, 60, 160, 20, TFT_BLACK);
          tft.setCursor(0, 60);
          tft.setTextColor(TFT_RED, TFT_BLACK);
          tft.printf("Err Cam Chunk %u", chunkIndex);
          Serial.printf("Error UART: Omitiendo chunk %u tras fallar todos los reintentos UART\n", chunkIndex);
          // Omitimos este chunk (no incrementamos bytesSent ni transmitimos). 
          // El concentrador lo detectará como perdido y lo pedirá en la fase de NACK.
          delay(100);
      }
      chunkIndex++; // Mantener la consistencia del índice de secuencia
  }

  delay(500); 
  
  packet.header.type = DATA_IMG_END;
  packet.header.seqNum = chunkIndex;
  memcpy(packet.payload, &imgSize, 4);
  memcpy(packet.payload + 4, &totalChunks, 2);
  radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);

  // Entrar en bucle de recuperación de fragmentos perdidos
  tft.fillRect(0, 60, 160, 20, TFT_BLACK);
  tft.setCursor(0, 60);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Esperando Confirm...");

  unsigned long sessionStart = millis();
  unsigned long lastEndTxTime = millis();
  radio.startReceive();
  bool waitingResponse = true;
  uint8_t endRetryCount = 0;

  while (waitingResponse && millis() - sessionStart < 120000) { // Timeout de seguridad de 120s
      // Retransmisión periódica de DATA_IMG_END si pasan 4 segundos sin recibir confirmación/petición
      if (millis() - lastEndTxTime > 4000) {
          if (endRetryCount < 5) {
              endRetryCount++;
              Serial.printf("LoRa Flow: Timeout de confirmación de 4s. Retransmitiendo DATA_IMG_END (intento %u/5)...\n", endRetryCount);
              
              packet.header.type = DATA_IMG_END;
              packet.header.seqNum = chunkIndex;
              memcpy(packet.payload, &imgSize, 4);
              memcpy(packet.payload + 4, &totalChunks, 2);
              radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);
              
              lastEndTxTime = millis();
              radio.startReceive();
          } else {
              Serial.println("LoRa Flow: Superado el límite de reintentos de DATA_IMG_END. Cancelando espera.");
              break;
          }
      }

      if (digitalRead(LORA_DIO1)) {
          LoRaPacket rxPacket;
          int state = radio.readData((uint8_t*)&rxPacket, sizeof(rxPacket));
          if (state == RADIOLIB_ERR_NONE) {
              if (rxPacket.header.syncWord[0] == LORA_SYNC_0 && rxPacket.header.syncWord[1] == LORA_SYNC_1) {
                  if (rxPacket.header.destId == MY_NODE_ID) {
                      if (rxPacket.header.type == ACK) {
                          // Concentrador recibió todo sin pérdidas
                          Serial.println("LoRa Flow: ¡Concentrador reporta 100% recibido! Sesión completada.");
                          tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                          tft.setCursor(0, 60);
                          tft.setTextColor(TFT_GREEN, TFT_BLACK);
                          tft.println("Confirmado OK!");
                          waitingResponse = false;
                      }
                      else if (rxPacket.header.type == CMD_REQ_MISSING) {
                          // Hay fragmentos perdidos. Retransmitirlos
                          uint16_t missingCount = 0;
                          memcpy(&missingCount, rxPacket.payload, 2);
                          uint16_t* sequences = (uint16_t*)(rxPacket.payload + 2);
                          
                          Serial.printf("LoRa Flow: ¡NACK recibido! %u fragmentos perdidos. Retransmitiendo...\n", missingCount);
                          tft.fillRect(0, 60, 160, 20, TFT_BLACK);
                          tft.setCursor(0, 60);
                          tft.setTextColor(TFT_RED, TFT_BLACK);
                          tft.printf("NACK: Reenviando %u...", missingCount);

                          packet.header.type = DATA_IMG_CHUNK;
                          
                          for (uint16_t i = 0; i < missingCount; i++) {
                              uint16_t seq = sequences[i];
                              if (seq <= totalChunks) {
                                  uint32_t offset = (seq - 1) * LORA_MAX_PAYLOAD;
                                  uint32_t chunkLen = min((uint32_t)LORA_MAX_PAYLOAD, imgSize - offset);
                                  
                                  // Solicitar fragmento dinámicamente sobre UART de la cámara
                                  if (getChunkFromCam(offset, chunkLen, packet.payload)) {
                                      packet.header.seqNum = seq;
                                      radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + chunkLen);
                                      Serial.printf("Retransmitiendo chunk %d (Offset %u, len %u)\n", seq, offset, chunkLen);
                                  } else {
                                      Serial.printf("Fallo al re-solicitar chunk %d a la cámara por UART!\n", seq);
                                  }
                                  delay(DELAY_TX);
                              }
                          }
                          
                          // Enviar DATA_IMG_END de nuevo para cerrar el lote de retransmisión
                          delay(500);
                          packet.header.type = DATA_IMG_END;
                          packet.header.seqNum = chunkIndex;
                          memcpy(packet.payload, &imgSize, 4);
                          memcpy(packet.payload + 4, &totalChunks, 2);
                          radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);
                          
                          // Reiniciar temporizadores tras transmisión exitosa de fragmentos
                          sessionStart = millis();
                          lastEndTxTime = millis();
                          endRetryCount = 0;
                          radio.startReceive();
                      }
                  }
              }
          }
          radio.startReceive();
      }
      delay(10);
  }

  // Notificar fin de sesión a la cámara para liberar memoria PSRAM
  camSerial.println("RELEASE_PIC");
  digitalWrite(CAM_EN, LOW);
  radio.startReceive();
}

bool checkSeenAndAdd(uint8_t srcId, uint8_t type, uint16_t seqNum) {
    for (int i = 0; i < seenCount; i++) {
        if (seenBuffer[i].srcId == srcId &&
            seenBuffer[i].type == type &&
            seenBuffer[i].seqNum == seqNum) {
            return true; // Ya lo vimos
        }
    }
    // No lo hemos visto, lo agregamos
    if (seenCount < 10) {
        seenBuffer[seenCount++] = {srcId, type, seqNum};
    } else {
        for (int i = 0; i < 9; i++) seenBuffer[i] = seenBuffer[i+1];
        seenBuffer[9] = {srcId, type, seqNum};
    }
    return false;
}

void handleLoRa() {
  if (digitalRead(LORA_DIO1)) {
    LoRaPacket rxPacket;
    int state = radio.readData((uint8_t*)&rxPacket, sizeof(LoRaPacket));
    
    if (state == RADIOLIB_ERR_NONE) {
      if (rxPacket.header.syncWord[0] == LORA_SYNC_0 && rxPacket.header.syncWord[1] == LORA_SYNC_1) {
        
        // TTL Check
        if (rxPacket.header.ttl > 0) {
            rxPacket.header.ttl--;
            
            // ¿Es para mí?
            if (rxPacket.header.destId == MY_NODE_ID) {
                // Prevenir procesamiento duplicado
                if (!checkSeenAndAdd(rxPacket.header.srcId, rxPacket.header.type, rxPacket.header.seqNum)) {
                    Serial.printf("Comando Recibido: Tipo 0x%02X de Nodo %d\n", rxPacket.header.type, rxPacket.header.srcId);
                    
                    if (rxPacket.header.type == CMD_REQ_TELEMETRY) {
                        sendSensorTelemetry(rxPacket.header.srcId);
                    } 
                    else if (rxPacket.header.type == CMD_REQ_IMAGE) {
                        requestAndSendImage(rxPacket.header.srcId);
                    }
                }
            } 
            // ¿Debo retransmitir (Enrutamiento Lineal)?
            else if (rxPacket.header.ttl > 0) {
                bool shouldRelay = false;
                
                // Downstream: Origen < Mi ID, y Destino > Mi ID
                if (rxPacket.header.srcId < MY_NODE_ID && rxPacket.header.destId > MY_NODE_ID) {
                    shouldRelay = true;
                }
                // Upstream: Origen > Mi ID, y Destino < Mi ID
                else if (rxPacket.header.srcId > MY_NODE_ID && rxPacket.header.destId < MY_NODE_ID) {
                    shouldRelay = true;
                }
                
                if (shouldRelay) {
                    if (!checkSeenAndAdd(rxPacket.header.srcId, rxPacket.header.type, rxPacket.header.seqNum)) {
                        size_t packetLen = radio.getPacketLength();
                        
                        tft.fillScreen(TFT_BLACK);
                        tft.setCursor(0, 0);
                        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                        tft.printf("Relay S:%d D:%d\n", rxPacket.header.srcId, rxPacket.header.destId);
                        tft.printf("Type 0x%02X\n", rxPacket.header.type);
                        
                        radio.transmit((uint8_t*)&rxPacket, packetLen);
                        radio.startReceive();
                        Serial.printf("Relay OK: S:%d D:%d\n", rxPacket.header.srcId, rxPacket.header.destId);
                    }
                }
            }
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
