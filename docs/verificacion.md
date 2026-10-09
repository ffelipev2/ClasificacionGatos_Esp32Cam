# Verificación del proyecto

Actualización: 9 de octubre de 2026. Firmware `2.0-calibracion`.

- **Clasificador y registro:** 85 comprobaciones nativas aprobadas con Visual C++ en C++14, sin advertencias. Incluyen naranja pálido, naranja brillante, tonos con brillo similar al gris, conservación de capturas buenas entre capturas descartadas, fin de sesión sin muestras y éxito en el último intento permitido. También cubren fondo vacío, colores desconocidos y estabilidad temporal.
- **Firmware:** aplicación y clasificador recompilados y enlazados con el núcleo y bibliotecas Arduino-ESP32 3.3.2, usando Xtensa GCC 14.2.0 y las opciones oficiales para AI Thinker, PSRAM, QIO de 80 MHz y partición Huge APP.
- **Aplicación:** 1.101.424 bytes. La imagen integrada ocupa 4.194.304 bytes. Esptool 5.1.0 confirmó checksum y hash válidos de la aplicación; los SHA-256 de ambos archivos están en `dist/SHA256SUMS.txt`.
- **Panel:** JavaScript válido sintácticamente; muestra capturas válidas y el motivo de espera. Presentación visual no verificada en navegador.
- **Migración:** el formato persistido mantiene el mismo tamaño. La versión 2 acepta una calibración de versión 1 válida, conserva el fondo y los ajustes y borra los perfiles de pelaje que usaban el histograma antiguo. Tras cargar solo la aplicación y reiniciar, la placa recuperó la calibración desde NVS; el área de primer plano distinta de cero confirma que dispone del fondo guardado.

La primera versión sí se cargó en COM17: esptool verificó la escritura, y se observaron la red local y 18 capturas de cámara consecutivas tras el reinicio. Ese registro corresponde a la versión 1, no valida físicamente la versión 2.

La placa se reconectó durante el trabajo. Se respaldaron 20.480 bytes de NVS en `.build/backups/2026-10-09-COM17-nvs-v1.bin` y se cargó la aplicación en `0x10000`, conservando la memoria NVS. Esptool verificó el hash de los datos escritos.

Después del reinicio, se observó `Firmware: 2.0-calibracion`, la red local, el mensaje de recuperación de calibración y 18 capturas consecutivas durante diez segundos, sin errores de cámara ni reinicios repetidos. Los nuevos perfiles naranja y gris están pendientes de que el usuario los registre con sus gatos. La precisión real requiere validar el montaje. No se compiló la alternativa PlatformIO.
