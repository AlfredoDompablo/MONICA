/**
 * @file main.cpp
 * @brief Firmware de Producción para la Cámara ESP32-CAM con Pantalla OLED y Encoder Rotativo.
 * 
 * Este programa administra el módulo de captura de imagen OV2640/OV5640 acoplado a un microcontrolador
 * Freenove ESP32-S3 WROOM CAM. Cuenta con una interfaz gráfica en una pantalla OLED SSD1306 (I2C)
 * controlada por un encoder rotativo que permite configurar de forma manual más de 20 parámetros
 * de la cámara en tiempo real (brillo, contraste, resolución, balance de blancos, etc.).
 * 
 * Conexión UART con la placa Heltec Wireless Tracker (MCU Maestro):
 *  - Recibe comandos simples ("TAKE_PIC", "GET_CHUNK <offset> <len>", "RELEASE_PIC") y transmite
 *    los datos binarios crudos a través de un canal UART de alta velocidad dedicado (UART1).
 * 
 * Diseñado y documentado profesionalmente para control y diagnóstico local.
 */

#include <Arduino.h>
#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// --- PINES FÍSICOS Y DIMENSIONES DE LA PANTALLA OLED SSD1306 ---
// ============================================================================
#define OLED_SDA 47           ///< Línea de Datos I2C (SDA)
#define OLED_SCL 21           ///< Línea de Reloj I2C (SCL)
#define SCREEN_WIDTH 128      ///< Ancho de la pantalla en píxeles
#define SCREEN_HEIGHT 64      ///< Alto de la pantalla en píxeles
#define OLED_RESET    -1      ///< Pin de reset del OLED (No utilizado, conectado a VCC)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================================
// --- ASIGNACIÓN DE PINES DE LA CÁMARA (Freenove ESP32-S3 Cam Board) ---
// ============================================================================
#define PWDN_GPIO_NUM     -1  ///< Pin de apagado de cámara (No disponible en hardware)
#define RESET_GPIO_NUM    -1  ///< Pin de reinicio de cámara (No disponible en hardware)
#define XCLK_GPIO_NUM     15  ///< Entrada de reloj externo del sistema
#define SIOD_GPIO_NUM     4   ///< Datos SCCB (SDA)
#define SIOC_GPIO_NUM     5   ///< Reloj SCCB (SCL)
#define Y9_GPIO_NUM       16  ///< Bus de datos paralelo D7
#define Y8_GPIO_NUM       17  ///< Bus de datos paralelo D6
#define Y7_GPIO_NUM       18  ///< Bus de datos paralelo D5
#define Y6_GPIO_NUM       12  ///< Bus de datos paralelo D4
#define Y5_GPIO_NUM       10  ///< Bus de datos paralelo D3
#define Y4_GPIO_NUM       8   ///< Bus de datos paralelo D2
#define Y3_GPIO_NUM       9   ///< Bus de datos paralelo D1
#define Y2_GPIO_NUM       11  ///< Bus de datos paralelo D0
#define VSYNC_GPIO_NUM    6   ///< Sincronización vertical
#define HREF_GPIO_NUM     7   ///< Referencia horizontal
#define PCLK_GPIO_NUM     13  ///< Reloj de píxeles (Pixel Clock)

// ============================================================================
// --- CONEXIÓN DE COMUNICACIÓN SERIE UART CON PLACA HELTEC (Master MCU) ---
// ============================================================================
#define HELTEC_UART_RX 41     ///< Pin RX de recepción de comandos
#define HELTEC_UART_TX 42     ///< Pin TX de transmisión de bytes JPG
HardwareSerial heltecSerial(1); ///< Canal de hardware UART1 dedicado para no interferir con depuración USB

// ============================================================================
// --- PINES DEL ENCODER ROTATIVO (Puertos de Tarjeta SD Liberados) ---
// ============================================================================
#define PIN_CLK 38            ///< Entrada de pulso de giro (Clock)
#define PIN_DT  39            ///< Entrada de dirección de giro (Data)
#define PIN_SW  40            ///< Entrada del botón de pulsación (Switch)

// ============================================================================
// --- VARIABLES DE ESTADO Y REFERENCIAS GLOBALES ---
// ============================================================================
sensor_t * s = NULL;              ///< Puntero al sensor CMOS físico
camera_fb_t * currentFb = NULL;   ///< Puntero al Frame Buffer en PSRAM

/**
 * @struct MenuItem
 * @brief Estructura de mapeo para parámetros del menú interactivo en el OLED.
 */
