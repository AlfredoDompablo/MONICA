/**
 * @file main.cpp
 * @brief Firmware de Producción - Edición LIGHT (Compacta/Autónoma) para la
 * Cámara ESP32-CAM.
 *
 * Esta versión está específicamente diseñada para nodos remotos de bajo consumo
 * que no requieren una pantalla OLED de diagnóstico local ni un encoder
 * rotativo. Se enfoca exclusivamente en proporcionar la comunicación UART de
 * alta velocidad de captura de imagen OV2640/OV5640 de la manera más liviana y
 * eficiente en términos de ciclos de reloj y ahorro de energía de batería.
 *
 * Protocolo de Comandos Soportado a través de UART:
 *  - "TAKE_PIC"                    ➡️ Dispara la cámara y captura un frame JPG
 * en PSRAM.
 *  - "GET_CHUNK <offset> <len>"    ➡️ Lee y transmite dinámicamente un fragmento
 * binario de la imagen.
 *  - "RELEASE_PIC"                 ➡️ Libera el búfer de captura en PSRAM para
 * conservar energía.
 *  - "SET_RES <val>"               ➡️ Configura dinámicamente la resolución de
 * imagen deseada (0 al 21).
 *
 * Diseñado y documentado profesionalmente para robustez industrial.
 */

#include "esp_camera.h"
#include <Arduino.h>
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
// --- ASIGNACIÓN DE PINES DE LA CÁMARA (Freenove ESP32-S3 Cam Board) ---
// ============================================================================
#define PWDN_GPIO_NUM                                                          \
  -1 ///< Pin de apagado por hardware (No disponible en este modelo)
#define RESET_GPIO_NUM                                                         \
  -1 ///< Pin de reinicio por hardware (No disponible en este modelo)
#define XCLK_GPIO_NUM                                                          \
  15 ///< Entrada de reloj del sistema para el sensor de la cámara
#define SIOD_GPIO_NUM 4  ///< Bus de datos SCCB (Equivalente a SDA de I2C)
#define SIOC_GPIO_NUM 5  ///< Bus de reloj SCCB (Equivalente a SCL de I2C)
#define Y9_GPIO_NUM 16   ///< Bit de datos 9 del bus paralelo de imagen
#define Y8_GPIO_NUM 17   ///< Bit de datos 8 del bus paralelo de imagen
#define Y7_GPIO_NUM 18   ///< Bit de datos 7 del bus paralelo de imagen
#define Y6_GPIO_NUM 12   ///< Bit de datos 6 del bus paralelo de imagen
#define Y5_GPIO_NUM 10   ///< Bit de datos 5 del bus paralelo de imagen
#define Y4_GPIO_NUM 8    ///< Bit de datos 4 del bus paralelo de imagen
#define Y3_GPIO_NUM 9    ///< Bit de datos 3 del bus paralelo de imagen
#define Y2_GPIO_NUM 11   ///< Bit de datos 2 del bus paralelo de imagen
#define VSYNC_GPIO_NUM 6 ///< Pin de sincronización vertical (Inicio de Frame)
#define HREF_GPIO_NUM 7  ///< Pin de referencia horizontal (Línea Activa)
#define PCLK_GPIO_NUM 13 ///< Pin de reloj de píxeles (Pixel Clock)

// ============================================================================
// --- CONEXIÓN DE COMUNICACIÓN SERIE UART CON MASTER MCU (Heltec) ---
// ============================================================================
#define HELTEC_UART_RX 41 ///< Pin RX para recibir comandos desde el Heltec
#define HELTEC_UART_TX 42 ///< Pin TX para enviar respuestas y bytes de imagen
HardwareSerial heltecSerial(
    1); ///< Canal de hardware UART1 para aislar logs del puerto USB

