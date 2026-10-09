#include <cassert>
#include <cstdio>
#include <vector>
#include "../firmware/XIAOS3Gatos/jpeg_frame.h"

int main() {
  // SOF0 + SOS con escape FF00, marcador de reinicio y EOI.
  const std::vector<uint8_t> frame = {
    0xff,0xd8,
    0xff,0xc0,0,11,8,0,240,1,64,1,1,0x11,0,
    0xff,0xda,0,8,1,1,0,0,63,0,
    1,2,0xff,0,0xd9,3,0xff,0xd0,4,0xff,0xd9
  };
  assert(cats::jpegLength(frame.data(), frame.size(), 320, 240) == frame.size());
  assert(!cats::jpegLength(frame.data(), frame.size(), 640, 480));
  assert(!cats::jpegLength(nullptr, 100, 320, 240));
  assert(!cats::latestJpeg(nullptr, 100, 320, 240).bytes);
  for (size_t size = 0; size < frame.size(); ++size)
    assert(!cats::jpegLength(frame.data(), size, 320, 240));
  std::vector<uint8_t> joined = frame;
  joined.insert(joined.end(), frame.begin(), frame.end());
  joined.insert(joined.end(), frame.begin(), frame.end());
  auto selected = cats::latestJpeg(joined.data(), joined.size(), 320, 240);
  assert(selected.images == 3 && selected.offset == frame.size() * 2 && selected.bytes == frame.size());
  joined.pop_back();  // La tercera imagen quedó incompleta.
  selected = cats::latestJpeg(joined.data(), joined.size(), 320, 240);
  assert(selected.images == 2 && selected.offset == frame.size() && selected.bytes == frame.size());
  // Una miniatura con SOI/EOI dentro de APP no termina la imagen principal.
  std::vector<uint8_t> thumbnail = {0xff,0xd8,0xff,0xe1,0,8,0xff,0xd8,1,2,0xff,0xd9};
  thumbnail.insert(thumbnail.end(), frame.begin() + 2, frame.end());
  assert(cats::jpegLength(thumbnail.data(), thumbnail.size(), 320, 240) == thumbnail.size());
  auto bad = frame;
  bad[4] = 0xff; bad[5] = 0xff;  // Segmento mayor que el buffer.
  assert(!cats::jpegLength(bad.data(), bad.size(), 320, 240));
  bad[4] = 0; bad[5] = 1;  // Longitud inválida.
  assert(!cats::jpegLength(bad.data(), bad.size(), 320, 240));
  bad = frame; bad[10] = 0;  // Dimensión diferente de la imagen fija.
  assert(!cats::jpegLength(bad.data(), bad.size(), 320, 240));
  // Varios escaneos, como en JPEG progresivo.
  auto scans = frame;
  scans.resize(scans.size() - 2);
  scans.insert(scans.end(), frame.begin() + 15, frame.end());
  assert(cats::jpegLength(scans.data(), scans.size(), 320, 240) == scans.size());
  // Bytes malformados no deben producir vistas fuera del buffer.
  uint32_t random = 12345;
  std::vector<uint8_t> noise(256);
  for (unsigned sample = 0; sample < 10000; ++sample) {
    for (auto &byte : noise) { random = random * 1664525u + 1013904223u; byte = uint8_t(random >> 24); }
    noise[0] = 0xff; noise[1] = 0xd8;
    const auto view = cats::latestJpeg(noise.data(), noise.size(), 320, 240);
    assert(view.offset <= noise.size() && view.bytes <= noise.size() - view.offset);
  }
  std::puts("jpeg_frame: límites, dimensiones, escaneos y selección de la última imagen correctos");
}