struct MenuItem {
  const char* name;                              ///< Nombre legible del parámetro en pantalla
  int minVal;                                    ///< Límite mínimo de configuración
  int maxVal;                                    ///< Límite máximo de configuración
  int currentVal;                                ///< Valor activo actual
  void (*updateFunc)(sensor_t * s, int val);     ///< Callback para aplicar cambio en registros del sensor
};

// --- Callbacks de actualización inmediata de los registros del sensor ---
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

/**
 * @brief Obtiene la cadena descriptiva de una resolución por su índice.
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

// --- Menú de Calibración ---
MenuItem menu[] = {
  {"Res. Imagen", 0, 21, 10, up_fs}, ///< Por defecto XGA (1024x768)
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
int menuIndex = 0;             ///< Opción actualmente seleccionada
int scrollOffset = 0;          ///< Desplazamiento del menú vertical
bool inEditMode = false;       ///< Bandera que indica si estamos en modo edición de un parámetro
int lastClk = HIGH;            ///< Estado anterior del encoder para detectar dirección de giro

void updateMenu();
void handleEncoder();
void handleMenuButton();
void takeAndSendPhoto();

/**
 * @brief Inicializa los periféricos, pantalla OLED, sensores y pines del encoder.
 */
void setup() {
  Serial.begin(115200); 
  Serial.println("\n[SISTEMA] Iniciando firmware de camara (Version OLED/Encoder)...");
  Serial.flush();
  
  // Iniciar la comunicación serie de comandos con el Heltec
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

  // Inicializar bus I2C y pantalla OLED local
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] OLED init failed!");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Freenove ESP32-S3");
    display.println("Modo Diagnostico: OK");
    display.display();
  }

  // --- CONFIGURACIÓN DE LA CÁMARA ---
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
  
  // Frecuencia del reloj XCLK ajustada a 10MHz para máxima estabilidad con OV5640
  config.xclk_freq_hz = 10000000;  
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Reservar buffers de captura según la memoria disponible
  if(psramFound()){
    Serial.println("[INFO] PSRAM detectada.");
    config.frame_size = FRAMESIZE_QSXGA; 
    config.jpeg_quality = 14;            ///< Mayor fidelidad inicial (14)
    config.fb_count = 1; 
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("[WARNING] Usando DRAM interna (Limitado a VGA).");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Inicializar controlador
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
    display.println("ERR CAM INIT");
    display.display();
    return;
  }

  s = esp_camera_sensor_get();
  
  // Forzar límites de resolución física
  if (!psramFound() && menu[0].currentVal > 8) {
    menu[0].currentVal = 8; 
  }
  s->set_framesize(s, (framesize_t)menu[0].currentVal);

  // Parámetros de control predeterminados
  s->set_vflip(s, 0); 
  s->set_brightness(s, 0); 
  s->set_saturation(s, 0); 
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_contrast(s, 1);

  // Inicializar pines de interacción física del encoder
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);

  updateMenu();
}

/**
 * @brief Bucle de ejecución. Monitorea el encoder local e interacciones serie.
 */