// ============================================================================
// --- VARIABLES DE ESTADO GLOBAL ---
// ============================================================================
sensor_t *s = NULL; ///< Puntero al controlador del sensor físico de la cámara
camera_fb_t *currentFb =
    NULL; ///< Frame buffer que retiene el frame capturado en PSRAM

/**
 * @brief Resolución de captura JPEG inicial por defecto.
 * 21 representa FRAMESIZE_QSXGA (2560x1920) para el sensor OV5640.
 */
int currentFrameSize = 21;
int camBrightness = 0;
int camContrast = 1;
int camQuality = 14;
int camSaturation = 0;
int camSpecialEffect = 0;
int camWhitebal = 1;
int camAwbGain = 1;
int camWbMode = 0;
int camExposureCtrl = 1;
int camAec2 = 0;
int camAeLevel = 0;
int camAecValue = 300;
int camGainCtrl = 1;
int camAgcGain = 0;
int camGainceiling = 0;
int camBpc = 0;
int camWpc = 1;
int camRawGma = 1;
int camLenc = 1;
int camHmirror = 0;
int camVflip = 0;
int camDcw = 1;
int camColorbar = 0;

void saveCameraSettings() {
  prefs.begin("camera_cfg", false);
  prefs.putInt("fs", currentFrameSize);
  prefs.putInt("br", camBrightness);
  prefs.putInt("co", camContrast);
  prefs.putInt("qty", camQuality);
  prefs.putInt("sa", camSaturation);
  prefs.putInt("ef", camSpecialEffect);
  prefs.putInt("wb", camWhitebal);
  prefs.putInt("aw", camAwbGain);
  prefs.putInt("wm", camWbMode);
  prefs.putInt("ec", camExposureCtrl);
  prefs.putInt("a2", camAec2);
  prefs.putInt("al", camAeLevel);
  prefs.putInt("av", camAecValue);
  prefs.putInt("gc", camGainCtrl);
  prefs.putInt("ag", camAgcGain);
  prefs.putInt("gl", camGainceiling);
  prefs.putInt("bp", camBpc);
  prefs.putInt("wp", camWpc);
  prefs.putInt("rg", camRawGma);
  prefs.putInt("lc", camLenc);
  prefs.putInt("hm", camHmirror);
  prefs.putInt("vf", camVflip);
  prefs.putInt("dw", camDcw);
  prefs.putInt("cb", camColorbar);
  prefs.end();
  Serial.println("[NVS] Configuracion de camara guardada (LIGHT).");
}

void loadCameraSettings() {
  prefs.begin("camera_cfg", true);
  currentFrameSize = prefs.getInt("fs", currentFrameSize);
  camBrightness = prefs.getInt("br", camBrightness);
  camContrast = prefs.getInt("co", camContrast);
  camQuality = prefs.getInt("qty", camQuality);
  camSaturation = prefs.getInt("sa", camSaturation);
  camSpecialEffect = prefs.getInt("ef", camSpecialEffect);
  camWhitebal = prefs.getInt("wb", camWhitebal);
  camAwbGain = prefs.getInt("aw", camAwbGain);
  camWbMode = prefs.getInt("wm", camWbMode);
  camExposureCtrl = prefs.getInt("ec", camExposureCtrl);
  camAec2 = prefs.getInt("a2", camAec2);
  camAeLevel = prefs.getInt("al", camAeLevel);
  camAecValue = prefs.getInt("av", camAecValue);
  camGainCtrl = prefs.getInt("gc", camGainCtrl);
  camAgcGain = prefs.getInt("ag", camAgcGain);
  camGainceiling = prefs.getInt("gl", camGainceiling);
  camBpc = prefs.getInt("bp", camBpc);
  camWpc = prefs.getInt("wp", camWpc);
  camRawGma = prefs.getInt("rg", camRawGma);
  camLenc = prefs.getInt("lc", camLenc);
  camHmirror = prefs.getInt("hm", camHmirror);
  camVflip = prefs.getInt("vf", camVflip);
  camDcw = prefs.getInt("dw", camDcw);
  camColorbar = prefs.getInt("cb", camColorbar);
  prefs.end();
  Serial.println("[NVS] Configuracion de camara cargada (LIGHT).");
}

