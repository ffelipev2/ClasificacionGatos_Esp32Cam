# Más fluidez sin vaciar la imagen · 3.4

El usuario confirmó que 3.3 eliminó la vista negra y pidió más fluidez. La cámara ya capturaba alrededor de 26–28 imágenes/s, mientras el panel tenía un límite de 15. La versión **3.4-vista-fluida**, cargada el **13 de octubre de 2026**, aumenta el objetivo a 30/s y separa la recepción del dibujo para reducir tiempos de espera.

## Cambio

- La placa mantiene JPEG en seis buffers protegidos de PSRAM y continúa capturando mientras analiza o envía otra imagen.
- `/frame?after=N` espera como máximo 100 ms una captura nueva. No retiene un buffer durante la espera; si no aparece otra imagen devuelve HTTP 204.
- La tablet recibe la siguiente foto mientras decodifica y dibuja la anterior. Solo hay una transferencia HTTP activa y una imagen pendiente; una imagen pendiente antigua se reemplaza por la más reciente.
- El intervalo mínimo pasa de 67 a 33 ms y se adapta a la duración de decodificación de la tablet. La velocidad efectiva depende del sensor y del enlace.
- Los errores no borran el canvas. Se mantienen los límites de tiempo, la pausa al ocultar la página, la recuperación al volver y la calibración NVS versión 2. Resolución, calidad JPEG y algoritmo no cambian.

La PSRAM permite mantener imágenes independientes durante captura, análisis y envío. El incremento de esta versión viene de quitar el límite de la vista y solapar tareas; no requiere llenar la RAM con una cola larga de imágenes atrasadas.

## Resultados

Compilación y enlace para Arduino-ESP32 3.3.2 / ESP-IDF 5.5.1 sin advertencias. Aplicación de **1 092 704 bytes**, validada con esptool. Carga de solo aplicación en COM30, respaldo NVS previo y hash escrito verificado.

| Prueba HTTP interna de 60 segundos | 3.3 | 3.4 |
|---|---:|---:|
| Imágenes recibidas completas | 895 | 1 549 |
| Velocidad media | 14,9/s | 25,8/s |
| Números repetidos | 2 | 0 |
| Fallos de imagen | 0 | 0 |
| Respuestas HTTP 200 de la API | 11 | 11 |
| Reconocimiento | 8,0 análisis/s | 8,0 análisis/s |

El incremento de velocidad media es aproximadamente **73 %**. En 3.4 hubo cero respuestas de espera, cero errores del contador de cámara, cero avisos `FB-OVF` y calibración conservada: fondo registrado, ocho muestras naranja y ocho gris. La última medición de captura fue 23,8 FPS; los registros de la prueba incluyen captura variable entre aproximadamente 24 y 28 FPS, por lo que no se promete un valor fijo de 30 FPS.

Pasaron **25 comprobaciones del panel**, incluida recepción durante una decodificación bloqueada, sustitución de la imagen pendiente por la más reciente y respuestas HTTP 204 sin borrar la vista. El navegador real con tamaño de tablet mantuvo la vista ante timeout de estado de 11,2 segundos, HTTP 503, JPEG corruptos y transferencias parciales, sin borrar el canvas ni tener dos fotos en recepción. Con retardos programados de 20 ms en la respuesta y 50 ms en la decodificación se midieron **17,4 FPS**, comprobando el solapamiento.

Las mediciones están en `vista-fluida-mediciones.json` y `vista-fluida-browser.json`; el descriptor de imagen está en `imagen-vista-fluida-verificada.txt`. El ZIP incluye los registros de serie y carga. Las pruebas usan TCP local en la placa y fallos simulados en el navegador; **la velocidad visible en la tablet debe comprobarse con su Wi-Fi real**.

Después de actualizar, recarga **http://192.168.4.1** y comprueba que el pie diga **Vista 3.4**. La velocidad «Vista» mide imágenes dibujadas, no el objetivo configurado. Para repetir la prueba de placa escribe **p** por USB y espera un minuto, sin otro visor abierto.