void loop() {
  handleEncoder();      
  handleMenuButton();   

  if (heltecSerial.available()) {
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();
    
    // Comando: GET_CHUNK <offset> <len>
    if (cmd.startsWith("GET_CHUNK")) {
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (firstSpace != -1 && secondSpace != -1) {
        uint32_t offset = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t len = cmd.substring(secondSpace + 1).toInt();
        
        if (currentFb && offset + len <= currentFb->len) {
          heltecSerial.write(currentFb->buf + offset, len);
        } else {
          uint8_t* zeroBuf = (uint8_t*)calloc(len, 1);
          if (zeroBuf) {
            heltecSerial.write(zeroBuf, len);
            free(zeroBuf);
          }
        }
      }
    }
    // Comando: TAKE_PIC
    else if (cmd == "TAKE_PIC") {
      takeAndSendPhoto();
    }
    // Comando: RELEASE_PIC
    else if (cmd == "RELEASE_PIC") {
      if (currentFb) {
        esp_camera_fb_return(currentFb);
        currentFb = NULL;
        Serial.println("[INFO] Foto liberada de PSRAM.");
      }
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Foto Liberada");
      display.display();
      delay(500);
      updateMenu();
    }
    // Comando: SET_CONFIG <res> <br> <co> <qty> <sa> <ef> <wb> <aw> <wm> <ec> <a2> <al> <av> <gc> <ag> <gl> <bp> <wp> <rg> <lc> <hm> <vf> <dw> <cb>
    else if (cmd.startsWith("SET_CONFIG")) {
      int res = 10, br = 0, co = 1, qty = 14;
      int sa = 0, ef = 0, wb = 1, aw = 1, wm = 0, ec = 1, a2 = 0, al = 0, av = 300;
      int gc = 1, ag = 0, gl = 0, bp = 0, wp = 1, rg = 1, lc = 1, hm = 0, vf = 0, dw = 1, cb = 0;
      
      int parsed = sscanf(cmd.c_str() + 11, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                          &res, &br, &co, &qty, &sa, &ef, &wb, &aw, &wm, &ec, &a2, &al, &av, &gc, &ag, &gl, &bp, &wp, &rg, &lc, &hm, &vf, &dw, &cb);
      
      if (parsed >= 3) {
        res = constrain(res, 0, 21);
        br = constrain(br, -2, 2);
        co = constrain(co, -2, 2);
        if (parsed >= 4) qty = constrain(qty, 10, 63);
        if (parsed >= 5) sa = constrain(sa, -2, 2);
        if (parsed >= 6) ef = constrain(ef, 0, 6);
        if (parsed >= 7) wb = constrain(wb, 0, 1);
        if (parsed >= 8) aw = constrain(aw, 0, 1);
        if (parsed >= 9) wm = constrain(wm, 0, 4);
        if (parsed >= 10) ec = constrain(ec, 0, 1);
        if (parsed >= 11) a2 = constrain(a2, 0, 1);
        if (parsed >= 12) al = constrain(al, -2, 2);
        if (parsed >= 13) av = constrain(av, 0, 1200);
        if (parsed >= 14) gc = constrain(gc, 0, 1);
        if (parsed >= 15) ag = constrain(ag, 0, 30);
        if (parsed >= 16) gl = constrain(gl, 0, 6);
        if (parsed >= 17) bp = constrain(bp, 0, 1);
        if (parsed >= 18) wp = constrain(wp, 0, 1);
        if (parsed >= 19) rg = constrain(rg, 0, 1);
        if (parsed >= 20) lc = constrain(lc, 0, 1);
        if (parsed >= 21) hm = constrain(hm, 0, 1);
        if (parsed >= 22) vf = constrain(vf, 0, 1);
        if (parsed >= 23) dw = constrain(dw, 0, 1);
        if (parsed >= 24) cb = constrain(cb, 0, 1);
        
        menu[0].currentVal = res;
        menu[1].currentVal = br;
        menu[2].currentVal = co;
        if (parsed >= 5) menu[3].currentVal = sa;
        if (parsed >= 6) menu[4].currentVal = ef;
        if (parsed >= 7) menu[5].currentVal = wb;
        if (parsed >= 8) menu[6].currentVal = aw;
        if (parsed >= 9) menu[7].currentVal = wm;
        if (parsed >= 10) menu[8].currentVal = ec;
        if (parsed >= 11) menu[9].currentVal = a2;
        if (parsed >= 12) menu[10].currentVal = al;
        if (parsed >= 13) menu[11].currentVal = av;
        if (parsed >= 14) menu[12].currentVal = gc;
        if (parsed >= 15) menu[13].currentVal = ag;
        if (parsed >= 16) menu[14].currentVal = gl;
        if (parsed >= 17) menu[15].currentVal = bp;
        if (parsed >= 18) menu[16].currentVal = wp;
        if (parsed >= 19) menu[17].currentVal = rg;
        if (parsed >= 20) menu[18].currentVal = lc;
        if (parsed >= 21) menu[19].currentVal = hm;
        if (parsed >= 22) menu[20].currentVal = vf;
        if (parsed >= 23) menu[21].currentVal = dw;
        if (parsed >= 24) menu[22].currentVal = cb;

        if (s) {
          s->set_framesize(s, (framesize_t)res);
          s->set_brightness(s, br);
          s->set_contrast(s, co);
          if (parsed >= 4) s->set_quality(s, qty);
          if (parsed >= 5) s->set_saturation(s, sa);
          if (parsed >= 6) s->set_special_effect(s, ef);
          if (parsed >= 7) s->set_whitebal(s, wb);
          if (parsed >= 8) s->set_awb_gain(s, aw);
          if (parsed >= 9) s->set_wb_mode(s, wm);
          if (parsed >= 10) s->set_exposure_ctrl(s, ec);
          if (parsed >= 11) s->set_aec2(s, a2);
          if (parsed >= 12) s->set_ae_level(s, al);
          if (parsed >= 13) s->set_aec_value(s, av);
          if (parsed >= 14) s->set_gain_ctrl(s, gc);
          if (parsed >= 15) s->set_agc_gain(s, ag);
          if (parsed >= 16) s->set_gainceiling(s, (gainceiling_t)gl);
          if (parsed >= 17) s->set_bpc(s, bp);
          if (parsed >= 18) s->set_wpc(s, wp);
          if (parsed >= 19) s->set_raw_gma(s, rg);
          if (parsed >= 20) s->set_lenc(s, lc);
          if (parsed >= 21) s->set_hmirror(s, hm);
          if (parsed >= 22) s->set_vflip(s, vf);
          if (parsed >= 23) s->set_dcw(s, dw);
          if (parsed >= 24) s->set_colorbar(s, cb);
        }
        
        Serial.printf("[CAMERA] Configurada remotamente: Res=%d, Br=%d, Co=%d, Qty=%d, Sa=%d, Ef=%d, WB=%d\n", res, br, co, qty, sa, ef, wb);
        heltecSerial.println("CONF_ACK");
      }
    }
  }
}

