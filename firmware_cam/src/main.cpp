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
#include <WiFi.h>
#include "esp_http_server.h"
#include <Preferences.h>

// --- CONFIGURACIÓN DE WIFI PARA DIAGNÓSTICO EN VIVO ---
#define WIFI_SSID "HOME"
#define WIFI_PASS "I5isdeperropulgos0"
#define PIN_BOOT 0

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
Preferences prefs;



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

void saveCameraSettings() {
  prefs.begin("camera_cfg", false);
  prefs.putInt("fs", menu[0].currentVal);
  prefs.putInt("br", menu[1].currentVal);
  prefs.putInt("co", menu[2].currentVal);
  prefs.putInt("sa", menu[3].currentVal);
  prefs.putInt("ef", menu[4].currentVal);
  prefs.putInt("wb", menu[5].currentVal);
  prefs.putInt("aw", menu[6].currentVal);
  prefs.putInt("wm", menu[7].currentVal);
  prefs.putInt("ec", menu[8].currentVal);
  prefs.putInt("a2", menu[9].currentVal);
  prefs.putInt("al", menu[10].currentVal);
  prefs.putInt("av", menu[11].currentVal);
  prefs.putInt("gc", menu[12].currentVal);
  prefs.putInt("ag", menu[13].currentVal);
  prefs.putInt("gl", menu[14].currentVal);
  prefs.putInt("bp", menu[15].currentVal);
  prefs.putInt("wp", menu[16].currentVal);
  prefs.putInt("rg", menu[17].currentVal);
  prefs.putInt("lc", menu[18].currentVal);
  prefs.putInt("hm", menu[19].currentVal);
  prefs.putInt("vf", menu[20].currentVal);
  prefs.putInt("dw", menu[21].currentVal);
  prefs.putInt("cb", menu[22].currentVal);
  prefs.end();
  Serial.println("[NVS] Configuracion de camara guardada.");
}

void loadCameraSettings() {
  prefs.begin("camera_cfg", true);
  menu[0].currentVal = prefs.getInt("fs", menu[0].currentVal);
  menu[1].currentVal = prefs.getInt("br", menu[1].currentVal);
  menu[2].currentVal = prefs.getInt("co", menu[2].currentVal);
  menu[3].currentVal = prefs.getInt("sa", menu[3].currentVal);
  menu[4].currentVal = prefs.getInt("ef", menu[4].currentVal);
  menu[5].currentVal = prefs.getInt("wb", menu[5].currentVal);
  menu[6].currentVal = prefs.getInt("aw", menu[6].currentVal);
  menu[7].currentVal = prefs.getInt("wm", menu[7].currentVal);
  menu[8].currentVal = prefs.getInt("ec", menu[8].currentVal);
  menu[9].currentVal = prefs.getInt("a2", menu[9].currentVal);
  menu[10].currentVal = prefs.getInt("al", menu[10].currentVal);
  menu[11].currentVal = prefs.getInt("av", menu[11].currentVal);
  menu[12].currentVal = prefs.getInt("gc", menu[12].currentVal);
  menu[13].currentVal = prefs.getInt("ag", menu[13].currentVal);
  menu[14].currentVal = prefs.getInt("gl", menu[14].currentVal);
  menu[15].currentVal = prefs.getInt("bp", menu[15].currentVal);
  menu[16].currentVal = prefs.getInt("wp", menu[16].currentVal);
  menu[17].currentVal = prefs.getInt("rg", menu[17].currentVal);
  menu[18].currentVal = prefs.getInt("lc", menu[18].currentVal);
  menu[19].currentVal = prefs.getInt("hm", menu[19].currentVal);
  menu[20].currentVal = prefs.getInt("vf", menu[20].currentVal);
  menu[21].currentVal = prefs.getInt("dw", menu[21].currentVal);
  menu[22].currentVal = prefs.getInt("cb", menu[22].currentVal);
  prefs.end();
  Serial.println("[NVS] Configuracion de camara cargada.");
}

