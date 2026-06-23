/**
 * @file main.cpp
 * @brief Firmware de Producción - Edición LIGHT (Compacta/Autónoma) para la Cámara ESP32-CAM.
 * 
 * Esta versión está específicamente diseñada para nodos remotos de bajo consumo que no requieren
 * una pantalla OLED de diagnóstico local ni un encoder rotativo. Se enfoca exclusivamente en
 * proporcionar la comunicación UART de alta velocidad de captura de imagen OV2640/OV5640 de la manera
 * más liviana y eficiente en términos de ciclos de reloj y ahorro de energía de batería.
 * 
 * Protocolo de Comandos Soportado a través de UART:
 *  - "TAKE_PIC"                    ➡️ Dispara la cámara y captura un frame JPG en PSRAM.
 *  - "GET_CHUNK <offset> <len>"    ➡️ Lee y transmite dinámicamente un fragmento binario de la imagen.
 *  - "RELEASE_PIC"                 ➡️ Libera el búfer de captura en PSRAM para conservar energía.
 *  - "SET_RES <val>"               ➡️ Configura dinámicamente la resolución de imagen deseada (0 al 21).
 * 
 * Diseñado y documentado profesionalmente para robustez industrial.
 */

#include <Arduino.h>
#include "esp_camera.h"

// ============================================================================
// --- ASIGNACIÓN DE PINES DE LA CÁMARA (Freenove ESP32-S3 Cam Board) ---
// ============================================================================
#define PWDN_GPIO_NUM     -1  ///< Pin de apagado por hardware (No disponible en este modelo)
#define RESET_GPIO_NUM    -1  ///< Pin de reinicio por hardware (No disponible en este modelo)
#define XCLK_GPIO_NUM     15  ///< Entrada de reloj del sistema para el sensor de la cámara
#define SIOD_GPIO_NUM     4   ///< Bus de datos SCCB (Equivalente a SDA de I2C)
#define SIOC_GPIO_NUM     5   ///< Bus de reloj SCCB (Equivalente a SCL de I2C)
#define Y9_GPIO_NUM       16  ///< Bit de datos 9 del bus paralelo de imagen
#define Y8_GPIO_NUM       17  ///< Bit de datos 8 del bus paralelo de imagen
#define Y7_GPIO_NUM       18  ///< Bit de datos 7 del bus paralelo de imagen
#define Y6_GPIO_NUM       12  ///< Bit de datos 6 del bus paralelo de imagen
#define Y5_GPIO_NUM       10  ///< Bit de datos 5 del bus paralelo de imagen
#define Y4_GPIO_NUM       8   ///< Bit de datos 4 del bus paralelo de imagen
#define Y3_GPIO_NUM       9   ///< Bit de datos 3 del bus paralelo de imagen
#define Y2_GPIO_NUM       11  ///< Bit de datos 2 del bus paralelo de imagen
#define VSYNC_GPIO_NUM    6   ///< Pin de sincronización vertical (Inicio de Frame)
#define HREF_GPIO_NUM     7   ///< Pin de referencia horizontal (Línea Activa)
#define PCLK_GPIO_NUM     13  ///< Pin de reloj de píxeles (Pixel Clock)

// ============================================================================
// --- CONEXIÓN DE COMUNICACIÓN SERIE UART CON MASTER MCU (Heltec) ---
// ============================================================================
#define HELTEC_UART_RX 41     ///< Pin RX para recibir comandos desde el Heltec
#define HELTEC_UART_TX 42     ///< Pin TX para enviar respuestas y bytes de imagen
HardwareSerial heltecSerial(1); ///< Canal de hardware UART1 para aislar logs del puerto USB

// ============================================================================
// --- VARIABLES DE ESTADO GLOBAL ---
// ============================================================================
sensor_t * s = NULL;              ///< Puntero al controlador del sensor físico de la cámara
camera_fb_t * currentFb = NULL;   ///< Frame buffer que retiene el frame capturado en PSRAM

/**
 * @brief Resolución de captura JPEG inicial por defecto.
 * 21 representa FRAMESIZE_QSXGA (2560x1920) para el sensor OV5640.
 */
int currentFrameSize = 21; 

