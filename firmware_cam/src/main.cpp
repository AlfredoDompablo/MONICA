#include <Arduino.h>
#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pines OLED
#define I2C_SDA 47
#define I2C_SCL 21
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

void setup() {
  Serial.begin(115200); // Debug por USB
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Freenove ESP32-S3");
    display.println("Modo Borde: ACTIVO");
    display.display();
  }

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
  config.xclk_freq_hz = 12000000; // 12MHz más estable como en tu código
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    Serial.println("PSRAM DETECTED");
    config.frame_size = FRAMESIZE_VGA; // Reducido para LoRa
    config.jpeg_quality = 10;
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
    display.println("ERR CAM INIT");
    display.display();
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // Configuraciones base del usuario
  s->set_vflip(s, 0); 
  s->set_brightness(s, 0); 
  s->set_saturation(s, 0); 
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_contrast(s, 1);
}

void takeAndSendPhoto() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(">> PETICION UART");
  display.println("Capturando VGA...");
  display.display();

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error capture");
    display.println("ERROR DE CAMARA");
    display.display();
    return;
  }

  display.print("Tamano: ");
  display.print(fb->len / 1024);
  display.println(" KB");
  display.println("Enviando...");
  display.display();

  // Notificar al Master MCU el tamaño exacto con 4 bytes binarios puros
  uint32_t imgSize = fb->len;
  heltecSerial.write((uint8_t*)&imgSize, 4);

  // Volcar el binario crudo por UART hacia la Heltec de inmediato
  heltecSerial.write(fb->buf, fb->len);
  
  // Limpiar
  esp_camera_fb_return(fb);

  display.println("OK.");
  display.display();
  Serial.println("Foto enviada a Heltec");
}

void loop() {
  if (heltecSerial.available()) {
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();
    
    // Depuración: Mostrar lo que recibió de la Heltec en el monitor serie USB
    Serial.print("Recibido de Heltec: [");
    Serial.print(cmd);
    Serial.println("]");

    if (cmd == "TAKE_PIC") {
      takeAndSendPhoto();
    }
  }
}
