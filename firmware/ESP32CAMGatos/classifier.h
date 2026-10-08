#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cats {
constexpr size_t GRID_W = 40;
constexpr size_t GRID_H = 30;
constexpr size_t PIXELS = GRID_W * GRID_H;
constexpr size_t HIST_BINS = 14;
constexpr size_t FEATURES = HIST_BINS + 4;

struct Rgb { uint8_t r, g, b; };
struct Options {
  uint8_t x = 20, y = 15, width = 60, height = 70;
  uint8_t difference = 25;       // Diferencia media RGB respecto del fondo.
  uint8_t minForeground = 15;    // Porcentaje mínimo de la zona ocupado.
};
struct Features { float value[FEATURES] = {}; };
struct Prototype {
  Features mean;
  uint16_t count = 0;
};
struct Model {
  Rgb background[PIXELS] = {};
  bool backgroundReady = false;
  Prototype orange, gray;
};
enum class Label : uint8_t { Calibration, Empty, Orange, Gray, Unknown, LowLight, Error };
struct Analysis {
  Features features;
  float foreground = 0, brightness = 0;
  float orangeDistance = 1, grayDistance = 1;
  size_t useful = 0;
  Label label = Label::Calibration;
};

bool validOptions(const Options &options);
bool validModel(const Model &model);
Rgb fromRgb565LE(const uint8_t *pixel);
Analysis analyze(const Rgb *pixels, const Model &model, const Options &options);
float distance(const Features &a, const Features &b);
bool suitable(const Analysis &analysis, Label label);
bool addSample(Prototype &prototype, const Features &features);
bool separable(const Prototype &orange, const Prototype &gray);
const char *name(Label label);

// Solo confirma naranja/gris después de varias capturas consecutivas.
// Vacío, error e indeterminado borran inmediatamente la salida anterior.
class Stabilizer {
 public:
  explicit Stabilizer(unsigned frames = 3) : required_(frames ? frames : 1) {}
  Label update(Label label);
  void reset();
 private:
  unsigned required_, hits_ = 0;
  Label candidate_ = Label::Unknown;
};
}  // namespace cats

