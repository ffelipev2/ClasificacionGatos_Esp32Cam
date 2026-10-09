# Recuperación del vídeo congelado · 3.2

El usuario informó que se congelaba el vídeo mientras el panel seguía respondiendo. La versión 3.1 mantenía un manejador HTTP ocupado durante toda la transmisión. Una conexión antigua podía retrasar una nueva; el navegador dependía de avisos de error o de que el contador de clientes llegara a cero para reintentar.

## Corrección

- Peticiones de vídeo asíncronas con dos tareas como máximo, para relevar una conexión antigua. El último visor abierto tiene prioridad. La API permanece en su servidor separado.
- Presupuesto de un segundo para enviar una imagen completa, incluidas las escrituras parciales. Un cliente que deja de leer libera la petición y sus buffers.
- Finalización de las peticiones y cierre del transporte desde la tarea HTTPD, para evitar reutilizar un descriptor mientras aún lo usa un trabajador.
- Detección en el panel de tres segundos sin progreso del vídeo, aunque la conexión siga abierta. Renovación cada 30 segundos para recuperar también problemas de renderizado. Pausa y recuperación al ocultar/regresar a la pestaña, restaurar la página o recuperar Wi-Fi.
- Expiración de cada sesión del servidor a los 35 segundos. El panel la renueva; la URL directa requiere abrirla de nuevo después de ese límite.
- Seis buffers de vista en PSRAM, para proteger imágenes durante el relevo de conexiones, el análisis y las fotos.
- Buffers de cámara ampliados de 15 360 a unos 96 000 bytes: se inicializa en SVGA para reservar memoria y luego se configura el sensor en QVGA. El vídeo sigue siendo 320 × 240.
- Validación de segmentos JPEG y dimensiones antes de decodificar. Si hay imágenes concatenadas, se toma la última completa. Se respetan miniaturas en segmentos APP, escapes de datos y marcadores de reinicio.
- Se mantiene el clasificador, los ajustes y el formato NVS versión 2. Se carga solo la aplicación, con respaldo previo de NVS.

## Prueba en la placa conectada

Compilación para Arduino-ESP32 3.3.2 / ESP-IDF 5.5.1 y carga en COM30. El hash de la aplicación se verificó durante la escritura. La imagen es exclusiva de ESP32-S3 y ocupa **1 084 224 bytes**; el encabezado, checksum y hash interno se comprobaron con esptool. Ver `imagen-recuperacion-verificada.txt`.

La prueba USB **r** abre un visor, lo mantiene abierto al pedir el siguiente, recibe vídeo y consulta la API; después deja de leer sin cerrar TCP. Repite durante un minuto y comprueba también una conexión larga hasta su expiración. Se ejecutó junto con la prueba **t**, sin reiniciar entre ambas, en un registro de 125 segundos:

| Comprobación | Resultado |
|---|---|
| Relevos con el visor anterior abierto | 11 correctos; máximo 23 ms hasta respuesta HTTP |
| API durante los relevos | 11 respuestas HTTP 200 |
| Clientes detenidos | 11 expiraciones por timeout; cero trabajadores restantes al terminar |
| Sesión larga | Cierre después de 34 999 ms |
| Fallos de la prueba de recuperación | 0 |
| Vídeo, prueba de ocho segundos | 205 imágenes únicas, 25,6 FPS, cero duplicados |
| Reconocimiento durante esa prueba | 8,0 análisis/s |
| Errores del contador de cámara | 0 |
| Calibración recuperada | Fondo registrado, 8 muestras naranja y 8 gris |
| RAM interna mínima durante recuperación | 138 624 bytes libres |

Antes de ampliar los buffers se observaron diez avisos `FB-OVF` en una prueba equivalente. El registro final con buffers ampliados no muestra esos avisos. No se observaron retrocesos en el contador de imágenes durante el registro final. Las capturas JPEG concatenadas no aparecieron en esta ejecución (`merged=0`); su extracción se verifica con pruebas sintéticas.

Los respaldos NVS de antes de las actualizaciones tienen el mismo SHA256, y los contadores de calibración se conservaron en la ejecución final. Los respaldos permanecen fuera del repositorio y del ZIP.

Los resultados están en `recuperacion-mediciones.json`. El ZIP incluye `recuperacion-final-COM30.log` y `carga-recuperacion-COM30.log`. El uso de TCP local comprueba el servidor, las tareas y sus recursos; no mide el enlace Wi-Fi ni el renderizado del teléfono y no sustituye una prueba de uso prolongada.

## Pruebas del código

- 85 comprobaciones del clasificador, sin cambios en el algoritmo.
- Protección, reciclaje y recuperación del pool con seis buffers.
- JPEG truncados, concatenados, miniaturas, dimensiones incorrectas, escapes, reinicios, varios escaneos y 10 000 entradas malformadas para verificar límites.
- 16 comprobaciones de la interfaz: continuidad sin reconexiones innecesarias, vídeo detenido con socket abierto, renovación periódica, Wi-Fi interrumpido, pestaña oculta y regreso a una página restaurada.

Las pruebas de interfaz usan un DOM simulado. Después de actualizar, recarga el panel real para recibir la nueva lógica de recuperación.

## Referencias

- [Espressif: API HTTP asíncrona y envío por sesión](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/protocols/esp_http_server.html).
- [Espressif: inicialización grande y reducción de la imagen en CameraWebServer](https://github.com/espressif/arduino-esp32/blob/3.3.2/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino).
- [Espressif: reserva de buffers JPEG y avisos del driver](https://github.com/espressif/esp32-camera/blob/master/driver/cam_hal.c).
