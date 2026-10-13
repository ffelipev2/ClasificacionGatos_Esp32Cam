# Envío de fotos al teléfono · 3.5

El usuario confirmó **Vista 3.4** y observó **7–14 FPS en el teléfono**. Una lectura de serie mostró captura de **27,8 FPS**, reconocimiento de **8 análisis/s** y cero errores de cámara. Esto sitúa el límite observado después de la captura, pero no identifica por sí solo si corresponde a Wi-Fi o a decodificación del navegador.

## Cambio

La versión **3.5-envio-fluido**, cargada el **13 de octubre de 2026**, mantiene la vista en canvas, los seis buffers JPEG en PSRAM y la cola acotada del navegador. Conserva resolución 320 × 240, calidad JPEG 12, algoritmo, ajustes y formato NVS versión 2.

El [código oficial de HTTPD en ESP-IDF 5.5.1](https://raw.githubusercontent.com/espressif/esp-idf/v5.5.1/components/esp_http_server/src/httpd_txrx.c) escribe el estado y la cabecera base, después cada campo, separador y valor por separado. Con los cuatro campos de esta ruta son **18 llamadas para la cabecera**. Ahora se agrupa en un buffer fijo de 512 bytes y se escribe como un solo bloque. El JPEG sigue enviándose directamente desde su buffer protegido, sin otra copia completa.

Se activa **TCP_NODELAY** en los sockets de fotos. [Espressif documenta esta opción](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-guides/lwip.html) para desactivar Nagle. Agrupar la cabecera evita sustituir las esperas por muchas escrituras diminutas. La posible interacción entre segmentos cortos y los ACK del teléfono fue una hipótesis de coste de transferencia. Después de cargar 3.5, el usuario informó **15–21 FPS en el teléfono**, frente a **7–14 FPS** con Vista 3.4. Se registra la mejora observada; no se atribuye todo el rango a una causa única.

El límite de cinco segundos comprende toda la foto, también los envíos parciales. Un fallo cierra la respuesta y libera el buffer; el panel conserva la última imagen. No se amplían las colas ni se cambia la cámara.

El panel muestra **Cámara (FPS), Recepción (ms), Dibujo (ms) y Foto (KB)**. Recepción incluye la petición HTTP, la espera de una captura nueva y la recepción completa. Dibujo mide decodificación y dibujo en el dispositivo. Son promedios suavizados; se solapan. Las métricas anteriores de análisis y decodificación siguen correspondiendo a la placa.

El pie dice **Versión 3.5** para distinguir el número de versión del indicador **Vista … FPS**. Recarga **http://192.168.4.1** después de actualizar y deja un solo visor abierto.

## Verificación

- Compilación y enlace con Arduino-ESP32 3.3.2, sin advertencias; aplicación de **1 093 488 bytes**. Imagen completa de 8 MB, bytes de aplicación y SHA256 comprobados.
- Carga de solo aplicación en **COM30**, con respaldo NVS previo y hash de escritura verificado. La calibración conserva fondo y ocho muestras de cada gato.
- Prueba HTTP interna de 60 segundos: **1 605 imágenes únicas, 26,7/s**, cero duplicados, cero esperas HTTP 204, cero fallos, once respuestas correctas de la API y reconocimiento de **8,0/s**. Cero errores de cámara y cero avisos FB-OVF.
- Las 85 comprobaciones del clasificador, las pruebas de buffers/JPEG y las nuevas pruebas de cabecera pasaron. Estas últimas cubren fragmentación del terminador, JPEG binario, todas las divisiones de una respuesta de ejemplo, exceso de capacidad, fallo de escritura y reutilización.
- Pasaron **28 comprobaciones del panel**, incluidos los nuevos tiempos, recuperación, concurrencia y cola acotada.
- Navegador Edge real con viewport de tablet: cero vaciados de canvas o errores JavaScript, una sola recepción activa y recuperación ante una consulta de estado bloqueada 11,2 s, HTTP 503, JPEG corruptos y transferencias parciales. Con retardos programados de 20 ms en recepción y 50 ms en decodificación mantuvo **17,4 FPS**.

- Navegador con viewport de **teléfono 430 × 932**: panel sin desbordamiento horizontal, métricas visibles, **17,9 FPS** con los mismos retardos programados y conservación de la imagen ante los fallos anteriores. Cero errores JavaScript y una recepción activa como máximo.
- Comprobación del usuario en el teléfono: **15–21 FPS en 3.5**, frente a **7–14 FPS en 3.4**.

La media interna anterior era 25,8/s. Estos valores usan TCP local, luz y carga variables; no representan el enlace Wi-Fi del teléfono. El rango de FPS reportado por el usuario corresponde a una observación del panel; no es una prueba comparativa automatizada.

Los datos están en [envio-fluido-mediciones.json](envio-fluido-mediciones.json), [envio-tablet-browser.json](envio-tablet-browser.json), [envio-telefono-browser.json](envio-telefono-browser.json) e [imagen-envio-fluido-verificada.txt](imagen-envio-fluido-verificada.txt). El ZIP incluye los registros de carga y serie; excluye respaldos NVS y credenciales locales.

Para repetir las pruebas:

~~~powershell
.\tests\run-tests.ps1
node .\tests\web_ui_test.cjs
# Requiere Playwright y un navegador Chromium instalado.
node .\tests\preview_browser_test.cjs --phone
~~~

La prueba de placa se inicia escribiendo **p** por USB y esperando un minuto, sin otros visores. No cambia la calibración ni la red del computador.