void applyLoadedSettings(sensor_t *sensor) {
  if (!sensor) return;
  sensor->set_framesize(sensor, (framesize_t)currentFrameSize);
  sensor->set_brightness(sensor, camBrightness);
  sensor->set_contrast(sensor, camContrast);
  sensor->set_quality(sensor, camQuality);
  sensor->set_saturation(sensor, camSaturation);
  sensor->set_special_effect(sensor, camSpecialEffect);
  sensor->set_whitebal(sensor, camWhitebal);
  sensor->set_awb_gain(sensor, camAwbGain);
  sensor->set_wb_mode(sensor, camWbMode);
  sensor->set_exposure_ctrl(sensor, camExposureCtrl);
  sensor->set_aec2(sensor, camAec2);
  sensor->set_ae_level(sensor, camAeLevel);
  sensor->set_aec_value(sensor, camAecValue);
  sensor->set_gain_ctrl(sensor, camGainCtrl);
  sensor->set_agc_gain(sensor, camAgcGain);
  sensor->set_gainceiling(sensor, (gainceiling_t)camGainceiling);
  sensor->set_bpc(sensor, camBpc);
  sensor->set_wpc(sensor, camWpc);
  sensor->set_raw_gma(sensor, camRawGma);
  sensor->set_lenc(sensor, camLenc);
  sensor->set_hmirror(sensor, camHmirror);
  sensor->set_vflip(sensor, camVflip);
  sensor->set_dcw(sensor, camDcw);
  sensor->set_colorbar(sensor, camColorbar);
}

