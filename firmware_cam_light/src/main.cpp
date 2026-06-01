/**
 * @file main.cpp
 * @brief Firmware de Producción - Edición LIGHT (Compacta/Autónoma) para la Cámara ESP32-CAM.
 * 
 * Esta versión está específicamente diseñada para nodos remotos de bajo consumo que no requieren
 * una pantalla OLED de diagnóstico local ni un encoder rotativo. Se enfoca exclusivamente en
 * proporcionar la comunicación UART de alta velocidad de captura de imagen OV2640 de la manera
 * más liviana y eficiente en términos de ciclos de reloj y ahorro de energía de batería.
 * 
 * Protocolo de Comandos Soportado:
 *  - "TAKE_PIC"                    ➡️ Dispara la cámara y captura un frame JPG en PSRAM.
 *  - "GET_CHUNK <offset> <len>"    ➡️ Lee y transmite dinámicamente un fragmento binario de la imagen.
 *  - "RELEASE_PIC"                 ➡️ Libera el búfer de captura en PSRAM para conservar energía.
 *  - "SET_RES <val>"               ➡️ Configura dinámicamente la resolución de imagen deseada (0 al 21).
 */

#include <Arduino.h>
#include "esp_camera.h"

// --- Asignación de Pines de la Cámara OV2640 (Freenove ESP32-S3 Board) ---
#define PWDN_GPIO_NUM     -1  // Pin de apagado por hardware (no disponible).
#define RESET_GPIO_NUM    -1  // Pin de reinicio por hardware (no disponible).
#define XCLK_GPIO_NUM     15  // Entrada de reloj de la cámara.
#define SIOD_GPIO_NUM     4   // Bus de datos SCCB (Datos I2C).
#define SIOC_GPIO_NUM     5   // Bus de reloj SCCB (Reloj I2C).
#define Y9_GPIO_NUM       16  // Bit de datos 9 de imagen.
#define Y8_GPIO_NUM       17  // Bit de datos 8 de imagen.
#define Y7_GPIO_NUM       18  // Bit de datos 7 de imagen.
#define Y6_GPIO_NUM       12  // Bit de datos 6 de imagen.
#define Y5_GPIO_NUM       10  // Bit de datos 5 de imagen.
#define Y4_GPIO_NUM       8   // Bit de datos 4 de imagen.
#define Y3_GPIO_NUM       9   // Bit de datos 3 de imagen.
#define Y2_GPIO_NUM       11  // Bit de datos 2 de imagen.
#define VSYNC_GPIO_NUM    6   // Sincronización vertical.
#define HREF_GPIO_NUM     7   // Referencia horizontal.
#define PCLK_GPIO_NUM     13  // Reloj de píxeles de imagen.

// --- Conexión de Comunicación Serie UART con Heltec (Master MCU) ---
#define HELTEC_UART_RX 41     // Pin RX de recepción de comandos.
#define HELTEC_UART_TX 42     // Pin TX de envío de bytes de foto.
HardwareSerial heltecSerial(1); // Canal de hardware UART1 dedicado para evitar ruidos de depuración del puerto USB.

// --- Variables de Estado Global ---
sensor_t * s = NULL;            // Puntero del controlador del sensor físico OV2640.
camera_fb_t * currentFb = NULL;   // Puntero del Frame Buffer retenido temporalmente en RAM.

// Resolución de captura JPEG inicial por defecto (21 = FRAMESIZE_QSXGA)
int currentFrameSize = 21; 

/**
 * @brief Obtiene el nombre legible de la resolución según su índice entero de configuración.
 * @param val Índice de la resolución.
 * @return Cadena de caracteres con el formato en píxeles.
 */
const char* getFrameSizeName(int val) {
  switch(val) {
    case 0: return "96x96";
    case 1: return "QQVGA";
    case 2: return "QCIF";
    case 3: return "HQVGA";
    case 4: return "240x240";
    case 5: return "QVGA";
    case 6: return "CIF";
    case 7: return "HVGA";
    case 8: return "VGA";
    case 9: return "SVGA";
    case 10: return "XGA";
    case 11: return "HD";
    case 12: return "SXGA";
    case 13: return "UXGA";
    case 14: return "FHD";
    case 15: return "P_HD";
    case 16: return "P_3MP";
    case 17: return "QXGA";
    case 18: return "QHD";
    case 19: return "WQXGA";
    case 20: return "P_FHD";
    case 21: return "QSXGA";
    default: return "INV";
  }
}

void takeAndSendPhoto();

/**
 * @brief Configuración inicial del hardware de la cámara en versión compacta (LIGHT).
 */