// Handler para la página de inicio (diagnóstico)
static esp_err_t index_handler(httpd_req_t *req) {

    const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>MONICA Live Diagnostics</title>
  <style>
    body { font-family: sans-serif; background: #121212; color: #e0e0e0; padding: 20px; }
    h1 { color: #fff; text-align: center; }
    .main { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }
    .pane { background: #1e1e1e; padding: 15px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    img { max-width: 100%; border-radius: 6px; display: block; margin: 0 auto; }
    .control-row { display: flex; justify-content: space-between; margin: 10px 0; align-items: center; gap: 15px; }
    label { font-weight: bold; font-size: 14px; }
    input[type=range] { width: 130px; }
    select { background: #333; color: #fff; border: 1px solid #555; padding: 4px; border-radius: 4px; }
    #toast {
      visibility: hidden;
      min-width: 250px;
      margin-left: -125px;
      background-color: #4caf50;
      color: #fff;
      text-align: center;
      border-radius: 4px;
      padding: 12px;
      position: fixed;
      z-index: 1000;
      left: 50%;
      bottom: 30px;
      font-size: 15px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.5);
    }
    #toast.show {
      visibility: visible;
      -webkit-animation: fadein 0.5s, fadeout 0.5s 1.5s;
      animation: fadein 0.5s, fadeout 0.5s 1.5s;
    }
    @-webkit-keyframes fadein {
      from {bottom: 0; opacity: 0;} 
      to {bottom: 30px; opacity: 1;}
    }
    @keyframes fadein {
      from {bottom: 0; opacity: 0;}
      to {bottom: 30px; opacity: 1;}
    }
    @-webkit-keyframes fadeout {
      from {bottom: 30px; opacity: 1;} 
      to {bottom: 0; opacity: 0;}
    }
    @keyframes fadeout {
      from {bottom: 30px; opacity: 1;}
      to {bottom: 0; opacity: 0;}
    }
  </style>
</head>
<body>
  <h1>MONICA Live Web Diagnostics</h1>
  <div class="main">
    <div class="pane" style="flex: 1 1 500px;">
      <h3 style="text-align:center;">Stream en Vivo</h3>
      <img id="stream-img" src="" />
    </div>
    <div class="pane" style="flex: 1 1 350px;">
      <h3>Ajustes del Sensor</h3>
      <div id="settings" style="max-height: 550px; overflow-y: auto; padding-right: 5px;"></div>
    </div>
  </div>
  <div id="toast">Configuracion guardada en NVS</div>
  <script>
    const params = [
      {name: "Res. Imagen", var: "framesize", type: "select", options: [
        {val:0, text:"96x96"}, {val:1, text:"QQVGA"}, {val:2, text:"QCIF"}, {val:3, text:"HQVGA"}, 
        {val:4, text:"240x240"}, {val:5, text:"QVGA"}, {val:6, text:"CIF"}, {val:7, text:"HVGA"}, 
        {val:8, text:"VGA"}, {val:9, text:"SVGA"}, {val:10, text:"XGA"}, {val:11, text:"HD"}, 
        {val:12, text:"SXGA"}, {val:13, text:"UXGA"}, {val:14, text:"FHD"}, {val:17, text:"QXGA"}, {val:21, text:"QSXGA"}
      ], def: 10},
      {name: "Brillo", var: "brightness", type: "range", min: -2, max: 2, def: 0},
      {name: "Contraste", var: "contrast", type: "range", min: -2, max: 2, def: 1},
      {name: "Saturacion", var: "saturation", type: "range", min: -2, max: 2, def: 0},
      {name: "Efecto Especial", var: "special_effect", type: "select", options: [
        {val:0, text:"Normal"}, {val:1, text:"Negativo"}, {val:2, text:"Escala Grises"}, {val:3, text:"Rojizo"}, {val:4, text:"Verdoso"}, {val:5, text:"Azulado"}, {val:6, text:"Sepia"}
      ], def: 0},
      {name: "Bal. Blancos", var: "whitebal", type: "select", options: [
        {val:0, text:"Manual"}, {val:1, text:"Auto"}
      ], def: 1},
      {name: "AWB Gain", var: "awb_gain", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 1},
      {name: "Modo WB", var: "wb_mode", type: "select", options: [
        {val:0, text:"Auto"}, {val:1, text:"Soleado"}, {val:2, text:"Nublado"}, {val:3, text:"Oficina"}, {val:4, text:"Hogar"}
      ], def: 0},
      {name: "Exp Ctrl (AEC)", var: "exposure_ctrl", type: "select", options: [
        {val:0, text:"Manual"}, {val:1, text:"Auto"}
      ], def: 1},
      {name: "AEC2", var: "aec2", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 0},
      {name: "Nivel AE", var: "ae_level", type: "range", min: -2, max: 2, def: 0},
      {name: "Val AEC", var: "aec_value", type: "range", min: 0, max: 1200, def: 300},
      {name: "Gain Ctrl (AGC)", var: "gain_ctrl", type: "select", options: [
        {val:0, text:"Manual"}, {val:1, text:"Auto"}
      ], def: 1},
      {name: "AGC Gain", var: "agc_gain", type: "range", min: 0, max: 30, def: 0},
      {name: "Gain Ceiling", var: "gainceiling", type: "select", options: [
        {val:0, text:"2x"}, {val:1, text:"4x"}, {val:2, text:"8x"}, {val:3, text:"16x"}, {val:4, text:"32x"}, {val:5, text:"64x"}, {val:6, text:"128x"}
      ], def: 0},
      {name: "BPC", var: "bpc", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 0},
      {name: "WPC", var: "wpc", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 1},
      {name: "Raw GMA", var: "raw_gma", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 1},
      {name: "Lens Corr", var: "lenc", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 1},
      {name: "H-Mirror", var: "hmirror", type: "select", options: [
        {val:0, text:"Normal"}, {val:1, text:"Espejo"}
      ], def: 0},
      {name: "V-Flip", var: "vflip", type: "select", options: [
        {val:0, text:"Normal"}, {val:1, text:"Volteado"}
      ], def: 0},
      {name: "DCW", var: "dcw", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 1},
      {name: "Colorbar", var: "colorbar", type: "select", options: [
        {val:0, text:"Desactivado"}, {val:1, text:"Activado"}
      ], def: 0}
    ];

    function showToast(msg) {
      const toast = document.getElementById("toast");
      toast.innerText = msg;
      toast.className = "show";
      setTimeout(() => { toast.className = toast.className.replace("show", ""); }, 2000);
    }

    let pendingRequests = {};
    function updateParam(name, val) {
      if (pendingRequests[name]) return;
      pendingRequests[name] = true;
      fetch(`/control?var=${name}&val=${val}`)
        .then(() => {
          setTimeout(() => { pendingRequests[name] = false; }, 100); // 100ms throttle
        })
        .catch(() => { pendingRequests[name] = false; });
    }

    function saveToNVS() {
      fetch('/control?var=save&val=1')
        .then(res => {
          if (res.ok) showToast("Configuración guardada en NVS");
          else alert("Error al guardar en NVS");
        });
    }

    const container = document.getElementById("settings");
    params.forEach(p => {
      const row = document.createElement("div");
      row.className = "control-row";
      const lbl = document.createElement("label");
      lbl.innerText = p.name;
      row.appendChild(lbl);
      if (p.type === "range") {
        const slider = document.createElement("input");
        slider.type = "range"; slider.min = p.min; slider.max = p.max; slider.value = p.def;
        slider.id = p.var;
        slider.style.flex = "1";
        
        const numInput = document.createElement("input");
        numInput.type = "number"; numInput.min = p.min; numInput.max = p.max; numInput.value = p.def;
        numInput.id = p.var + "-num";
        numInput.style.width = "65px";
        numInput.style.background = "#333";
        numInput.style.color = "white";
        numInput.style.border = "1px solid #555";
        numInput.style.padding = "4px";
        numInput.style.borderRadius = "4px";
        numInput.style.marginLeft = "10px";

        slider.oninput = () => {
          numInput.value = slider.value;
          updateParam(p.var, slider.value);
        };
        numInput.oninput = () => {
          slider.value = numInput.value;
          updateParam(p.var, numInput.value);
        };

        const controlContainer = document.createElement("div");
        controlContainer.style.display = "flex";
        controlContainer.style.alignItems = "center";
        controlContainer.style.flex = "1";
        controlContainer.style.justifyContent = "flex-end";
        
        controlContainer.appendChild(slider);
        controlContainer.appendChild(numInput);
        row.appendChild(controlContainer);
      } else {
        const sel = document.createElement("select");
        sel.id = p.var;
        sel.style.width = "120px";
        p.options.forEach(opt => {
          const o = document.createElement("option");
          o.value = opt.val; o.innerText = opt.text;
          if(opt.val === p.def) o.selected = true;
          sel.appendChild(o);
        });
        sel.onchange = () => {
          updateParam(p.var, sel.value);
        };
        row.appendChild(sel);
      }
      container.appendChild(row);
    });

    // Agregar botón de guardar al final
    const saveBtn = document.createElement("button");
    saveBtn.innerText = "Guardar en NVS";
    saveBtn.style.width = "100%";
    saveBtn.style.marginTop = "15px";
    saveBtn.style.background = "#4caf50";
    saveBtn.style.color = "white";
    saveBtn.style.border = "none";
    saveBtn.style.padding = "10px";
    saveBtn.style.borderRadius = "4px";
    saveBtn.style.fontWeight = "bold";
    saveBtn.style.cursor = "pointer";
    saveBtn.onclick = saveToNVS;
    container.appendChild(saveBtn);

    // Cargar valores reales al iniciar la página
    fetch('/status')
      .then(res => res.json())
      .then(data => {
        params.forEach(p => {
          if (data[p.var] !== undefined) {
            const ctrl = document.getElementById(p.var);
            if (ctrl) {
              ctrl.value = data[p.var];
            }
            if (p.type === "range") {
              const numCtrl = document.getElementById(p.var + "-num");
              if (numCtrl) numCtrl.value = data[p.var];
            }
          }
        });
      })
      .catch(err => console.error("Error cargando status:", err));

    // Cargar stream desde puerto dedicado 81
    document.getElementById('stream-img').src = `${window.location.protocol}//${window.location.hostname}:81/stream`;
  </script>
</body>
</html>
)rawliteral";
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

// Handler para la API /status que retorna la configuración actual en JSON
static esp_err_t status_handler(httpd_req_t *req) {
    char json_response[512];
    char *p = json_response;
    p += sprintf(p, "{");
    p += sprintf(p, "\"framesize\":%d,", menu[0].currentVal);
    p += sprintf(p, "\"brightness\":%d,", menu[1].currentVal);
    p += sprintf(p, "\"contrast\":%d,", menu[2].currentVal);
    p += sprintf(p, "\"saturation\":%d,", menu[3].currentVal);
    p += sprintf(p, "\"special_effect\":%d,", menu[4].currentVal);
    p += sprintf(p, "\"whitebal\":%d,", menu[5].currentVal);
    p += sprintf(p, "\"awb_gain\":%d,", menu[6].currentVal);
    p += sprintf(p, "\"wb_mode\":%d,", menu[7].currentVal);
    p += sprintf(p, "\"exposure_ctrl\":%d,", menu[8].currentVal);
    p += sprintf(p, "\"aec2\":%d,", menu[9].currentVal);
    p += sprintf(p, "\"ae_level\":%d,", menu[10].currentVal);
    p += sprintf(p, "\"aec_value\":%d,", menu[11].currentVal);
    p += sprintf(p, "\"gain_ctrl\":%d,", menu[12].currentVal);
    p += sprintf(p, "\"agc_gain\":%d,", menu[13].currentVal);
    p += sprintf(p, "\"gainceiling\":%d,", menu[14].currentVal);
    p += sprintf(p, "\"bpc\":%d,", menu[15].currentVal);
    p += sprintf(p, "\"wpc\":%d,", menu[16].currentVal);
    p += sprintf(p, "\"raw_gma\":%d,", menu[17].currentVal);
    p += sprintf(p, "\"lenc\":%d,", menu[18].currentVal);
    p += sprintf(p, "\"hmirror\":%d,", menu[19].currentVal);
    p += sprintf(p, "\"vflip\":%d,", menu[20].currentVal);
    p += sprintf(p, "\"dcw\":%d,", menu[21].currentVal);
    p += sprintf(p, "\"colorbar\":%d", menu[22].currentVal);
    p += sprintf(p, "}");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    return ESP_OK;
}

static esp_err_t control_handler(httpd_req_t *req) {
    char buf[128];
    char var[32] = {0};
    char val[32] = {0};
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if (httpd_query_key_value(buf, "var", var, sizeof(var)) == ESP_OK &&
            httpd_query_key_value(buf, "val", val, sizeof(val)) == ESP_OK) {
            int val_int = atoi(val);
            sensor_t * s_cam = esp_camera_sensor_get();
            int menu_idx = -1;
            if (strcmp(var, "framesize") == 0) menu_idx = 0;
            else if (strcmp(var, "brightness") == 0) menu_idx = 1;
            else if (strcmp(var, "contrast") == 0) menu_idx = 2;
            else if (strcmp(var, "saturation") == 0) menu_idx = 3;
            else if (strcmp(var, "special_effect") == 0) menu_idx = 4;
            else if (strcmp(var, "whitebal") == 0) menu_idx = 5;
            else if (strcmp(var, "awb_gain") == 0) menu_idx = 6;
            else if (strcmp(var, "wb_mode") == 0) menu_idx = 7;
            else if (strcmp(var, "exposure_ctrl") == 0) menu_idx = 8;
            else if (strcmp(var, "aec2") == 0) menu_idx = 9;
            else if (strcmp(var, "ae_level") == 0) menu_idx = 10;
            else if (strcmp(var, "aec_value") == 0) menu_idx = 11;
            else if (strcmp(var, "gain_ctrl") == 0) menu_idx = 12;
            else if (strcmp(var, "agc_gain") == 0) menu_idx = 13;
            else if (strcmp(var, "gainceiling") == 0) menu_idx = 14;
            else if (strcmp(var, "bpc") == 0) menu_idx = 15;
            else if (strcmp(var, "wpc") == 0) menu_idx = 16;
            else if (strcmp(var, "raw_gma") == 0) menu_idx = 17;
            else if (strcmp(var, "lenc") == 0) menu_idx = 18;
            else if (strcmp(var, "hmirror") == 0) menu_idx = 19;
            else if (strcmp(var, "vflip") == 0) menu_idx = 20;
            else if (strcmp(var, "dcw") == 0) menu_idx = 21;
            else if (strcmp(var, "colorbar") == 0) menu_idx = 22;

            if (menu_idx != -1) {
                menu[menu_idx].currentVal = val_int;
                if (s_cam) {
                    menu[menu_idx].updateFunc(s_cam, val_int);
                }
            } else if (strcmp(var, "save") == 0) {
                saveCameraSettings();
            }
        }
    }
    httpd_resp_send_chunk(req, "OK", 2);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[ERROR] Failed to get camera frame");
            res = ESP_FAIL;
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 37);
        }
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        if (res != ESP_OK) break;
        delay(1);
    }
    return res;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t control_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = control_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &control_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        Serial.println("[HTTP] Servidor de control iniciado en puerto 80 con endpoint de status");
    }

    // Arrancar el streaming en un puerto secundario (81) para evitar bloqueos
    config.server_port = 81;
    config.ctrl_port = 32769; // Usar puerto de control diferente para evitar conflictos
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.println("[HTTP] Servidor de stream iniciado en puerto 81");
    }
}

