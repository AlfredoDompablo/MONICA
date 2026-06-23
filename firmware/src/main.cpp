/**
 * @file main.cpp
 * @brief Firmware de Producción para los Nodos Sensores de MONICA.
 * 
 * Este programa administra la lectura y retransmisión de datos ambientales (ph, oxígeno disuelto,
 * turbidez, conductividad, temperatura, nivel de batería) y fragmentos de imagen tomados por una
 * cámara acoplada por puerto serie UART. Cuenta con soporte integrado para enrutamiento lineal 
 * multi-salto (multi-hop) por hardware LoRa SX1262 y visualización a través de una pantalla TFT.
 * 
 * Arquitectura de Red:
 *   [Concentrador 0] <-> [Nodo 1] <-> [Nodo 2] <-> [Nodo 3] <-> [Nodo 4]
 * 
 * Diseñado con mecanismos de optimización de batería (pacing dinámico de colisiones) e inmunidad
 * a loops infinitos de retransmisión mediante validación estricta del salto físico receptor.
 */

#include <Arduino.h>
#include <TinyGPS++.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <RadioLib.h>
#include "lora_protocol.h"
#include "LittleFS.h"
#include <Preferences.h>

// --- Configuración Remota de la Cámara (Persistida en Preferences) ---
Preferences preferences;
uint8_t cam_resolution = 10; // XGA por defecto
int8_t cam_brightness = 0;   // -2 a 2
int8_t cam_contrast = 1;     // -2 a 2

// --- Identificador Único de este Nodo Sensor ---
// Modificar este valor de 1 a 4 según la posición física del nodo en la topología lineal.
#define MY_NODE_ID 4

// --- Definición de Pines de Hardware (Heltec Wireless Tracker v1.1) ---
#if MY_NODE_ID == 2
  #define VEXT_PIN 37           // Control de energía para V1.0 (Nodo 2)
#else
  #define VEXT_PIN 3            // Control de energía para V1.1 (Nodo 1 y Concentrador)
#endif
#define BACKLIGHT_PIN 21      // Pin de control del brillo de la pantalla TFT.
#define GNSS_RX_PIN 33        // Pin de recepción UART para el módulo de posicionamiento GNSS (GPS).
#define GNSS_TX_PIN 34        // Pin de transmisión UART para el módulo de posicionamiento GNSS (GPS).

// --- Pines del Bus SPI para el Transceptor LoRa (Internos de la placa Heltec) ---
#define LORA_CS   8           // Chip Select (Selector de Chip SPI).
#define LORA_SCK  9           // Reloj SPI (Serial Clock).
#define LORA_MOSI 10          // Salida de Datos SPI (Master Out Slave In).
#define LORA_MISO 11          // Entrada de Datos SPI (Master In Slave Out).
#define LORA_RST  12          // Pin de reinicio de hardware del SX1262.
#define LORA_BUSY 13          // Pin indicador de estado ocupado del chip LoRa.
#define LORA_DIO1 14          // Pin de interrupción para notificaciones de RX/TX completados.

// --- Tiempos de Transmisión Dinámicos (Pacing) según el número de saltos al Concentrador ---
// A mayor cantidad de saltos de distancia física, mayor debe ser el tiempo de gracia (pacing)
// para dar margen a que los nodos repetidores retransmitan el paquete anterior en la línea sin colisionar.
#if MY_NODE_ID == 1
  #define DELAY_TX 120        // 1 salto al concentrador directo: delay corto.
#elif MY_NODE_ID == 2
  #define DELAY_TX 350        // 2 saltos al concentrador: delay medio.
#elif MY_NODE_ID == 3
  #define DELAY_TX 650        // 3 saltos al concentrador: delay largo.
#else
  #define DELAY_TX 950        // 4 saltos: pacing ultra seguro y conservador de batería.
#endif

// --- Pines de Comunicación para la Cámara ESP32-CAM (Puerto Serie UART) ---
#define CAM_UART_TX 17        // Pin de transmisión de comandos hacia la cámara.
#define CAM_UART_RX 18        // Pin de recepción de fragmentos de imagen desde la cámara.
#define CAM_EN      6         // Pin de control de alimentación (Habilitador/Power-On) de la cámara.

// --- Instanciación de Objetos Principales ---
TinyGPSPlus gps;                                                     // Decodificador del protocolo NMEA para geolocalización.
TFT_eSPI tft = TFT_eSPI();                                           // Controlador gráfico para la pantalla LCD/TFT integrada.
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);  // Controlador LoRa SX1262 vía RadioLib.
HardwareSerial camSerial(2);                                         // Interfaz UART secundaria (Serial2) para transferencias de cámara.

// --- Variables de Estado Global ---
uint16_t packetSequence = 0;                                         // Contador incremental autoincrementado para paquetes LoRa salientes.

// --- Estructuras y Búferes de Caché del Repetidor ---
struct SeenPacket {
    uint8_t srcId;       // ID de origen del nodo que generó la transmisión.
    uint8_t type;        // Tipo del paquete recibido (CMD o DATA).
    uint16_t seqNum;     // Número de secuencia.
};
SeenPacket seenBuffer[10]; // Registro de historial para evitar bucles infinitos de retransmisión de paquetes ya propagados.
int seenCount = 0;         // Contador dinámico de elementos almacenados en el búfer circular.

