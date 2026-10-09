#pragma once
#include <stddef.h>
#include <stdint.h>

namespace cats {
struct JpegFrame {
  size_t offset = 0, bytes = 0;
  unsigned images = 0;
};

// Algunas capturas del sensor pueden contener varios JPEG concatenados.
// Respetar las longitudes de segmentos (incluidas miniaturas en APP) y los
// escapes FF00/reinicios del escaneo; tomar solo la última imagen completa.
// Verificar dimensiones antes de decodificar en el buffer fijo RGB565.
inline size_t jpegLength(const uint8_t *data, size_t size, unsigned width, unsigned height) {
  if (!data || size < 4 || data[0] != 0xff || data[1] != 0xd8) return 0;
  size_t pos = 2;
  bool scanning = false, hasScan = false, dimensions = false;
  while (pos < size) {
    if (scanning) while (pos < size && data[pos] != 0xff) ++pos;
    if (pos >= size || data[pos++] != 0xff) return 0;
    while (pos < size && data[pos] == 0xff) ++pos;
    if (pos >= size) return 0;
    const uint8_t marker = data[pos++];
    if (scanning && (marker == 0 || (marker >= 0xd0 && marker <= 0xd7))) continue;
    if (marker == 0xd9) return hasScan && dimensions ? pos : 0;
    if (marker == 0 || marker == 0xd8 || (marker >= 0xd0 && marker <= 0xd7)) return 0;
    if (marker == 1) continue;  // TEM: marcador independiente, sin longitud.
    scanning = false;
    if (size - pos < 2) return 0;
    const size_t length = (size_t(data[pos]) << 8) | data[pos + 1];
    if (length < 2 || length > size - pos) return 0;
    if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
      if (length < 8) return 0;
      const unsigned h = (unsigned(data[pos + 3]) << 8) | data[pos + 4];
      const unsigned w = (unsigned(data[pos + 5]) << 8) | data[pos + 6];
      if (w != width || h != height) return 0;
      dimensions = true;
    }
    if (marker == 0xda) {
      if (!dimensions || length < 6) return 0;
      scanning = hasScan = true;
    }
    pos += length;
  }
  return 0;
}

inline JpegFrame latestJpeg(const uint8_t *data, size_t size, unsigned width, unsigned height) {
  JpegFrame result;
  if (!data) return result;
  size_t offset = 0;
  while (offset < size) {
    const size_t bytes = jpegLength(data + offset, size - offset, width, height);
    if (!bytes) break;  // Conservar la imagen completa anterior si la cola se truncó.
    result.offset = offset;
    result.bytes = bytes;
    ++result.images;
    offset += bytes;
  }
  return result;
}
}  // namespace cats
