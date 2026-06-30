/**
 * @file main.cpp
 * @brief Firmware de Producción para los Nodos Sensores de MONICA.
 *
 * Este programa administra la lectura y retransmisión de datos ambientales (ph,
 * oxígeno disuelto, turbidez, conductividad, temperatura, nivel de batería) y
 * fragmentos de imagen tomados por una cámara acoplada por puerto serie UART.
 * Cuenta con soporte integrado para enrutamiento lineal multi-salto (multi-hop)
 * por hardware LoRa SX1262 y visualización a través de una pantalla TFT.
 *
 * Arquitectura de Red:
 *   [Concentrador 0] <-> [Nodo 1] <-> [Nodo 2] <-> [Nodo 3] <-> [Nodo 4]
 *
 * Diseñado con mecanismos de optimización de batería (pacing dinámico de
 * colisiones) e inmunidad a loops infinitos de retransmisión mediante
 * validación estricta del salto físico receptor.
 */

#include "LittleFS.h"
#include "lora_protocol.h"
#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TinyGPS++.h>

// --- Configuración Remota de la Cámara (Persistida en Preferences) ---
Preferences preferences;
uint8_t cam_resolution = 10; // XGA por defecto
int8_t cam_brightness = 0;   // -2 a 2
int8_t cam_contrast = 1;     // -2 a 2
uint8_t cam_quality = 24;    // Calidad JPEG (10-63, default balanceado)
int8_t cam_saturation = 0;
uint8_t cam_special_effect = 0;
uint8_t cam_whitebal = 1;
uint8_t cam_awb_gain = 1;
uint8_t cam_wb_mode = 0;
uint8_t cam_exposure_ctrl = 1;
uint8_t cam_aec2 = 0;
int8_t cam_ae_level = 0;
uint16_t cam_aec_value = 300;
uint8_t cam_gain_ctrl = 1;
uint8_t cam_agc_gain = 0;
uint8_t cam_gainceiling = 0;
uint8_t cam_bpc = 0;
uint8_t cam_wpc = 1;
uint8_t cam_raw_gma = 1;
uint8_t cam_lenc = 1;
uint8_t cam_hmirror = 0;
uint8_t cam_vflip = 0;
uint8_t cam_dcw = 1;
uint8_t cam_colorbar = 0;

// --- Identificador Único de este Nodo Sensor ---
// Modificar este valor de 1 a 4 según la posición física del nodo en la
// topología lineal.
#define MY_NODE_ID 4

// --- Definición de Pines de Hardware (Heltec Wireless Tracker v1.1) ---
#if MY_NODE_ID == 2
#define VEXT_PIN 37 // Control de energía para V1.0 (Nodo 2)
#else
#define VEXT_PIN 3 // Control de energía para V1.1 (Nodo 1 y Concentrador)
#endif
#define BACKLIGHT_PIN 21 // Pin de control del brillo de la pantalla TFT.
#define GNSS_RX_PIN                                                            \
  33 // Pin de recepción UART para el módulo de posicionamiento GNSS (GPS).
#define GNSS_TX_PIN                                                            \
  34 // Pin de transmisión UART para el módulo de posicionamiento GNSS (GPS).

// --- Pines del Bus SPI para el Transceptor LoRa (Internos de la placa Heltec)
// ---
#define LORA_CS 8    // Chip Select (Selector de Chip SPI).
#define LORA_SCK 9   // Reloj SPI (Serial Clock).
#define LORA_MOSI 10 // Salida de Datos SPI (Master Out Slave In).
#define LORA_MISO 11 // Entrada de Datos SPI (Master In Slave Out).
#define LORA_RST 12  // Pin de reinicio de hardware del SX1262.
#define LORA_BUSY 13 // Pin indicador de estado ocupado del chip LoRa.
#define LORA_DIO1                                                              \
  14 // Pin de interrupción para notificaciones de RX/TX completados.

// --- Tiempos de Transmisión Dinámicos (Pacing) según el número de saltos al
// Concentrador --- A mayor cantidad de saltos de distancia física, mayor debe
// ser el tiempo de gracia (pacing) para dar margen a que los nodos repetidores
// retransmitan el paquete anterior en la línea sin colisionar.
#if MY_NODE_ID == 1
#define DELAY_TX 40  // 1 salto al concentrador directo: delay corto optimizado.
#elif MY_NODE_ID == 2
#define DELAY_TX 150 // 2 saltos al concentrador: delay medio optimizado.
#elif MY_NODE_ID == 3
#define DELAY_TX 300 // 3 saltos al concentrador: delay largo optimizado.
#else
#define DELAY_TX 450 // 4 saltos: pacing ultra rápido optimizado.
#endif

// --- Pines de Comunicación para la Cámara ESP32-CAM (Puerto Serie UART) ---
#define CAM_UART_TX 17 // Pin de transmisión de comandos hacia la cámara.
#define CAM_UART_RX                                                            \
  18 // Pin de recepción de fragmentos de imagen desde la cámara.
#define CAM_EN                                                                 \
  6 // Pin de control de alimentación (Habilitador/Power-On) de la cámara.

// --- Instanciación de Objetos Principales ---
TinyGPSPlus gps; // Decodificador del protocolo NMEA para geolocalización.
TFT_eSPI tft =
    TFT_eSPI(); // Controlador gráfico para la pantalla LCD/TFT integrada.
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST,
                          LORA_BUSY); // Controlador LoRa SX1262 vía RadioLib.
HardwareSerial camSerial(
    2); // Interfaz UART secundaria (Serial2) para transferencias de cámara.

// --- Variables de Estado Global ---
uint16_t packetSequence =
    0; // Contador incremental autoincrementado para paquetes LoRa salientes.

// --- Estructuras y Búferes de Caché del Repetidor ---
struct SeenPacket {
  uint8_t srcId;   // ID de origen del nodo que generó la transmisión.
  uint8_t type;    // Tipo del paquete recibido (CMD o DATA).
  uint16_t seqNum; // Número de secuencia.
};
SeenPacket seenBuffer[10]; // Registro de historial para evitar bucles infinitos
                           // de retransmisión de paquetes ya propagados.
int seenCount =
    0; // Contador dinámico de elementos almacenados en el búfer circular.

/**
 * @brief Retardo inteligente que procesa y decodifica las ráfagas GPS NMEA en
 * segundo plano.
 * @param ms Tiempo en milisegundos que durará la espera.
 *
 * Protege contra bloqueos infinitos limitando el procesamiento a 50 bytes por
 * ciclo de lectura en caso de que exista ruido electromagnético o flotación en
 * el pin RX de hardware.
 */
