/**
 * @file main.cpp
 * @brief Firmware de Producción para la Cámara ESP32-CAM con Pantalla OLED y Encoder Rotativo.
 * 
 * Este programa administra el módulo de captura de imagen OV2640 acoplado a un microcontrolador
 * Freenove ESP32-S3 WROOM CAM. Cuenta con una interfaz gráfica en una pantalla OLED SSD1306 (I2C)
 * controlada por un encoder rotativo que permite configurar de forma manual más de 20 parámetros
 * de la cámara en tiempo real (brillo, contraste, resolución, balance de blancos, etc.).
 * 
 * Conexión UART con la placa Heltec Wireless Tracker (MCU Maestro):
 *  - Recibe comandos simples ("TAKE_PIC", "GET_CHUNK <offset> <len>", "RELEASE_PIC") y transmite
 *    los datos binarios crudos a través de un canal UART de alta velocidad dedicado (UART1).
 */

#include <Arduino.h>
#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Pines Físicos y Dimensiones de la Pantalla OLED SSD1306 ---
#define OLED_SDA 47           // Línea de Datos I2C.
#define OLED_SCL 21           // Línea de Reloj I2C.
#define SCREEN_WIDTH 128      // Ancho en píxeles.
#define SCREEN_HEIGHT 64      // Alto en píxeles.
#define OLED_RESET    -1      // Pin de reinicio de la pantalla (no utilizado, conectado a VCC).
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Asignación de Pines de la Cámara OV2640 (Freenove ESP32-S3 Board) ---
#define PWDN_GPIO_NUM     -1  // Pin de apagado por hardware (no disponible).
#define RESET_GPIO_NUM    -1  // Pin de reinicio por hardware (no disponible).
#define XCLK_GPIO_NUM     15  // Entrada de reloj del sistema.
#define SIOD_GPIO_NUM     4   // Bus de datos SCCB (Equivalente a SDA).
#define SIOC_GPIO_NUM     5   // Bus de reloj SCCB (Equivalente a SCL).
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
#define PCLK_GPIO_NUM     13  // Reloj de píxeles.

// --- Conexión Serie de Comunicación con Placa Heltec (Master MCU) ---
#define HELTEC_UART_RX 41     // Pin RX de recepción de comandos.
#define HELTEC_UART_TX 42     // Pin TX de envío de bytes JPG.
HardwareSerial heltecSerial(1); // Canal UART1 dedicado para no causar interferencias con el USB de depuración.

// --- Pines del Encoder Rotativo ---
// Mapeados estratégicamente a los pines liberados del lector de tarjetas SD de la cámara
#define PIN_CLK 38            // Entrada de pulso de giro (Clock).
#define PIN_DT  39            // Entrada de dirección de giro (Data).
#define PIN_SW  40            // Entrada del interruptor de pulsación (Switch).

// --- Referencias y Búferes Globales ---
sensor_t * s = NULL;          // Puntero del controlador del sensor OV2640.
camera_fb_t * currentFb = NULL; // Búfer de cuadro (Frame Buffer) activo retenido en memoria RAM.

// --- Estructura para los Elementos del Menú ---
struct MenuItem {
  const char* name;                              // Etiqueta del parámetro a mostrar en la pantalla.
  int minVal;                                    // Límite mínimo de configuración del sensor.
  int maxVal;                                    // Límite máximo de configuración del sensor.
  int currentVal;                                // Valor actual configurado.
  void (*updateFunc)(sensor_t * s, int val);     // Puntero a la función callback de actualización del sensor.
};

// --- Callbacks de Actualización Directa de Parámetros del Sensor ---
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
 * @brief Obtiene el nombre estandarizado de la resolución según su índice entero.
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

// --- Definición del Menú con los valores por defecto iniciales ---
MenuItem menu[] = {
  {"Res. Imagen", 0, 21, 21, up_fs}, // Por defecto QSXGA (21)
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

const int MENU_TOTAL = sizeof(menu) / sizeof(MenuItem); // Cantidad de parámetros del menú.
int menuIndex = 0;                                       // Índice del parámetro actualmente seleccionado.
int scrollOffset = 0;                                    // Desplazamiento vertical en pantalla de la lista del menú.
bool inEditMode = false;                                 // Indica si estamos modificando el valor numérico del parámetro.
int lastClk = HIGH;                                      // Último valor de reloj del encoder rotativo para detectar cambios.

void updateMenu();
void handleEncoder();
void handleMenuButton();
void takeAndSendPhoto();

/**
 * @brief Inicialización de periféricos y sensores.
 */
void setup() {
  Serial.begin(115200); // Terminal USB de depuración
  Serial.println("\n[SISTEMA] Iniciando firmware de camara...");
  Serial.flush();
  
  // Serie dedicada para hablar con el maestro (Placa Heltec)
  heltecSerial.begin(115200, SERIAL_8N1, HELTEC_UART_RX, HELTEC_UART_TX);

  // Inicializar bus I2C y pantalla OLED
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
  
  // Reservar el búfer máximo posible si hay PSRAM disponible para permitir cambios
  // dinámicos de resoluciones altas sin colapsar por falta de espacio en memoria.
  if(psramFound()){
    Serial.println("PSRAM DETECTED");
    config.frame_size = FRAMESIZE_QSXGA; // Límite en 5 megapíxeles.
    config.jpeg_quality = 14;            // Compresión de alta calidad.
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

  // Inicializar el subsistema esp-camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    display.println("ERR CAM INIT");
    display.display();
    return;
  }

  s = esp_camera_sensor_get();
  
  // Forzar la resolución inicial del menú (VGA o QSXGA si hay PSRAM)
  if (!psramFound() && menu[0].currentVal > 8) {
    menu[0].currentVal = 8; // Forzar VGA si no hay PSRAM
  }
  s->set_framesize(s, (framesize_t)menu[0].currentVal);

  // Inicialización de parámetros del sensor
  s->set_vflip(s, 0); 
  s->set_brightness(s, 0); 
  s->set_saturation(s, 0); 
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_contrast(s, 1);

  // Inicializar pines del encoder rotativo
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);

  // Renderizar la primera pantalla del menú interactivo
  updateMenu();
}

