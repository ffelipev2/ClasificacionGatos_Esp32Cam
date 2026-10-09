#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <esp_timer.h>
// Ambos servidores definen HTTP_ANY; usamos métodos GET/POST explícitos.
#undef HTTP_ANY
#include <esp_http_server.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "classifier.h"
#include "web_ui.h"
#include "preview_pool.h"
#include "jpeg_frame.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Este firmware requiere la XIAO ESP32S3 Sense."
#endif

using cats::Label;
constexpr int FRAME_W = 320, FRAME_H = 240;
constexpr int ANALYSIS_W = FRAME_W / 2, ANALYSIS_H = FRAME_H / 2;
constexpr uint32_t STATE_MAGIC = 0x43415432;
constexpr uint32_t STATE_VERSION = 2;
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
  cats::SamplingProgress progress;
  unsigned long started = 0, lastSample = 0;
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
uint8_t *decoded = nullptr;
uint8_t *previewBytes[cats::PreviewPool::SLOTS] = {};
cats::PreviewPool previewPool;
SemaphoreHandle_t sharedMutex = nullptr;
QueueHandle_t commandQueue = nullptr;
uint32_t frameNumber = 0;
unsigned long lastGoodFrame = 0;
float actualFps = 0, captureMs = 0, decodeMs = 0, analysisMs = 0, workMs = 0;
uint32_t captureErrors = 0;
uint32_t lastAnalyzedPreview = 0;
bool cameraReady = false, storageReady = false;
String message = "Registra el fondo vacío y después ambos gatos.";
// Solo se modifica bajo sharedMutex; los lectores nunca retienen ese mutex
// mientras decodifican JPEG o escriben en una conexión de red.
struct VideoState {
  uint32_t frame = 0, goodAt = 0, errors = 0, drops = 0;
  uint32_t epoch = 0, sentFrame = 0, sentAt = 0, starts = 0, timeouts = 0, merged = 0;
  float fps = 0, captureMs = 0, streamFps = 0;
  unsigned clients = 0;
} video;
uint32_t previewAt[cats::PreviewPool::SLOTS] = {};
httpd_handle_t streamServer = nullptr;

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
  storageReady = preferences.begin("cat-colors-s3", false);
  if (!storageReady) { message = "No se pudo abrir la memoria. La calibración no persistirá."; return; }
  if (!preferences.isKey("state") || preferences.getBytesLength("state") != sizeof(state)) return;
  preferences.getBytes("state", &state, sizeof(state));
  if (state.magic != STATE_MAGIC || (state.version != 1 && state.version != STATE_VERSION) ||
      checksum(state) != state.checksum || !cats::validOptions(state.options) ||
      !cats::validModel(state.model)) {
    state = SavedState{};
    message = "Calibración inválida; registra el fondo y ambos gatos.";
  } else if (state.version == 1) {
    // El histograma nuevo necesita nuevas muestras de los gatos.
    // Conservar el fondo y la zona al actualizar solo la aplicación.
    memset(&state.model.orange, 0, sizeof(state.model.orange));
    memset(&state.model.gray, 0, sizeof(state.model.gray));
    state.version = STATE_VERSION;
    message = saveState() ? "Versión actualizada. Fondo conservado; registra ambos gatos otra vez." :
                           "Fondo conservado en RAM; registra ambos gatos para guardar la actualización.";
  } else message = "Calibración recuperada de la memoria.";
}
void clearModel() {
  memset(&state.model, 0, sizeof(state.model));
  stabilizer.reset();
  stableLabel = Label::Calibration;
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
    Serial.println("No se pudo crear la red Wi-Fi. Revisa AP_PASSWORD (mínimo 8 caracteres).");
    return;
  }
  Serial.print("Red: "); Serial.println(AP_SSID);
  Serial.print("Panel: http://"); Serial.println(WiFi.softAPIP());
}


void cancelLearning(const String &reason) {
  learning.active = false;
  message = reason;
  stabilizer.reset();
  stableLabel = Label::Unknown;
}
void advanceLearning() {
  if (!learning.active) return;
  if (millis() - learning.lastSample < LEARNING_INTERVAL_MS) return;
  learning.lastSample = millis();
  const cats::TrainingIssue issue = learning.background ?
    (analysis.brightness < 0.10f ? cats::TrainingIssue::LowLight : cats::TrainingIssue::Ready) :
    cats::trainingIssue(analysis, state.options);
  const cats::SampleState step = learning.progress.update(issue == cats::TrainingIssue::Ready);
  if (step == cats::SampleState::TimedOut) {
    cancelLearning("No se completó el registro: " + String(learning.progress.accepted()) +
                   "/" + String(CALIBRATION_FRAMES) + " imágenes válidas. " + message);
    return;
  }
  if (step == cats::SampleState::Waiting) {
    if (issue == cats::TrainingIssue::LowLight)
      message = "Esperando más luz; las capturas válidas se conservan.";
    else if (issue == cats::TrainingIssue::NoForeground)
      message = "Acerca el pelaje a la zona verde. Área detectada: " + String(analysis.foreground * 100, 0) + "% (mínimo " + String(state.options.minForeground) + "%).";
    else message = "Evita sombras profundas y reflejos; las capturas válidas se conservan.";
    return;
  }
  if (learning.background) {
    for (size_t i = 0; i < cats::PIXELS; ++i) {
      learning.sums[i][0] += samples[i].r;
      learning.sums[i][1] += samples[i].g;
      learning.sums[i][2] += samples[i].b;
    }
  } else {
    if (!cats::addSample(learning.candidate, analysis.features)) {
      cancelLearning("Límite de muestras alcanzado. Borra la calibración para comenzar otra vez.");
      return;
    }
  }
  message = "Captura válida guardada. Continúa mostrando el pelaje elegido.";
  if (step != cats::SampleState::Complete) return;

  if (learning.background) {
    clearModel();
    for (size_t i = 0; i < cats::PIXELS; ++i)
      state.model.background[i] = cats::Rgb{
        uint8_t(learning.sums[i][0] / learning.progress.accepted()),
        uint8_t(learning.sums[i][1] / learning.progress.accepted()),
        uint8_t(learning.sums[i][2] / learning.progress.accepted())};
    state.model.backgroundReady = true;
  } else {
    if (learning.label == Label::Orange) state.model.orange = learning.candidate;
    else state.model.gray = learning.candidate;
  }
  learning.active = false;
  stabilizer.reset();
  stableLabel = Label::Calibration;
  message = saveState() ? "Registro guardado. Puedes añadir más posiciones de cada gato." :
                         "Registro activo, pero no se pudo guardar en memoria; se perderá al reiniciar.";
  if (!cats::separable(state.model.orange, state.model.gray))
    message += " Ambos perfiles se parecen demasiado: repite las muestras con mejor luz y una zona centrada en el pelaje.";
}

