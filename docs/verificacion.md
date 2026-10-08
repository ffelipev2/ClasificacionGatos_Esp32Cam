# Verificación del proyecto

Fecha: 8 de octubre de 2026.

- **Clasificador:** 52 comprobaciones nativas aprobadas, compiladas con Visual C++ en C++14. Incluyen variaciones de brillo y tono, fondo gris vacío, colores desconocidos, calibración incompleta y cambio de resultados sin conservar una clase anterior.
- **Firmware completo:** compilado y enlazado para ESP32 con el SDK instalado Arduino-ESP32 3.3.2 y su compilador Xtensa GCC 14.2.0. Se usaron las opciones oficiales para AI Thinker, QIO de 80 MHz, PSRAM y partición Huge APP. Se compilaron la aplicación y 86 archivos del núcleo, bibliotecas y clasificador.
- **Aplicación:** 1.099.264 bytes de imagen binaria; cabe en la partición de aplicación de 3 MB.
- **RAM interna estática:** 74.988 bytes para las secciones de datos y BSS; no incluye memoria dinámica ni los buffers de cámara. La conversión de imagen y la copia JPEG usan PSRAM.
- **Binario integrado:** 4.194.304 bytes. Checksum y hash de imagen de aplicación válidos según esptool 5.1.0. El SHA-256 del archivo integrado está en `dist/SHA256SUMS.txt`.
- **Panel:** JavaScript comprobado sintácticamente. No había un navegador conectado para verificar su presentación visual.

Arduino CLI encontró una restricción de acceso al resolver directorios de paquetes en este entorno. La compilación se completó invocando directamente el compilador y las opciones de las recetas oficiales del SDK, con una copia temporal dentro del proyecto. No se modificó la instalación de Arduino.

Posteriormente se cargó el firmware en la ESP32 conectada a COM17, con flash de 4 MB. Esptool verificó el hash de los datos escritos. Tras reiniciarla, el registro serie confirmó la creación de la red `ESP32CAM-Gatos`, el panel en `http://192.168.4.1` y 18 capturas analizadas consecutivas durante la observación, sin errores de cámara ni reinicios repetidos.

La placa está en estado `sin_calibrar`, pendiente de registrar el fondo y ambos gatos. No se comprobó el acceso HTTP desde un teléfono, la persistencia de una calibración ni la precisión con gatos reales. La alternativa PlatformIO no se compiló en este entorno. Detalles en `carga.md`.

