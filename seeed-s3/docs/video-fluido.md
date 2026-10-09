# Corrección de la vista entrecortada · 3.1-video

La versión 3.0 consultaba una foto después de cada estado, cada 200 ms. Aunque la placa analizaba ocho imágenes por segundo, el panel tenía un máximo teórico de cinco fotos por segundo, con demoras adicionales por cada solicitud HTTP.

## Cambio

- Una tarea captura JPEG continuamente, sin esperar la decodificación del clasificador. Dos framebuffers de cámara mantienen corta la cola de capturas.
- El analizador toma la última imagen disponible cada 125 ms, con el mismo muestreo, conversión de color y calibración.
- Cinco buffers de vista en PSRAM permiten que escritor, analizador, foto y vídeo usen imágenes independientes. Un buffer leído no se sobrescribe.
- El navegador abre una conexión MJPEG continua al puerto 81. El servidor del puerto 80 sigue disponible para calibración y estado. Consultar el estado no cambia la URL del vídeo.
- El panel consulta el estado cada 500 ms, muestra velocidades separadas de vídeo y reconocimiento, reintenta cuando se corta la conexión y pausa el vídeo al ocultar la pestaña.
- Se mantiene el formato NVS versión 2 y se actualiza solamente la aplicación; la calibración guardada se conserva.

## Comprobación

- Compilación y enlace para Arduino-ESP32 3.3.2 / ESP32-S3 sin advertencias.
- Las 85 comprobaciones del clasificador siguen pasando. Pruebas del pool ampliadas a cinco lectores y lectores simultáneos de una misma imagen.
- 18 comprobaciones del JavaScript en DOM simulado, incluyendo conexión al puerto correcto, URL estable durante consultas de estado y calibración, pausa, reconexión y desconexión. No sustituye una prueba visual real en el teléfono.
- Carga en COM30, respaldo previo de NVS y hash de aplicación verificado.
- Prueba HTTP interna en la placa: **199 imágenes únicas durante ocho segundos, 24,9 FPS, cero duplicados**; respuesta del estado **HTTP 200** con el vídeo abierto y **7,9 análisis/s** durante la prueba. Ver `video-diagnostico-COM30.log`.
- Registro adicional: captura a **27,8 FPS**, reconocimiento a **8,0 análisis/s**, PSRAM **8,0 MB**, IP **192.168.4.1**, cero errores de cámara.

La versión final ajusta el envío del diagnóstico USB al espacio disponible para que no bloquee al analizador. Se recompiló sin advertencias, se volvió a cargar solo la aplicación y se verificó el hash (`carga-video-final-COM30.log`). La nueva prueba recibió **207 imágenes únicas en ocho segundos, 25,9 FPS**, API **HTTP 200**, **8,0 análisis/s** y cero duplicados. El registro muestra ocho mediciones, todas sin errores de cámara, con captura entre **25,8 y 26,8 FPS** y envío entre **25,8 y 26,7 FPS** durante la prueba. Ver `video-diagnostico-final-COM30.log` y `video-mediciones-finales.json`. El ZIP y los binarios corresponden a esta aplicación final.

La prueba interna usa TCP local dentro de la placa y no mide el enlace Wi-Fi, el renderizado del navegador ni la precisión con gatos reales. La velocidad mostrada en el panel distingue captura y envío; debe comprobarse visualmente tras recargar la página en el teléfono. Un único visor de vídeo es la configuración recomendada.

La arquitectura del servidor multipart se basa en el [ejemplo oficial CameraWebServer de Espressif](https://github.com/espressif/arduino-esp32/blob/3.3.2/libraries/ESP32/examples/Camera/CameraWebServer/app_httpd.cpp).