// Cámara, modelos y NVS pertenecen exclusivamente a cameraTask.
// El servidor intercambia comandos y copias pequeñas; nunca toca esos datos.
enum class CommandKind : uint8_t { Learn, Settings, Reset };
struct Command {
  CommandKind kind = CommandKind::Reset;
  cats::Options options;
  Label label = Label::Unknown;
  bool background = false;
};
struct Snapshot {
  cats::Options options;
  Label label = Label::Calibration, job = Label::Unknown;
  uint32_t frame = 0, goodAt = 0, errors = 0;
  unsigned orange = 0, gray = 0, progress = 0, attempts = 0;
  bool background = false, busy = false, jobBackground = false, ready = false;
  float foreground = 0, useful = 0, fps = 0;
  float captureMs = 0, decodeMs = 0, analysisMs = 0, workMs = 0;
  uint32_t videoFrame = 0, videoAt = 0, previewDrops = 0, videoErrors = 0;
  uint32_t streamFrame = 0, streamAt = 0, streamStarts = 0, streamTimeouts = 0, mergedJpegs = 0;
  float videoFps = 0, streamFps = 0;
  unsigned streamClients = 0;
  size_t freePsram = 0, freeHeap = 0;
  char message[512] = {};
};
Snapshot shared;
bool commandPending = false;  // Protegido por sharedMutex.

void publishSnapshot(bool commandDone = false) {
  Snapshot next;
  next.options = state.options;
  next.label = stableLabel;
  next.job = learning.label;
  next.frame = frameNumber;
  next.goodAt = lastGoodFrame;
  next.errors = captureErrors;
  next.orange = state.model.orange.count;
  next.gray = state.model.gray.count;
  next.progress = learning.progress.accepted();
  next.attempts = learning.progress.attempts();
  next.background = state.model.backgroundReady;
  next.busy = learning.active;
  next.jobBackground = learning.background;
  next.ready = cameraReady;
  next.foreground = analysis.foreground;
  next.useful = float(analysis.useful) / cats::PIXELS;
  next.fps = actualFps;
  next.captureMs = captureMs;
  next.decodeMs = decodeMs;
  next.analysisMs = analysisMs;
  next.workMs = workMs;
  next.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  next.freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  snprintf(next.message, sizeof(next.message), "%s", message.c_str());
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  if (commandDone) commandPending = false;
  shared = next;
  xSemaphoreGive(sharedMutex);
}
Snapshot readSnapshot() {
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  Snapshot copy = shared;
  copy.busy = copy.busy || commandPending;
  copy.videoFrame = video.frame;
  copy.videoAt = video.goodAt;
  copy.videoFps = video.fps;
  copy.previewDrops = video.drops;
  copy.videoErrors = video.errors;
  copy.streamFps = video.streamFps;
  copy.streamClients = video.clients;
  copy.streamFrame = video.sentFrame;
  copy.streamAt = video.sentAt;
  copy.streamStarts = video.starts;
  copy.streamTimeouts = video.timeouts;
  copy.mergedJpegs = video.merged;
  copy.errors += video.errors;
  xSemaphoreGive(sharedMutex);
  return copy;
}