/**
 * @brief Obtiene la cadena de texto legible de una resolución basada en su índice.
 * @param val Índice entero de la resolución (0 a 21).
 * @return Nombre descriptivo de la resolución.
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
 * @brief Inicialización de hardware para la cámara y puerto serie.
 */
void setup() {
  Serial.begin(115200); ///< Puerto serie USB para depuración local
  Serial.println("\n[SISTEMA] Iniciando firmware de camara (Version LIGHT Standalone)...");
  Serial.flush();
  
  // Iniciar la UART serie dedicada para interactuar con el nodo maestro Heltec
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

  // --- CONFIGURACIÓN E INICIALIZACIÓN DE LA CÁMARA ---
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
  
  // Configuración del reloj de reloj externo (XCLK).
  // Ajustado a 10MHz para máxima estabilidad de comunicación serial en sensores OV5640
  config.xclk_freq_hz = 10000000;  
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Asignar el búfer físico dinámicamente dependiendo de la presencia de memoria PSRAM externa
  if(psramFound()){
    Serial.println("[INFO] PSRAM detectada. Habilitando soporte para resoluciones ultra altas.");
    config.frame_size = FRAMESIZE_QSXGA; ///< Permite escalamientos dinámicos hasta 5 MegaPíxeles
    config.jpeg_quality = 24;            ///< Calidad de imagen JPEG inicial balanceada (24)
    config.fb_count = 1;                 ///< Búfer único para minimizar la huella de memoria PSRAM
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("[WARNING] No se encontró PSRAM. Usando DRAM interna (Limitado a VGA).");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 24;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Inicializar el controlador de cámara de bajo nivel
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Inicialización de cámara fallida: 0x%x\n", err);
    return;
  }

  // Obtener el puntero para manipular registros del sensor directamente
  s = esp_camera_sensor_get();
  
  // Forzar la resolución inicial segura (limitar a VGA si no hay PSRAM)
  if (!psramFound() && currentFrameSize > 8) {
    currentFrameSize = 8; 
  }
  s->set_framesize(s, (framesize_t)currentFrameSize);

  // Configuración de parámetros de imagen por defecto
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
 * @brief Bucle principal de ejecución del firmware de la cámara.
 * Escucha comandos UART e invoca el flujo de trabajo correspondiente.
 */
void loop() {
  if (heltecSerial.available()) {
    // Leer el comando delimitado por nueva línea (\n)
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();
    
    // ------------------------------------------------------------------------
    // COMANDO: GET_CHUNK <offset> <len>
    // Retorna una ráfaga binaria del buffer JPG actual a partir del desplazamiento.
    // ------------------------------------------------------------------------
    if (cmd.startsWith("GET_CHUNK")) {
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      
      if (firstSpace != -1 && secondSpace != -1) {
        uint32_t offset = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t len = cmd.substring(secondSpace + 1).toInt();
        
        // Escribir bytes en binario crudo por la UART
        if (currentFb && offset + len <= currentFb->len) {
          heltecSerial.write(currentFb->buf + offset, len);
        } else {
          // Búfer defensivo: rellenar con ceros ante errores para no congelar la UART receptora
          uint8_t* zeroBuf = (uint8_t*)calloc(len, 1);
          if (zeroBuf) {
            heltecSerial.write(zeroBuf, len);
            free(zeroBuf);
          }
        }
      }
    }
    // ------------------------------------------------------------------------
    // COMANDO: TAKE_PIC
    // Dispara y captura la foto en PSRAM.
    // ------------------------------------------------------------------------
    else if (cmd == "TAKE_PIC") {
      takeAndSendPhoto();
    }
    // ------------------------------------------------------------------------
    // COMANDO: RELEASE_PIC
    // Libera el frame buffer retenido en memoria para optimizar corriente de fuga.
    // ------------------------------------------------------------------------
    else if (cmd == "RELEASE_PIC") {
      if (currentFb) {
        esp_camera_fb_return(currentFb);
        currentFb = NULL;
        Serial.println("[INFO] Frame buffer liberado de PSRAM.");
      }
    }
    // ------------------------------------------------------------------------
    // COMANDO: SET_RES <val>
    // Configura dinámicamente la resolución sobre la marcha.
    // ------------------------------------------------------------------------
    else if (cmd.startsWith("SET_RES")) {
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
          Serial.printf("[ERROR] Valor de resolución inválido: %d (Rango esperado: 0-21)\n", val);
        }
      }
    }
    // ------------------------------------------------------------------------
    // COMANDO: SET_CONFIG <res> <br> <co>
    // Configura la resolución, brillo y contraste.
    // ------------------------------------------------------------------------
    else if (cmd.startsWith("SET_CONFIG")) {
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      int thirdSpace = cmd.indexOf(' ', secondSpace + 1);
      if (firstSpace != -1 && secondSpace != -1 && thirdSpace != -1) {
        int res = cmd.substring(firstSpace + 1, secondSpace).toInt();
        int br = cmd.substring(secondSpace + 1, thirdSpace).toInt();
        int co = cmd.substring(thirdSpace + 1).toInt();
        
        res = constrain(res, 0, 21);
        br = constrain(br, -2, 2);
        co = constrain(co, -2, 2);
        
        currentFrameSize = res;
        
        if (s) {
          s->set_framesize(s, (framesize_t)res);
          s->set_brightness(s, br);
          s->set_contrast(s, co);
        }
        
        Serial.printf("[CAMERA] Configurada remotamente (LIGHT): Res=%d, Br=%d, Co=%d\n", res, br, co);
        heltecSerial.println("CONF_ACK");
      }
    }
  }
}