void runWebServerMode() {
  Serial.println("[WIFI] Iniciando servidor web de diagnostico...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("MODO SERVIDOR WEB");
  display.print("Conectando a:\n");
  display.println(WIFI_SSID);
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("CONECTADO OK!");
    display.print("SSID: ");
    display.println(WIFI_SSID);
    display.print("IP: ");
    display.println(WiFi.localIP().toString());
    display.println("P: 80, Stream: 81");
    display.display();

    startCameraServer();

    // Bucle infinito bloqueante
    while (true) {
      delay(1000);
    }
  } else {
    Serial.println("\n[WIFI] Fallo de conexion WiFi. Iniciando en modo Access Point...");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    String apSSID = "MONICA_CAM_OLED";
    WiFi.softAP(apSSID.c_str(), "antigravity");
    IPAddress myIP = WiFi.softAPIP();
    Serial.printf("[WIFI] Access Point creado con éxito. SSID: %s\n", apSSID.c_str());
    Serial.printf("[WIFI] Dirección IP del AP: %s\n", myIP.toString().c_str());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("MODO ACCESS POINT");
    display.printf("SSID: %s\n", apSSID.c_str());
    display.println("Pass: antigravity");
    display.printf("IP: %s\n", myIP.toString().c_str());
    display.display();

    startCameraServer();

    while (true) {
      delay(1000);
    }
  }
}

