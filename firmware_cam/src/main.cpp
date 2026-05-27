#include <Arduino.h>
#include "esp_camera.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pines OLED
#define OLED_SDA 47
#define OLED_SCL 21
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

// Pines del encoder rotativo (Mapeados a los pines de la SD)
#define PIN_CLK 38
#define PIN_DT  39
#define PIN_SW  40

sensor_t * s = NULL;
camera_fb_t * currentFb = NULL;

struct MenuItem {
  const char* name;
  int minVal;
  int maxVal;
  int currentVal;
  void (*updateFunc)(sensor_t * s, int val);
};

// Mapeo de funciones de actualización de parámetros del sensor
void up_br(sensor_t *s, int v) { if (s) s->set_brightness(s, v); }
void up_co(sensor_t *s, int v) { if (s) s->set_contrast(s, v); }
void up_sa(sensor_t *s, int v) { if (s) s->set_saturation(s, v); }
void up_ef(sensor_t *s, int v) { if (s) s->set_special_effect(s, v); }
void up_wb(sensor_t *s, int v) { if (s) s->set_whitebal(s, v); }
void up_aw(sensor_t *s, int v) { if (s) s->set_awb_gain(s, v); }
void up_wm(sensor_t *s, int v) { if (s) s->set_wb_mode(s, v); }
void up_ec(sensor_t *s, int v) { if (s) s->set_exposure_ctrl(s, v); }
void up_a2(sensor_t *s, int v) { if (s) s->set_aec2(s, v); }
void up_al(sensor_t *s, int v) { if (s) s->set_ae_level(s, v); }
void up_av(sensor_t *s, int v) { if (s) s->set_aec_value(s, v); }
void up_gc(sensor_t *s, int v) { if (s) s->set_gain_ctrl(s, v); }
void up_ag(sensor_t *s, int v) { if (s) s->set_agc_gain(s, v); }
void up_gl(sensor_t *s, int v) { if (s) s->set_gainceiling(s, (gainceiling_t)v); }
void up_bp(sensor_t *s, int v) { if (s) s->set_bpc(s, v); }
void up_wp(sensor_t *s, int v) { if (s) s->set_wpc(s, v); }
void up_rg(sensor_t *s, int v) { if (s) s->set_raw_gma(s, v); }
void up_lc(sensor_t *s, int v) { if (s) s->set_lenc(s, v); }
void up_hm(sensor_t *s, int v) { if (s) s->set_hmirror(s, v); }
void up_vf(sensor_t *s, int v) { if (s) s->set_vflip(s, v); }
void up_dw(sensor_t *s, int v) { if (s) s->set_dcw(s, v); }
void up_cb(sensor_t *s, int v) { if (s) s->set_colorbar(s, v); }
void up_fs(sensor_t *s, int v) { if (s) s->set_framesize(s, (framesize_t)v); }

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

MenuItem menu[] = {
  {"Res. Imagen", 0, 21, 8, up_fs},
  {"Brillo", -2, 2, 0, up_br},
  {"Contraste", -2, 2, 1, up_co},
  {"Saturacion", -2, 2, 0, up_sa},
  {"Efecto Esp", 0, 6, 0, up_ef},
  {"W. Balance", 0, 1, 1, up_wb},
  {"AWB Gain", 0, 1, 1, up_aw},
  {"WB Modo", 0, 4, 0, up_wm},
  {"Exp Ctrl", 0, 1, 1, up_ec},
  {"AEC2", 0, 1, 0, up_a2},
  {"AE Level", -2, 2, 0, up_al},
  {"AEC Value", 0, 1200, 300, up_av},
  {"Gain Ctrl", 0, 1, 1, up_gc},
  {"AGC Gain", 0, 30, 0, up_ag},
  {"G. Ceiling", 0, 6, 0, up_gl},
  {"BPC", 0, 1, 0, up_bp},
  {"WPC", 0, 1, 1, up_wp},
  {"Raw GMA", 0, 1, 1, up_rg},
  {"Lens Corr", 0, 1, 1, up_lc},
  {"H-Mirror", 0, 1, 0, up_hm},
  {"V-Flip", 0, 1, 0, up_vf},
  {"DCW", 0, 1, 1, up_dw},
  {"Colorbar", 0, 1, 0, up_cb}
};

const int MENU_TOTAL = sizeof(menu) / sizeof(MenuItem);
int menuIndex = 0;
int scrollOffset = 0;
bool inEditMode = false;
int lastClk = HIGH;

void updateMenu();
void handleEncoder();
void handleMenuButton();
void takeAndSendPhoto();

void setup() {
  Serial.begin(115200); // Debug por USB
  Serial.println("\n[SISTEMA] Iniciando firmware de camara...");
  Serial.flush();
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

  Wire.begin(OLED_SDA, OLED_SCL);
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
  config.xclk_freq_hz = 12000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    Serial.println("PSRAM DETECTED");
    config.frame_size = FRAMESIZE_QSXGA; // Reservar el buffer máximo nativo en PSRAM para permitir escalamiento dinámico posterior
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
    display.println("ERR CAM INIT");
    display.display();
    return;
  }

  s = esp_camera_sensor_get();
  // Aplicar la resolución inicial del menú (VGA) para que la cámara no inicie capturando en 5MP de golpe
  s->set_framesize(s, (framesize_t)menu[0].currentVal);

  // Configuraciones base del sensor
  s->set_vflip(s, 0); 
  s->set_brightness(s, 0); 
  s->set_saturation(s, 0); 
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_contrast(s, 1);

  // Inicializar pines del encoder con pull-up interno
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);

  updateMenu();
}