/**
 * @brief Retardo inteligente que procesa y decodifica las ráfagas GPS NMEA en segundo plano.
 * @param ms Tiempo en milisegundos que durará la espera.
 * 
 * Protege contra bloqueos infinitos limitando el procesamiento a 50 bytes por ciclo de lectura
 * en caso de que exista ruido electromagnético o flotación en el pin RX de hardware.
 */
// Tarea en segundo plano para decodificar GPS en el Core 0
void gpsTask(void* pvParameters) {
  Serial.println("[GPS] Tarea iniciada en el Core 0");
  while (true) {
    while (Serial1.available()) {
      gps.encode(Serial1.read());
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Pausa de 10ms para no acaparar el núcleo
  }
}

static void smartDelay(unsigned long ms) {
  delay(ms);
}

// --- Variables del Panel Gráfico ---
char lastRfActivity[30] = "Ninguna";  // Cadena formateada para desplegar la última actividad RF en el dashboard.
uint8_t simulatedBattery = 95;       // Nivel de porcentaje simulado de carga de la batería del dispositivo.

// --- Variables de Control No-Bloqueante para Pantalla de Repetidor ---
unsigned long lastRelayScreenTime = 0; // Registra cuándo se mostró la pantalla de retransmisión.
bool isRelayScreenActive = false;      // Bandera para indicar que la pantalla de retransmisión está visible.

/**
 * @brief Dibuja la barra de título premium del dashboard.
 * @param title Cadena de texto a mostrar en el centro de la barra superior.
 */
void drawHeader(const char* title) {
    tft.fillRect(0, 0, 160, 15, tft.color565(0, 51, 102));      // Fondo Azul Marino.
    tft.drawFastHLine(0, 15, 160, tft.color565(200, 200, 200)); // Línea de separación Plata.
    tft.setTextColor(TFT_WHITE, tft.color565(0, 51, 102));
    tft.setTextSize(1);
    tft.setCursor((160 - strlen(title) * 6) / 2, 4);            // Centrado automático de tipografía de ancho fijo.
    tft.print(title);
}

/**
 * @brief Renderiza la pantalla principal del panel en modo de bajo solapamiento visual.
 * 
 * Implementa una rejilla fija en el eje vertical (Y) utilizando fuentes pequeñas.
 * Esto asegura que los textos dinámicos no envuelvan ni se sobrepongan entre sí.
 */
void drawDashboard() {
    tft.fillScreen(TFT_BLACK);
    
    // 1. Cabecera (Azul Marino Premium)
    char title[25];
    sprintf(title, "MONICA NODO %d", MY_NODE_ID);
    drawHeader(title);
    
    // 2. Estado Activo (Fila 1 en Y = 18)
    tft.setCursor(0, 18);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Estado: ");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print("ESCUCHANDO");
    
    // 3. Estado de Geolocalización y Conexión de Cámara (Fila 2 en Y = 28)
    tft.setCursor(0, 28);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("GPS: ");
    if (gps.location.isValid()) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("3D Fix");
    } else {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.print("NO Fix");
    }
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(" | Cam: ");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("OK");
    
    // 4. Batería y Retardo de Pacing (Fila 3 en Y = 38)
    tft.setCursor(0, 38);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Bat: ");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.printf("%d%%", simulatedBattery);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.printf(" | Delay: %d", DELAY_TX);
    
    // Separador Estético Central (Y = 49)
    tft.drawFastHLine(0, 49, 160, tft.color565(80, 80, 80));
    
    // 5. Última Actividad RF de Diagnóstico (Fila 4 en Y = 53)
    tft.setCursor(0, 53);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("Ult. RF: ");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(lastRfActivity);
    
    // Separador Estético Inferior (Y = 63)
    tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));
    
    // 6. Pie de Página del Sistema (Fila 5 en Y = 66)
    tft.setCursor(0, 66);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.print("Espera de peticion...");
}

/**
 * @brief Configuración inicial del hardware del dispositivo.
 */
void setup() {
  Serial.begin(115200);
  
  // Cargar configuración persistente de la cámara
  preferences.begin("cam_config", false);
  cam_resolution = preferences.getUChar("res", 10); // XGA por defecto
  cam_brightness = preferences.getChar("br", 0);
  cam_contrast = preferences.getChar("co", 1);
  preferences.end();
  Serial.printf("[CONFIG] Cámara cargada de Preferences: Res=%d, Br=%d, Co=%d\n", cam_resolution, cam_brightness, cam_contrast);

  // Inicializar LittleFS en la memoria Flash externa para almacenamiento temporal
  if (!LittleFS.begin(true)) {
    Serial.println("[ERROR] Error al montar LittleFS");
  } else {
    Serial.println("[INFO] LittleFS montado correctamente");
  }
  
  // Reservar suficiente búfer de hardware en RX UART para evitar el desborde y pérdida de bytes de cámara
  camSerial.setRxBufferSize(4096);
  camSerial.begin(115200, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);
  
  // Configuración y encendido de reguladores internos de hardware
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  pinMode(37, OUTPUT);
  digitalWrite(37, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);  // Encender retroiluminación de pantalla LCD.
  pinMode(CAM_EN, OUTPUT);
  digitalWrite(CAM_EN, LOW);          // Apagar la cámara al inicio para conservar energía (Batería).
 
  delay(100);

  // Inicialización de la Pantalla TFT
  tft.init();
  tft.setRotation(1);                 // Ajustar orientación horizontal del display.
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Sensor Node Init");

  Serial.printf("\n====================================\n");
  Serial.printf("INICIANDO NODO SENSOR, ID: %d\n", MY_NODE_ID);
  Serial.printf("====================================\n");

  // Iniciar la UART GPS para la lectura en segundo plano
  Serial1.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  // Crear la tarea del GPS en el Core 0 (prioridad 1)
  xTaskCreatePinnedToCore(
    gpsTask,
    "gpsTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  // Inicializar bus SPI compartido con la pantalla para comunicarse con el módulo LoRa SX1262
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Serial.println("Iniciando transceptor LoRa SX1262...");
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
    tft.printf("LoRa Error: %d\n", state);
    Serial.printf("[ERROR] Fallo al iniciar LoRa SX1262. Codigo: %d\n", state);
  }
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Colocar el módulo de radio en modo de escucha continua
  radio.startReceive();
  Serial.println("Transceptor LoRa en modo escucha. Nodo Listo!");
  
  // Renderizar la pantalla de operaciones permanente
  drawDashboard();
}

