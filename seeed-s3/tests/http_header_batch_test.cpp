#include "../firmware/XIAOS3Gatos/http_header_batch.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>

int main() {
  const std::string header = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: 8\r\n"
    "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n"
    "Access-Control-Expose-Headers: X-Frame-Number\r\nX-Frame-Number: 99\r\n\r\n";
  const std::string jpeg("\xff\xd8\r\n\r\n\xff\xd9", 8);
  cats::HttpHeaderBatch batch;
  std::vector<std::string> writes;
  auto write = [&](const char *data, size_t bytes) {
    writes.emplace_back(data, bytes); return true;
  };
  // Cabecera dividida en escrituras de un byte, incluidos los separadores.
  for (size_t i = 0; i < header.size(); ++i) {
    assert(batch.write(header.data() + i, 1, write));
    assert(writes.size() == (i + 1 == header.size() ? 1u : 0u));
  }
  assert(batch.write(jpeg.data(), jpeg.size(), write));
  assert(writes.size() == 2 && writes[0] == header && writes[1] == jpeg);
  // Un cambio en la fragmentación del SDK no debe cambiar los bytes enviados.
  const std::string response = header + jpeg;
  for (size_t split = 0; split <= response.size(); ++split) {
    batch.reset(); writes.clear();
    assert(batch.write(response.data(), split, write));
    assert(batch.write(response.data() + split, response.size() - split, write));
    std::string sent;
    for (const auto &part : writes) sent += part;
    assert(sent == response);
    assert(!writes.empty() && writes[0] == header);
  }
  batch.reset(); writes.clear();
  const std::string tooLong(513, 'x');
  assert(!batch.write(tooLong.data(), tooLong.size(), write));
  assert(writes.empty());
  batch.reset();
  unsigned failedWrites = 0;
  assert(!batch.write(response.data(), response.size(), [&](const char *, size_t) {
    ++failedWrites; return false;
  }));
  assert(failedWrites == 1);  // No enviar el JPEG si falla la cabecera.
  batch.reset(); writes.clear();
  assert(batch.write(response.data(), response.size(), write));
  assert(writes.size() == 2 && writes[0] + writes[1] == response);
  printf("http_header_batch: fragmentación, JPEG binario, límite, fallo y reutilización correctos\n");
}
