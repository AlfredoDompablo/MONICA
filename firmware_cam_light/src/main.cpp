#include <Arduino.h>
#include "esp_camera.h"

// Pines Freenove ESP32-S3 WROOM CAM
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// UART con Heltec
#define HELTEC_UART_RX 41
#define HELTEC_UART_TX 42
HardwareSerial heltecSerial(1); // Usamos UART1 para no interferir con el USB

sensor_t * s = NULL;
camera_fb_t * currentFb = NULL;

// Resolución por defecto (8 = FRAMESIZE_VGA)
int currentFrameSize = 8; 

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

void setup() {
  Serial.begin(115200); // Debug por USB
  Serial.println("\n[SISTEMA] Iniciando firmware de camara (Version LIGHT Standalone)...");
  Serial.flush();
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

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
  config.xclk_freq_hz = 12000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    Serial.println("PSRAM DETECTED");
    config.frame_size = FRAMESIZE_QSXGA; // Reservar buffer máximo nativo en PSRAM
    config.jpeg_quality = 14;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("NO PSRAM - Usando DRAM interna");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  s = esp_camera_sensor_get();
  // Aplicar resolución inicial por defecto (VGA)
  s->set_framesize(s, (framesize_t)currentFrameSize);

  // Configuraciones base del sensor
  s->set_vflip(s, 0); 
  s->set_brightness(s, 0); 
  s->set_saturation(s, 0); 
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_contrast(s, 1);

  Serial.println("[SISTEMA] Inicialización de cámara completada. Esperando comandos...");
}

void loop() {
  // Escuchar comandos UART desde la Heltec
  if (heltecSerial.available()) {
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.startsWith("GET_CHUNK")) {
      // Parsear offset y len
      // Formato: "GET_CHUNK <offset> <len>"
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (firstSpace != -1 && secondSpace != -1) {
        uint32_t offset = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t len = cmd.substring(secondSpace + 1).toInt();
        
        if (currentFb && offset + len <= currentFb->len) {
          heltecSerial.write(currentFb->buf + offset, len);
        } else {
          // Si no hay foto o es inválido, enviar ceros para no colgar la UART
          uint8_t* zeroBuf = (uint8_t*)calloc(len, 1);
          if (zeroBuf) {
            heltecSerial.write(zeroBuf, len);
            free(zeroBuf);
          }
        }
      }
    }
    else if (cmd == "TAKE_PIC") {
      takeAndSendPhoto();
    }
    else if (cmd == "RELEASE_PIC") {
      if (currentFb) {
        esp_camera_fb_return(currentFb);
        currentFb = NULL;
        Serial.println("[INFO] Foto liberada de PSRAM");
      }
    }
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

void takeAndSendPhoto() {
  Serial.printf("[INFO] Petición TAKE_PIC recibida. Capturando en resolución %s...\n", getFrameSizeName(currentFrameSize));

  // Liberar búfer anterior si existiese
  if (currentFb) {
    esp_camera_fb_return(currentFb);
    currentFb = NULL;
  }

  currentFb = esp_camera_fb_get();
  if (!currentFb) {
    Serial.println("[ERROR] Captura fallida");
    uint32_t zeroSize = 0;
    heltecSerial.write((uint8_t*)&zeroSize, 4);
    return;
  }

  // Verificar si el marcador FF D9 está presente en el búfer de la cámara
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

  // Notificar al Master MCU el tamaño exacto con 4 bytes binarios puros
  uint32_t imgSize = currentFb->len;
  heltecSerial.write((uint8_t*)&imgSize, 4);
  
  Serial.printf("[INFO] Foto capturada. Tamaño: %u bytes. Esperando peticiones de chunks...\n", imgSize);
}
