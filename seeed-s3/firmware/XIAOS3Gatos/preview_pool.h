#pragma once
#include <stddef.h>
#include <stdint.h>

namespace cats {
// El firmware protege estas operaciones con un mutex. Copiar/enviar los
// bytes se hace fuera del mutex: ningún escritor reutiliza un buffer leído.
class PreviewPool {
 public:
  // Espacio para escritor, analizador, foto, última imagen y dos tareas
  // de vídeo durante el relevo de una conexión antigua por una nueva.
  static constexpr unsigned SLOTS = 6;
  int beginWrite() {
    for (unsigned i = 0; i < SLOTS; ++i) {
      if (int(i) != current_ && !slots_[i].writing && !slots_[i].readers) {
        slots_[i].writing = true;
        return int(i);
      }
    }
    return -1;
  }
  bool publish(int slot, size_t bytes, uint32_t frame) {
    if (!valid(slot) || !slots_[slot].writing || !bytes) return false;
    slots_[slot].bytes = bytes;
    slots_[slot].frame = frame;
    slots_[slot].writing = false;
    current_ = slot;
    return true;
  }
  void abortWrite(int slot) {
    if (valid(slot)) slots_[slot].writing = false;
  }
  int acquire(size_t &bytes, uint32_t &frame) {
    if (current_ < 0) { bytes = 0; frame = 0; return -1; }
    Slot &s = slots_[current_];
    ++s.readers;
    bytes = s.bytes;
    frame = s.frame;
    return current_;
  }
  void release(int slot) {
    if (valid(slot) && slots_[slot].readers) --slots_[slot].readers;
  }
  void invalidate() { current_ = -1; }
 private:
  struct Slot {
    size_t bytes = 0;
    uint32_t frame = 0, readers = 0;
    bool writing = false;
  } slots_[SLOTS];
  int current_ = -1;
  static bool valid(int slot) { return slot >= 0 && slot < int(SLOTS); }
};
}  // namespace cats