unsigned long lastButtonTime = 0;
bool lastButtonState = HIGH;
unsigned long lastEncoderTime = 0;

/**
 * @brief Gestiona el giro del encoder rotativo.
 * Cambia la selección de menú o el valor numérico según si estamos en modo edición o navegación.
 */
void handleEncoder() {
  int clk = digitalRead(PIN_CLK);
  if (clk != lastClk) {
    unsigned long now = millis();
    if (now - lastEncoderTime > 5) { // Ignorar rebotes menores a 5ms
      lastEncoderTime = now;
      if (clk == LOW) {
        bool dir = digitalRead(PIN_DT) != clk; ///< true: Horario, false: Antihorario
        if (!inEditMode) {
          // Modo Navegación: Cambiar selección de fila en pantalla
          menuIndex = dir ? min(menuIndex + 1, MENU_TOTAL - 1) : max(menuIndex - 1, 0);
          
          // Ajustar scroll automático de 5 renglones
          if (menuIndex >= scrollOffset + 5) scrollOffset = menuIndex - 4;
          if (menuIndex < scrollOffset) scrollOffset = menuIndex;
        } else {
          // Modo Edición: Incrementar o decrementar el parámetro del menú
          menu[menuIndex].currentVal = constrain(menu[menuIndex].currentVal + (dir ? 1 : -1), 
                                               menu[menuIndex].minVal, menu[menuIndex].maxVal);
          // Aplicar el valor modificado inmediatamente al sensor
          menu[menuIndex].updateFunc(s, menu[menuIndex].currentVal);
        }
        updateMenu();
      }
    }
    lastClk = clk;
  }
}

/**
 * @brief Gestiona la pulsación física del botón del encoder de manera no-bloqueante.
 * Alterna el estado de edición.
 */
void handleMenuButton() {
  bool currentButtonState = digitalRead(PIN_SW);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    unsigned long now = millis();
    if (now - lastButtonTime > 250) { // Debounce de 250ms
      lastButtonTime = now;
      inEditMode = !inEditMode;
      updateMenu();
      Serial.printf("[UI] Cambio a modo %s\n", inEditMode ? "EDICION" : "NAVEGACION");
    }
  }
  lastButtonState = currentButtonState;
}

/**
 * @brief Dispara el CMOS y captura la imagen a PSRAM.
 * Transmite el tamaño exacto `SIZE:<bytes>` al maestro e informa localmente en el OLED.
 */
void takeAndSendPhoto() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(">> PETICION UART");

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

  currentFb = esp_camera_fb_get();
  if (!currentFb) {
    Serial.println("[ERROR] Captura CMOS fallida.");
    display.println("ERROR DE CAMARA");
    display.display();
    
    heltecSerial.println("SIZE:0"); // Responder tamaño 0 formateado como texto
    delay(1000);
    updateMenu();
    return;
  }

  display.print("Tamano: ");
  display.print(currentFb->len / 1024);
  display.println(" KB");
  display.println("Esperando chunks...");
  display.display();

  // Validar fin de imagen JPEG
  if (currentFb->len >= 2) {
      uint8_t b1 = currentFb->buf[currentFb->len - 2];
      uint8_t b2 = currentFb->buf[currentFb->len - 1];
      Serial.printf("[DEBUG] Últimos dos bytes en PSRAM: %02X %02X (Esperado: FF D9)\n", b1, b2);
  }

  uint32_t imgSize = currentFb->len;
  heltecSerial.printf("SIZE:%u\n", imgSize);
  Serial.printf("[INFO] Foto capturada. Tamaño: %u bytes.\n", imgSize);
}

/**
 * @brief Renderiza la interfaz de menú interactivo en la pantalla OLED.
 */
void updateMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.print("MENU: "); 
  display.print(menuIndex + 1); 
  display.print("/"); 
  display.println(MENU_TOTAL);
  
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE); 

  // Mostrar 5 opciones simultáneas
  for (int i = 0; i < 5; i++) {
    int idx = i + scrollOffset;
    if (idx >= MENU_TOTAL) break;
    
    display.setCursor(0, 12 + (i * 10));
    
    // Indicador de selección activa
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
