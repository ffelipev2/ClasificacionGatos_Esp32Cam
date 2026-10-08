#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "classifier.h"
#include "web_ui.h"

using cats::Label;
constexpr int FRAME_W = 320, FRAME_H = 240;
constexpr uint32_t STATE_MAGIC = 0x43415432;
constexpr uint32_t STATE_VERSION = 1;
struct SavedState {
  uint32_t magic = STATE_MAGIC, version = STATE_VERSION;
  cats::Options options;
  cats::Model model;
  uint32_t checksum = 0;
};
struct Learning {
  bool active = false;
  bool background = false;
  Label label = Label::Unknown;
  unsigned count = 0;
  cats::Prototype candidate;
  uint16_t sums[cats::PIXELS][3] = {};
};

SavedState state;
Learning learning;
WebServer server(80);
Preferences preferences;
cats::Stabilizer stabilizer(STABLE_FRAMES);
cats::Rgb samples[cats::PIXELS];
cats::Analysis analysis;
Label stableLabel = Label::Calibration;
uint8_t *decoded = nullptr, *jpeg = nullptr;
size_t jpegLength = 0;
uint32_t frameNumber = 0;
unsigned long lastCapture = 0, lastGoodFrame = 0;
bool cameraReady = false, storageReady = false;
String message = "Registra el fondo vacío y después ambos gatos.";

uint32_t checksum(const SavedState &s) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&s);
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < offsetof(SavedState, checksum); ++i)
    hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}
bool saveState() {
  if (!storageReady) return false;
  state.checksum = checksum(state);
  return preferences.putBytes("state", &state, sizeof(state)) == sizeof(state);
}
void loadState() {
  storageReady = preferences.begin("cat-colors", false);
  if (!storageReady) { message = "No se pudo abrir la memoria. La calibración no persistirá."; return; }
  if (preferences.getBytesLength("state") != sizeof(state)) return;
  preferences.getBytes("state", &state, sizeof(state));
  if (state.magic != STATE_MAGIC || state.version != STATE_VERSION ||
      checksum(state) != state.checksum || !cats::validOptions(state.options) ||
      !cats::validModel(state.model)) {
    state = SavedState{};
    message = "Calibración inválida; registra el fondo y ambos gatos.";
  } else message = "Calibración recuperada de la memoria.";
}
void clearModel() {
  memset(&state.model, 0, sizeof(state.model));
  stabilizer.reset();
  stableLabel = Label::Calibration;
}

bool beginCamera() {
  if (!psramFound()) { message = "PSRAM no disponible. Usa AI Thinker ESP32-CAM con PSRAM habilitada."; return false; }
  decoded = static_cast<uint8_t *>(heap_caps_malloc(FRAME_W * FRAME_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  jpeg = static_cast<uint8_t *>(heap_caps_malloc(MAX_JPEG_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!decoded || !jpeg) { message = "No hay memoria para las imágenes."; return false; }

  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  // Pines del modelo AI Thinker con OV2640.
  c.pin_d0 = 5; c.pin_d1 = 18; c.pin_d2 = 19; c.pin_d3 = 21;
  c.pin_d4 = 36; c.pin_d5 = 39; c.pin_d6 = 34; c.pin_d7 = 35;
  c.pin_xclk = 0; c.pin_pclk = 22; c.pin_vsync = 25; c.pin_href = 23;
  c.pin_sccb_sda = 26; c.pin_sccb_scl = 27;
  c.pin_pwdn = 32; c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  const esp_err_t error = esp_camera_init(&c);
  if (error != ESP_OK) { message = "No se pudo iniciar la cámara: 0x" + String(error, HEX); return false; }
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_saturation(s, 0);
    s->set_brightness(s, 0);
  }
  return true;
}

void beginNetwork() {
  if (strlen(WIFI_SSID)) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setAutoReconnect(true);
      WiFi.setSleep(false);
      Serial.print("Panel: http://"); Serial.println(WiFi.localIP());
      return;
    }
    Serial.println("Wi-Fi no disponible; creando red local.");
  }
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    message = "No se pudo crear la red Wi-Fi. Revisa AP_PASSWORD (mínimo 8 caracteres).";
    return;
  }
  Serial.print("Red: "); Serial.println(AP_SSID);
  Serial.print("Panel: http://"); Serial.println(WiFi.softAPIP());
}