void setup() {
  Serial.begin(115200); // Puerto USB para depuración local
  Serial.println("\n[SISTEMA] Iniciando firmware de camara (Version LIGHT Standalone)...");
  Serial.flush();
  
  // Iniciar la UART serie de alta velocidad
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

  // --- CONFIGURACIÓN DE LA CÁMARA OV2640 ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 12000000;  // Reducido a 8MHz para bajar picos de corriente en QSXGA
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Determinar la locación de búfer física de manera dinámica según la presencia de memoria PSRAM
  if(psramFound()){
    Serial.println("PSRAM DETECTED");
    config.frame_size = FRAMESIZE_QSXGA; // Permite escalamientos dinámicos posteriores
    config.jpeg_quality = 14;            // Alta fidelidad JPG
    config.fb_count = 1;                 // Búfer único para ahorrar memoria PSRAM en resoluciones ultra altas (QSXGA)
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("NO PSRAM - Usando DRAM interna");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Inicializar biblioteca de cámara de bajo nivel
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  s = esp_camera_sensor_get();
  
  // Aplicar resolución inicial por defecto (VGA o QSXGA si hay PSRAM)
  if (!psramFound() && currentFrameSize > 8) {
    currentFrameSize = 8; // Forzar VGA si no hay PSRAM
  }
  s->set_framesize(s, (framesize_t)currentFrameSize);

  // Parámetros base estables de captura
  s->set_vflip(s, 0); 
  s->set_brightness(s, 0); 
  s->set_saturation(s, 0); 
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_contrast(s, 1);

  Serial.println("[SISTEMA] Inicialización de cámara completada. Esperando comandos...");
}

/**
 * @brief Bucle principal de escucha de comandos serie.
 */
void loop() {
  if (heltecSerial.available()) {
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();
    
    // Comando 1: Solicitar un fragmento exacto de la foto
    if (cmd.startsWith("GET_CHUNK")) {
      // Formato: "GET_CHUNK <offset> <len>"
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (firstSpace != -1 && secondSpace != -1) {
        uint32_t offset = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t len = cmd.substring(secondSpace + 1).toInt();
        
        // Escribir ráfaga binaria por la UART si es válida
        if (currentFb && offset + len <= currentFb->len) {
          heltecSerial.write(currentFb->buf + offset, len);
        } else {
          // Si no hay foto activa o es inválido, enviar ceros para no colgar la UART receptora
          uint8_t* zeroBuf = (uint8_t*)calloc(len, 1);
          if (zeroBuf) {
            heltecSerial.write(zeroBuf, len);
            free(zeroBuf);
          }
        }
      }
    }
    // Comando 2: Tomar una foto
    else if (cmd == "TAKE_PIC") {
      takeAndSendPhoto();
    }
    // Comando 3: Liberar el Frame Buffer de PSRAM
    else if (cmd == "RELEASE_PIC") {
      if (currentFb) {
        esp_camera_fb_return(currentFb);
        currentFb = NULL;
        Serial.println("[INFO] Foto liberada de PSRAM");
      }
    }
    // Comando 4: Configurar la resolución dinámicamente sobre la marcha
    else if (cmd.startsWith("SET_RES")) {
      // Formato: "SET_RES <val>" (ej: "SET_RES 21" para QSXGA, "SET_RES 8" para VGA)
      int spaceIdx = cmd.indexOf(' ');
      if (spaceIdx != -1) {
        int val = cmd.substring(spaceIdx + 1).toInt();
        if (val >= 0 && val <= 21) {
          currentFrameSize = val;
          if (s) {
            s->set_framesize(s, (framesize_t)currentFrameSize);
            Serial.printf("[INFO] Resolución cambiada dinámicamente a %s (%d)\n", getFrameSizeName(currentFrameSize), currentFrameSize);
          }
        } else {
          Serial.printf("[ERROR] Valor de resolución inválido: %d (rango esperado 0-21)\n", val);
        }
      }
    }
  }
}

/**
 * @brief Captura una foto en la resolución activa de hardware.
 * 
 * Flujo:
 *  1. Libera búferes antiguos para prevenir desbordes de memoria.
 *  2. Llama al motor de hardware OV2640 `esp_camera_fb_get()`.
 *  3. Ejecuta validaciones de diagnóstico del fin de imagen JPEG (`FF D9`).
 *  4. Transmite los 4 bytes de longitud del archivo al maestro.
 *  5. Retiene el cuadro en la RAM PSRAM para posterior descarga.
 */
void takeAndSendPhoto() {
  Serial.printf("[INFO] Petición TAKE_PIC recibida. Capturando en resolución %s...\n", getFrameSizeName(currentFrameSize));

  // Liberar búfer antiguo
  if (currentFb) {
    esp_camera_fb_return(currentFb);
    currentFb = NULL;
  }

  // Capturar
  currentFb = esp_camera_fb_get();
  if (!currentFb) {
    Serial.println("[ERROR] Captura fallida");
    uint32_t zeroSize = 0;
    heltecSerial.write((uint8_t*)&zeroSize, 4); // Responder tamaño 0 ante fallos
    return;
  }

  // --- ANÁLISIS DE INTEGRIDAD JPEG (EOI - End of Image) ---
  if (currentFb->len >= 2) {
      uint8_t b1 = currentFb->buf[currentFb->len - 2];
      uint8_t b2 = currentFb->buf[currentFb->len - 1];
      Serial.printf("[CAMERA DEBUG] Últimos dos bytes en PSRAM: %02X %02X (Esperado: FF D9)\n", b1, b2);
      
      bool foundEOI = false;
      for (size_t i = 0; i < currentFb->len - 1; i++) {
          if (currentFb->buf[i] == 0xFF && currentFb->buf[i+1] == 0xD9) {
              Serial.printf("[CAMERA DEBUG] Marcador FF D9 encontrado en el índice %u (Distancia al final: %u bytes)\n", i, currentFb->len - i - 2);
              foundEOI = true;
              break;
          }
      }
      if (!foundEOI) {
          Serial.println("[CAMERA DEBUG] Advertencia: Marcador FF D9 NO ENCONTRADO en el búfer de la cámara");
      }
  }

  // Escribir 4 bytes en binario del tamaño de la imagen sobre la UART
  uint32_t imgSize = currentFb->len;
  heltecSerial.write((uint8_t*)&imgSize, 4);
  
  Serial.printf("[INFO] Foto capturada. Tamaño: %u bytes. Esperando peticiones de chunks...\n", imgSize);
}