void checkBootButton() {
  if (digitalRead(PIN_BOOT) == LOW) {
    unsigned long pressStart = millis();
    bool stillPressed = true;
    while (millis() - pressStart < 2000) {
      if (digitalRead(PIN_BOOT) == HIGH) {
        stillPressed = false;
        break;
      }
      delay(50);
    }
    if (stillPressed) {
      runWebServerMode();
    }
  }
}

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
  
  // Cargar configuracion persistente de NVS
  loadCameraSettings();

  // Forzar límites de resolución física
  if (!psramFound() && menu[0].currentVal > 8) {
    menu[0].currentVal = 8; 
  }
  s->set_framesize(s, (framesize_t)menu[0].currentVal);

  // Aplicar la configuracion cargada al sensor
  s->set_brightness(s, menu[1].currentVal);
  s->set_contrast(s, menu[2].currentVal);
  s->set_saturation(s, menu[3].currentVal);
  s->set_special_effect(s, menu[4].currentVal);
  s->set_whitebal(s, menu[5].currentVal);
  s->set_awb_gain(s, menu[6].currentVal);
  s->set_wb_mode(s, menu[7].currentVal);
  s->set_exposure_ctrl(s, menu[8].currentVal);
  s->set_aec2(s, menu[9].currentVal);
  s->set_ae_level(s, menu[10].currentVal);
  s->set_aec_value(s, menu[11].currentVal);
  s->set_gain_ctrl(s, menu[12].currentVal);
  s->set_agc_gain(s, menu[13].currentVal);
  s->set_gainceiling(s, (gainceiling_t)menu[14].currentVal);
  s->set_bpc(s, menu[15].currentVal);
  s->set_wpc(s, menu[16].currentVal);
  s->set_raw_gma(s, menu[17].currentVal);
  s->set_lenc(s, menu[18].currentVal);
  s->set_hmirror(s, menu[19].currentVal);
  s->set_vflip(s, menu[20].currentVal);
  s->set_dcw(s, menu[21].currentVal);
  s->set_colorbar(s, menu[22].currentVal);


  // Inicializar pines de interacción física del encoder y BOOT
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  updateMenu();
}


