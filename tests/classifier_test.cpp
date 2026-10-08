#include "../firmware/ESP32CAMGatos/classifier.h"
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
  check(suitable(orange, Label::Orange), "pelaje naranja admite calibración");
  check(!suitable(orange, Label::Gray), "naranja no admite etiqueta gris");
  for (int i = 0; i < 8; ++i) check(addSample(model.orange, orange.features), "añadir muestra naranja");
  check(analyze(scene, model, options).label == Label::Calibration, "no clasificar si falta gris");

  fill(Rgb{140, 142, 145});
  Analysis gray = analyze(scene, model, options);
  check(suitable(gray, Label::Gray), "pelaje gris admite calibración");
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