// Tarea en segundo plano para decodificar GPS en el Core 0
void gpsTask(void *pvParameters) {
  Serial.println("[GPS] Tarea iniciada en el Core 0");
  while (true) {
    while (Serial1.available()) {
      gps.encode(Serial1.read());
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Pausa de 10ms para no acaparar el núcleo
  }
}

static void smartDelay(unsigned long ms) { delay(ms); }

// --- Variables del Panel Gráfico ---
char lastRfActivity[30] = "Ninguna"; // Cadena formateada para desplegar la
                                     // última actividad RF en el dashboard.

float getBatteryVoltage() {
  // GPIO2 es Vext_ctrl (enciende TFT y resetea UC6580). Debe permanecer en
  // HIGH.
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  delay(10); // Estabilizar

  // analogReadMilliVolts obtiene los mV calibrados internamente por el ESP32-S3
  float mv = analogReadMilliVolts(1);

  // Relación del divisor de tensión de la placa: 4.9 (resistores de 390k y
  // 100k)
  float voltage = (mv * 4.9) / 1000.0;
  return voltage;
}

uint8_t getBatteryPercent() {
  float voltage = getBatteryVoltage();
  if (voltage >= 4.2)
    return 100;
  if (voltage <= 3.2)
    return 0;
  return (uint8_t)((voltage - 3.2) * 100.0);
}

// --- Variables de Control No-Bloqueante para Pantalla de Repetidor ---
unsigned long lastRelayScreenTime =
    0; // Registra cuándo se mostró la pantalla de retransmisión.
bool isRelayScreenActive = false; // Bandera para indicar que la pantalla de
                                  // retransmisión está visible.

// Icono de Satélite de 8x8 píxeles
const uint8_t satellite_icon[] PROGMEM = {0x00, 0x60, 0x68, 0x1c,
                                          0x38, 0x76, 0x26, 0x00};

// Variables de estado de pantalla para el Nodo Sensor
String screenStatus = "ESCUCHANDO";
String screenActivity = "Listo";
String screenProgress = "";

/**
 * @brief Dibuja la barra de título premium del dashboard.
 * @param title Cadena de texto a mostrar en el centro de la barra superior.
 */
void drawHeader(const char *title) {
  tft.fillRect(0, 0, 160, 15, tft.color565(0, 51, 102)); // Fondo Azul Marino.
  tft.drawFastHLine(0, 15, 160,
                    tft.color565(200, 200, 200)); // Línea de separación Plata.
  tft.setTextColor(TFT_WHITE, tft.color565(0, 51, 102));
  tft.setTextSize(1);
  tft.setCursor((160 - strlen(title) * 6) / 2,
                4); // Centrado automático de tipografía de ancho fijo.
  tft.print(title);
}

/**
 * @brief Dibuja un icono visual de batería según el porcentaje de carga.
 */
void drawBatteryIcon(int x, int y, int percent) {
  tft.drawRect(x, y, 12, 7, TFT_WHITE);
  tft.fillRect(x + 12, y + 2, 2, 3, TFT_WHITE); // Polo positivo

  uint16_t color = TFT_GREEN;
  if (percent < 20)
    color = TFT_RED;
  else if (percent < 50)
    color = TFT_YELLOW;

  int w = (percent * 10) / 100;
  if (w > 10)
    w = 10;
  if (w > 0) {
    tft.fillRect(x + 1, y + 1, w, 5, color);
  }
}

/**
 * @brief Renderiza la pantalla principal del panel en modo unificado y
 * profesional.
 */
void updateTFT() {
  tft.fillScreen(TFT_BLACK);

  // 1. Cabecera (Azul Marino Premium)
  char title[25];
  sprintf(title, "MONICA NODO %d", MY_NODE_ID);
  drawHeader(title);

  // 2. Fila de Estados (GPS, BAT) en y = 18
  tft.setCursor(4, 18);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("GPS:");
  bool gpsFixed = gps.location.isValid();
  tft.setTextColor(gpsFixed ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  tft.print(gpsFixed ? "FIX" : "NO");

  // Icono de batería y texto (medición real)
  uint8_t batPercent = getBatteryPercent();
  drawBatteryIcon(115, 18, batPercent);
  tft.setCursor(132, 18);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf("%d%%", batPercent);

  // Separador intermedio (y = 28)
  tft.drawFastHLine(0, 28, 160, tft.color565(80, 80, 80));

  // 3. Tarjeta central de Estado (y = 32 a 58)
  tft.drawRoundRect(4, 32, 152, 26, 3, tft.color565(120, 120, 120));
  tft.setCursor(8, 36);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(screenStatus);

  tft.setCursor(8, 46);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print(screenActivity);

  // 4. Progreso / Logs Inferiores (y = 62 a 79)
  tft.drawFastHLine(0, 62, 160, tft.color565(80, 80, 80));
  tft.setCursor(4, 67);
  if (screenProgress.length() > 0) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print(screenProgress);
  } else {
    // Dibujar el icono de satélite y cantidad de satélites conectados
    tft.drawBitmap(4, 67, satellite_icon, 8, 8,
                   gps.satellites.isValid() && gps.satellites.value() > 0
                       ? TFT_GREEN
                       : TFT_YELLOW);
    tft.setCursor(16, 67);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.printf("Sats: %d",
               gps.satellites.isValid() ? gps.satellites.value() : 0);

    tft.setCursor(80, 67);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.printf("Pacing: %d", DELAY_TX);
  }
}

/**
 * @brief Actualiza de forma segura el estado de la pantalla y fuerza el
 * redibujado.
 */
void setScreenStatus(String status, String activity, String progress = "") {
  screenStatus = status;
  screenActivity = activity;
  screenProgress = progress;
  updateTFT();
}

/**
 * @brief Función de envoltura heredada para mantener la compatibilidad en todo
 * el código.
 */
void drawDashboard() { updateTFT(); }

/**
 * @brief Configuración inicial del hardware del dispositivo.
 */
void setup() {
  Serial.begin(115200);

  preferences.begin("cam_config", false);
  cam_resolution = preferences.getUChar("res", 10); // XGA por defecto
  cam_brightness = preferences.getChar("br", 0);
  cam_contrast = preferences.getChar("co", 1);
  cam_quality = preferences.getUChar("qty", 24);
  cam_saturation = preferences.getChar("sat", 0);
  cam_special_effect = preferences.getUChar("ef", 0);
  cam_whitebal = preferences.getUChar("wb", 1);
  cam_awb_gain = preferences.getUChar("awg", 1);
  cam_wb_mode = preferences.getUChar("wbm", 0);
  cam_exposure_ctrl = preferences.getUChar("ec", 1);
  cam_aec2 = preferences.getUChar("aec2", 0);
  cam_ae_level = preferences.getChar("ael", 0);
  cam_aec_value = preferences.getUShort("aev", 300);
  cam_gain_ctrl = preferences.getUChar("gc", 1);
  cam_agc_gain = preferences.getUChar("agg", 0);
  cam_gainceiling = preferences.getUChar("gcl", 0);
  cam_bpc = preferences.getUChar("bpc", 0);
  cam_wpc = preferences.getUChar("wpc", 1);
  cam_raw_gma = preferences.getUChar("rgm", 1);
  cam_lenc = preferences.getUChar("lnc", 1);
  cam_hmirror = preferences.getUChar("hmr", 0);
  cam_vflip = preferences.getUChar("vfl", 0);
  cam_dcw = preferences.getUChar("dcw", 1);
  cam_colorbar = preferences.getUChar("cbr", 0);
  preferences.end();
  Serial.printf("[CONFIG] Cámara cargada de Preferences. Res=%d, Br=%d, Co=%d, "
                "Qty=%d, Sat=%d\n",
                cam_resolution, cam_brightness, cam_contrast, cam_quality,
                cam_saturation);

  // Inicializar LittleFS en la memoria Flash externa para almacenamiento
  // temporal
  if (!LittleFS.begin(true)) {
    Serial.println("[ERROR] Error al montar LittleFS");
  } else {
    Serial.println("[INFO] LittleFS montado correctamente");
  }

  // Reservar suficiente búfer de hardware en RX UART para evitar el desborde y
  // pérdida de bytes de cámara
  camSerial.setRxBufferSize(4096);
  camSerial.begin(115200, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);

  // Configuración y encendido de reguladores internos de hardware
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  pinMode(37, OUTPUT);
  digitalWrite(37, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN,
               HIGH); // Encender retroiluminación de pantalla LCD.
  pinMode(CAM_EN, OUTPUT);
  digitalWrite(
      CAM_EN,
      LOW); // Apagar la cámara al inicio para conservar energía (Batería).

  delay(100);

  // Inicialización de la Pantalla TFT
  tft.init();
  tft.setRotation(1); // Ajustar orientación horizontal del display.
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
  xTaskCreatePinnedToCore(gpsTask, "gpsTask", 4096, NULL, 1, NULL, 0);

  // Inicializar bus SPI compartido con la pantalla para comunicarse con el
  // módulo LoRa SX1262
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Serial.println("Iniciando transceptor LoRa SX1262...");
  tft.println("Iniciando LoRa...");

  int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SF, LORA_CR,
                          LORA_SYNC_WORD, LORA_POWER, LORA_PREAMBLE_LEN,
                          LORA_TCXO_VOLTAGE, LORA_USE_REGULATOR);

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
 * @param destId Identificador único del nodo de destino (normalmente 0, que es
 * el concentrador).
 */
void sendSensorTelemetry(uint8_t destId) {
  LoRaPacket packet;
  memset(&packet, 0, sizeof(LoRaPacket));

  // Construir cabecera del protocolo LoRa unificado de MONICA
  packet.header.syncWord[0] = LORA_SYNC_0;
  packet.header.syncWord[1] = LORA_SYNC_1;
  packet.header.srcId = MY_NODE_ID;
  packet.header.destId = destId;

  // Enrutamiento Lineal: Si viaja al concentrador (hacia ID menor), el
  // siguiente salto físico es mi ID - 1
  packet.header.nextHopId =
      (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
  packet.header.type = DATA_TELEMETRY;
  packet.header.seqNum = packetSequence++;
  packet.header.ttl = 5; // Evita la propagación perpetua de ruido en el aire

  // Cargar payload de sensores con lecturas ambientales
  SensorPayload *sp = (SensorPayload *)packet.payload;
  sp->latitude = gps.location.isValid() ? gps.location.lat() : 0.0;
  sp->longitude = gps.location.isValid() ? gps.location.lng() : 0.0;
  sp->ph = random(60, 80) / 10.0;
  sp->dissolved_oxygen = random(70, 90) / 10.0;
  sp->turbidity = random(0, 50);
  sp->conductivity = random(400, 600);
  sp->temperature = random(200, 300) / 10.0;
  sp->battery_level = getBatteryPercent();

  size_t packetSize = sizeof(LoRaHeader) + sizeof(SensorPayload);
  uint8_t nextHop = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);

  // Actualizar pantalla con el diseño unificado
  setScreenStatus("ENVIANDO TELEM", "Dest: Conc (" + String(destId) + ")", "Pacing: " + String(DELAY_TX) + " ms");

  // Transmitir paquete por LoRa
  int state = radio.transmit((uint8_t *)&packet, packetSize);

  if (state == RADIOLIB_ERR_NONE) {
    setScreenStatus("TELEMETRIA OK", "Transmitido!", "QoS: Directo");
    Serial.println("Telemetria enviada con exito");
    strcpy(lastRfActivity, "TX Telem OK");
  } else {
    setScreenStatus("TELEMETRIA ERR", "Fallo al enviar", "Error: " + String(state));
    sprintf(lastRfActivity, "TX Telem ERR %d", state);
  }

  // Regresar inmediatamente al modo de escucha
  radio.startReceive();
  delay(1500); // Dar margen para visualizar la información en pantalla
  drawDashboard();
}

/**
 * @brief Solicita un fragmento de bytes específico a la cámara UART con
 * reintentos robustos.
 * @param offset Desplazamiento en bytes de la imagen a solicitar.
 * @param len Longitud del fragmento de imagen a leer.
 * @param dest Búfer de destino en memoria RAM para guardar el fragmento
 * recibido.
 * @return true si la lectura fue completa y exitosa, false en caso contrario.
 *
 * Implementa 3 intentos robustos de lectura con vaciado de ruido residual del
 * canal serie.
 */
bool getChunkFromCam(uint32_t offset, uint32_t len, uint8_t *dest) {
  unsigned long timeoutLimit =
      1000 + len / 10; // Calcular timeout dinámico: 1s base + 100ms por cada
                       // 1KB (1000 baud UART safety)
  for (int retry = 0; retry < 3; retry++) {
    while (camSerial.available())
      camSerial.read(); // Limpiar ruidos o bytes colgados en el búfer RX

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
    Serial.printf("Intento %d: getChunkFromCam falló (leído %u de %u)\n",
                  retry + 1, readBytes, len);
    delay(50); // Pausa estretégica antes de reintentar
  }
  return false; // Falló de comunicación con la cámara
}

/**
 * @brief Captura y transmite secuencialmente una imagen por bloques LoRa de
 * ancho máximo (LORA_MAX_PAYLOAD).
 * @param destId Identificador único del nodo de destino (Concentrador).
 *
 * Flujo Completo:
 *  1. Envía un paquete ACK para indicar la recepción del comando y encender la
 * cámara.
 *  2. Habilita y enciende la cámara, toma la foto (`TAKE_PIC`) y lee el tamaño
 * total de la imagen.
 *  3. Transmite el paquete inicial `DATA_IMG_START` con los metadatos de la
 * imagen.
 *  4. Envía cada fragmento `DATA_IMG_CHUNK` con un pacing dinámico controlado
 * (`DELAY_TX`).
 *  5. Envía `DATA_IMG_END` e inicia la máquina de retransmisión selectiva (NACK
 * / `CMD_REQ_MISSING`).
 *  6. Vuelve a transmitir fragmentos faltantes solicitados hasta completar la
 * confirmación final (`ACK`).
 */
void requestAndSendImage(uint8_t destId) {
  // Enviar ACK inicial de gracia
  LoRaPacket ackPacket;
  ackPacket.header.syncWord[0] = LORA_SYNC_0;
  ackPacket.header.syncWord[1] = LORA_SYNC_1;
  ackPacket.header.srcId = MY_NODE_ID;
  ackPacket.header.destId = destId;
  ackPacket.header.nextHopId =
      (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
  ackPacket.header.type = ACK;
  ackPacket.header.seqNum = packetSequence++;
  ackPacket.header.ttl = 5;
  radio.transmit((uint8_t *)&ackPacket, sizeof(LoRaHeader));
  delay(100);

  // Inicializar interfaz gráfica de captura
  setScreenStatus("CAPTURA CAMARA", "Tomando foto...", "Espere 2 segundos...");

  // Encender la cámara ESP32-CAM físicamente liberando energía
  Serial.println(
      "[CAMERA DEBUG] Encendiendo camara fisicamente (CAM_EN = HIGH)...");
  digitalWrite(CAM_EN, HIGH);
  delay(2000); // Tiempo de arranque necesario para inicialización de CMOS de
               // cámara

  Serial.println("[CAMERA DEBUG] Vaciando buffer RX de camSerial...");
  while (camSerial.available())
    camSerial.read(); // Limpiar remanentes serie

  // Solicitar disparo de foto directamente (la camara recuerda su ultima configuracion en NVS)
  Serial.println("[CAMERA DEBUG] Enviando TAKE_PIC a la camara...");
  camSerial.println("TAKE_PIC");

  unsigned long waitStart = millis();
  bool imgIncoming = false;
  uint32_t imgSize = 0;

  // Leer tamaño de imagen entrante en forma de palabra (ej: "SIZE:12345")
  Serial.println("[CAMERA DEBUG] Esperando respuesta de tamano (SIZE)...");
  while (millis() - waitStart < 8000) {
    if (camSerial.available()) {
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

  // Comprobación de límites lógicos para imagen en Flash (soportamos hasta 2MB
  // en Flash)
  if (!imgIncoming || imgSize <= 0 || imgSize > 2097152) {
    Serial.printf("[CAMERA DEBUG] ERROR: Peticion de imagen fallida o tamano "
                  "excede el limite. imgIncoming=%d, imgSize=%u\n",
                  imgIncoming, imgSize);
                  
    // Enviar NACK de error de cámara al concentrador
    LoRaPacket nackPacket;
    memset(&nackPacket, 0, sizeof(LoRaHeader));
    nackPacket.header.syncWord[0] = LORA_SYNC_0;
    nackPacket.header.syncWord[1] = LORA_SYNC_1;
    nackPacket.header.srcId = MY_NODE_ID;
    nackPacket.header.destId = destId;
    nackPacket.header.nextHopId = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
    nackPacket.header.type = NACK;
    nackPacket.header.seqNum = packetSequence++;
    nackPacket.header.ttl = 5;
    
    if (!imgIncoming) {
      strcpy((char *)nackPacket.payload, "CAM_INIT_ERR");
    } else {
      strcpy((char *)nackPacket.payload, "CAM_SIZE_ERR");
    }
    radio.transmit((uint8_t *)&nackPacket, sizeof(LoRaHeader) + 13);
    
    setScreenStatus("ERROR CAMARA", "Tamanio invalido", "Apagando camara");
    digitalWrite(CAM_EN, LOW); // Apagar cámara ante error
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  Serial.printf("[CAMERA DEBUG] Tamano de imagen valido recibido: %u bytes. "
                "Creando /temp_img.jpg en LittleFS...\n",
                imgSize);
  // Abrir archivo temporal en la Flash usando LittleFS (modo escritura descarta
  // la foto anterior)
  File imgFile = LittleFS.open("/temp_img.jpg", "w");
  if (!imgFile) {
    Serial.println(
        "[CAMERA DEBUG] ERROR: No se pudo abrir /temp_img.jpg en LittleFS!");
        
    // Enviar NACK de error de almacenamiento al concentrador
    LoRaPacket nackPacket;
    memset(&nackPacket, 0, sizeof(LoRaHeader));
    nackPacket.header.syncWord[0] = LORA_SYNC_0;
    nackPacket.header.syncWord[1] = LORA_SYNC_1;
    nackPacket.header.srcId = MY_NODE_ID;
    nackPacket.header.destId = destId;
    nackPacket.header.nextHopId = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
    nackPacket.header.type = NACK;
    nackPacket.header.seqNum = packetSequence++;
    nackPacket.header.ttl = 5;
    
    strcpy((char *)nackPacket.payload, "FS_WRITE_ERR");
    radio.transmit((uint8_t *)&nackPacket, sizeof(LoRaHeader) + 13);
    
    setScreenStatus("ERROR FLASH", "Err Inicializar", "Apagando camara");
    digitalWrite(CAM_EN, LOW);
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  setScreenStatus("CAMARA LINK", "Descargando...", "Preparando Flash");

  // Descargar en ráfagas de 200 bytes para no saturar la RAM
  Serial.println(
      "[CAMERA DEBUG] Iniciando descarga de fragmentos de camara...");
  uint32_t bytesDownloaded = 0;
  uint8_t tempBuffer[200];
  bool downloadSuccess = true;

  while (bytesDownloaded < imgSize) {
    uint32_t lenToDownload = min((uint32_t)200, imgSize - bytesDownloaded);
    if (getChunkFromCam(bytesDownloaded, lenToDownload, tempBuffer)) {
      imgFile.write(tempBuffer, lenToDownload);
      bytesDownloaded += lenToDownload;

      // Mostrar progreso en pantalla
      setScreenStatus("CAMARA LINK", "Descargando...",
                      String((bytesDownloaded * 100) / imgSize) +
                          "% completado");
    } else {
      downloadSuccess = false;
      Serial.printf(
          "[CAMERA DEBUG] ERROR: Fallo al descargar chunk en offset %u!\n",
          bytesDownloaded);
      break;
    }
  }

  imgFile.close();

  if (!downloadSuccess) {
    Serial.println("[CAMERA DEBUG] ERROR: Descarga incompleta, eliminando "
                   "archivo temporal.");
                   
    // Enviar NACK al concentrador
    LoRaPacket nackPacket;
    memset(&nackPacket, 0, sizeof(LoRaHeader));
    nackPacket.header.syncWord[0] = LORA_SYNC_0;
    nackPacket.header.syncWord[1] = LORA_SYNC_1;
    nackPacket.header.srcId = MY_NODE_ID;
    nackPacket.header.destId = destId;
    nackPacket.header.nextHopId = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);
    nackPacket.header.type = NACK;
    nackPacket.header.seqNum = packetSequence++;
    nackPacket.header.ttl = 5;
    
    strcpy((char *)nackPacket.payload, "CAM_READ_ERR");
    radio.transmit((uint8_t *)&nackPacket, sizeof(LoRaHeader) + 13);
    
    setScreenStatus("ERROR CAMARA", "Descarga fallida", "Archivo borrado");
    LittleFS.remove("/temp_img.jpg");
    digitalWrite(CAM_EN, LOW);
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  Serial.println("[CAMERA DEBUG] Descarga a Flash exitosa. Liberando frame "
                 "buffer en camara y apagando...");
  // --- ¡APAGADO INMEDIATO DE LA CÁMARA PARA AHORRO EXTREMO DE ENERGÍA! ---
  // La imagen ya está guardada al 100% de manera permanente en la memoria Flash
  // del Heltec. Apagamos la cámara físicamente para ahorrar energía durante la
  // lenta transmisión por LoRa.
  camSerial.println("RELEASE_PIC");
  digitalWrite(CAM_EN, LOW);

  uint8_t nextHop = (destId < MY_NODE_ID) ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);

  // Inicializar interfaz de streaming usando el estilo unificado
  setScreenStatus("ENVIANDO FOTO", "Size: " + String(imgSize / 1024.0, 1) + " KB", "Ruta: N" + String(MY_NODE_ID) + "->N" + String(nextHop));

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

  radio.transmit((uint8_t *)&packet, sizeof(LoRaHeader) + 6);
  delay(
      1000); // Tiempo de guarda para inicializar archivo en SD del concentrador

  packet.header.type = DATA_IMG_CHUNK;
  uint16_t chunkIndex = 1;
  uint32_t bytesSent = 0;

  // Abrir el archivo de la imagen en modo lectura para iniciar el streaming
  // LoRa
  File readImgFile = LittleFS.open("/temp_img.jpg", "r");
  if (!readImgFile) {
    setScreenStatus("ERROR FLASH", "Err Abrir temp_img.jpg", "Volviendo...");
    radio.startReceive();
    delay(1500);
    drawDashboard();
    return;
  }

  // Transmisión en bucle de los fragmentos desde la Flash local
  while (bytesSent < imgSize) {
    uint32_t chunkLen = min((uint32_t)LORA_MAX_PAYLOAD, imgSize - bytesSent);

    // Leer directamente el fragmento desde la memoria Flash
    readImgFile.seek(bytesSent);
    readImgFile.read(packet.payload, chunkLen);

    packet.header.seqNum = chunkIndex;
    radio.transmit((uint8_t *)&packet, sizeof(LoRaHeader) + chunkLen);
    bytesSent += chunkLen;

    setScreenStatus("ENVIANDO CHUNKS", "Progreso: " + String(chunkIndex) + "/" + String(totalChunks), "Ruta: N" + String(MY_NODE_ID) + "->N" + String(nextHop));

    // Aplicación estricta de pacing para prevención de colisiones por
    // desbordamiento de búfer repetidor
    delay(DELAY_TX);
    chunkIndex++;
  }

  delay(500);

  // Transmitir END para cerrar el lote
  packet.header.type = DATA_IMG_END;
  packet.header.seqNum = chunkIndex;
  memcpy(packet.payload, &imgSize, 4);
  memcpy(packet.payload + 4, &totalChunks, 2);
  radio.transmit((uint8_t *)&packet, sizeof(LoRaHeader) + 6);

  setScreenStatus("ESPERANDO ACK", "Esp. Confirmacion", "Intento END: 1");

  unsigned long sessionStart = millis();
  unsigned long lastEndTxTime = millis();
  radio.startReceive();

  bool waitingResponse = true;
  uint8_t endRetryCount = 0;

  // --- Bucle de Recuperación NACK Selectiva (ARQ) ---
  while (waitingResponse && millis() - sessionStart <
                                120000) { // Timeout de resguardo de 2 minutos
    // Re-enviar END si pasan 4 segundos sin respuesta del receptor
    if (millis() - lastEndTxTime > 4000) {
      if (endRetryCount < 5) {
        endRetryCount++;
        Serial.printf("LoRa Flow: Timeout de confirmación de 4s. "
                      "Retransmitiendo DATA_IMG_END (intento %u/5)...\n",
                      endRetryCount);

        packet.header.type = DATA_IMG_END;
        packet.header.seqNum = chunkIndex;
        memcpy(packet.payload, &imgSize, 4);
        memcpy(packet.payload + 4, &totalChunks, 2);
        radio.transmit((uint8_t *)&packet, sizeof(LoRaHeader) + 6);

        setScreenStatus("ESPERANDO ACK", "Retransmitiendo END", "Intento END: " + String(endRetryCount + 1));

        lastEndTxTime = millis();
        radio.startReceive();
      } else {
        Serial.println("LoRa Flow: Superado el límite de reintentos de "
                       "DATA_IMG_END. Cancelando espera.");
        break;
      }
    }

    // Procesar eventos LoRa recibidos
    if (digitalRead(LORA_DIO1)) {
      LoRaPacket rxPacket;
      int state = radio.readData((uint8_t *)&rxPacket, sizeof(rxPacket));
      if (state == RADIOLIB_ERR_NONE) {
        if (rxPacket.header.syncWord[0] == LORA_SYNC_0 &&
            rxPacket.header.syncWord[1] == LORA_SYNC_1) {

          // Validación física crítica del salto: solo procesar si va dirigido a
          // mi ID
          if (rxPacket.header.nextHopId != MY_NODE_ID) {
            radio.startReceive();
            continue;
          }

          if (rxPacket.header.destId == MY_NODE_ID) {
            if (rxPacket.header.type == ACK) {
              // ¡Concentrador recibió todo sin pérdidas!
              Serial.println("LoRa Flow: ¡Concentrador reporta 100% recibido! "
                             "Sesión completada.");
              setScreenStatus("FOTO CONFIRMADA", "Confirmado [OK]", "Completado");
              strcpy(lastRfActivity, "TX Img OK");
              waitingResponse = false;
            } else if (rxPacket.header.type == CMD_REQ_MISSING) {
              // Procesar lista selectiva de fragmentos faltantes
              uint16_t missingCount = 0;
              memcpy(&missingCount, rxPacket.payload, 2);

              // Evitar desbordamiento de búfer y asegurar la alineación de
              // memoria
              if (missingCount > 98)
                missingCount = 98;
              uint16_t sequences[98];
              memcpy(sequences, rxPacket.payload + 2, missingCount * 2);

              Serial.printf("LoRa Flow: ¡NACK recibido! %u fragmentos "
                            "perdidos. Retransmitiendo...\n",
                            missingCount);
              setScreenStatus("NACK RECIBIDO", "Reenviando " + String(missingCount) + " chunks", "Ruta: N" + String(MY_NODE_ID) + "->N" + String(nextHop));

              packet.header.type = DATA_IMG_CHUNK;

              // Retransmitir cada bloque pedido leyendo de la memoria Flash
              // local
              for (uint16_t i = 0; i < missingCount; i++) {
                uint16_t seq = sequences[i];
                if (seq <= totalChunks) {
                  uint32_t offset = (seq - 1) * LORA_MAX_PAYLOAD;
                  uint32_t chunkLen =
                      min((uint32_t)LORA_MAX_PAYLOAD, imgSize - offset);

                  // Leer directamente el fragmento desde la memoria Flash
                  readImgFile.seek(offset);
                  readImgFile.read(packet.payload, chunkLen);

                  packet.header.seqNum = seq;
                  radio.transmit((uint8_t *)&packet,
                                 sizeof(LoRaHeader) + chunkLen);
                  Serial.printf("Retransmitiendo chunk %d (Offset %u, len %u) "
                                "desde Flash local\n",
                                seq, offset, chunkLen);
                  delay(DELAY_TX +
                        40); // Pacing de retransmisión más holgado para evitar
                             // saturación del receptor por SD card writes
                }
              }

              // Enviar DATA_IMG_END de nuevo para cerrar el lote de
              // retransmisión
              delay(500);
              packet.header.type = DATA_IMG_END;
              packet.header.seqNum = chunkIndex;
              memcpy(packet.payload, &imgSize, 4);
              memcpy(packet.payload + 4, &totalChunks, 2);
              radio.transmit((uint8_t *)&packet, sizeof(LoRaHeader) + 6);

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
    if (seenBuffer[i].srcId == srcId && seenBuffer[i].type == type &&
        seenBuffer[i].seqNum == seqNum) {
      return true;
    }
  }
  if (seenCount < 10) {
    seenBuffer[seenCount++] = {srcId, type, seqNum};
  } else {
    for (int i = 0; i < 9; i++)
      seenBuffer[i] = seenBuffer[i + 1];
    seenBuffer[9] = {srcId, type, seqNum};
  }
  return false;
}

/**
 * @brief Gestiona la escucha, decodificación y retransmisión (routing) del
 * protocolo LoRa.
 */
void handleLoRa() {
  if (digitalRead(LORA_DIO1)) {
    LoRaPacket rxPacket;
    size_t packetLen = radio.getPacketLength();
    int state = radio.readData((uint8_t *)&rxPacket, sizeof(LoRaPacket));

    Serial.printf(
        "[DEBUG LoRa] Evento RX! Estado lectura: %d, Longitud: %u bytes\n",
        state, packetLen);

    if (state == RADIOLIB_ERR_NONE) {
      if (rxPacket.header.syncWord[0] == LORA_SYNC_0 &&
          rxPacket.header.syncWord[1] == LORA_SYNC_1) {
        Serial.printf("[DEBUG LoRa] Sync OK! Src: %d, Dest: %d, nextHop: %d, "
                      "Tipo: 0x%02X, TTL: %d\n",
                      rxPacket.header.srcId, rxPacket.header.destId,
                      rxPacket.header.nextHopId, rxPacket.header.type,
                      rxPacket.header.ttl);

        // Comprobar tiempo de vida del paquete
        if (rxPacket.header.ttl > 0) {
          rxPacket.header.ttl--;

          // --- FILTRO CRÍTICO DE HOP FÍSICO ---
          // Solo procesar si el paquete indica explícitamente que soy el
          // siguiente salto receptor.
          if (rxPacket.header.nextHopId != MY_NODE_ID) {
            Serial.printf("[DEBUG LoRa] Salto Fisico Ignorado: nextHopId (%d) "
                          "!= MY_NODE_ID (%d)\n",
                          rxPacket.header.nextHopId, MY_NODE_ID);
            radio.startReceive();
            return;
          }

          // ¿El paquete está dirigido formalmente a mi ID final?
          if (rxPacket.header.destId == MY_NODE_ID) {
            Serial.printf("Comando Recibido: Tipo 0x%02X de Nodo %d\n",
                          rxPacket.header.type, rxPacket.header.srcId);

            uint8_t prevHop = (rxPacket.header.srcId < MY_NODE_ID)
                                  ? (MY_NODE_ID - 1)
                                  : (MY_NODE_ID + 1);

            // Mostrar comando recibido usando la pantalla unificada
            setScreenStatus("CMD RECIBIDO", "Tipo: 0x" + String(rxPacket.header.type, HEX) + " de N" + String(rxPacket.header.srcId), "Ruta: N" + String(prevHop) + "->N" + String(MY_NODE_ID));

            sprintf(lastRfActivity, "RX Cmd 0x%02X de %d", rxPacket.header.type,
                    rxPacket.header.srcId);

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
                nackPacket.header.nextHopId =
                    (rxPacket.header.srcId < MY_NODE_ID) ? (MY_NODE_ID - 1)
                                                         : (MY_NODE_ID + 1);
                nackPacket.header.type = NACK;
                nackPacket.header.seqNum = packetSequence++;
                nackPacket.header.ttl = 5;

                strcpy((char *)nackPacket.payload, "GPS_NO_FIX");
                radio.transmit((uint8_t *)&nackPacket, sizeof(LoRaHeader) + 11);
                radio.startReceive();
                Serial.println("[GPS WARNING] Petición de telemetría recibida "
                               "pero no hay GPS fix. Enviado NACK.");
              }
            } else if (rxPacket.header.type == CMD_REQ_IMAGE) {
              delay(80);
              requestAndSendImage(rxPacket.header.srcId);
            } else if (rxPacket.header.type == CMD_CONFIG_CAM) {
              delay(80);
              CameraConfigPayload *ccp =
                  (CameraConfigPayload *)rxPacket.payload;

              cam_resolution = constrain(ccp->resolution, 0, 21);
              cam_brightness = constrain(ccp->brightness, -2, 2);
              cam_contrast = constrain(ccp->contrast, -2, 2);
              cam_quality = constrain(ccp->quality, 10, 63);
              cam_saturation = constrain(ccp->saturation, -2, 2);
              cam_special_effect = constrain(ccp->special_effect, 0, 6);
              cam_whitebal = constrain(ccp->whitebal, 0, 1);
              cam_awb_gain = constrain(ccp->awb_gain, 0, 1);
              cam_wb_mode = constrain(ccp->wb_mode, 0, 4);
              cam_exposure_ctrl = constrain(ccp->exposure_ctrl, 0, 1);
              cam_aec2 = constrain(ccp->aec2, 0, 1);
              cam_ae_level = constrain(ccp->ae_level, -2, 2);
              cam_aec_value = constrain(ccp->aec_value, 0, 1200);
              cam_gain_ctrl = constrain(ccp->gain_ctrl, 0, 1);
              cam_agc_gain = constrain(ccp->agc_gain, 0, 30);
              cam_gainceiling = constrain(ccp->gainceiling, 0, 6);
              cam_bpc = constrain(ccp->bpc, 0, 1);
              cam_wpc = constrain(ccp->wpc, 0, 1);
              cam_raw_gma = constrain(ccp->raw_gma, 0, 1);
              cam_lenc = constrain(ccp->lenc, 0, 1);
              cam_hmirror = constrain(ccp->hmirror, 0, 1);
              cam_vflip = constrain(ccp->vflip, 0, 1);
              cam_dcw = constrain(ccp->dcw, 0, 1);
              cam_colorbar = constrain(ccp->colorbar, 0, 1);

              // Guardar en Preferences de forma permanente
              preferences.begin("cam_config", false);
              preferences.putUChar("res", cam_resolution);
              preferences.putChar("br", cam_brightness);
              preferences.putChar("co", cam_contrast);
              preferences.putUChar("qty", cam_quality);
              preferences.putChar("sat", cam_saturation);
              preferences.putUChar("ef", cam_special_effect);
              preferences.putUChar("wb", cam_whitebal);
              preferences.putUChar("awg", cam_awb_gain);
              preferences.putUChar("wbm", cam_wb_mode);
              preferences.putUChar("ec", cam_exposure_ctrl);
              preferences.putUChar("aec2", cam_aec2);
              preferences.putChar("ael", cam_ae_level);
              preferences.putUShort("aev", cam_aec_value);
              preferences.putUChar("gc", cam_gain_ctrl);
              preferences.putUChar("agg", cam_agc_gain);
              preferences.putUChar("gcl", cam_gainceiling);
              preferences.putUChar("bpc", cam_bpc);
              preferences.putUChar("wpc", cam_wpc);
              preferences.putUChar("rgm", cam_raw_gma);
              preferences.putUChar("lnc", cam_lenc);
              preferences.putUChar("hmr", cam_hmirror);
              preferences.putUChar("vfl", cam_vflip);
              preferences.putUChar("dcw", cam_dcw);
              preferences.putUChar("cbr", cam_colorbar);
              preferences.end();

              Serial.printf("[CONFIG] Nueva configuración de cámara guardada: "
                            "Res=%d, Br=%d, Co=%d, Qty=%d\n",
                            cam_resolution, cam_brightness, cam_contrast,
                            cam_quality);

              // Encender la cámara físicamente para guardar su configuración local
              Serial.println("[CAMERA DEBUG] Encendiendo camara para configuracion remota (CAM_EN = HIGH)...");
              digitalWrite(CAM_EN, HIGH);
              delay(2000); // Esperar que arranque la camara

              // Vaciar buffer RX
              while (camSerial.available()) camSerial.read();

              // Enviar config por serial
              Serial.println("[CAMERA] Enviando configuracion a la camara...");
              camSerial.printf("SET_CONFIG %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
                               "%d %d %d %d %d %d %d %d\n",
                               cam_resolution, cam_brightness, cam_contrast, cam_quality,
                               cam_saturation, cam_special_effect, cam_whitebal,
                               cam_awb_gain, cam_wb_mode, cam_exposure_ctrl, cam_aec2,
                               cam_ae_level, cam_aec_value, cam_gain_ctrl, cam_agc_gain,
                               cam_gainceiling, cam_bpc, cam_wpc, cam_raw_gma, cam_lenc,
                               cam_hmirror, cam_vflip, cam_dcw, cam_colorbar);

              // Esperar confirmación
              unsigned long configAckStart = millis();
              bool gotConfigAck = false;
              while (millis() - configAckStart < 500) {
                if (camSerial.available()) {
                  String resp = camSerial.readStringUntil('\n');
                  resp.trim();
                  if (resp == "CONF_ACK") {
                    gotConfigAck = true;
                    break;
                  }
                }
                delay(5);
              }
              Serial.printf("[CAMERA] Confirmacion de configuracion remota: %s\n", gotConfigAck ? "OK" : "TIMEOUT");

              // Apagar la camara para ahorrar energia
              digitalWrite(CAM_EN, LOW);

              // Responder con ACK de confirmación si la cámara contestó, o NACK en caso contrario
              if (gotConfigAck) {
                LoRaPacket ackPacket;
                ackPacket.header.syncWord[0] = LORA_SYNC_0;
                ackPacket.header.syncWord[1] = LORA_SYNC_1;
                ackPacket.header.srcId = MY_NODE_ID;
                ackPacket.header.destId = rxPacket.header.srcId;
                ackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID)
                                                 ? (MY_NODE_ID - 1)
                                                 : (MY_NODE_ID + 1);
                ackPacket.header.type = ACK;
                ackPacket.header.seqNum = packetSequence++;
                ackPacket.header.ttl = 5;
                radio.transmit((uint8_t *)&ackPacket, sizeof(LoRaHeader));
                radio.startReceive();
                Serial.println("[LORA] Enviado ACK de configuracion exitosa al concentrador.");
              } else {
                LoRaPacket nackPacket;
                nackPacket.header.syncWord[0] = LORA_SYNC_0;
                nackPacket.header.syncWord[1] = LORA_SYNC_1;
                nackPacket.header.srcId = MY_NODE_ID;
                nackPacket.header.destId = rxPacket.header.srcId;
                nackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID)
                                                 ? (MY_NODE_ID - 1)
                                                 : (MY_NODE_ID + 1);
                nackPacket.header.type = NACK;
                nackPacket.header.seqNum = packetSequence++;
                nackPacket.header.ttl = 5;
                strcpy((char *)nackPacket.payload, "CAMERA_TIMEOUT");
                radio.transmit((uint8_t *)&nackPacket, sizeof(LoRaHeader) + 15);
                radio.startReceive();
                Serial.println("[LORA WARNING] Enviado NACK (CAMERA_TIMEOUT) al concentrador.");
              }
            } else if (rxPacket.header.type == CMD_GET_CAM_CONFIG) {
              delay(80);
              Serial.println("[CAMERA DEBUG] Encendiendo camara para lectura de configuracion (CAM_EN = HIGH)...");
              digitalWrite(CAM_EN, HIGH);
              delay(2000); // Esperar que arranque la camara

              // Vaciar buffer RX
              while (camSerial.available()) camSerial.read();

              // Enviar peticion por serial
              Serial.println("[CAMERA] Solicitando configuracion a la camara...");
              camSerial.println("GET_CONFIG");

              // Esperar respuesta
              unsigned long configStart = millis();
              bool gotConfig = false;
              int res = 10, br = 0, co = 1, qty = 24;
              int sa = 0, ef = 0, wb = 1, aw = 1, wm = 0, ec = 1, a2 = 0, al = 0, av = 300;
              int gc = 1, ag = 0, gl = 0, bp = 0, wp = 1, rg = 1, lc = 1, hm = 0, vf = 0, dw = 1, cb = 0;

              while (millis() - configStart < 1500) {
                if (camSerial.available()) {
                  String resp = camSerial.readStringUntil('\n');
                  resp.trim();
                  if (resp.startsWith("CONFIG_RESP")) {
                    int parsed = sscanf(resp.c_str() + 12,
                                         "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                                         &res, &br, &co, &qty, &sa, &ef, &wb, &aw, &wm, &ec, &a2, &al, &av, &gc, &ag, &gl, &bp, &wp, &rg, &lc, &hm, &vf, &dw, &cb);
                    if (parsed >= 24) {
                      gotConfig = true;
                      break;
                    }
                  }
                }
                delay(5);
              }
              Serial.printf("[CAMERA] Lectura de configuracion remota: %s\n", gotConfig ? "OK" : "TIMEOUT");

              // Apagar la camara para ahorrar energia
              digitalWrite(CAM_EN, LOW);

              if (gotConfig) {
                // Guardar en Preferences de forma permanente para mantener sincronía
                cam_resolution = res;
                cam_brightness = br;
                cam_contrast = co;
                cam_quality = qty;
                cam_saturation = sa;
                cam_special_effect = ef;
                cam_whitebal = wb;
                cam_awb_gain = aw;
                cam_wb_mode = wm;
                cam_exposure_ctrl = ec;
                cam_aec2 = a2;
                cam_ae_level = al;
                cam_aec_value = av;
                cam_gain_ctrl = gc;
                cam_agc_gain = ag;
                cam_gainceiling = gl;
                cam_bpc = bp;
                cam_wpc = wp;
                cam_raw_gma = rg;
                cam_lenc = lc;
                cam_hmirror = hm;
                cam_vflip = vf;
                cam_dcw = dw;
                cam_colorbar = cb;

                preferences.begin("cam_config", false);
                preferences.putUChar("res", cam_resolution);
                preferences.putChar("br", cam_brightness);
                preferences.putChar("co", cam_contrast);
                preferences.putUChar("qty", cam_quality);
                preferences.putChar("sat", cam_saturation);
                preferences.putUChar("ef", cam_special_effect);
                preferences.putUChar("wb", cam_whitebal);
                preferences.putUChar("awg", cam_awb_gain);
                preferences.putUChar("wbm", cam_wb_mode);
                preferences.putUChar("ec", cam_exposure_ctrl);
                preferences.putUChar("aec2", cam_aec2);
                preferences.putChar("ael", cam_ae_level);
                preferences.putUShort("aev", cam_aec_value);
                preferences.putUChar("gc", cam_gain_ctrl);
                preferences.putUChar("agg", cam_agc_gain);
                preferences.putUChar("gcl", cam_gainceiling);
                preferences.putUChar("bpc", cam_bpc);
                preferences.putUChar("wpc", cam_wpc);
                preferences.putUChar("rgm", cam_raw_gma);
                preferences.putUChar("lnc", cam_lenc);
                preferences.putUChar("hmr", cam_hmirror);
                preferences.putUChar("vfl", cam_vflip);
                preferences.putUChar("dcw", cam_dcw);
                preferences.putUChar("cbr", cam_colorbar);
                preferences.end();

                // Responder con la configuracion al concentrador
                LoRaPacket respPacket;
                respPacket.header.syncWord[0] = LORA_SYNC_0;
                respPacket.header.syncWord[1] = LORA_SYNC_1;
                respPacket.header.srcId = MY_NODE_ID;
                respPacket.header.destId = rxPacket.header.srcId;
                respPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID)
                                                 ? (MY_NODE_ID - 1)
                                                 : (MY_NODE_ID + 1);
                respPacket.header.type = DATA_CAM_CONFIG;
                respPacket.header.seqNum = packetSequence++;
                respPacket.header.ttl = 5;

                CameraConfigPayload *ccp = (CameraConfigPayload *)respPacket.payload;
                ccp->resolution = res;
                ccp->brightness = br;
                ccp->contrast = co;
                ccp->quality = qty;
                ccp->saturation = sa;
                ccp->special_effect = ef;
                ccp->whitebal = wb;
                ccp->awb_gain = aw;
                ccp->wb_mode = wm;
                ccp->exposure_ctrl = ec;
                ccp->aec2 = a2;
                ccp->ae_level = al;
                ccp->aec_value = av;
                ccp->gain_ctrl = gc;
                ccp->agc_gain = ag;
                ccp->gainceiling = gl;
                ccp->bpc = bp;
                ccp->wpc = wp;
                ccp->raw_gma = rg;
                ccp->lenc = lc;
                ccp->hmirror = hm;
                ccp->vflip = vf;
                ccp->dcw = dw;
                ccp->colorbar = cb;

                radio.transmit((uint8_t *)&respPacket, sizeof(LoRaHeader) + sizeof(CameraConfigPayload));
                radio.startReceive();
                Serial.println("[LORA] Enviada configuracion de camara leida con exito al concentrador.");
              } else {
                LoRaPacket nackPacket;
                nackPacket.header.syncWord[0] = LORA_SYNC_0;
                nackPacket.header.syncWord[1] = LORA_SYNC_1;
                nackPacket.header.srcId = MY_NODE_ID;
                nackPacket.header.destId = rxPacket.header.srcId;
                nackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID)
                                                 ? (MY_NODE_ID - 1)
                                                 : (MY_NODE_ID + 1);
                nackPacket.header.type = NACK;
                nackPacket.header.seqNum = packetSequence++;
                nackPacket.header.ttl = 5;
                strcpy((char *)nackPacket.payload, "CAMERA_TIMEOUT");
                radio.transmit((uint8_t *)&nackPacket, sizeof(LoRaHeader) + 15);
                radio.startReceive();
                Serial.println("[LORA WARNING] Fallo al leer camara, enviado NACK.");
              }
            } else if (rxPacket.header.type == CMD_PING) {
              delay(80);
              LoRaPacket ackPacket;
              ackPacket.header.syncWord[0] = LORA_SYNC_0;
              ackPacket.header.syncWord[1] = LORA_SYNC_1;
              ackPacket.header.srcId = MY_NODE_ID;
              ackPacket.header.destId = rxPacket.header.srcId;
              ackPacket.header.nextHopId = (rxPacket.header.srcId < MY_NODE_ID)
                                               ? (MY_NODE_ID - 1)
                                               : (MY_NODE_ID + 1);
              ackPacket.header.type = ACK;
              ackPacket.header.seqNum = packetSequence++;
              ackPacket.header.ttl = 5;
              radio.transmit((uint8_t *)&ackPacket, sizeof(LoRaHeader));
              radio.startReceive();
            }
          }
          // --- ENRUTAMIENTO DE RED LINEAL ---
          // Si el destino no soy yo y aún tiene saltos de vida, propagarlo
          else if (rxPacket.header.ttl > 0) {
            bool shouldRelay = false;

            // Flujo Bajada: Concentrador hacia sensores remotos (ID Origen < Mi
            // ID, ID Destino > Mi ID)
            if (rxPacket.header.srcId < MY_NODE_ID &&
                rxPacket.header.destId > MY_NODE_ID) {
              shouldRelay = true;
            }
            // Flujo Subida: Sensores remotos hacia concentrador (ID Origen > Mi
            // ID, ID Destino < Mi ID)
            else if (rxPacket.header.srcId > MY_NODE_ID &&
                     rxPacket.header.destId < MY_NODE_ID) {
              shouldRelay = true;
            }

            if (shouldRelay) {
              size_t packetLen = radio.getPacketLength();

              bool isUpstream = (rxPacket.header.srcId > MY_NODE_ID &&
                                 rxPacket.header.destId < MY_NODE_ID);
              uint8_t prevHop =
                  isUpstream ? (MY_NODE_ID + 1) : (MY_NODE_ID - 1);
              uint8_t nextHop =
                  isUpstream ? (MY_NODE_ID - 1) : (MY_NODE_ID + 1);

              // Solo renderizar el cambio de pantalla si no es una ráfaga de
              // chunks rápidos (evita latencias críticas)
              if (rxPacket.header.type != DATA_IMG_CHUNK) {
                setScreenStatus("REPETIDOR LORA", "Relay: N" + String(rxPacket.header.srcId) + "->N" + String(rxPacket.header.destId), "Ruta: N" + String(prevHop) + "->N" + String(MY_NODE_ID) + "->N" + String(nextHop));
              }

              // Modificar la cabecera con el ID del salto físico receptor
              // directo
              rxPacket.header.nextHopId = nextHop;

              // Backoff defensivo para des-sincronizar colisiones en el aire
              delay(40);

              radio.transmit((uint8_t *)&rxPacket, packetLen);
              radio.startReceive(); // Regresar inmediatamente a modo escucha
              Serial.printf("Relay OK: S:%d D:%d\n", rxPacket.header.srcId,
                            rxPacket.header.destId);

              sprintf(lastRfActivity, "Relay %d->%d OK", rxPacket.header.srcId,
                      rxPacket.header.destId);

              if (rxPacket.header.type != DATA_IMG_CHUNK) {
                // Configurar el temporizador no-bloqueante en lugar de usar un
                // delay() que congele el receptor. Esto permite que el nodo
                // enrute instantáneamente la respuesta de vuelta sin perder
                // paquetes.
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
  static unsigned long lastScreenUpdate = 0;

  // Latido periódico (Heartbeat) de diagnóstico local cada 5 segundos con info
  // de batería
  if (millis() - lastHeartbeat > 5000) {
    float vbat = getBatteryVoltage();
    float mv = analogReadMilliVolts(1);
    Serial.printf("[HEARTBEAT] Nodo %d | Vbat: %.2fV (ADC calibrated: %.1fmV, "
                  "Pct: %d%%)\n",
                  MY_NODE_ID, vbat, mv, getBatteryPercent());
    lastHeartbeat = millis();
  }

  // Actualizar automáticamente la pantalla cada 5 segundos para refrescar GPS y
  // Batería
  if (millis() - lastScreenUpdate > 5000) {
    lastScreenUpdate = millis();
    if (!isRelayScreenActive && screenStatus == "ESCUCHANDO") {
      updateTFT();
    }
  }

  // Retornar al dashboard en espera de forma no-bloqueante tras 2 segundos en
  // pantalla de repetidor
  if (isRelayScreenActive && (millis() - lastRelayScreenTime > 2000)) {
    isRelayScreenActive = false;
    drawDashboard();
  }

  smartDelay(10); // Alimentar decodificador GPS
  handleLoRa();   // Procesar actividades de red
}