bool beginCamera() {
  if (!psramFound() || ESP.getPsramSize() < 4 * 1024 * 1024) {
    message = "Habilita OPI PSRAM para la XIAO ESP32S3 Sense.";
    return false;
  }
  // La imagen de análisis tiene cuatro veces menos píxeles que el JPEG.
  // Intentar RAM interna para acelerar el acceso; reservar las vistas en PSRAM.
  decoded = static_cast<uint8_t *>(heap_caps_malloc(
    ANALYSIS_W * ANALYSIS_H * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!decoded) decoded = static_cast<uint8_t *>(heap_caps_malloc(
    ANALYSIS_W * ANALYSIS_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  for (unsigned i = 0; i < cats::PreviewPool::SLOTS; ++i)
    previewBytes[i] = static_cast<uint8_t *>(heap_caps_malloc(
      MAX_JPEG_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  bool memoryOk = decoded != nullptr;
  for (auto buffer : previewBytes) memoryOk = memoryOk && buffer;
  if (!memoryOk) {
    heap_caps_free(decoded); decoded = nullptr;
    for (auto &buffer : previewBytes) { heap_caps_free(buffer); buffer = nullptr; }
    message = "No hay memoria suficiente para las imágenes.";
    return false;
  }
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  // Pines oficiales de la expansión Sense; sirven para OV2640 y OV3660.
  c.pin_d0 = 15; c.pin_d1 = 17; c.pin_d2 = 18; c.pin_d3 = 16;
  c.pin_d4 = 14; c.pin_d5 = 12; c.pin_d6 = 11; c.pin_d7 = 48;
  c.pin_xclk = 10; c.pin_pclk = 13; c.pin_vsync = 38; c.pin_href = 47;
  c.pin_sccb_sda = 40; c.pin_sccb_scl = 39;
  c.pin_pwdn = -1; c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  // El driver reserva JPEG como ancho*alto/5: QVGA solo deja 15 KB.
  // Reservar 96 KB por framebuffer en PSRAM y luego capturar en QVGA,
  // como el ejemplo oficial que inicializa grande y reduce el sensor.
  c.frame_size = FRAMESIZE_SVGA;
  c.jpeg_quality = 12;
  c.fb_count = 2;  // Cola corta para reducir el retraso del vídeo.
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  const esp_err_t error = esp_camera_init(&c);
  if (error != ESP_OK) {
    message = "No se pudo iniciar la cámara: 0x" + String(error, HEX);
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (!sensor || sensor->set_framesize(sensor, FRAMESIZE_QVGA) != 0) {
    esp_camera_deinit();
    message = "No se pudo configurar la cámara en 320 × 240.";
    return false;
  }
  if (sensor) {
    sensor->set_whitebal(sensor, 1);
    sensor->set_awb_gain(sensor, 1);
    sensor->set_wb_mode(sensor, 0);
    sensor->set_saturation(sensor, 0);
    sensor->set_brightness(sensor, 0);
    Serial.printf("Sensor de cámara: 0x%04x\n", sensor->id.PID);
  }
  return true;
}

void captureProducer(void *) {
  uint32_t rateSince = millis(), rateFrames = 0;
  for (;;) {
    const int64_t started = esp_timer_get_time();
    camera_fb_t *fb = esp_camera_fb_get();
    const float elapsed = (esp_timer_get_time() - started) / 1000.0f;
    cats::JpegFrame jpeg;
    if (fb && fb->format == PIXFORMAT_JPEG && fb->len <= MAX_JPEG_BYTES)
      jpeg = cats::latestJpeg(fb->buf, fb->len, FRAME_W, FRAME_H);
    const bool ok = fb && fb->width == FRAME_W && fb->height == FRAME_H &&
                    fb->format == PIXFORMAT_JPEG && jpeg.bytes;
    if (!ok) {
      if (fb) esp_camera_fb_return(fb);
      xSemaphoreTake(sharedMutex, portMAX_DELAY);
      ++video.errors;
      video.fps = 0;
      previewPool.invalidate();
      xSemaphoreGive(sharedMutex);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    const int slot = previewPool.beginWrite();
    xSemaphoreGive(sharedMutex);
    if (slot >= 0) {
      const size_t bytes = jpeg.bytes;
      memcpy(previewBytes[slot], fb->buf + jpeg.offset, bytes);
      esp_camera_fb_return(fb);
      const uint32_t now = millis();
      xSemaphoreTake(sharedMutex, portMAX_DELAY);
      video.goodAt = now;
      video.captureMs = elapsed;
      if (jpeg.images > 1) ++video.merged;
      previewAt[slot] = now;
      previewPool.publish(slot, bytes, ++video.frame);
      ++rateFrames;
      if (now - rateSince >= 1000) {
        video.fps = rateFrames * 1000.0f / (now - rateSince);
        rateFrames = 0;
        rateSince = now;
      }
      xSemaphoreGive(sharedMutex);
    } else {
      esp_camera_fb_return(fb);
      xSemaphoreTake(sharedMutex, portMAX_DELAY);
      ++video.drops;
      xSemaphoreGive(sharedMutex);
    }
    // La cámara marca el ritmo; el analizador nunca retiene un framebuffer.
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

enum class FrameResult { Waiting, Good, Failed };
FrameResult capture() {
  const int64_t started = esp_timer_get_time();
  size_t bytes;
  uint32_t frame;
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  const int slot = previewPool.acquire(bytes, frame);
  const uint32_t capturedAt = slot >= 0 ? previewAt[slot] : 0;
  captureMs = video.captureMs;
  xSemaphoreGive(sharedMutex);
  if (slot < 0) return FrameResult::Waiting;
  if (frame == lastAnalyzedPreview || millis() - capturedAt > 2500) {
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    previewPool.release(slot);
    xSemaphoreGive(sharedMutex);
    return FrameResult::Waiting;
  }
  lastAnalyzedPreview = frame;
  const bool ok = jpg2rgb565(previewBytes[slot], bytes, decoded, JPG_SCALE_2X);
  decodeMs = (esp_timer_get_time() - started) / 1000.0f;
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  previewPool.release(slot);
  xSemaphoreGive(sharedMutex);
  if (!ok) return FrameResult::Failed;
  ++frameNumber;
  const uint32_t now = millis();
  if (lastGoodFrame && now != lastGoodFrame) {
    const float fps = 1000.0f / (now - lastGoodFrame);
    actualFps = actualFps ? actualFps * 0.8f + fps * 0.2f : fps;
  }
  lastGoodFrame = now;
  const int64_t analyzeAt = esp_timer_get_time();
  const cats::Options &o = state.options;
  for (size_t y = 0; y < cats::GRID_H; ++y)
    for (size_t x = 0; x < cats::GRID_W; ++x) {
      const unsigned px = ANALYSIS_W * (o.x + (x + 0.5f) * o.width / cats::GRID_W) / 100.0f;
      const unsigned py = ANALYSIS_H * (o.y + (y + 0.5f) * o.height / cats::GRID_H) / 100.0f;
      samples[y * cats::GRID_W + x] = cats::fromRgb565LE(
        decoded + (py * ANALYSIS_W + px) * 2);
    }
  analysis = cats::analyze(samples, state.model, state.options);
  analysisMs = (esp_timer_get_time() - analyzeAt) / 1000.0f;
  workMs = (esp_timer_get_time() - started) / 1000.0f;
  return FrameResult::Good;
}

void applyCommand(const Command &command) {
  if (learning.active) { message = "Espera a que termine el registro."; return; }
  if (command.kind == CommandKind::Learn) {
    if (!cameraReady || !frameNumber || millis() - lastGoodFrame > 2500) {
      message = "La cámara aún no tiene una imagen válida."; return;
    }
    if (!command.background && !state.model.backgroundReady) {
      message = "Registra primero el fondo vacío."; return;
    }
    memset(&learning, 0, sizeof(learning));
    learning.active = true;
    learning.progress.start(CALIBRATION_FRAMES, MAX_CALIBRATION_ATTEMPTS);
    learning.started = millis();
    learning.lastSample = millis();
    learning.background = command.background;
    learning.label = command.label;
    if (!learning.background)
      learning.candidate = learning.label == Label::Orange ? state.model.orange : state.model.gray;
    stabilizer.reset();
    stableLabel = Label::Calibration;
    message = "Registro iniciado. Mantén el pelaje en la zona durante unos dos segundos.";
  } else {
    if (command.kind == CommandKind::Settings) state.options = command.options;
    clearModel();
    analysis = cats::Analysis{};
    if (!cameraReady) stableLabel = Label::Error;
    message = saveState() ? "Guardado. Registra el fondo y ambos gatos." :
                           "Cambio activo en RAM; no se pudo guardar en memoria.";
  }
}

void cameraTask(void *) {
  loadState();
  cameraReady = beginCamera();
  if (cameraReady && xTaskCreatePinnedToCore(captureProducer, "cat-capture", 4096,
                                           nullptr, 2, nullptr, 0) != pdPASS) {
    cameraReady = false;
    message = "No se pudo iniciar la captura de vídeo.";
  }
  if (!cameraReady) stableLabel = Label::Error;
  publishSnapshot();
  unsigned long lastStarted = millis() - CAPTURE_INTERVAL_MS;
  unsigned long lastLog = millis();
  for (;;) {
    Command command;
    if (xQueueReceive(commandQueue, &command, 0) == pdTRUE) {
      applyCommand(command);
      publishSnapshot(true);
    }
    if (learning.active && millis() - learning.started >= CALIBRATION_TIMEOUT_MS) {
      cancelLearning("Tiempo agotado con " + String(learning.progress.accepted()) + "/" +
                     String(CALIBRATION_FRAMES) + " imágenes válidas. " + message);
      publishSnapshot();
    }
    if (cameraReady && millis() - lastStarted >= CAPTURE_INTERVAL_MS) {
      lastStarted = millis();
      const FrameResult result = capture();
      if (result == FrameResult::Good) {
        if (learning.active) advanceLearning();
        else stableLabel = stabilizer.update(analysis.label);
      } else if (result == FrameResult::Failed) {
        ++captureErrors;
        if (learning.active) {
          if (millis() - learning.lastSample >= LEARNING_INTERVAL_MS) {
            learning.lastSample = millis();
            if (learning.progress.update(false) == cats::SampleState::TimedOut)
              cancelLearning("No se pudo completar el registro por errores de cámara.");
          }
        }
        stabilizer.reset();
        stableLabel = Label::Error;
        analysis = cats::Analysis{};
        actualFps = 0;
        message = "Falló la conversión de una imagen. Revisa la cámara y la alimentación.";
      } else if (!lastGoodFrame || millis() - lastGoodFrame > 2500) {
        stabilizer.reset();
        stableLabel = Label::Error;
        actualFps = 0;
      }
      publishSnapshot();
    }
    if (millis() - lastLog >= 2000) {
      lastLog = millis();
      // No bloquear esperando un monitor USB desconectado.
      const Snapshot s = readSnapshot();
      char line[220];
      const int len = snprintf(line, sizeof(line), "frame=%lu,%s,fps=%.1f,video=%.1f,stream=%.1f,decode=%.1fms,analysis=%.1fms,errors=%lu,psram=%.1fMB,ip=%s\n",
          static_cast<unsigned long>(frameNumber), cats::name(stableLabel), actualFps,
          s.videoFps, s.streamFps, decodeMs, analysisMs, static_cast<unsigned long>(s.errors),
          ESP.getPsramSize() / 1048576.0f,
          (WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
      if (len > 0 && len < int(sizeof(line)) && Serial.availableForWrite() >= len)
        Serial.write(reinterpret_cast<const uint8_t *>(line), len);
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
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
  const Snapshot s = readSnapshot();
  const unsigned long age = s.frame ? millis() - s.goodAt : 0;
  const Label label = s.frame && age > 2500 ? Label::Error : s.label;
  String out;
  out.reserve(1500);
  out = "{\"label\":\""; out += cats::name(label);
  out += "\",\"version\":\""; out += FIRMWARE_VERSION;
  out += "\",\"frame\":"; out += s.frame;
  out += ",\"ageMs\":"; out += age;
  out += ",\"foreground\":"; out += String(s.foreground, 3);
  out += ",\"useful\":"; out += String(s.useful, 3);
  out += ",\"fps\":"; out += String(age > 2500 ? 0 : s.fps, 1);
  const unsigned long videoAge = s.videoFrame ? millis() - s.videoAt : 0;
  out += ",\"videoFrame\":"; out += s.videoFrame;
  out += ",\"videoAgeMs\":"; out += videoAge;
  out += ",\"videoFps\":"; out += String(videoAge > 2500 ? 0 : s.videoFps, 1);
  out += ",\"streamFps\":"; out += String(s.streamFps, 1);
  out += ",\"streamClients\":"; out += s.streamClients;
  out += ",\"streamFrame\":"; out += s.streamFrame;
  out += ",\"streamAgeMs\":"; out += s.streamFrame ? millis() - s.streamAt : 0;
  out += ",\"streamStarts\":"; out += s.streamStarts;
  out += ",\"streamTimeouts\":"; out += s.streamTimeouts;
  out += ",\"mergedJpegs\":"; out += s.mergedJpegs;
  out += ",\"streamPort\":"; out += streamServer ? STREAM_PORT : 0;
  out += ",\"previewDrops\":"; out += s.previewDrops;
  out += ",\"captureMs\":"; out += String(s.captureMs, 2);
  out += ",\"decodeMs\":"; out += String(s.decodeMs, 2);
  out += ",\"analysisMs\":"; out += String(s.analysisMs, 2);
  out += ",\"workMs\":"; out += String(s.workMs, 2);
  out += ",\"freePsram\":"; out += static_cast<unsigned long>(s.freePsram);
  out += ",\"freeHeap\":"; out += static_cast<unsigned long>(s.freeHeap);
  out += ",\"cameraErrors\":"; out += s.errors;
  out += ",\"background\":"; out += s.background ? "true" : "false";
  out += ",\"orangeSamples\":"; out += s.orange;
  out += ",\"graySamples\":"; out += s.gray;
  out += ",\"busy\":"; out += s.busy ? "true" : "false";
  out += ",\"job\":\""; out += s.jobBackground ? "fondo" : cats::name(s.job);
  out += "\",\"progress\":"; out += s.progress;
  out += ",\"attempts\":"; out += s.attempts;
  out += ",\"total\":"; out += CALIBRATION_FRAMES;
  out += ",\"message\":\""; out += jsonEscape(s.message);
  out += "\",\"options\":{\"x\":"; out += s.options.x;
  out += ",\"y\":"; out += s.options.y;
  out += ",\"width\":"; out += s.options.width;
  out += ",\"height\":"; out += s.options.height;
  out += ",\"difference\":"; out += s.options.difference;
  out += ",\"minForeground\":"; out += s.options.minForeground;
  out += "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}
void enqueue(const Command &command) {
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  const bool busy = shared.busy || commandPending;
  if (!busy) commandPending = true;
  xSemaphoreGive(sharedMutex);
  if (busy) { respond(409, "Espera a que termine la acción anterior."); return; }
  if (xQueueSend(commandQueue, &command, 0) != pdTRUE) {
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    commandPending = false;
    xSemaphoreGive(sharedMutex);
    respond(503, "No se pudo iniciar la acción; vuelve a intentarlo.");
    return;
  }
  respond(202, "Acción recibida. Consulta el progreso en el panel.");
}
void startLearning() {
  const Snapshot s = readSnapshot();
  if (!s.ready || !s.frame || millis() - s.goodAt > 2500) {
    respond(503, "La cámara aún no tiene una imagen válida."); return;
  }
  const String label = server.arg("label");
  if (label != "fondo" && label != "naranja" && label != "gris") {
    respond(400, "Etiqueta inválida."); return;
  }
  if (label != "fondo" && !s.background) {
    respond(409, "Registra primero el fondo vacío."); return;
  }
  Command command;
  command.kind = CommandKind::Learn;
  command.background = label == "fondo";
  command.label = label == "naranja" ? Label::Orange : Label::Gray;
  enqueue(command);
}
bool numericArg(const char *key, uint8_t &target) {
  if (!server.hasArg(key)) return false;
  const String text = server.arg(key);
  if (!text.length() || text.length() > 3) return false;
  for (size_t i = 0; i < text.length(); ++i)
    if (text[i] < '0' || text[i] > '9') return false;
  const long value = strtol(text.c_str(), nullptr, 10);
  if (value > 100) return false;
  target = uint8_t(value);
  return true;
}
void settings() {
  Command command;
  command.kind = CommandKind::Settings;
  auto &o = command.options;
  if (!numericArg("x", o.x) || !numericArg("y", o.y) ||
      !numericArg("width", o.width) || !numericArg("height", o.height) ||
      !numericArg("difference", o.difference) ||
      !numericArg("minForeground", o.minForeground) || !cats::validOptions(o)) {
    respond(400, "Valores inválidos; la zona debe quedar dentro de la imagen."); return;
  }
  enqueue(command);
}

// Dos tareas permiten relevar una conexión atascada. El último visor tiene
// prioridad; el servidor HTTP queda libre para recibir su reconexión.
struct StreamJob {
  httpd_req_t *req = nullptr;
  int socket = -1;
  uint32_t epoch = 0, sendStarted = 0;
  bool active = false, timedOut = false;
} streamJobs[2];

// El timeout SO_SNDTIMEO limita cada send(), no la imagen completa. Un
// cliente que lee muy despacio puede prolongar indefinidamente los envíos
// parciales. Este límite engloba cabecera, JPEG y todas sus escrituras.
int boundedStreamSend(httpd_handle_t, int socket, const char *buf, size_t bytes, int flags) {
  for (;;) {
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    StreamJob *job = nullptr;
    for (auto &candidate : streamJobs)
      if (candidate.active && candidate.socket == socket) { job = &candidate; break; }
    const bool current = job && job->epoch == video.epoch;
    const bool expired = job && millis() - job->sendStarted >= STREAM_SEND_TIMEOUT_MS;
    if (expired && current) job->timedOut = true;
    xSemaphoreGive(sharedMutex);
    if (!current) return HTTPD_SOCK_ERR_FAIL;
    if (expired) return HTTPD_SOCK_ERR_TIMEOUT;
    const int sent = send(socket, buf, bytes, flags | MSG_DONTWAIT);
    if (sent > 0) return sent;
    if (!sent || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
      return HTTPD_SOCK_ERR_FAIL;
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// Completar y cerrar desde la propia tarea HTTPD evita que un descriptor
// se reutilice entre la finalización asíncrona y su cierre.
void finishStream(void *argument) {
  StreamJob &job = *static_cast<StreamJob *>(argument);
  httpd_req_async_handler_complete(job.req);
  shutdown(job.socket, SHUT_RDWR);  // HTTPD recoge el EOF y libera la sesión.
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  --video.clients;
  if (job.epoch == video.epoch) video.streamFps = 0;
  if (job.timedOut) ++video.timeouts;
  job.active = false;
  job.req = nullptr;
  xSemaphoreGive(sharedMutex);
}

void streamTask(void *argument) {
  StreamJob &job = *static_cast<StreamJob *>(argument);
  httpd_req_t *req = job.req;
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=catframe");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Connection", "close");
  uint32_t lastFrame = 0, waitingSince = millis(), rateSince = millis(), rateFrames = 0;
  const uint32_t started = millis();
  esp_err_t result = ESP_OK;
  for (;;) {
    size_t bytes;
    uint32_t frame;
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    const bool current = job.epoch == video.epoch;
    const int slot = previewPool.acquire(bytes, frame);
    const uint32_t capturedAt = slot >= 0 ? previewAt[slot] : 0;
    xSemaphoreGive(sharedMutex);
    if (!current || millis() - started >= STREAM_SESSION_MS ||
        slot < 0 || frame == lastFrame || millis() - capturedAt > 2500) {
      if (slot >= 0) {
        xSemaphoreTake(sharedMutex, portMAX_DELAY);
        previewPool.release(slot);
        xSemaphoreGive(sharedMutex);
      }
      if (!current || millis() - started >= STREAM_SESSION_MS) break;
      if (millis() - waitingSince > 2500) { result = ESP_FAIL; break; }
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    char header[140];
    const int len = snprintf(header, sizeof(header),
      "\r\n--catframe\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\nX-Frame-Number: %lu\r\n\r\n",
      static_cast<unsigned>(bytes), static_cast<unsigned long>(frame));
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    job.sendStarted = millis();
    xSemaphoreGive(sharedMutex);
    result = httpd_resp_send_chunk(req, header, len);
    if (result == ESP_OK)
      result = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(previewBytes[slot]), bytes);
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    previewPool.release(slot);  // También liberarlo si el cliente se desconecta.
    xSemaphoreGive(sharedMutex);
    if (result != ESP_OK) break;
    const uint32_t now = millis();
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    const bool stillCurrent = job.epoch == video.epoch;
    if (stillCurrent) {
      video.sentFrame = frame;
      video.sentAt = now;
    }
    ++rateFrames;
    if (now - rateSince >= 1000) {
      if (stillCurrent) video.streamFps = rateFrames * 1000.0f / (now - rateSince);
      rateSince = now;
      rateFrames = 0;
    }
    xSemaphoreGive(sharedMutex);
    waitingSince = now;
    lastFrame = frame;
  }
  // Ya no hay buffers retenidos. Si el control HTTP está ocupado, esperar
  // sin consumir CPU hasta que pueda liberar esta petición y su sesión.
  while (httpd_queue_work(streamServer, finishStream, &job) != ESP_OK)
    vTaskDelay(pdMS_TO_TICKS(2));
  vTaskDelete(nullptr);
}

esp_err_t streamHandler(httpd_req_t *req) {
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  StreamJob *job = nullptr;
  for (auto &candidate : streamJobs)
    if (!candidate.active) { job = &candidate; break; }
  if (job) job->active = true;
  xSemaphoreGive(sharedMutex);
  if (!job) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_hdr(req, "Retry-After", "1");
    return httpd_resp_send(req, "Reintenta el vídeo en un segundo.", HTTPD_RESP_USE_STRLEN);
  }
  httpd_req_t *copy = nullptr;
  if (httpd_req_async_handler_begin(req, &copy) != ESP_OK) {
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    job->active = false;
    xSemaphoreGive(sharedMutex);
    return ESP_FAIL;
  }
  const int socket = httpd_req_to_sockfd(copy);
  httpd_sess_set_send_override(req->handle, socket, boundedStreamSend);
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  job->req = copy;
  job->socket = socket;
  job->epoch = ++video.epoch;
  job->timedOut = false;
  video.streamFps = 0;
  ++video.clients;
  ++video.starts;
  xSemaphoreGive(sharedMutex);
  if (xTaskCreatePinnedToCore(streamTask, "cat-stream", 4096, job, 1, nullptr, 1) == pdPASS)
    return ESP_OK;
  httpd_req_async_handler_complete(copy);
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  job->active = false;
  job->req = nullptr;
  --video.clients;
  xSemaphoreGive(sharedMutex);
  return ESP_FAIL;
}
void beginStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = STREAM_PORT;
  config.ctrl_port = 32769;
  config.core_id = 1;
  config.stack_size = 4096;
  config.max_open_sockets = 4;
  config.lru_purge_enable = true;
  config.send_wait_timeout = 1;
  config.recv_wait_timeout = 2;
  if (httpd_start(&streamServer, &config) != ESP_OK) {
    streamServer = nullptr;
    Serial.println("No se pudo iniciar el servidor de vídeo.");
    return;
  }
  httpd_uri_t route = {};
  route.uri = "/stream";
  route.method = HTTP_GET;
  route.handler = streamHandler;
  if (httpd_register_uri_handler(streamServer, &route) != ESP_OK) {
    httpd_stop(streamServer);
    streamServer = nullptr;
    Serial.println("No se pudo registrar el vídeo.");
  }
}
void beginServer() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/capture", HTTP_GET, [] {
    const Snapshot s = readSnapshot();
    if (!s.videoFrame || millis() - s.videoAt > 2500) {
      respond(503, "No hay imagen válida reciente."); return;
    }
    size_t bytes;
    uint32_t frame;
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    const int slot = previewPool.acquire(bytes, frame);
    xSemaphoreGive(sharedMutex);
    if (slot < 0) { respond(503, "No hay imagen válida."); return; }
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Number", String(frame));
    server.send_P(200, "image/jpeg", reinterpret_cast<const char *>(previewBytes[slot]), bytes);
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    previewPool.release(slot);
    xSemaphoreGive(sharedMutex);
  });
  server.on("/api/status", HTTP_GET, status);
  server.on("/api/learn", HTTP_POST, startLearning);
  server.on("/api/settings", HTTP_POST, settings);
  server.on("/api/reset", HTTP_POST, [] { enqueue(Command{}); });
  server.onNotFound([] { respond(404, "Ruta no encontrada."); });
  server.begin();
}

// Diagnóstico por USB: 't' comprueba vídeo y API dentro de la placa sin
// cambiar calibración ni conectarla a otra red. No mide el enlace Wi-Fi.
bool testRunning = false;  // Protegido por sharedMutex.
void streamTestTask(void *) {
  WiFiClient client;
  client.setTimeout(1000);
  bool connected = client.connect(IPAddress(127, 0, 0, 1), STREAM_PORT);
  if (connected) {
    client.print("GET /stream HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    const String first = client.readStringUntil('\n');
    connected = first.indexOf("200") >= 0;
  }
  uint32_t frames = 0, duplicates = 0, lastFrame = 0, number = 0;
  uint32_t started = millis();
  bool readingNumber = false, apiOk = false, apiChecked = false;
  constexpr char marker[] = "X-Frame-Number: ";
  unsigned matched = 0;
  uint8_t buffer[1024];
  while (connected && millis() - started < 8000) {
    const int available = client.available();
    if (available > 0) {
      const int length = client.read(buffer, sizeof(buffer));
      for (int i = 0; i < length; ++i) {
        const char c = buffer[i];
        if (readingNumber) {
          if (c >= '0' && c <= '9') number = number * 10 + c - '0';
          else {
            if (number == lastFrame) ++duplicates;
            lastFrame = number;
            ++frames;
            readingNumber = false;
          }
        } else {
          matched = c == marker[matched] ? matched + 1 : (c == marker[0] ? 1 : 0);
          if (matched == sizeof(marker) - 1) {
            matched = 0;
            number = 0;
            readingNumber = true;
          }
        }
      }
    } else if (!client.connected()) break;
    if (!apiChecked && millis() - started >= 2000) {
      apiChecked = true;
      WiFiClient probe;
      probe.setTimeout(1000);
      if (probe.connect(IPAddress(127, 0, 0, 1), 80)) {
        probe.print("GET /api/status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
        apiOk = probe.readStringUntil('\n').indexOf("200") >= 0;
      }
      probe.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  const float elapsed = (millis() - started) / 1000.0f;
  client.stop();
  const Snapshot s = readSnapshot();
  Serial.printf("SELFTEST connected=%u,frames=%lu,fps=%.1f,duplicates=%lu,api=%u,recognition=%.1f,bg=%u,orange=%u,gray=%u\n",
    connected, static_cast<unsigned long>(frames), elapsed ? frames / elapsed : 0,
    static_cast<unsigned long>(duplicates), apiOk, s.fps, s.background, s.orange, s.gray);
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  testRunning = false;
  xSemaphoreGive(sharedMutex);
  vTaskDelete(nullptr);
}

// 'r': mantener un visor antiguo abierto al conectar el siguiente y dejar
// clientes sin leer, como ocurre al suspender una pestaña o perder Wi-Fi.
// Solo lee vídeo/estado: no toca ajustes, modelos ni memoria persistente.
void reconnectTestTask(void *) {
  const Snapshot before = readSnapshot();
  uint32_t handoffs = 0, probes = 0, failures = 0, worstConnect = 0, bytesRead = 0;
  size_t minHeap = before.freeHeap;
  const uint32_t started = millis();
  while (millis() - started < 60000) {
    WiFiClient oldViewer, nextViewer;
    oldViewer.setTimeout(600);
    nextViewer.setTimeout(600);
    bool oldOk = oldViewer.connect(IPAddress(127, 0, 0, 1), STREAM_PORT, 600);
    if (oldOk) {
      oldViewer.print("GET /stream HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
      oldOk = oldViewer.readStringUntil('\n').indexOf("200") >= 0;
    }
    uint8_t buffer[1024];
    const uint32_t warmup = millis();
    while (oldOk && millis() - warmup < 400) {
      if (oldViewer.available()) oldViewer.read(buffer, sizeof(buffer));
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    // No cerrar oldViewer antes de pedir el vídeo nuevo.
    const uint32_t connectAt = millis();
    bool nextOk = nextViewer.connect(IPAddress(127, 0, 0, 1), STREAM_PORT, 600);
    if (nextOk) {
      nextViewer.print("GET /stream HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
      nextOk = nextViewer.readStringUntil('\n').indexOf("200") >= 0;
    }
    const uint32_t latency = millis() - connectAt;
    if (latency > worstConnect) worstConnect = latency;
    if (oldOk && nextOk && latency < 600) ++handoffs;
    else ++failures;
    oldViewer.stop();
    const uint32_t readingAt = millis();
    uint32_t received = 0;
    while (nextOk && millis() - readingAt < 2000) {
      if (nextViewer.available()) {
        const int n = nextViewer.read(buffer, sizeof(buffer));
        if (n > 0) received += n;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    bytesRead += received;
    if (!received) ++failures;
    WiFiClient probe;
    probe.setTimeout(600);
    bool apiOk = probe.connect(IPAddress(127, 0, 0, 1), 80, 600);
    if (apiOk) {
      probe.print("GET /api/status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
      apiOk = probe.readStringUntil('\n').indexOf("200") >= 0;
    }
    if (apiOk) ++probes;
    else ++failures;
    probe.stop();
    // El visor deja de leer sin cerrar TCP. El presupuesto por imagen
    // debe liberar su tarea y los buffers, manteniendo el análisis.
    vTaskDelay(pdMS_TO_TICKS(3000));
    const Snapshot paused = readSnapshot();
    if (paused.streamClients || paused.frame <= before.frame || paused.fps < 5) ++failures;
    if (paused.freeHeap < minHeap) minHeap = paused.freeHeap;
    nextViewer.stop();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  // Verificar también la expiración de una conexión larga que sigue
  // leyendo: no debe dejar una tarea o sesión ocupada tras los 35 s.
  WiFiClient longViewer;
  longViewer.setTimeout(600);
  bool longOk = longViewer.connect(IPAddress(127, 0, 0, 1), STREAM_PORT, 600);
  if (longOk) {
    longViewer.print("GET /stream HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    longOk = longViewer.readStringUntil('\n').indexOf("200") >= 0;
  }
  const uint32_t longStarted = millis();
  uint8_t longBuffer[1024];
  while (longOk && millis() - longStarted < STREAM_SESSION_MS + 2000) {
    if (longViewer.available()) longViewer.read(longBuffer, sizeof(longBuffer));
    else if (!longViewer.connected()) break;
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  const uint32_t sessionMs = millis() - longStarted;
  if (!longOk || longViewer.connected() || sessionMs < STREAM_SESSION_MS - 1000 ||
      sessionMs > STREAM_SESSION_MS + 1000) ++failures;
  longViewer.stop();
  vTaskDelay(pdMS_TO_TICKS(2000));
  const Snapshot after = readSnapshot();
  const bool preserved = before.background == after.background &&
    before.orange == after.orange && before.gray == after.gray;
  Serial.printf("RECONNECTTEST handoffs=%lu,api=%lu,failures=%lu,maxConnectMs=%lu,sessionMs=%lu,bytes=%lu,recognition=%.1f,cameraErrors=%lu,timeouts=%lu,clients=%u,calibration=%u\n",
    static_cast<unsigned long>(handoffs), static_cast<unsigned long>(probes),
    static_cast<unsigned long>(failures), static_cast<unsigned long>(worstConnect),
    static_cast<unsigned long>(sessionMs), static_cast<unsigned long>(bytesRead), after.fps,
    static_cast<unsigned long>(after.errors - before.errors),
    static_cast<unsigned long>(after.streamTimeouts - before.streamTimeouts), after.streamClients, preserved);
  Serial.printf("RECONNECTHEAP before=%u,after=%u,min=%u,merged=%lu,bg=%u,orange=%u,gray=%u\n",
    static_cast<unsigned>(before.freeHeap), static_cast<unsigned>(after.freeHeap),
    static_cast<unsigned>(minHeap), static_cast<unsigned long>(after.mergedJpegs),
    after.background, after.orange, after.gray);
  xSemaphoreTake(sharedMutex, portMAX_DELAY);
  testRunning = false;
  xSemaphoreGive(sharedMutex);
  vTaskDelete(nullptr);
}
void serialCommands() {
  while (Serial.available()) {
    const int command = Serial.read();
    if (command != 't' && command != 'r') continue;
    xSemaphoreTake(sharedMutex, portMAX_DELAY);
    const bool start = !testRunning;
    if (start) testRunning = true;
    xSemaphoreGive(sharedMutex);
    if (start && xTaskCreatePinnedToCore(command == 'r' ? reconnectTestTask : streamTestTask, "stream-test", 6144,
                                        nullptr, 1, nullptr, 1) != pdPASS) {
      xSemaphoreTake(sharedMutex, portMAX_DELAY);
      testRunning = false;
      xSemaphoreGive(sharedMutex);
    }
  }
}
void setup() {
  Serial.begin(115200);
  Serial.print("Firmware: "); Serial.println(FIRMWARE_VERSION);
  sharedMutex = xSemaphoreCreateMutex();
  commandQueue = xQueueCreate(1, sizeof(Command));
  if (!sharedMutex || !commandQueue) {
    Serial.println("No se pudieron crear las tareas. Reinicia la placa.");
    for (;;) delay(1000);
  }
  // El servidor Arduino va en núcleo 1; cámara y análisis en núcleo 0.
  snprintf(shared.message, sizeof(shared.message), "Iniciando cámara y memoria…");
  if (xTaskCreatePinnedToCore(cameraTask, "cat-camera", 12288, nullptr, 1, nullptr, 0) != pdPASS) {
    shared.label = Label::Error;
    snprintf(shared.message, sizeof(shared.message), "No se pudo iniciar la tarea de cámara.");
  }
  beginNetwork();
  beginStreamServer();
  beginServer();
}
void loop() {
  server.handleClient();
  serialCommands();
  delay(1);
}