/**
 * @brief Bucle de ejecución infinito. Escucha ráfagas serie y controla encoder.
 */
void loop() {
  handleEncoder();      // Leer giros del encoder
  handleMenuButton();   // Leer clicks del encoder

  // --- PARSER DE COMANDOS DESDE EL MASTER MCU (Heltec) ---
  if (heltecSerial.available()) {
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();
    
    // Comando 1: Solicitar un fragmento binario exacto de la foto actual
    if (cmd.startsWith("GET_CHUNK")) {
      // Formato esperado: "GET_CHUNK <offset> <len>"
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (firstSpace != -1 && secondSpace != -1) {
        uint32_t offset = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t len = cmd.substring(secondSpace + 1).toInt();
        
        // Escribir bytes si el búfer es válido y está dentro de los límites
        if (currentFb && offset + len <= currentFb->len) {
          heltecSerial.write(currentFb->buf + offset, len);
        } else {
          // Resguardo seguro: rellenar con ceros para no colgar la máquina serie del maestro
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
    // Comando 3: Liberar el cuadro capturado de la RAM PSRAM
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

/**
 * @brief Procesa el giro físico del encoder rotativo.
 */
void handleEncoder() {
  int clk = digitalRead(PIN_CLK);
  
  // Detectar flanco de bajada de la señal del canal CLK
  if (clk != lastClk && clk == LOW) {
    bool dir = digitalRead(PIN_DT) != clk; // Determinar dirección de giro (izquierda/derecha)
    
    if (!inEditMode) {
      // Modo Navegación: Mover la flecha selectora a lo largo del menú
      menuIndex = dir ? min(menuIndex + 1, MENU_TOTAL - 1) : max(menuIndex - 1, 0);
      
      // Control de scroll automático vertical de 5 renglones
      if (menuIndex >= scrollOffset + 5) scrollOffset++;
      if (menuIndex < scrollOffset) scrollOffset--;
    } else {
      // Modo Edición: Incrementar o decrementar el valor numérico del parámetro
      menu[menuIndex].currentVal = constrain(menu[menuIndex].currentVal + (dir ? 1 : -1), 
                                           menu[menuIndex].minVal, menu[menuIndex].maxVal);
      // Aplicar el cambio de forma inmediata al sensor
      menu[menuIndex].updateFunc(s, menu[menuIndex].currentVal);
    }
    updateMenu();
  }
  lastClk = clk;
}

/**
 * @brief Procesa la pulsación física sobre el eje del encoder para entrar/salir de edición.
 */
void handleMenuButton() {
  if (digitalRead(PIN_SW) == LOW) {
    delay(200); // Debounce defensivo contra falsos rebotes
    inEditMode = !inEditMode; // Alternar modo navegación / modo edición
    updateMenu();
    while(digitalRead(PIN_SW) == LOW); // Esperar a que el usuario suelte el botón
  }
}

/**
 * @brief Captura una instantánea JPEG utilizando los parámetros del sensor actuales.
 * 
 * Flujo:
 *  1. Detiene la visualización en menú e indica captura en OLED.
 *  2. Libera cualquier búfer anterior para evitar desbordes.
 *  3. Llama al motor de hardware `esp_camera_fb_get()`.
 *  4. Ejecuta un análisis de diagnóstico local para detectar la existencia física
 *     del marcador indicador de fin de archivo JPEG (`FF D9`) para certificar que la foto
 *     no tiene cortes o pérdidas de píxeles internos por interferencias de bus.
 *  5. Transmite los 4 bytes en binario del tamaño de la imagen sobre la UART al Heltec.
 *  6. Retiene el cuadro en memoria para proveer los fragmentos mediante el parser de `GET_CHUNK`.
 */
void takeAndSendPhoto() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(">> PETICION UART");

  // Buscar el nombre de la resolución actual
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

  // Liberar búfer antiguo
  if (currentFb) {
    esp_camera_fb_return(currentFb);
    currentFb = NULL;
  }

  // Capturar imagen
  currentFb = esp_camera_fb_get();
  if (!currentFb) {
    Serial.println("Error capture");
    display.println("ERROR DE CAMARA");
    display.display();
    
    // Responder tamaño 0 ante fallos
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

  // --- DIAGNÓSTICO FÍSICO DE INTEGRIDAD DE JPEG (EOI - End of Image) ---
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

  // Escribir 4 bytes en binario del tamaño total del archivo JPG
  uint32_t imgSize = currentFb->len;
  heltecSerial.write((uint8_t*)&imgSize, 4);
  
  Serial.printf("Foto capturada y retenida en PSRAM. Tam: %u bytes. Esperando peticiones de chunks...\n", imgSize);
}

/**
 * @brief Dibuja y refresca los parámetros del menú en el display SSD1306.
 */
void updateMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Renderizar indicador de renglón e índice (Paso actual)
  display.setCursor(0, 0);
  display.print("MENU: "); 
  display.print(menuIndex + 1); 
  display.print("/"); 
  display.println(MENU_TOTAL);
  
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE); // Línea horizontal divisora

  // Renderizar 5 opciones visibles dinámicas
  for (int i = 0; i < 5; i++) {
    int idx = i + scrollOffset;
    if (idx >= MENU_TOTAL) break;
    
    display.setCursor(0, 12 + (i * 10));
    
    // Dibujar indicador visual según modo (Navegación o Edición)
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