bool capture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;
  const bool fits = fb->width == FRAME_W && fb->height == FRAME_H &&
                    fb->format == PIXFORMAT_JPEG && fb->len <= MAX_JPEG_BYTES;
  const bool ok = fits && jpg2rgb565(fb->buf, fb->len, decoded, JPG_SCALE_NONE);
  if (ok) {
    memcpy(jpeg, fb->buf, fb->len);
    jpegLength = fb->len;
  }
  esp_camera_fb_return(fb);  // También devolverlo cuando falla la conversión.
  if (!ok) return false;

  const cats::Options &o = state.options;
  for (size_t y = 0; y < cats::GRID_H; ++y)
    for (size_t x = 0; x < cats::GRID_W; ++x) {
      // Muestrear el centro de cada celda dentro de la zona elegida.
      const unsigned px = FRAME_W * (o.x + (x + 0.5f) * o.width / cats::GRID_W) / 100.0f;
      const unsigned py = FRAME_H * (o.y + (y + 0.5f) * o.height / cats::GRID_H) / 100.0f;
      // jpg2rgb565 entrega RGB565 little endian tanto en Arduino 2 como en 3.
      samples[y * cats::GRID_W + x] = cats::fromRgb565LE(decoded + (py * FRAME_W + px) * 2);
    }
  ++frameNumber;
  lastGoodFrame = millis();
  analysis = cats::analyze(samples, state.model, state.options);
  return true;
}

void cancelLearning(const String &reason) {
  learning.active = false;
  message = reason;
  stabilizer.reset();
  stableLabel = Label::Unknown;
}
void advanceLearning() {
  if (!learning.active) return;
  if (analysis.brightness < 0.10f) { cancelLearning("Registro cancelado: hay muy poca luz."); return; }
  if (learning.background) {
    for (size_t i = 0; i < cats::PIXELS; ++i) {
      learning.sums[i][0] += samples[i].r;
      learning.sums[i][1] += samples[i].g;
      learning.sums[i][2] += samples[i].b;
    }
  } else {
    if (analysis.foreground < state.options.minForeground / 100.0f ||
        !cats::suitable(analysis, learning.label)) {
      cancelLearning("Registro cancelado: ocupa la zona con el pelaje elegido y evita manos, sombras o reflejos.");
      return;
    }
    if (!cats::addSample(learning.candidate, analysis.features)) {
      cancelLearning("Límite de muestras alcanzado. Borra la calibración para comenzar otra vez.");
      return;
    }
  }
  ++learning.count;
  if (learning.count < CALIBRATION_FRAMES) return;

  if (learning.background) {
    clearModel();
    for (size_t i = 0; i < cats::PIXELS; ++i)
      state.model.background[i] = cats::Rgb{
        uint8_t(learning.sums[i][0] / learning.count),
        uint8_t(learning.sums[i][1] / learning.count),
        uint8_t(learning.sums[i][2] / learning.count)};
    state.model.backgroundReady = true;
  } else {
    const cats::Prototype &orange = learning.label == Label::Orange ? learning.candidate : state.model.orange;
    const cats::Prototype &gray = learning.label == Label::Gray ? learning.candidate : state.model.gray;
    if (!cats::separable(orange, gray)) {
      cancelLearning("Registro rechazado: ambos colores se parecen demasiado. Mejora la luz y repite las muestras.");
      return;
    }
    if (learning.label == Label::Orange) state.model.orange = learning.candidate;
    else state.model.gray = learning.candidate;
  }
  learning.active = false;
  stabilizer.reset();
  stableLabel = Label::Calibration;
  message = saveState() ? "Registro guardado. Puedes añadir más posiciones de cada gato." :
                         "Registro activo, pero no se pudo guardar en memoria; se perderá al reiniciar.";
}

