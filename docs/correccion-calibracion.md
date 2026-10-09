# Corrección del registro del gato naranja

Versión `2.0-calibracion`, 9 de octubre de 2026.

El registro anterior exigía una proporción mínima de píxeles dentro de un tono naranja fijo y cancelaba toda la sesión con una sola captura rechazada. Además, descartaba el tono de los píxeles de baja saturación, como el pelaje crema, y trataba un canal naranja brillante como si fuera un reflejo blanco.

La corrección:

- Aprende la etiqueta naranja o gris que el usuario selecciona, comprobando iluminación, presencia en la zona y cantidad de píxeles útiles.
- Reúne ocho capturas válidas sin exigir que sean consecutivas, con un límite de 40 intentos y 25 segundos.
- Conserva las capturas buenas y explica el motivo cuando espera otra.
- Conserva el tono del pelaje pálido y admite pelaje brillante, manteniendo el rechazo de reflejos blancos.
- Guarda ambos perfiles incluso si se parecen demasiado. En ese caso avisa que deben mejorarse las muestras y mantiene la clasificación indeterminada.
- Conserva el fondo y los ajustes de la primera versión al actualizar solo la aplicación. Los perfiles de ambos gatos deben aprenderse de nuevo.

Pruebas: 85 comprobaciones aprobadas, firmware completo enlazado e imágenes binarias verificadas.

**Carga completada:** la placa se reconectó a COM17. Se respaldó NVS y se escribió únicamente `dist/ESP32CAMGatos-app.bin` desde `0x10000`; esptool verificó el hash de los datos escritos.

Tras reiniciarla, el registro serie confirmó `Firmware: 2.0-calibracion`, recuperación de la calibración guardada y 18 capturas consecutivas sin errores de cámara. El fondo se conserva; recarga `http://192.168.4.1` y vuelve a registrar ambos gatos para crear sus perfiles con el nuevo análisis. El registro con los gatos reales aún debe realizarlo el usuario.