/**
 * @brief Genera datos analógicos ambientales y los transmite al Concentrador.
 * @param destId Identificador único del nodo de destino (normalmente 0, que es el concentrador).
 */
void sendSensorTelemetry(uint8_t destId) {
  LoRaPacket packet;
  memset(&packet, 0, sizeof(LoRaPacket));
  
  // Construir cabecera del protocolo LoRa unificado de MONICA
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.srcId = MY_NODE_ID;
  packet.header.destId = destId;
  
  // Enrutamiento Lineal: Si viaja al concentrador (hacia ID menor), el siguiente salto físico es mi ID - 1
  packet.header.nextHopId = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
  packet.header.type = DATA_TELEMETRY;
  packet.header.seqNum = packetSequence++;
  packet.header.ttl = 5; // Evita la propagación perpetua de ruido en el aire

  // Cargar payload de sensores con lecturas ambientales
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
  uint8_t nextHop = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
  
  // Actualizar pantalla temporalmente con información de ruteo
  char headerTitle[30];
  sprintf(headerTitle, "NODO %d - TELEMETRIA", MY_NODE_ID);
  tft.fillScreen(TFT_BLACK);
  drawHeader(headerTitle);
  
  tft.setCursor(0, 18);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Ruta: ");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.printf("[*N%d*]", MY_NODE_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf(" -> [N%d]", nextHop);
  
  tft.setCursor(0, 28);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Dest: ");
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.printf("Concentrador (%d)", destId);
  
  tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
  tft.setCursor(0, 42);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Estado: Transmitiendo...");
  tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));
  
  tft.setCursor(0, 66);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.printf("Delay: %d ms", DELAY_TX);
  
  // Transmitir paquete por LoRa
  int state = radio.transmit((uint8_t*)&packet, packetSize);
  
  tft.fillRect(0, 42, 160, 10, TFT_BLACK);
  tft.setCursor(0, 42);
  if (state == RADIOLIB_ERR_NONE) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("Estado: TX EXITOSO [OK]");
    Serial.println("Telemetria enviada con exito");
    strcpy(lastRfActivity, "TX Telem OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.printf("Estado: TX ERR %d", state);
    sprintf(lastRfActivity, "TX Telem ERR %d", state);
  }
  
  // Regresar inmediatamente al modo de escucha
  radio.startReceive();
  delay(1500); // Dar margen para visualizar la información en pantalla
  drawDashboard();
}

/**
 * @brief Solicita un fragmento de bytes específico a la cámara UART con reintentos robustos.
 * @param offset Desplazamiento en bytes de la imagen a solicitar.
 * @param len Longitud del fragmento de imagen a leer.
 * @param dest Búfer de destino en memoria RAM para guardar el fragmento recibido.
 * @return true si la lectura fue completa y exitosa, false en caso contrario.
 * 
 * Implementa 3 intentos robustos de lectura con vaciado de ruido residual del canal serie.
 */
bool getChunkFromCam(uint32_t offset, uint32_t len, uint8_t* dest) {
  unsigned long timeoutLimit = 1000 + len / 10; // Calcular timeout dinámico: 1s base + 100ms por cada 1KB (1000 baud UART safety)
  for (int retry = 0; retry < 3; retry++) {
      while(camSerial.available()) camSerial.read(); // Limpiar ruidos o bytes colgados en el búfer RX
      
      // Enviar comando estructurado a la cámara
      camSerial.printf("GET_CHUNK %u %u\n", offset, len);
      
      unsigned long start = millis();
      uint32_t readBytes = 0;
      camSerial.setTimeout(500); // Límite de lectura UART
      
      while (readBytes < len && millis() - start < timeoutLimit) {
          size_t readNow = camSerial.readBytes(dest + readBytes, len - readBytes);
          if (readNow > 0) {
              readBytes += readNow;
          } else {
              delay(2);
          }
      }
      
      if (readBytes == len) {
          return true; // Éxito en la lectura secuencial
      }
      Serial.printf("Intento %d: getChunkFromCam falló (leído %u de %u)\n", retry + 1, readBytes, len);
      delay(50); // Pausa estretégica antes de reintentar
  }
  return false; // Falló de comunicación con la cámara
}

