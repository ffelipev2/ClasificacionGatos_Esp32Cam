#include "classifier.h"
#include <cmath>

namespace cats {
namespace {
float maximum(float a, float b) { return a > b ? a : b; }
float minimum(float a, float b) { return a < b ? a : b; }
struct Hsv { float h, s, v; };
Hsv hsv(const Rgb &p) {
  const float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
  const float hi = maximum(r, maximum(g, b));
  const float lo = minimum(r, minimum(g, b));
  const float delta = hi - lo;
  Hsv out = {0, hi > 0 ? delta / hi : 0, hi};
  if (delta > 0.0001f) {
    if (hi == r) out.h = 60.0f * ((g - b) / delta);
    else if (hi == g) out.h = 60.0f * (2.0f + (b - r) / delta);
    else out.h = 60.0f * (4.0f + (r - g) / delta);
    if (out.h < 0) out.h += 360.0f;
  }
  return out;
}
bool orangePixel(const Hsv &p) {
  return p.h >= 8 && p.h <= 52 && p.s >= 0.20f;
}
float rgbDifference(const Rgb &a, const Rgb &b) {
  return (std::fabs(float(a.r) - b.r) + std::fabs(float(a.g) - b.g) +
          std::fabs(float(a.b) - b.b)) / 3.0f;
}
bool validPrototype(const Prototype &p) {
  if (p.count > 2000) return false;
  for (size_t i = 0; i < FEATURES; ++i)
    if (!std::isfinite(p.mean.value[i]) || p.mean.value[i] < 0 ||
        p.mean.value[i] > 1.001f) return false;
  return true;
}
}  // namespace

bool validOptions(const Options &o) {
  return o.width >= 10 && o.height >= 10 && unsigned(o.x) + o.width <= 100 &&
         unsigned(o.y) + o.height <= 100 && o.difference >= 5 &&
         o.difference <= 80 && o.minForeground >= 5 && o.minForeground <= 80;
}
bool validModel(const Model &m) {
  return validPrototype(m.orange) && validPrototype(m.gray) &&
         (m.backgroundReady || (!m.orange.count && !m.gray.count));
}
Rgb fromRgb565LE(const uint8_t *p) {
  const uint16_t value = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
  const uint8_t r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
  return Rgb{uint8_t((r << 3) | (r >> 2)), uint8_t((g << 2) | (g >> 4)),
             uint8_t((b << 3) | (b >> 2))};
}

float distance(const Features &a, const Features &b) {
  float histogram = 0;
  for (size_t i = 0; i < HIST_BINS; ++i)
    histogram += std::fabs(a.value[i] - b.value[i]);
  return 0.50f * histogram / 2.0f +
         0.15f * std::fabs(a.value[14] - b.value[14]) +
         0.10f * std::fabs(a.value[15] - b.value[15]) +
         0.15f * std::fabs(a.value[16] - b.value[16]) +
         0.10f * std::fabs(a.value[17] - b.value[17]);
}

Analysis analyze(const Rgb *pixels, const Model &m, const Options &o) {
  Analysis out;
  if (!pixels || !validOptions(o)) { out.label = Label::Error; return out; }
  size_t foreground = 0;
  for (size_t i = 0; i < PIXELS; ++i) {
    const Hsv p = hsv(pixels[i]);
    out.brightness += p.v / PIXELS;
    if (!m.backgroundReady || rgbDifference(pixels[i], m.background[i]) < o.difference)
      continue;
    ++foreground;
    // Excluir sombras profundas y reflejos blancos muy brillantes.
    if (p.v < 0.12f || (p.v > 0.96f && p.s < 0.08f)) continue;
    ++out.useful;
    // Conservar el tono del pelaje pálido: antes se borraba toda su
    // información de color cuando la saturación era menor que 0.20.
    const float chromatic = maximum(0, minimum(1, (p.s - 0.04f) / 0.20f));
    out.features.value[12] += (1.0f - chromatic) * (1.0f - p.v);
    out.features.value[13] += (1.0f - chromatic) * p.v;
    if (chromatic > 0) {
      const float position = p.h / 30.0f;
      size_t bin = size_t(position);
      if (bin >= 12) bin = 11;
      const float fraction = position - bin;
      out.features.value[bin] += chromatic * (1.0f - fraction);
      out.features.value[(bin + 1) % 12] += chromatic * fraction;
    }
    out.features.value[14] += p.s;
    out.features.value[15] += p.v;
    out.features.value[16] += orangePixel(p) ? 1 : 0;
    out.features.value[17] += p.s < 0.20f ? 1 : 0;
  }
  out.foreground = float(foreground) / PIXELS;
  if (out.useful)
    for (size_t i = 0; i < FEATURES; ++i) out.features.value[i] /= out.useful;

  if (out.brightness < 0.10f) { out.label = Label::LowLight; return out; }
  if (!m.backgroundReady) return out;
  if (out.foreground < o.minForeground / 100.0f) {
    out.label = Label::Empty; return out;
  }
  if (!m.orange.count || !m.gray.count) return out;
  out.label = Label::Unknown;
  if (out.useful < PIXELS * o.minForeground / 200) return out;
  if (!separable(m.orange, m.gray)) return out;

  out.orangeDistance = distance(out.features, m.orange.mean);
  out.grayDistance = distance(out.features, m.gray.mean);
  const bool isOrange = out.orangeDistance < out.grayDistance;
  const float best = isOrange ? out.orangeDistance : out.grayDistance;
  const float other = isOrange ? out.grayDistance : out.orangeDistance;
  const Label candidate = isOrange ? Label::Orange : Label::Gray;
  // La etiqueta procede de las muestras del usuario, no de un tono fijo.
  const float separation = distance(m.orange.mean, m.gray.mean);
  const float margin = minimum(0.08f, separation * 0.25f);
  if (best <= 0.24f && other - best >= margin)
    out.label = candidate;
  return out;
}

TrainingIssue trainingIssue(const Analysis &a, const Options &o) {
  if (a.brightness < 0.10f) return TrainingIssue::LowLight;
  if (a.foreground < o.minForeground / 100.0f) return TrainingIssue::NoForeground;
  const size_t areaRequired = PIXELS * o.minForeground / 200;
  const size_t required = areaRequired > PIXELS / 20 ? areaRequired : PIXELS / 20;
  if (a.useful < required) return TrainingIssue::TooFewPixels;
  return TrainingIssue::Ready;
}

bool addSample(Prototype &p, const Features &f) {
  if (p.count >= 2000) return false;
  for (size_t i = 0; i < FEATURES; ++i)
    if (!std::isfinite(f.value[i]) || f.value[i] < 0 || f.value[i] > 1.001f)
      return false;
  ++p.count;
  for (size_t i = 0; i < FEATURES; ++i)
    p.mean.value[i] += (f.value[i] - p.mean.value[i]) / p.count;
  return true;
}
bool separable(const Prototype &orange, const Prototype &gray) {
  return !orange.count || !gray.count || distance(orange.mean, gray.mean) >= 0.08f;
}
void SamplingProgress::start(unsigned required, unsigned limit) {
  required_ = required ? required : 1;
  limit_ = limit < required_ ? required_ : limit;
  accepted_ = attempts_ = 0;
}
SampleState SamplingProgress::update(bool usable) {
  if (!required_) return SampleState::TimedOut;
  if (accepted_ >= required_) return SampleState::Complete;
  if (attempts_ >= limit_) return SampleState::TimedOut;
  ++attempts_;
  if (usable) ++accepted_;
  if (accepted_ >= required_) return SampleState::Complete;
  if (attempts_ >= limit_) return SampleState::TimedOut;
  return usable ? SampleState::Accepted : SampleState::Waiting;
}
const char *name(Label label) {
  switch (label) {
    case Label::Calibration: return "sin_calibrar";
    case Label::Empty: return "sin_objeto";
    case Label::Orange: return "naranja";
    case Label::Gray: return "gris";
    case Label::LowLight: return "poca_luz";
    case Label::Error: return "error_camara";
    default: return "indeterminado";
  }
}
Label Stabilizer::update(Label label) {
  if (label != Label::Orange && label != Label::Gray) { reset(); return label; }
  if (label != candidate_) { candidate_ = label; hits_ = 1; }
  else if (hits_ < required_) ++hits_;
  return hits_ >= required_ ? label : Label::Unknown;
}
void Stabilizer::reset() { candidate_ = Label::Unknown; hits_ = 0; }
}  // namespace cats