/**
 * @brief Dispara el sensor CMOS y captura un cuadro de imagen retenido en RAM.
 * 
 * Flujo de ejecución:
 *  1. Libera búferes obsoletos de capturas previas.
 *  2. Realiza 4 capturas dummy consecutivas para permitir que los lazos de control
 *     de ganancia automática (AGC) y exposición (AEC) del sensor OV5640 se estabilicen.
 *  3. Ejecuta la lectura final en PSRAM.
 *  4. Valida el marcador binario de fin de JPEG (End of Image - EOI: `0xFF 0xD9`).
 *  5. Envía la respuesta formateada `SIZE:<bytes>\n` a través del puerto serie.
 */
void takeAndSendPhoto() {
  Serial.printf("[INFO] Petición TAKE_PIC recibida. Capturando en resolución %s...\n", getFrameSizeName(currentFrameSize));

  // Limpiar referencias anteriores
  if (currentFb) {
    esp_camera_fb_return(currentFb);
    currentFb = NULL;
  }

  // Capturas de descarte para estabilización automática de brillo y ganancia del lente CMOS
  for (int i = 0; i < 4; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
    delay(80); 
  }

  // Captura del cuadro final útil
  currentFb = esp_camera_fb_get();
  if (!currentFb) {
    Serial.println("[ERROR] Captura del sensor CMOS fallida.");
    heltecSerial.println("SIZE:0"); 
    return;
  }

  // --- DIAGNÓSTICO DE INTEGRIDAD DE ARCHIVO JPEG (EOI - End of Image) ---
  if (currentFb->len >= 2) {
      uint8_t b1 = currentFb->buf[currentFb->len - 2];
      uint8_t b2 = currentFb->buf[currentFb->len - 1];
      Serial.printf("[DEBUG] Últimos dos bytes en PSRAM: %02X %02X (Esperado: FF D9)\n", b1, b2);
      
      bool foundEOI = false;
      for (size_t i = 0; i < currentFb->len - 1; i++) {
          if (currentFb->buf[i] == 0xFF && currentFb->buf[i+1] == 0xD9) {
              Serial.printf("[DEBUG] Marcador de integridad FF D9 encontrado en índice %u\n", i);
              foundEOI = true;
              break;
          }
      }
      if (!foundEOI) {
          Serial.println("[WARNING] Marcador de cierre JPEG FF D9 no detectado. Posible corrupción de frame.");
      }
  }

  // Reportar tamaño total al MCU maestro
  uint32_t imgSize = currentFb->len;
  heltecSerial.printf("SIZE:%u\n", imgSize);
  
  Serial.printf("[INFO] Foto capturada exitosamente. Tamaño: %u bytes. Esperando descarga...\n", imgSize);
}