const char *getFrameSizeName(int val) {
  switch (val) {
  case 0:  return "96x96";
  case 1:  return "QQVGA";
  case 2:  return "QCIF";
  case 3:  return "HQVGA";
  case 4:  return "240x240";
  case 5:  return "QVGA";
  case 6:  return "CIF";
  case 7:  return "HVGA";
  case 8:  return "VGA";
  case 9:  return "SVGA";
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


// Handler para la página de inicio (diagnóstico)
// Handler para la página de inicio (diagnóstico)
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>MONICA Live Diagnostics (LIGHT)</title>
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
  <h1>MONICA Live Web Diagnostics (LIGHT)</h1>
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
    p += sprintf(p, "\"framesize\":%d,", currentFrameSize);
    p += sprintf(p, "\"brightness\":%d,", camBrightness);
    p += sprintf(p, "\"contrast\":%d,", camContrast);
    p += sprintf(p, "\"saturation\":%d,", camSaturation);
    p += sprintf(p, "\"special_effect\":%d,", camSpecialEffect);
    p += sprintf(p, "\"whitebal\":%d,", camWhitebal);
    p += sprintf(p, "\"awb_gain\":%d,", camAwbGain);
    p += sprintf(p, "\"wb_mode\":%d,", camWbMode);
    p += sprintf(p, "\"exposure_ctrl\":%d,", camExposureCtrl);
    p += sprintf(p, "\"aec2\":%d,", camAec2);
    p += sprintf(p, "\"ae_level\":%d,", camAeLevel);
    p += sprintf(p, "\"aec_value\":%d,", camAecValue);
    p += sprintf(p, "\"gain_ctrl\":%d,", camGainCtrl);
    p += sprintf(p, "\"agc_gain\":%d,", camAgcGain);
    p += sprintf(p, "\"gainceiling\":%d,", camGainceiling);
    p += sprintf(p, "\"bpc\":%d,", camBpc);
    p += sprintf(p, "\"wpc\":%d,", camWpc);
    p += sprintf(p, "\"raw_gma\":%d,", camRawGma);
    p += sprintf(p, "\"lenc\":%d,", camLenc);
    p += sprintf(p, "\"hmirror\":%d,", camHmirror);
    p += sprintf(p, "\"vflip\":%d,", camVflip);
    p += sprintf(p, "\"dcw\":%d,", camDcw);
    p += sprintf(p, "\"colorbar\":%d", camColorbar);
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
            if (s_cam) {
                if (strcmp(var, "framesize") == 0) {
                    if (!psramFound()) val_int = min(val_int, 8);
                    currentFrameSize = val_int;
                    s_cam->set_framesize(s_cam, (framesize_t)val_int);
                }
                else if (strcmp(var, "brightness") == 0) { camBrightness = val_int; s_cam->set_brightness(s_cam, val_int); }
                else if (strcmp(var, "contrast") == 0) { camContrast = val_int; s_cam->set_contrast(s_cam, val_int); }
                else if (strcmp(var, "quality") == 0) { camQuality = val_int; s_cam->set_quality(s_cam, val_int); }
                else if (strcmp(var, "saturation") == 0) { camSaturation = val_int; s_cam->set_saturation(s_cam, val_int); }
                else if (strcmp(var, "special_effect") == 0) { camSpecialEffect = val_int; s_cam->set_special_effect(s_cam, val_int); }
                else if (strcmp(var, "whitebal") == 0) { camWhitebal = val_int; s_cam->set_whitebal(s_cam, val_int); }
                else if (strcmp(var, "awb_gain") == 0) { camAwbGain = val_int; s_cam->set_awb_gain(s_cam, val_int); }
                else if (strcmp(var, "wb_mode") == 0) { camWbMode = val_int; s_cam->set_wb_mode(s_cam, val_int); }
                else if (strcmp(var, "exposure_ctrl") == 0) { camExposureCtrl = val_int; s_cam->set_exposure_ctrl(s_cam, val_int); }
                else if (strcmp(var, "aec2") == 0) { camAec2 = val_int; s_cam->set_aec2(s_cam, val_int); }
                else if (strcmp(var, "ae_level") == 0) { camAeLevel = val_int; s_cam->set_ae_level(s_cam, val_int); }
                else if (strcmp(var, "aec_value") == 0) { camAecValue = val_int; s_cam->set_aec_value(s_cam, val_int); }
                else if (strcmp(var, "gain_ctrl") == 0) { camGainCtrl = val_int; s_cam->set_gain_ctrl(s_cam, val_int); }
                else if (strcmp(var, "agc_gain") == 0) { camAgcGain = val_int; s_cam->set_agc_gain(s_cam, val_int); }
                else if (strcmp(var, "gainceiling") == 0) { camGainceiling = val_int; s_cam->set_gainceiling(s_cam, (gainceiling_t)val_int); }
                else if (strcmp(var, "bpc") == 0) { camBpc = val_int; s_cam->set_bpc(s_cam, val_int); }
                else if (strcmp(var, "wpc") == 0) { camWpc = val_int; s_cam->set_wpc(s_cam, val_int); }
                else if (strcmp(var, "raw_gma") == 0) { camRawGma = val_int; s_cam->set_raw_gma(s_cam, val_int); }
                else if (strcmp(var, "lenc") == 0) { camLenc = val_int; s_cam->set_lenc(s_cam, val_int); }
                else if (strcmp(var, "hmirror") == 0) { camHmirror = val_int; s_cam->set_hmirror(s_cam, val_int); }
                else if (strcmp(var, "vflip") == 0) { camVflip = val_int; s_cam->set_vflip(s_cam, val_int); }
                else if (strcmp(var, "dcw") == 0) { camDcw = val_int; s_cam->set_dcw(s_cam, val_int); }
                else if (strcmp(var, "colorbar") == 0) { camColorbar = val_int; s_cam->set_colorbar(s_cam, val_int); }
                else if (strcmp(var, "save") == 0) { saveCameraSettings(); }
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
  Serial.println("[WIFI] Iniciando servidor web de diagnostico (LIGHT)...");
  Serial.printf("[WIFI] Conectando a: %s\n", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

    startCameraServer();

    // Bucle infinito bloqueante
    while (true) {
      delay(1000);
    }
  } else {
    Serial.println("\n[WIFI] Fallo de conexion WiFi. Iniciando en modo Access Point...");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    String apSSID = "MONICA_CAM_LIGHT";
    WiFi.softAP(apSSID.c_str(), "antigravity");
    IPAddress myIP = WiFi.softAPIP();
    Serial.printf("[WIFI] Access Point creado con éxito. SSID: %s\n", apSSID.c_str());
    Serial.printf("[WIFI] Dirección IP del AP: %s\n", myIP.toString().c_str());

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

void takeAndSendPhoto();


/**
 * @brief Inicialización de hardware para la cámara y puerto serie.
 */
void setup() {
  Serial.begin(115200); ///< Puerto serie USB para depuración local
  Serial.println(
      "\n[SISTEMA] Iniciando firmware de camara (Version LIGHT Standalone)...");
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
  // Ajustado a 10MHz para máxima estabilidad de comunicación serial en sensores
  // OV5640
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Asignar el búfer físico dinámicamente dependiendo de la presencia de
  // memoria PSRAM externa
  if (psramFound()) {
    Serial.println("[INFO] PSRAM detectada. Usando doble búfer en PSRAM.");
    config.frame_size = FRAMESIZE_QSXGA; ///< Permite escalamientos dinámicos
                                         ///< hasta 5 MegaPíxeles
    config.jpeg_quality =
        14; ///< Calidad de imagen JPEG inicial balanceada (24)
    config.fb_count =
        2; ///< Doble búfer necesario para CAMERA_GRAB_LATEST de forma estable
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("[WARNING] No se encontró PSRAM. Usando DRAM interna "
                   "(Limitado a VGA).");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
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

  // Cargar configuracion persistente de NVS
  loadCameraSettings();

  // Forzar la resolución inicial segura (limitar a VGA si no hay PSRAM)
  if (!psramFound() && currentFrameSize > 8) {
    currentFrameSize = 8;
  }
  
  // Aplicar configuracion cargada al sensor
  applyLoadedSettings(s);


  Serial.println(
      "[SISTEMA] Inicialización de cámara completada. Esperando comandos...");

  pinMode(PIN_BOOT, INPUT_PULLUP);
}


/**
 * @brief Bucle principal de ejecución del firmware de la cámara.
 * Escucha comandos UART e invoca el flujo de trabajo correspondiente.
 */
void loop() {
  checkBootButton();
  if (heltecSerial.available()) {

    // Leer el comando delimitado por nueva línea (\n)
    String cmd = heltecSerial.readStringUntil('\n');
    cmd.trim();

    // ------------------------------------------------------------------------
    // COMANDO: GET_CHUNK <offset> <len>
    // Retorna una ráfaga binaria del buffer JPG actual a partir del
    // desplazamiento.
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
          // Búfer defensivo: rellenar con ceros ante errores para no congelar
          // la UART receptora
          uint8_t *zeroBuf = (uint8_t *)calloc(len, 1);
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
    // Libera el frame buffer retenido en memoria para optimizar corriente de
    // fuga.
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
            Serial.printf(
                "[INFO] Resolución cambiada dinámicamente a %s (%d)\n",
                getFrameSizeName(currentFrameSize), currentFrameSize);
          }
        } else {
          Serial.printf("[ERROR] Valor de resolución inválido: %d (Rango "
                        "esperado: 0-21)\n",
                        val);
        }
      }
    }
    // ------------------------------------------------------------------------
    // COMANDO: GET_CONFIG
    // Devuelve todos los parámetros del sensor.
    // ------------------------------------------------------------------------
    else if (cmd == "GET_CONFIG") {
      sensor_t * s = esp_camera_sensor_get();
      int res = currentFrameSize;
      int br = camBrightness;
      int co = camContrast;
      int qty = s ? s->status.quality : camQuality;
      int sa = camSaturation;
      int ef = camSpecialEffect;
      int wb = camWhitebal;
      int aw = camAwbGain;
      int wm = camWbMode;
      int ec = camExposureCtrl;
      int a2 = camAec2;
      int al = camAeLevel;
      int av = camAecValue;
      int gc = camGainCtrl;
      int ag = camAgcGain;
      int gl = camGainceiling;
      int bp = camBpc;
      int wp = camWpc;
      int rg = camRawGma;
      int lc = camLenc;
      int hm = camHmirror;
      int vf = camVflip;
      int dw = camDcw;
      int cb = camColorbar;
      heltecSerial.printf("CONFIG_RESP %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                          res, br, co, qty, sa, ef, wb, aw, wm, ec, a2, al, av, gc, ag, gl, bp, wp, rg, lc, hm, vf, dw, cb);
    }
    // ------------------------------------------------------------------------
    // COMANDO: SET_CONFIG <res> <br> <co> <qty> <sa> <ef> <wb> <aw> <wm> <ec>
    // <a2> <al> <av> <gc> <ag> <gl> <bp> <wp> <rg> <lc> <hm> <vf> <dw> <cb>
    // Configura todos los parámetros del sensor.
    // ------------------------------------------------------------------------
    else if (cmd.startsWith("SET_CONFIG")) {
      int res = 10, br = 0, co = 1, qty = 24;
      int sa = 0, ef = 0, wb = 1, aw = 1, wm = 0, ec = 1, a2 = 0, al = 0,
          av = 300;
      int gc = 1, ag = 0, gl = 0, bp = 0, wp = 1, rg = 1, lc = 1, hm = 0,
          vf = 0, dw = 1, cb = 0;

      int parsed =
          sscanf(cmd.c_str() + 11,
                 "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
                 "%d %d %d %d",
                 &res, &br, &co, &qty, &sa, &ef, &wb, &aw, &wm, &ec, &a2, &al,
                 &av, &gc, &ag, &gl, &bp, &wp, &rg, &lc, &hm, &vf, &dw, &cb);

      if (parsed >= 3) {
        res = constrain(res, 0, 21);
        if (!psramFound()) {
          res = min(res, 8); // Limit to VGA if no PSRAM
        }
        br = constrain(br, -2, 2);
        co = constrain(co, -2, 2);
        if (parsed >= 4)
          qty = constrain(qty, 10, 63);
        if (parsed >= 5)
          sa = constrain(sa, -2, 2);
        if (parsed >= 6)
          ef = constrain(ef, 0, 6);
        if (parsed >= 7)
          wb = constrain(wb, 0, 1);
        if (parsed >= 8)
          aw = constrain(aw, 0, 1);
        if (parsed >= 9)
          wm = constrain(wm, 0, 4);
        if (parsed >= 10)
          ec = constrain(ec, 0, 1);
        if (parsed >= 11)
          a2 = constrain(a2, 0, 1);
        if (parsed >= 12)
          al = constrain(al, -2, 2);
        if (parsed >= 13)
          av = constrain(av, 0, 1200);
        if (parsed >= 14)
          gc = constrain(gc, 0, 1);
        if (parsed >= 15)
          ag = constrain(ag, 0, 30);
        if (parsed >= 16)
          gl = constrain(gl, 0, 6);
        if (parsed >= 17)
          bp = constrain(bp, 0, 1);
        if (parsed >= 18)
          wp = constrain(wp, 0, 1);
        if (parsed >= 19)
          rg = constrain(rg, 0, 1);
        if (parsed >= 20)
          lc = constrain(lc, 0, 1);
        if (parsed >= 21)
          hm = constrain(hm, 0, 1);
        if (parsed >= 22)
          vf = constrain(vf, 0, 1);
        if (parsed >= 23)
          dw = constrain(dw, 0, 1);
        if (parsed >= 24)
          cb = constrain(cb, 0, 1);

        currentFrameSize = res;
        camBrightness = br;
        camContrast = co;
        if (parsed >= 4) camQuality = qty;
        if (parsed >= 5) camSaturation = sa;
        if (parsed >= 6) camSpecialEffect = ef;
        if (parsed >= 7) camWhitebal = wb;
        if (parsed >= 8) camAwbGain = aw;
        if (parsed >= 9) camWbMode = wm;
        if (parsed >= 10) camExposureCtrl = ec;
        if (parsed >= 11) camAec2 = a2;
        if (parsed >= 12) camAeLevel = al;
        if (parsed >= 13) camAecValue = av;
        if (parsed >= 14) camGainCtrl = gc;
        if (parsed >= 15) camAgcGain = ag;
        if (parsed >= 16) camGainceiling = gl;
        if (parsed >= 17) camBpc = bp;
        if (parsed >= 18) camWpc = wp;
        if (parsed >= 19) camRawGma = rg;
        if (parsed >= 20) camLenc = lc;
        if (parsed >= 21) camHmirror = hm;
        if (parsed >= 22) camVflip = vf;
        if (parsed >= 23) camDcw = dw;
        if (parsed >= 24) camColorbar = cb;

        if (s) {
          applyLoadedSettings(s);
        }
        saveCameraSettings();


        Serial.printf("[CAMERA] Configurada remotamente (LIGHT): Res=%d, "
                      "Br=%d, Co=%d, Qty=%d, Sa=%d, Ef=%d\n",
                      res, br, co, qty, sa, ef);
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
 *  2. Realiza 4 capturas dummy consecutivas para permitir que los lazos de
 * control de ganancia automática (AGC) y exposición (AEC) del sensor OV5640 se
 * estabilicen.
 *  3. Ejecuta la lectura final en PSRAM.
 *  4. Valida el marcador binario de fin de JPEG (End of Image - EOI: `0xFF
 * 0xD9`).
 *  5. Envía la respuesta formateada `SIZE:<bytes>\n` a través del puerto serie.
 */
void takeAndSendPhoto() {
  Serial.printf(
      "[INFO] Petición TAKE_PIC recibida. Capturando en resolución %s...\n",
      getFrameSizeName(currentFrameSize));

  // Limpiar referencias anteriores
  if (currentFb) {
    esp_camera_fb_return(currentFb);
    currentFb = NULL;
  }

  // Capturas de descarte para estabilización automática de brillo y ganancia
  // del lente CMOS. Reducido a 2 descartes rápidos en PSRAM o 5 en DRAM para evitar demoras y fallos.
  int discardCount = psramFound() ? 2 : 5;
  for (int i = 0; i < discardCount; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
    delay(10);
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
    Serial.printf(
        "[DEBUG] Últimos dos bytes en PSRAM: %02X %02X (Esperado: FF D9)\n", b1,
        b2);

    bool foundEOI = false;
    for (size_t i = 0; i < currentFb->len - 1; i++) {
      if (currentFb->buf[i] == 0xFF && currentFb->buf[i + 1] == 0xD9) {
        Serial.printf(
            "[DEBUG] Marcador de integridad FF D9 encontrado en índice %u\n",
            i);
        foundEOI = true;
        break;
      }
    }
    if (!foundEOI) {
      Serial.println("[WARNING] Marcador de cierre JPEG FF D9 no detectado. "
                     "Posible corrupción de frame.");
    }
  }

  // Reportar tamaño total al MCU maestro
  uint32_t imgSize = currentFb->len;
  heltecSerial.printf("SIZE:%u\n", imgSize);

  Serial.printf("[INFO] Foto capturada exitosamente. Tamaño: %u bytes. "
                "Esperando descarga...\n",
                imgSize);
}