/**
 * @brief Bucle de ejecución. Monitorea el encoder local e interacciones serie.
 */
void loop() {
  checkBootButton();
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
      delay(500);
      updateMenu();
    }
    // Comando: GET_CONFIG
    else if (cmd == "GET_CONFIG") {
      sensor_t * s = esp_camera_sensor_get();
      int res = menu[0].currentVal;
      int br = menu[1].currentVal;
      int co = menu[2].currentVal;
      int qty = s ? s->status.quality : 14;
      int sa = menu[3].currentVal;
      int ef = menu[4].currentVal;
      int wb = menu[5].currentVal;
      int aw = menu[6].currentVal;
      int wm = menu[7].currentVal;
      int ec = menu[8].currentVal;
      int a2 = menu[9].currentVal;
      int al = menu[10].currentVal;
      int av = menu[11].currentVal;
      int gc = menu[12].currentVal;
      int ag = menu[13].currentVal;
      int gl = menu[14].currentVal;
      int bp = menu[15].currentVal;
      int wp = menu[16].currentVal;
      int rg = menu[17].currentVal;
      int lc = menu[18].currentVal;
      int hm = menu[19].currentVal;
      int vf = menu[20].currentVal;
      int dw = menu[21].currentVal;
      int cb = menu[22].currentVal;
      heltecSerial.printf("CONFIG_RESP %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                          res, br, co, qty, sa, ef, wb, aw, wm, ec, a2, al, av, gc, ag, gl, bp, wp, rg, lc, hm, vf, dw, cb);
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
        
        saveCameraSettings();
        
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
      if (!inEditMode) {
        saveCameraSettings();
      }
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
  for (int i = 0; i < 20; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
    delay(50); 
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
