#include "../firmware/XIAOS3Gatos/classifier.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace cats;
static unsigned checks = 0;
static void check(bool ok, const char *description) {
  ++checks;
  if (!ok) { std::fprintf(stderr, "FALLO: %s\n", description); std::exit(1); }
}
static Rgb scene[PIXELS];
static void fill(Rgb color, size_t count = PIXELS) {
  for (size_t i = 0; i < count; ++i) scene[i] = color;
}
int main() {
  Options options;
  Model model;
  fill(Rgb{40, 50, 60});
  check(analyze(scene, model, options).label == Label::Calibration, "no clasificar antes de calibrar");
  for (size_t i = 0; i < PIXELS; ++i) model.background[i] = scene[i];
  model.backgroundReady = true;
  check(analyze(scene, model, options).label == Label::Empty, "fondo vacío con modelos pendientes");

  fill(Rgb{200, 120, 55});
  Analysis orange = analyze(scene, model, options);
  check(trainingIssue(orange, options) == TrainingIssue::Ready, "pelaje naranja admite calibración");
  for (int i = 0; i < 8; ++i) check(addSample(model.orange, orange.features), "añadir muestra naranja");
  check(analyze(scene, model, options).label == Label::Calibration, "no clasificar si falta gris");

  fill(Rgb{140, 142, 145});
  Analysis gray = analyze(scene, model, options);
  check(trainingIssue(gray, options) == TrainingIssue::Ready, "pelaje gris admite calibración");
  for (int i = 0; i < 8; ++i) check(addSample(model.gray, gray.features), "añadir muestra gris");
  check(separable(model.orange, model.gray), "prototipos distintos");
  check(analyze(scene, model, options).label == Label::Gray, "identificar gris");
  fill(Rgb{125, 127, 130});
  check(analyze(scene, model, options).label == Label::Gray, "gris con variación moderada de brillo");
  fill(Rgb{200, 135, 55});
  check(analyze(scene, model, options).label == Label::Orange, "naranja al cruzar límite de tono");
  fill(Rgb{185, 111, 51});
  check(analyze(scene, model, options).label == Label::Orange, "identificar naranja con menor brillo");
  fill(Rgb{200, 120, 55});
  fill(Rgb{40, 50, 60}, PIXELS * 3 / 4);
  check(analyze(scene, model, options).label == Label::Orange, "excluir fondo alrededor del pelaje");
  fill(Rgb{40, 50, 60});
  check(analyze(scene, model, options).label == Label::Empty, "no confundir fondo vacío con gato gris");
  fill(Rgb{150, 150, 150});
  Model grayBackground = model;
  for (size_t i = 0; i < PIXELS; ++i) grayBackground.background[i] = scene[i];
  check(analyze(scene, grayBackground, options).label == Label::Empty, "fondo gris sigue siendo vacío");
  fill(Rgb{45, 80, 190});
  check(analyze(scene, model, options).label == Label::Unknown, "rechazar azul");
  fill(Rgb{80, 170, 70});
  check(analyze(scene, model, options).label == Label::Unknown, "rechazar verde");
  fill(Rgb{255, 255, 255});
  check(analyze(scene, model, options).label == Label::Unknown, "rechazar reflejo blanco");
  fill(Rgb{8, 8, 8});
  check(analyze(scene, model, options).label == Label::LowLight, "poca luz");
  fill(Rgb{40, 50, 60});
  fill(Rgb{200, 120, 55}, PIXELS / 10);
  check(analyze(scene, model, options).label == Label::Empty, "ignorar objeto demasiado pequeño");

  Model ambiguous = model;
  ambiguous.gray = ambiguous.orange;
  fill(Rgb{200, 120, 55});
  check(!separable(ambiguous.orange, ambiguous.gray), "rechazar entrenamiento indistinguible");
  check(analyze(scene, ambiguous, options).label == Label::Unknown, "rechazar empate");

  // Regresión: el pelaje crema quedaba sin tono y nunca podía aprenderse.
  Model paleModel = model;
  paleModel.orange = Prototype{};
  paleModel.gray = Prototype{};
  fill(Rgb{220, 205, 190});
  const Analysis pale = analyze(scene, paleModel, options);
  check(pale.features.value[16] == 0, "escena reproduce naranja fuera del antiguo filtro fijo");
  check(trainingIssue(pale, options) == TrainingIssue::Ready, "aceptar naranja pálido etiquetado por el usuario");
  check(addSample(paleModel.orange, pale.features), "guardar perfil de naranja pálido");
  fill(Rgb{218, 219, 220});
  const Analysis paleGray = analyze(scene, paleModel, options);
  check(addSample(paleModel.gray, paleGray.features), "guardar gris con el mismo brillo del naranja pálido");
  check(separable(paleModel.orange, paleModel.gray), "conservar el tono del pelaje pálido");
  check(analyze(scene, paleModel, options).label == Label::Gray, "identificar gris de brillo similar");
  fill(Rgb{220, 205, 190});
  check(analyze(scene, paleModel, options).label == Label::Orange, "identificar naranja pálido sin filtro fijo");
  fill(Rgb{220, 204, 189});
  check(analyze(scene, paleModel, options).label == Label::Orange, "naranja pálido con variación pequeña");
  fill(Rgb{255, 140, 45});
  const Analysis brightOrange = analyze(scene, model, options);
  check(trainingIssue(brightOrange, options) == TrainingIssue::Ready, "no confundir un canal naranja brillante con reflejo blanco");
  check(analyze(scene, model, options).label == Label::Orange, "identificar naranja brillante");

  SamplingProgress progress;
  progress.start(8, 40);
  for (int i = 0; i < 7; ++i) {
    check(progress.update(true) == SampleState::Accepted, "guardar captura válida en registro");
    check(progress.update(false) == SampleState::Waiting, "captura mala no cancela ni borra el progreso");
  }
  check(progress.accepted() == 7 && progress.attempts() == 14, "conservar siete muestras entre capturas malas");
  check(progress.update(true) == SampleState::Complete, "completar con ocho capturas no consecutivas");
  check(progress.update(false) == SampleState::Complete && progress.accepted() == 8, "registro completo permanece completo");
  progress.start(8, 40);
  for (int i = 0; i < 39; ++i) progress.update(false);
  check(progress.update(false) == SampleState::TimedOut, "sin muestras terminar por tiempo limitado");
  check(progress.update(true) == SampleState::TimedOut && progress.accepted() == 0, "no modificar sesión agotada");
  progress.start(8, 8);
  for (int i = 0; i < 7; ++i) progress.update(true);
  check(progress.update(true) == SampleState::Complete, "aceptar la última captura dentro del límite");
  check(progress.update(false) == SampleState::Complete, "no convertir un éxito final en agotamiento");

  fill(Rgb{40, 50, 60});
  check(trainingIssue(analyze(scene, paleModel, options), options) == TrainingIssue::NoForeground, "seguir esperando si el gato sale de la zona");
  fill(Rgb{255, 255, 255});
  check(trainingIssue(analyze(scene, paleModel, options), options) == TrainingIssue::TooFewPixels, "no aprender reflejos saturados");
  fill(Rgb{8, 8, 8});
  check(trainingIssue(analyze(scene, paleModel, options), options) == TrainingIssue::LowLight, "esperar iluminación suficiente");
  Features invalid = orange.features;
  invalid.value[0] = std::numeric_limits<float>::quiet_NaN();
  const auto count = model.orange.count;
  check(!addSample(model.orange, invalid) && model.orange.count == count, "rechazar NaN sin mutar modelo");
  check(validModel(model), "modelo válido");
  model.orange.mean.value[0] = std::numeric_limits<float>::infinity();
  check(!validModel(model), "rechazar modelo corrupto");

  Stabilizer filter(3);
  check(filter.update(Label::Orange) == Label::Unknown, "primera lectura provisional");
  check(filter.update(Label::Orange) == Label::Unknown, "segunda lectura provisional");
  check(filter.update(Label::Orange) == Label::Orange, "tercera lectura confirma naranja");
  check(filter.update(Label::Gray) == Label::Unknown, "cambio de gato borra salida anterior");
  check(filter.update(Label::Empty) == Label::Empty, "vacío borra la confirmación inmediatamente");
  check(filter.update(Label::Gray) == Label::Unknown, "reiniciar secuencia después de vacío");
  filter.update(Label::Gray);
  check(filter.update(Label::Gray) == Label::Gray, "confirmar gris");
  check(filter.update(Label::Error) == Label::Error, "error no conserva gato anterior");

  const uint8_t red[] = {0x00, 0xf8}, blue[] = {0x1f, 0x00};
  check(fromRgb565LE(red).r == 255 && fromRgb565LE(red).b == 0, "orden de bytes: rojo");
  check(fromRgb565LE(blue).b == 255 && fromRgb565LE(blue).r == 0, "orden de bytes: azul");
  options.x = 95;
  check(!validOptions(options), "zona fuera de la imagen");
  check(analyze(scene, model, options).label == Label::Error, "evitar analizar zona inválida");
  std::printf("OK: %u comprobaciones del clasificador\n", checks);
}