String jsonEscape(const String &value) {
  String out;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (static_cast<uint8_t>(c) >= 32) out += c;
  }
  return out;
}
void respond(int code, const String &text) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", "{\"message\":\"" + jsonEscape(text) + "\"}");
}
void status() {
  const cats::Options &o = state.options;
  String out;
  out.reserve(800);
  out = "{\"label\":\""; out += cats::name(stableLabel);
  out += "\",\"frame\":"; out += frameNumber;
  out += ",\"ageMs\":"; out += frameNumber ? millis() - lastGoodFrame : 0;
  out += ",\"foreground\":"; out += String(analysis.foreground, 3);
  out += ",\"background\":"; out += state.model.backgroundReady ? "true" : "false";
  out += ",\"orangeSamples\":"; out += state.model.orange.count;
  out += ",\"graySamples\":"; out += state.model.gray.count;
  out += ",\"busy\":"; out += learning.active ? "true" : "false";
  out += ",\"job\":\""; out += learning.background ? "fondo" : cats::name(learning.label);
  out += "\",\"progress\":"; out += learning.count;
  out += ",\"total\":"; out += CALIBRATION_FRAMES;
  out += ",\"message\":\""; out += jsonEscape(message);
  out += "\",\"options\":{\"x\":"; out += o.x;
  out += ",\"y\":"; out += o.y;
  out += ",\"width\":"; out += o.width;
  out += ",\"height\":"; out += o.height;
  out += ",\"difference\":"; out += o.difference;
  out += ",\"minForeground\":"; out += o.minForeground;
  out += "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}
void startLearning() {
  if (!cameraReady || !frameNumber || millis() - lastGoodFrame > 2500) {
    respond(503, "La cámara aún no tiene una imagen válida."); return;
  }
  if (learning.active) { respond(409, "Ya hay un registro en curso."); return; }
  const String label = server.arg("label");
  if (label != "fondo" && label != "naranja" && label != "gris") {
    respond(400, "Etiqueta inválida."); return;
  }
  if (label != "fondo" && !state.model.backgroundReady) {
    respond(409, "Registra primero el fondo vacío."); return;
  }
  // Evitar un objeto temporal de 7 KB en la pila de la tarea Arduino.
  memset(&learning, 0, sizeof(learning));
  learning.active = true;
  learning.background = label == "fondo";
  learning.label = label == "naranja" ? Label::Orange : Label::Gray;
  if (!learning.background)
    learning.candidate = learning.label == Label::Orange ? state.model.orange : state.model.gray;
  stabilizer.reset();
  stableLabel = Label::Calibration;
  message = "Registro iniciado; mantén la escena durante cuatro segundos.";
  respond(202, message);
}
bool numericArg(const char *key, uint8_t &target) {
  if (!server.hasArg(key)) return false;
  const String text = server.arg(key);
  if (!text.length()) return false;
  for (size_t i = 0; i < text.length(); ++i)
    if (text[i] < '0' || text[i] > '9') return false;
  if (text.length() > 3) return false;
  const long value = strtol(text.c_str(), nullptr, 10);
  if (value > 100) return false;
  target = uint8_t(value);
  return true;
}
void settings() {
  if (learning.active) { respond(409, "Espera a que termine el registro."); return; }
  cats::Options options;
  if (!numericArg("x", options.x) || !numericArg("y", options.y) ||
      !numericArg("width", options.width) || !numericArg("height", options.height) ||
      !numericArg("difference", options.difference) ||
      !numericArg("minForeground", options.minForeground) || !cats::validOptions(options)) {
    respond(400, "Valores inválidos; la zona debe quedar dentro de la imagen."); return;
  }
  state.options = options;
  clearModel();
  message = saveState() ? "Ajustes guardados. Registra el fondo y ambos gatos otra vez." :
                         "Ajustes activos, pero no se pudieron guardar en memoria.";
  respond(200, message);
}
void beginServer() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/capture", HTTP_GET, [] {
    if (!jpegLength || stableLabel == Label::Error) { respond(503, "No hay imagen válida."); return; }
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Number", String(frameNumber));
    server.send_P(200, "image/jpeg", reinterpret_cast<const char *>(jpeg), jpegLength);
  });
  server.on("/api/status", HTTP_GET, status);
  server.on("/api/learn", HTTP_POST, startLearning);
  server.on("/api/settings", HTTP_POST, settings);
  server.on("/api/reset", HTTP_POST, [] {
    if (learning.active) { respond(409, "Espera a que termine el registro."); return; }
    clearModel();
    message = saveState() ? "Calibración borrada. Registra el fondo y ambos gatos." :
                           "Calibración borrada en RAM; no se pudo actualizar la memoria.";
    respond(200, message);
  });
  server.onNotFound([] { respond(404, "Ruta no encontrada."); });
  server.begin();
}
void setup() {
  Serial.begin(115200);
  // Mantener apagado el flash blanco, que altera el color del pelaje.
  pinMode(4, OUTPUT); digitalWrite(4, LOW);
  loadState();
  cameraReady = beginCamera();
  if (!cameraReady) stableLabel = Label::Error;
  beginNetwork();
  beginServer();
  Serial.println(message);
}
void loop() {
  server.handleClient();
  if (cameraReady && millis() - lastCapture >= CAPTURE_INTERVAL_MS) {
    lastCapture = millis();
    if (capture()) {
      if (learning.active) advanceLearning();
      else stableLabel = stabilizer.update(analysis.label);
      Serial.printf("%lu,%s,area=%.3f,naranja=%.3f,gris=%.3f\n",
                    static_cast<unsigned long>(frameNumber), cats::name(stableLabel),
                    analysis.foreground, analysis.orangeDistance, analysis.grayDistance);
    } else {
      if (learning.active) cancelLearning("Registro cancelado por un error de cámara.");
      stabilizer.reset();
      stableLabel = Label::Error;
      jpegLength = 0;
      message = "Falló una captura o su conversión. Revisa la cámara y la alimentación.";
    }
  }
  delay(2);
}