void loop() {
  handleEncoder();
  handleMenuButton();

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
        Serial.println("Foto liberada de PSRAM");
      }
      display.println("Foto Liberada");
      display.display();
      delay(500);
      updateMenu();
    }
  }
}

void handleEncoder() {
  int clk = digitalRead(PIN_CLK);
  if (clk != lastClk && clk == LOW) {
    bool dir = digitalRead(PIN_DT) != clk;
    if (!inEditMode) {
      menuIndex = dir ? min(menuIndex + 1, MENU_TOTAL - 1) : max(menuIndex - 1, 0);
      if (menuIndex >= scrollOffset + 5) scrollOffset++;
      if (menuIndex < scrollOffset) scrollOffset--;
    } else {
      menu[menuIndex].currentVal = constrain(menu[menuIndex].currentVal + (dir ? 1 : -1), 
                                           menu[menuIndex].minVal, menu[menuIndex].maxVal);
      menu[menuIndex].updateFunc(s, menu[menuIndex].currentVal);
    }
    updateMenu();
  }
  lastClk = clk;
}

void handleMenuButton() {
  if (digitalRead(PIN_SW) == LOW) {
    delay(200); // Debounce de botón
    inEditMode = !inEditMode;
    updateMenu();
    while(digitalRead(PIN_SW) == LOW);
  }
}

void takeAndSendPhoto() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(">> PETICION UART");

  // Buscar dinámicamente el nombre de la resolución actual
  const char* fsName = "VGA";
  for (int i = 0; i < MENU_TOTAL; i++) {
    if (strcmp(menu[i].name, "Res. Imagen") == 0) {
      fsName = getFrameSizeName(menu[i].currentVal);
      break;
    }
  }

  display.print("Capturando ");
  display.print(fsName);
  display.println("...");
  display.display();

  // Liberar búfer anterior si existiese
  if (currentFb) {
    esp_camera_fb_return(currentFb);
    currentFb = NULL;
  }

  currentFb = esp_camera_fb_get();
  if (!currentFb) {
    Serial.println("Error capture");
    display.println("ERROR DE CAMARA");
    display.display();
    uint32_t zeroSize = 0;
    heltecSerial.write((uint8_t*)&zeroSize, 4);
    delay(1000);
    updateMenu();
    return;
  }

  display.print("Tamano: ");
  display.print(currentFb->len / 1024);
  display.println(" KB");
  display.println("Esperando chunks...");
  display.display();

  // Verificar si el marcador FF D9 está presente en el búfer de la cámara
  if (currentFb->len >= 2) {
      uint8_t b1 = currentFb->buf[currentFb->len - 2];
      uint8_t b2 = currentFb->buf[currentFb->len - 1];
      Serial.printf("[CAMERA DEBUG] Últimos dos bytes en PSRAM: %02X %02X (Esperado: FF D9)\n", b1, b2);
      
      bool foundEOI = false;
      for (size_t i = 0; i < currentFb->len - 1; i++) {
          if (currentFb->buf[i] == 0xFF && currentFb->buf[i+1] == 0xD9) {
              Serial.printf("[CAMERA DEBUG] ¡Marcador FF D9 encontrado en el índice %u! (Distancia al final: %u bytes)\n", i, currentFb->len - i - 2);
              foundEOI = true;
              break;
          }
      }
      if (!foundEOI) {
          Serial.println("[CAMERA DEBUG] ¡Marcador FF D9 NO ENCONTRADO en todo el búfer de la cámara!");
      }
  }

  // Notificar al Master MCU el tamaño exacto con 4 bytes binarios puros
  uint32_t imgSize = currentFb->len;
  heltecSerial.write((uint8_t*)&imgSize, 4);
  
  Serial.printf("Foto capturada y retenida en PSRAM. Tam: %u bytes. Esperando peticiones de chunks...\n", imgSize);
}

void updateMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Parte Superior: Indice del menú
  display.setCursor(0, 0);
  display.print("MENU: "); 
  display.print(menuIndex + 1); 
  display.print("/"); 
  display.println(MENU_TOTAL);
  
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  for (int i = 0; i < 5; i++) {
    int idx = i + scrollOffset;
    if (idx >= MENU_TOTAL) break;
    display.setCursor(0, 12 + (i * 10));
    if (idx == menuIndex) display.print(inEditMode ? " >[" : " > ");
    else display.print("   ");
    display.print(menu[idx].name);
    display.print(": ");
    if (strcmp(menu[idx].name, "Res. Imagen") == 0) {
      display.print(getFrameSizeName(menu[idx].currentVal));
    } else {
      display.print(menu[idx].currentVal);
    }
    if (inEditMode && idx == menuIndex) display.print("]");
  }
  display.display();
}
