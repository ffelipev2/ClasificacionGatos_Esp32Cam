#include <cassert>
#include <cstdio>
#include "../firmware/XIAOS3Gatos/preview_pool.h"

int main() {
  cats::PreviewPool pool;
  size_t bytes = 999;
  uint32_t frame = 999;
  assert(pool.acquire(bytes, frame) == -1 && bytes == 0 && frame == 0);
  assert(!pool.publish(-1, 10, 1));
  const int first = pool.beginWrite();
  assert(first >= 0);
  assert(!pool.publish(first, 0, 1));
  assert(pool.publish(first, 100, 1));
  const int reader1 = pool.acquire(bytes, frame);
  assert(reader1 == first && bytes == 100 && frame == 1);

  const int second = pool.beginWrite();
  assert(second >= 0 && second != reader1);
  assert(pool.publish(second, 200, 2));
  const int reader2 = pool.acquire(bytes, frame);
  assert(reader2 == second && bytes == 200 && frame == 2);
  const int third = pool.beginWrite();
  assert(third >= 0 && third != reader1 && third != reader2);
  assert(pool.publish(third, 300, 3));
  const int reader3 = pool.acquire(bytes, frame);
  assert(reader3 == third && bytes == 300 && frame == 3);
  // Mantener lectores adicionales, como analizador, vídeo y foto simultáneos.
  for (unsigned i = 3; i < cats::PreviewPool::SLOTS; ++i) {
    const int slot = pool.beginWrite();
    assert(slot >= 0 && slot != reader1 && slot != reader2 && slot != reader3);
    assert(pool.publish(slot, 100 * (i + 1), i + 1));
    assert(pool.acquire(bytes, frame) == slot && bytes == 100 * (i + 1) && frame == i + 1);
  }
  // Un cliente que conserva cada buffer agota el pool: se omite la vista,
  // sin sobrescribir una imagen que se está enviando ni parar el análisis.
  assert(pool.beginWrite() == -1);
  pool.release(reader1);
  const int recycled = pool.beginWrite();
  assert(recycled == first);
  pool.abortWrite(recycled);
  assert(!pool.publish(recycled, 400, 4));
  assert(pool.beginWrite() == recycled);
  assert(pool.publish(recycled, 400, 4));
  assert(pool.acquire(bytes, frame) == recycled && bytes == 400 && frame == 4);
  const int extraReader = pool.acquire(bytes, frame);
  assert(extraReader == recycled);
  pool.release(extraReader);  // Sigue ocupado por el lector anterior.
  assert(pool.beginWrite() == -1);

  // Invalidar una captura no libera imágenes que aún tienen lectores.
  pool.invalidate();
  assert(pool.acquire(bytes, frame) == -1 && bytes == 0 && frame == 0);
  assert(pool.beginWrite() == -1);
  pool.release(reader2);
  const int recovered = pool.beginWrite();
  assert(recovered == second);
  assert(pool.publish(recovered, 500, 5));
  assert(pool.acquire(bytes, frame) == recovered && bytes == 500 && frame == 5);
  pool.release(-1);
  pool.abortWrite(99);
  assert(!pool.publish(99, 50, 6));
  std::puts("preview_pool: protección de lectores, reciclaje y recuperación correctos");
}
