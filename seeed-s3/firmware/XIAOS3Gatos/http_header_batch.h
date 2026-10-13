#pragma once
#include <stddef.h>

namespace cats {

// HTTPD escribe por separado el estado, cada campo y sus separadores.
// Agrupar solo la cabecera evita paquetes diminutos con TCP_NODELAY.
// El JPEG se transmite desde su buffer protegido, sin copiarlo aquí.
class HttpHeaderBatch {
 public:
  void reset() { size_ = 0; complete_ = false; }

  template <typename WriteAll>
  bool write(const char *data, size_t bytes, WriteAll writeAll) {
    if (complete_) return !bytes || writeAll(data, bytes);
    size_t used = 0;
    while (used < bytes) {
      if (size_ == sizeof(header_)) return false;
      header_[size_++] = data[used++];
      if (size_ >= 4 && header_[size_ - 4] == '\r' && header_[size_ - 3] == '\n' &&
          header_[size_ - 2] == '\r' && header_[size_ - 1] == '\n') {
        complete_ = true;
        if (!writeAll(header_, size_)) return false;
        return used == bytes || writeAll(data + used, bytes - used);
      }
    }
    return true;
  }

 private:
  char header_[512];
  size_t size_ = 0;
  bool complete_ = false;
};

}  // namespace cats
