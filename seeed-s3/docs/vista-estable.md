# Vista estable en tablets · 3.3

La versión **3.3-vista-estable** conserva la imagen durante los reintentos de conexión y los errores de estado, evitando que la vista se vacíe. Se verificó y cargó el **13 de octubre de 2026**, conservando la calibración.

## Cambio

El panel anterior vaciaba el elemento de imagen al reintentar el vídeo o cuando una consulta de estado agotaba su timeout de 10 segundos. El panel nuevo solicita JPEG completos a `/frame` en el puerto 81, con una sola imagen pendiente y un intervalo mínimo de 67 ms, aproximadamente 15 imágenes/s. Cada imagen se decodifica antes de reemplazar la anterior en un canvas. Los errores y las imágenes incompletas conservan lo ya dibujado y muestran un aviso de reconexión.

La consulta de estado es independiente de las imágenes. El panel no fuerza renovaciones periódicas y pausa las peticiones al ocultar la página. Usa `createImageBitmap` cuando está disponible, con alternativa mediante `Image` para tablets antiguas. Las URL temporales se liberan. El reconocimiento sigue ejecutándose en la placa; no cambian el algoritmo, los modelos, los ajustes ni el formato NVS.

Se conserva `/stream` como diagnóstico MJPEG, con su límite de sesión de 35 segundos. El panel habitual usa `/frame`.

## Verificación

- Compilación y enlace con las herramientas oficiales Arduino-ESP32 3.3.2 / ESP-IDF 5.5.1. Aplicación: **1 090 432 bytes**. Checksum y hash interno de imagen válidos; hashes de los cinco binarios actualizados.
- Carga de solo aplicación en **COM30**, después de identificar ESP32-S3 y flash de 8 MB y respaldar NVS. esptool confirmó el hash escrito.
- Prueba HTTP interna de un minuto: **895 JPEG completos, 14,9 solicitudes/s**, dos números repetidos, cero fallos y **11 respuestas HTTP 200 de la API**. La interfaz omite imágenes cuyo número se repite.
- Reconocimiento durante esa prueba: **8,0 análisis/s**, cero errores del contador de cámara y cero avisos `FB-OVF`. Fondo registrado y **8 muestras de naranja / 8 de gris** conservadas.
- **18 comprobaciones del JavaScript**: persistencia de la imagen, timeout de estado, fallos de red, dimensiones incorrectas, compatibilidad de decodificación, imágenes repetidas, una sola petición activa, pausa y restauración de la página.
- Navegador **Microsoft Edge real**, sin ventana visible, con viewport de tablet **768 × 1024**. Se inyectaron una consulta de estado bloqueada por **11,2 segundos**, respuestas HTTP 503, JPEG corruptos y una respuesta JPEG parcial que agotó el timeout. La vista conservó píxeles opacos y recuperó imágenes; no hubo llamadas para borrar el canvas, errores JavaScript ni más de una imagen en recepción.

Las mediciones están en `vista-estable-mediciones.json` y `vista-estable-browser.json`; `imagen-vista-estable-verificada.txt` describe el binario. Los registros de carga y serie se incluyen en el ZIP. Los respaldos NVS y las credenciales locales se excluyen.

El navegador de prueba usa imágenes sintéticas y fallos de red simulados; la prueba de placa usa TCP local. **Queda pendiente comprobar el resultado en la tablet del usuario y su enlace Wi-Fi real.** No se cambió la red del computador.

La revisión también detectó que el `platformio.ini` histórico usa Arduino 2.0.17, cuyo SDK no dispone de las funciones HTTP asíncronas utilizadas. Se marcó explícitamente esa incompatibilidad. La ruta verificada es Arduino IDE/CLI con core 3.3.2; queda pendiente migrar la configuración PlatformIO si se desea usarla.

## Repetir las pruebas

Desde la raíz del repositorio, con Node.js y Microsoft Edge instalado:

```powershell
node .\seeed-s3\tests\web_ui_test.cjs
npm install --prefix .build/browser-tests playwright@1.64.0 --no-audit --no-fund
node .\seeed-s3\tests\preview_browser_test.cjs
```

Para otro navegador Chromium, establece `PREVIEW_BROWSER` con su ejecutable. La prueba crea un perfil temporal y servidores HTTP locales; no usa el perfil personal del navegador. Para probar la placa por USB, escribe **p** en el monitor serie y espera aproximadamente un minuto hasta `PREVIEWTEST`, sin otro visor abierto.

Después de actualizar, abre **http://192.168.4.1** y recarga la página. El pie debe indicar **Vista 3.3**. Si conservas una pestaña antigua, seguirá usando la interfaz anterior hasta recargarla.