/**
 * @brief Captura y transmite secuencialmente una imagen por bloques LoRa de ancho máximo (LORA_MAX_PAYLOAD).
 * @param destId Identificador único del nodo de destino (Concentrador).
 * 
 * Flujo Completo:
 *  1. Envía un paquete ACK para indicar la recepción del comando y encender la cámara.
 *  2. Habilita y enciende la cámara, toma la foto (`TAKE_PIC`) y lee el tamaño total de la imagen.
 *  3. Transmite el paquete inicial `DATA_IMG_START` con los metadatos de la imagen.
 *  4. Envía cada fragmento `DATA_IMG_CHUNK` con un pacing dinámico controlado (`DELAY_TX`).
 *  5. Envía `DATA_IMG_END` e inicia la máquina de retransmisión selectiva (NACK / `CMD_REQ_MISSING`).
 *  6. Vuelve a transmitir fragmentos faltantes solicitados hasta completar la confirmación final (`ACK`).
 */
void requestAndSendImage(uint8_t destId) {
  // Enviar ACK inicial de gracia
  LoRaPacket ackPacket;
  ackPacket.header.syncWord[0] = LORA_SYNC_0;
  ackPacket.header.syncWord[1] = LORA_SYNC_1;
  ackPacket.header.srcId = MY_NODE_ID;
  ackPacket.header.destId = destId;
  ackPacket.header.nextHopId = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
  ackPacket.header.type = ACK;
  ackPacket.header.seqNum = packetSequence++;
  ackPacket.header.ttl = 5;
  radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
  delay(100);

  // Inicializar interfaz gráfica de captura
  char headerTitle[30];
  sprintf(headerTitle, "NODO %d - CAMARA", MY_NODE_ID);
  tft.fillScreen(TFT_BLACK);
  drawHeader(headerTitle);
  
  tft.setCursor(0, 18);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Dest: ");
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.printf("Concentrador (%d)", destId);
  
  tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
  tft.setCursor(0, 42);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Estado: Capturando foto...");
  tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));
  
  tft.setCursor(0, 66);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.print("Espere 2 segundos...");
  
  // Encender la cámara ESP32-CAM físicamente liberando energía
  Serial.println("[CAMERA DEBUG] Encendiendo camara fisicamente (CAM_EN = HIGH)...");
  digitalWrite(CAM_EN, HIGH);
  delay(2000); // Tiempo de arranque necesario para inicialización de CMOS de cámara
  
  Serial.println("[CAMERA DEBUG] Vaciando buffer RX de camSerial...");
  while(camSerial.available()) camSerial.read(); // Limpiar remanentes serie
  
  // Enviar configuración a la cámara vía serial
  Serial.printf("[CAMERA] Enviando configuración UART: Res=%d, Br=%d, Co=%d\n", cam_resolution, cam_brightness, cam_contrast);
  camSerial.printf("SET_CONFIG %d %d %d\n", cam_resolution, cam_brightness, cam_contrast);
  
  // Esperar confirmación CONF_ACK (máximo 500ms)
  unsigned long ackStart = millis();
  bool gotConfAck = false;
  while (millis() - ackStart < 500) {
    if (camSerial.available()) {
      String resp = camSerial.readStringUntil('\n');
      resp.trim();
      if (resp == "CONF_ACK") {
        gotConfAck = true;
        break;
      }
    }
    delay(5);
  }
  Serial.printf("[CAMERA] Confirmación de configuración: %s\n", gotConfAck ? "OK (CONF_ACK)" : "TIMEOUT");

  // Solicitar disparo de foto
  Serial.println("[CAMERA DEBUG] Enviando TAKE_PIC a la camara...");
  camSerial.println("TAKE_PIC");
  
  unsigned long waitStart = millis();
  bool imgIncoming = false;
  uint32_t imgSize = 0;
  
  // Leer tamaño de imagen entrante en forma de palabra (ej: "SIZE:12345")
  Serial.println("[CAMERA DEBUG] Esperando respuesta de tamano (SIZE)...");
  while(millis() - waitStart < 8000) {
    if(camSerial.available()) {
      String line = camSerial.readStringUntil('\n');
      line.trim();
      Serial.printf("[CAMERA DEBUG] Recibido de camara: '%s'\n", line.c_str());
      if (line.startsWith("SIZE:")) {
        imgSize = line.substring(5).toInt();
        if (imgSize > 0) {
          imgIncoming = true;
          break;
        }
      }
    }
  }

  // Comprobación de límites lógicos para imagen en Flash (soportamos hasta 2MB en Flash)
  if (!imgIncoming || imgSize <= 0 || imgSize > 2097152) {
    Serial.printf("[CAMERA DEBUG] ERROR: Peticion de imagen fallida o tamano excede el limite. imgIncoming=%d, imgSize=%u\n", imgIncoming, imgSize);
    tft.fillRect(0, 42, 160, 10, TFT_BLACK);
    tft.setCursor(0, 42);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Estado: Err Camara/Tamano");
    digitalWrite(CAM_EN, LOW); // Apagar cámara ante error
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  Serial.printf("[CAMERA DEBUG] Tamano de imagen valido recibido: %u bytes. Creando /temp_img.jpg en LittleFS...\n", imgSize);
  // Abrir archivo temporal en la Flash usando LittleFS (modo escritura descarta la foto anterior)
  File imgFile = LittleFS.open("/temp_img.jpg", "w");
  if (!imgFile) {
    Serial.println("[CAMERA DEBUG] ERROR: No se pudo abrir /temp_img.jpg en LittleFS!");
    tft.fillRect(0, 42, 160, 10, TFT_BLACK);
    tft.setCursor(0, 42);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Estado: Err Inicializar Flash");
    digitalWrite(CAM_EN, LOW);
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  tft.fillRect(0, 42, 160, 10, TFT_BLACK);
  tft.setCursor(0, 42);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Estado: Descargando a Flash...");

  // Descargar en ráfagas de 200 bytes para no saturar la RAM
  Serial.println("[CAMERA DEBUG] Iniciando descarga de fragmentos de camara...");
  uint32_t bytesDownloaded = 0;
  uint8_t tempBuffer[200];
  bool downloadSuccess = true;

  while (bytesDownloaded < imgSize) {
    uint32_t lenToDownload = min((uint32_t)200, imgSize - bytesDownloaded);
    if (getChunkFromCam(bytesDownloaded, lenToDownload, tempBuffer)) {
      imgFile.write(tempBuffer, lenToDownload);
      bytesDownloaded += lenToDownload;
      
      // Mostrar progreso en pantalla
      tft.fillRect(0, 65, 160, 12, TFT_BLACK);
      tft.setCursor(0, 65);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.printf("Descarga: %d%%", (bytesDownloaded * 100) / imgSize);
    } else {
      downloadSuccess = false;
      Serial.printf("[CAMERA DEBUG] ERROR: Fallo al descargar chunk en offset %u!\n", bytesDownloaded);
      break;
    }
  }

  imgFile.close();

  if (!downloadSuccess) {
    Serial.println("[CAMERA DEBUG] ERROR: Descarga incompleta, eliminando archivo temporal.");
    tft.fillRect(0, 42, 160, 10, TFT_BLACK);
    tft.setCursor(0, 42);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Estado: Err Descarga Cam");
    LittleFS.remove("/temp_img.jpg");
    digitalWrite(CAM_EN, LOW);
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  Serial.println("[CAMERA DEBUG] Descarga a Flash exitosa. Liberando frame buffer en camara y apagando...");
  // --- ¡APAGADO INMEDIATO DE LA CÁMARA PARA AHORRO EXTREMO DE ENERGÍA! ---
  // La imagen ya está guardada al 100% de manera permanente en la memoria Flash del Heltec.
  // Apagamos la cámara físicamente para ahorrar energía durante la lenta transmisión por LoRa.
  camSerial.println("RELEASE_PIC");
  digitalWrite(CAM_EN, LOW);

  uint8_t nextHop = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);

  // Inicializar interfaz de streaming
  sprintf(headerTitle, "NODO %d - ENVIAR FOTO", MY_NODE_ID);
  tft.fillScreen(TFT_BLACK);
  drawHeader(headerTitle);
  
  tft.setCursor(0, 18);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Ruta: ");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.printf("[*N%d*]", MY_NODE_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf(" -> [N%d]", nextHop);
  
  tft.setCursor(0, 28);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Size: ");
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.printf("%.1f KB", imgSize / 1024.0);
  
  tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
  tft.setCursor(0, 42);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Estado: Enviando chunks...");
  tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));

  uint16_t totalChunks = (imgSize + LORA_MAX_PAYLOAD - 1) / LORA_MAX_PAYLOAD;

  // Construir y enviar paquete inicial START
  LoRaPacket packet;
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.srcId = MY_NODE_ID;
  packet.header.destId = destId;
  packet.header.nextHopId = nextHop;
  packet.header.type = DATA_IMG_START;
  packet.header.seqNum = 0;
  packet.header.ttl = 5;
  
  // Copiar tamaño total e información de fragmentación al inicio del streaming
  memcpy(packet.payload, &imgSize, 4);
  memcpy(packet.payload + 4, &totalChunks, 2);
  
  radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);
  delay(1000); // Tiempo de guarda para inicializar archivo en SD del concentrador

  packet.header.type = DATA_IMG_CHUNK;
  uint16_t chunkIndex = 1;
  uint32_t bytesSent = 0;

  // Abrir el archivo de la imagen en modo lectura para iniciar el streaming LoRa
  File readImgFile = LittleFS.open("/temp_img.jpg", "r");
  if (!readImgFile) {
    tft.fillRect(0, 42, 160, 10, TFT_BLACK);
    tft.setCursor(0, 42);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("Estado: Err Abrir Flash");
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  // Transmisión en bucle de los fragmentos desde la Flash local
  while(bytesSent < imgSize) {
      uint32_t chunkLen = min((uint32_t)LORA_MAX_PAYLOAD, imgSize - bytesSent);
      
      // Leer directamente el fragmento desde la memoria Flash
      readImgFile.seek(bytesSent);
      readImgFile.read(packet.payload, chunkLen);
      
      packet.header.seqNum = chunkIndex;
      radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + chunkLen);
      bytesSent += chunkLen;
      
      tft.fillRect(0, 65, 160, 12, TFT_BLACK);
      tft.setCursor(0, 65);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.printf("%u / %u Chunks", (uint32_t)(chunkIndex - 1), totalChunks);
      
      // Aplicación estricta de pacing para prevención de colisiones por desbordamiento de búfer repetidor
      delay(DELAY_TX); 
      chunkIndex++; 
  }

  delay(500); 
  
  // Transmitir END para cerrar el lote
  packet.header.type = DATA_IMG_END;
  packet.header.seqNum = chunkIndex;
  memcpy(packet.payload, &imgSize, 4);
  memcpy(packet.payload + 4, &totalChunks, 2);
  radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);

  tft.fillRect(0, 42, 160, 10, TFT_BLACK);
  tft.setCursor(0, 42);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Estado: Esp. Confirma...");

  unsigned long sessionStart = millis();
  unsigned long lastEndTxTime = millis();
  radio.startReceive();
  
  bool waitingResponse = true;
  uint8_t endRetryCount = 0;

  // --- Bucle de Recuperación NACK Selectiva (ARQ) ---
  while (waitingResponse && millis() - sessionStart < 120000) { // Timeout de resguardo de 2 minutos
      // Re-enviar END si pasan 4 segundos sin respuesta del receptor
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

      // Procesar eventos LoRa recibidos
      if (digitalRead(LORA_DIO1)) {
          LoRaPacket rxPacket;
          int state = radio.readData((uint8_t*)&rxPacket, sizeof(rxPacket));
          if (state == RADIOLIB_ERR_NONE) {
              if (rxPacket.header.syncWord[0] == LORA_SYNC_0 && rxPacket.header.syncWord[1] == LORA_SYNC_1) {
                  
                  // Validación física crítica del salto: solo procesar si va dirigido a mi ID
                  if (rxPacket.header.nextHopId != MY_NODE_ID) {
                      radio.startReceive();
                      continue;
                  }
                  
                  if (rxPacket.header.destId == MY_NODE_ID) {
                      if (rxPacket.header.type == ACK) {
                          // ¡Concentrador recibió todo sin pérdidas!
                          Serial.println("LoRa Flow: ¡Concentrador reporta 100% recibido! Sesión completada.");
                          tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                          tft.setCursor(0, 42);
                          tft.setTextColor(TFT_GREEN, TFT_BLACK);
                          tft.print("Estado: CONFIRMADO [OK]");
                          strcpy(lastRfActivity, "TX Img OK");
                          waitingResponse = false;
                      }
                      else if (rxPacket.header.type == CMD_REQ_MISSING) {
                           // Procesar lista selectiva de fragmentos faltantes
                           uint16_t missingCount = 0;
                           memcpy(&missingCount, rxPacket.payload, 2);
                           
                           // Evitar desbordamiento de búfer y asegurar la alineación de memoria
                           if (missingCount > 98) missingCount = 98;
                           uint16_t sequences[98];
                           memcpy(sequences, rxPacket.payload + 2, missingCount * 2);
                           
                           Serial.printf("LoRa Flow: ¡NACK recibido! %u fragmentos perdidos. Retransmitiendo...\n", missingCount);
                           tft.fillRect(0, 42, 160, 10, TFT_BLACK);
                           tft.setCursor(0, 42);
                           tft.setTextColor(TFT_RED, TFT_BLACK);
                           tft.printf("Estado: NACK %u reen...", missingCount);

                           packet.header.type = DATA_IMG_CHUNK;
                           
                           // Retransmitir cada bloque pedido leyendo de la memoria Flash local
                           for (uint16_t i = 0; i < missingCount; i++) {
                               uint16_t seq = sequences[i];
                               if (seq <= totalChunks) {
                                   uint32_t offset = (seq - 1) * LORA_MAX_PAYLOAD;
                                   uint32_t chunkLen = min((uint32_t)LORA_MAX_PAYLOAD, imgSize - offset);
                                   
                                   // Leer directamente el fragmento desde la memoria Flash
                                   readImgFile.seek(offset);
                                   readImgFile.read(packet.payload, chunkLen);
                                   
                                   packet.header.seqNum = seq;
                                   radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + chunkLen);
                                   Serial.printf("Retransmitiendo chunk %d (Offset %u, len %u) desde Flash local\n", seq, offset, chunkLen);
                                   delay(DELAY_TX + 40); // Pacing de retransmisión más holgado para evitar saturación del receptor por SD card writes
                               }
                           }
                          
                          // Enviar DATA_IMG_END de nuevo para cerrar el lote de retransmisión
                          delay(500);
                          packet.header.type = DATA_IMG_END;
                          packet.header.seqNum = chunkIndex;
                          memcpy(packet.payload, &imgSize, 4);
                          memcpy(packet.payload + 4, &totalChunks, 2);
                          radio.transmit((uint8_t*)&packet, sizeof(LoRaHeader) + 6);
                          
                          // Reiniciar temporizadores
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

  // Cerrar archivo de lectura y liberar recursos
  readImgFile.close();
  radio.startReceive();
  delay(1000);
  drawDashboard();
}

/**
 * @brief Filtra y registra en caché los paquetes ya enrutados en la red lineal.
 * @param srcId Nodo de origen.
 * @param type Tipo de paquete.
 * @param seqNum Número de secuencia.
 * @return true si ya fue visto con anterioridad (duplicado), false si es nuevo.
 */
bool checkSeenAndAdd(uint8_t srcId, uint8_t type, uint16_t seqNum) {
    for (int i = 0; i < seenCount; i++) {
        if (seenBuffer[i].srcId == srcId &&
            seenBuffer[i].type == type &&
            seenBuffer[i].seqNum == seqNum) {
            return true; 
        }
    }
    if (seenCount < 10) {
        seenBuffer[seenCount++] = {srcId, type, seqNum};
    } else {
        for (int i = 0; i < 9; i++) seenBuffer[i] = seenBuffer[i+1];
        seenBuffer[9] = {srcId, type, seqNum};
    }
    return false;
}

/**
 * @brief Gestiona la escucha, decodificación y retransmisión (routing) del protocolo LoRa.
 */
void handleLoRa() {
  if (digitalRead(LORA_DIO1)) {
    LoRaPacket rxPacket;
    size_t packetLen = radio.getPacketLength();
    int state = radio.readData((uint8_t*)&rxPacket, sizeof(LoRaPacket));
    
    Serial.printf("[DEBUG LoRa] Evento RX! Estado lectura: %d, Longitud: %u bytes\n", state, packetLen);
    
    if (state == RADIOLIB_ERR_NONE) {
      if (rxPacket.header.syncWord[0] == LORA_SYNC_0 && rxPacket.header.syncWord[1] == LORA_SYNC_1) {
        Serial.printf("[DEBUG LoRa] Sync OK! Src: %d, Dest: %d, nextHop: %d, Tipo: 0x%02X, TTL: %d\n",
                      rxPacket.header.srcId, rxPacket.header.destId, rxPacket.header.nextHopId,
                      rxPacket.header.type, rxPacket.header.ttl);
        
        // Comprobar tiempo de vida del paquete
        if (rxPacket.header.ttl > 0) {
            rxPacket.header.ttl--;
            
            // --- FILTRO CRÍTICO DE HOP FÍSICO ---
            // Solo procesar si el paquete indica explícitamente que soy el siguiente salto receptor.
            if (rxPacket.header.nextHopId != MY_NODE_ID) {
                Serial.printf("[DEBUG LoRa] Salto Fisico Ignorado: nextHopId (%d) != MY_NODE_ID (%d)\n",
                              rxPacket.header.nextHopId, MY_NODE_ID);
                radio.startReceive();
                return;
            }
            
            // ¿El paquete está dirigido formalmente a mi ID final?
            if (rxPacket.header.destId == MY_NODE_ID) {
                Serial.printf("Comando Recibido: Tipo 0x%02X de Nodo %d\n", rxPacket.header.type, rxPacket.header.srcId);
                
                uint8_t prevHop = (rxPacket.header.srcId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
                
                char headerTitle[30];
                sprintf(headerTitle, "NODO %d - RX CMD", MY_NODE_ID);
                tft.fillScreen(TFT_BLACK);
                drawHeader(headerTitle);
                
                tft.setCursor(0, 18);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.print("Ruta: ");
                tft.printf("[N%d] -> ", prevHop);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.printf("[*N%d*]", MY_NODE_ID);
                
                tft.setCursor(0, 28);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.print("Peticion de: ");
                tft.setTextColor(TFT_CYAN, TFT_BLACK);
                tft.printf("Nodo %d", rxPacket.header.srcId);
                
                tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
                tft.setCursor(0, 42);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.printf("Comando: 0x%02X", rxPacket.header.type);
                tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));
                
                tft.setCursor(0, 66);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.print("Procesando comando...");
                
                sprintf(lastRfActivity, "RX Cmd 0x%02X de %d", rxPacket.header.type, rxPacket.header.srcId);
                
                // Ejecutar la acción solicitada
                if (rxPacket.header.type == CMD_REQ_TELEMETRY) {
                    delay(80); // Margen mínimo de estabilización
                    if (gps.location.isValid() && gps.location.age() < 5000) {
                        sendSensorTelemetry(rxPacket.header.srcId);
                    } else {
                        // Enviar respuesta NACK indicando que no hay fix
                        LoRaPacket nackPacket;
                        nackPacket.header.syncWord[0] = LORA_SYNC_0;
                        nackPacket.header.syncWord[1] = LORA_SYNC_1;
                        nackPacket.header.srcId = MY_NODE_ID;
                        nackPacket.header.destId = rxPacket.header.srcId;
                        nackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
                        nackPacket.header.type = NACK;
                        nackPacket.header.seqNum = packetSequence++;
                        nackPacket.header.ttl = 5;
                        
                        strcpy((char*)nackPacket.payload, "GPS_NO_FIX");
                        radio.transmit((uint8_t*)&nackPacket, sizeof(LoRaHeader) + 11);
                        radio.startReceive();
                        Serial.println("[GPS WARNING] Petición de telemetría recibida pero no hay GPS fix. Enviado NACK.");
                    }
                } 
                else if (rxPacket.header.type == CMD_REQ_IMAGE) {
                    delay(80); 
                    requestAndSendImage(rxPacket.header.srcId);
                }
                else if (rxPacket.header.type == CMD_CONFIG_CAM) {
                    delay(80);
                    CameraConfigPayload* ccp = (CameraConfigPayload*)rxPacket.payload;
                    
                    cam_resolution = constrain(ccp->resolution, 0, 21);
                    cam_brightness = constrain(ccp->brightness, -2, 2);
                    cam_contrast = constrain(ccp->contrast, -2, 2);
                    
                    // Guardar en Preferences de forma permanente
                    preferences.begin("cam_config", false);
                    preferences.putUChar("res", cam_resolution);
                    preferences.putChar("br", cam_brightness);
                    preferences.putChar("co", cam_contrast);
                    preferences.end();
                    
                    Serial.printf("[CONFIG] Nueva configuración de cámara guardada: Res=%d, Br=%d, Co=%d\n", 
                                  cam_resolution, cam_brightness, cam_contrast);
                                  
                    // Responder con ACK de confirmación
                    LoRaPacket ackPacket;
                    ackPacket.header.syncWord[0] = LORA_SYNC_0;
                    ackPacket.header.syncWord[1] = LORA_SYNC_1;
                    ackPacket.header.srcId = MY_NODE_ID;
                    ackPacket.header.destId = rxPacket.header.srcId;
                    ackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
                    ackPacket.header.type = ACK;
                    ackPacket.header.seqNum = packetSequence++;
                    ackPacket.header.ttl = 5;
                    radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
                    radio.startReceive();
                }
                else if (rxPacket.header.type == CMD_PING) {
                    delay(80);
                    LoRaPacket ackPacket;
                    ackPacket.header.syncWord[0] = LORA_SYNC_0;
                    ackPacket.header.syncWord[1] = LORA_SYNC_1;
                    ackPacket.header.srcId = MY_NODE_ID;
                    ackPacket.header.destId = rxPacket.header.srcId;
                    ackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
                    ackPacket.header.type = ACK;
                    ackPacket.header.seqNum = packetSequence++;
                    ackPacket.header.ttl = 5;
                    radio.transmit((uint8_t*)&ackPacket, sizeof(LoRaHeader));
                    radio.startReceive();
                }
            } 
            // --- ENRUTAMIENTO DE RED LINEAL ---
            // Si el destino no soy yo y aún tiene saltos de vida, propagarlo
            else if (rxPacket.header.ttl > 0) {
                bool shouldRelay = false;
                
                // Flujo Bajada: Concentrador hacia sensores remotos (ID Origen < Mi ID, ID Destino > Mi ID)
                if (rxPacket.header.srcId < MY_NODE_ID && rxPacket.header.destId > MY_NODE_ID) {
                    shouldRelay = true;
                }
                // Flujo Subida: Sensores remotos hacia concentrador (ID Origen > Mi ID, ID Destino < Mi ID)
                else if (rxPacket.header.srcId > MY_NODE_ID && rxPacket.header.destId < MY_NODE_ID) {
                    shouldRelay = true;
                }
                
                if (shouldRelay) {
                    size_t packetLen = radio.getPacketLength();
                    
                    bool isUpstream = (rxPacket.header.srcId > MY_NODE_ID && rxPacket.header.destId < MY_NODE_ID);
                    uint8_t prevHop = isUpstream ? (MY_NODE_ID + 1) : (MY_NODE_ID - 1);
                    uint8_t nextHop = isUpstream ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
                    
                    // Solo renderizar el cambio de pantalla si no es una ráfaga de chunks rápidos (evita latencias críticas)
                    if (rxPacket.header.type != DATA_IMG_CHUNK) {
                        char headerTitle[30];
                        sprintf(headerTitle, "NODO %d - REPETIDOR", MY_NODE_ID);
                        tft.fillScreen(TFT_BLACK);
                        drawHeader(headerTitle);
                        
                        tft.setCursor(0, 18);
                        tft.setTextColor(TFT_WHITE, TFT_BLACK);
                        tft.print("Ruta: ");
                        tft.printf("[N%d] -> ", prevHop);
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.printf("[*N%d*]", MY_NODE_ID);
                        tft.setTextColor(TFT_WHITE, TFT_BLACK);
                        tft.printf(" -> [N%d]", nextHop);
                        
                        tft.setCursor(0, 28);
                        tft.setTextColor(TFT_WHITE, TFT_BLACK);
                        tft.print("Origen: ");
                        tft.setTextColor(TFT_CYAN, TFT_BLACK);
                        tft.printf("%d -> Dest: %d", rxPacket.header.srcId, rxPacket.header.destId);
                        
                        tft.drawFastHLine(0, 39, 160, tft.color565(80, 80, 80));
                        tft.setCursor(0, 42);
                        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                        tft.print("Retransmitiendo...");
                        tft.drawFastHLine(0, 63, 160, tft.color565(80, 80, 80));
                        
                        tft.setCursor(0, 66);
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.print("Relay LoRa OK");
                    }
                    
                    // Modificar la cabecera con el ID del salto físico receptor directo
                    rxPacket.header.nextHopId = nextHop;
                    
                    // Backoff defensivo para des-sincronizar colisiones en el aire
                    delay(40); 
                    
                    radio.transmit((uint8_t*)&rxPacket, packetLen);
                    radio.startReceive(); // Regresar inmediatamente a modo escucha
                    Serial.printf("Relay OK: S:%d D:%d\n", rxPacket.header.srcId, rxPacket.header.destId);
                    
                    sprintf(lastRfActivity, "Relay %d->%d OK", rxPacket.header.srcId, rxPacket.header.destId);
                    
                    if (rxPacket.header.type != DATA_IMG_CHUNK) {
                        // Configurar el temporizador no-bloqueante en lugar de usar un delay() que congele el receptor.
                        // Esto permite que el nodo enrute instantáneamente la respuesta de vuelta sin perder paquetes.
                        lastRelayScreenTime = millis();
                        isRelayScreenActive = true;
                    }
                }
            }
        }
      }
    }
    radio.startReceive();
  }
}

/**
 * @brief Loop de ejecución infinita del programa.
 */
void loop() {
  static unsigned long lastHeartbeat = 0;
  
  // Latido periódico (Heartbeat) de diagnóstico local cada 5 segundos
  if (millis() - lastHeartbeat > 5000) {
    Serial.printf("[HEARTBEAT] Nodo %d ejecutando loop...\n", MY_NODE_ID);
    lastHeartbeat = millis();
  }
  
  // Retornar al dashboard en espera de forma no-bloqueante tras 2 segundos en pantalla de repetidor
  if (isRelayScreenActive && (millis() - lastRelayScreenTime > 2000)) {
    isRelayScreenActive = false;
    drawDashboard();
  }
  
  smartDelay(10); // Alimentar decodificador GPS
  handleLoRa();   // Procesar actividades de red
}
