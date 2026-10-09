# XIAO ESP32S3 Sense · Gato naranja o gris

Versión independiente del proyecto ESP32-CAM, optimizada para **Seeed Studio XIAO ESP32S3 Sense**, con **8 MB de flash y 8 MB de PSRAM OPI**. Todo el análisis ocurre en la placa, sin computador ni servicios externos. Conserva la calibración flexible del naranja pálido que funciona en la versión anterior.

## Qué cambia

| Característica | ESP32-CAM anterior | XIAO ESP32S3 Sense |
|---|---|---|
| Intervalo objetivo de análisis | 500 ms | 125 ms, hasta 8 análisis/s |
| Decodificación RGB565 | 320 × 240, 153 600 bytes | 160 × 120, 38 400 bytes |
| Vista del navegador | Fotos periódicas, JPEG 320 × 240 | Vídeo MJPEG continuo, 320 × 240 |
| Cámara y servidor | En el mismo bucle | Captura y análisis independientes; vídeo y API en servidores separados |
| Buffers de vista | Uno | Seis en PSRAM, protegidos durante lectura y envío |
| Consulta del estado | Cada 750 ms | Cada 500 ms, independiente del vídeo |
| Confirmación de una etiqueta | Tres imágenes consecutivas | Tres imágenes consecutivas |

**Versión 3.1-video:** reemplaza las fotos solicitadas cada 200 ms —un máximo teórico de 5 imágenes/s— por una conexión MJPEG continua en el puerto 81. La captura funciona al ritmo del sensor y el analizador toma las últimas imágenes cada 125 ms. El puerto 80 sigue atendiendo el panel, la calibración y los ajustes durante el vídeo. Recarga la página después de actualizar para recibir la interfaz nueva.

**Versión actual 3.2-recuperacion-video:** corrige la recuperación de un vídeo congelado. Las peticiones de vídeo se atienden en tareas asíncronas; una nueva conexión puede reemplazar la anterior sin ocupar el servidor HTTP. El último visor tiene prioridad. Un cliente que no consigue recibir una imagen completa en un segundo se desconecta y libera sus buffers. El panel detecta tres segundos sin imágenes enviadas, reintenta y renueva la conexión cada 30 segundos, incluso si el navegador no emite un error. También recupera el vídeo al regresar a la pestaña o recuperar Wi-Fi. Mantiene el análisis y la calibración de 3.1.

La cámara reserva unos **96 KB por framebuffer en PSRAM**, en lugar de los 15 KB que asigna el driver al iniciar directamente en QVGA. Se inicializa en SVGA para reservar ese espacio y luego se configura el sensor en **320 × 240**, siguiendo el patrón del ejemplo oficial. Cada captura se valida antes de enviarla y decodificarla: si contiene varios JPEG concatenados, se usa únicamente la última imagen completa con las dimensiones correctas. El clasificador mantiene su buffer pequeño y la misma zona de muestreo.

La [verificación de 3.2](docs/recuperacion-video.md) incluye reconexiones con un visor anterior abierto, clientes que dejan de leer y expiración de sesiones. La prueba interna usa TCP de bucle local dentro de la placa: **no mide la fluidez ni el rendimiento Wi-Fi del teléfono**. El panel distingue la velocidad del vídeo y del reconocimiento; la velocidad visible depende también del enlace inalámbrico y del navegador.

Los buffers grandes aprovechan la PSRAM. La imagen pequeña de análisis usa RAM interna cuando hay espacio, con PSRAM como alternativa. La cuadrícula de color sigue teniendo 40 × 30 puntos. La flash de 8 MB usa la partición oficial `default_8MB`: dos espacios de aplicación de unos 3,2 MB y un espacio de archivos reservado. Este firmware no implementa actualización por Wi-Fi ni usa todavía ese espacio de archivos.

## Carga con Arduino IDE

1. Instala **esp32 by Espressif Systems 3.3.2**.
2. Abre `firmware/XIAOS3Gatos/XIAOS3Gatos.ino`, conservando todos los archivos vecinos.
3. Selecciona **XIAO_ESP32S3** y estas opciones:

   | Opción | Valor |
   |---|---|
   | CPU Frequency | 240 MHz |
   | Flash Size | 8 MB |
   | Flash Mode | QIO 80 MHz |
   | PSRAM | **OPI PSRAM** |
   | Partition Scheme | Default with spiffs (3MB APP/1.5MB SPIFFS) |
   | USB Mode | Hardware CDC and JTAG |
   | USB CDC On Boot | Enabled |
   | Arduino Runs On | Core 1 |
   | Events Run On | Core 0 |

4. Conecta la XIAO por USB-C con un cable de datos, selecciona su puerto y carga.
5. Si no aparece o no entra al cargador, mantén **BOOT**, pulsa y suelta **RESET** y luego suelta BOOT. Vuelve a elegir el puerto si cambia. Después de cargar, pulsa RESET.

No necesita puente GPIO0–GND ni adaptador UART externo. La placa debe tener instalada la expansión Sense con la cámara y su antena Wi-Fi conectada. Usa una alimentación USB estable. No requiere microSD.

## Binarios ya compilados

En PowerShell, desde esta carpeta, con el paquete ESP32 de Arduino instalado:

```powershell
.\cargar.ps1 -Port COM18
```

Sustituye `COM18` por el puerto real. El script verifica el chip **ESP32-S3**, comprueba la flash y los hashes de los binarios, respalda NVS y carga bootloader, particiones y aplicación. Escribe solo esas regiones y conserva NVS. Para una actualización de este mismo proyecto:

```powershell
.\cargar.ps1 -Port COM18 -SoloAplicacion
```

`dist/XIAOS3Gatos-8MB.bin` es una imagen completa para instalar desde `0x0`; **sobrescribe también NVS**, por lo que el script usa los archivos separados para preservar la calibración. El firmware es exclusivo de S3: no cargues estos binarios en la ESP32-CAM AI Thinker.

## Uso

1. Conecta el teléfono a **XIAO-S3-Gatos**, contraseña **gatos2026**. Mantén la conexión aunque indique que no tiene internet.
2. Abre **http://192.168.4.1**.
3. Ajusta el rectángulo verde para cubrir el pelaje con la cámara fija y buena luz blanca.
4. Registra **fondo vacío → gato naranja → gato gris**. Cada registro reúne ocho imágenes válidas separadas al menos 250 ms, normalmente unos dos segundos. Descarta imágenes malas y conserva las buenas hasta 80 intentos o 25 segundos.
5. Puedes añadir más registros de cada gato en sus posiciones habituales. La calibración se guarda en NVS.

Es necesario calibrar esta placa desde cero: no importa automáticamente los datos de la ESP32-CAM. Cambiar zona, sensibilidad, ubicación o iluminación requiere repetir la calibración.

El sistema compara **color de pelaje en una escena fija**; no incorpora un detector neuronal de gatos. Una mano o tela de color parecido puede coincidir. Los perfiles demasiado similares producen «Indeterminado» y se pueden volver a aprender. Las pruebas sintéticas no miden precisión con gatos reales.

La actualización de 3.0 o 3.1 a 3.2 conserva el formato de calibración, el algoritmo y los ajustes. Usa `-SoloAplicacion` para mantener los registros guardados. Recarga la página para activar la recuperación automática.

Para conectarla a tu red de 2,4 GHz, copia `config.local.example.h` como `config.local.h` y completa `WIFI_SSID` y `WIFI_PASSWORD`. La IP se imprime por USB a 115200 baudios. Si no conecta en 15 segundos, crea su propia red.

## Compilación y pruebas

Con Arduino CLI y el core indicado instalados:

```powershell
.\compilar.ps1
```

También incluye `platformio.ini` para `seeed_xiao_esp32s3`, PSRAM OPI y flash de 8 MB. La alternativa PlatformIO usa espressif32 6.13.0 / Arduino 2.0.17 y **no fue compilada en esta sesión**; los binarios entregados se verificaron con Arduino-ESP32 3.3.2.

En Developer PowerShell for Visual Studio, o con g++ en PATH:

```powershell
.\tests\run-tests.ps1

# Recuperación de la interfaz; requiere Node.js.
node .\tests\web_ui_test.cjs
```

El registro de comprobación inicial está en `docs/verificacion.md`, la mejora de fluidez en `docs/video-fluido.md` y la recuperación en `docs/recuperacion-video.md`. La API conserva `/capture`, `/api/status`, `/api/learn`, `/api/settings` y `/api/reset`; las acciones se encolan y responden `202`. Consulta `busy` y `message` para conocer su finalización. `/api/status` incluye `fps` para reconocimiento, `videoFps` para captura, `streamFps` para envío de vídeo, `streamClients`, `streamPort`, `videoFrame`, `videoAgeMs`, `previewDrops`, tiempos y memoria. La versión 3.2 añade `streamFrame`, `streamAgeMs`, `streamStarts` y `streamTimeouts` para detectar progreso y diagnosticar reconexiones, y `mergedJpegs` para contar capturas con varias imágenes.

El flujo se abre en `http://192.168.4.1:81/stream`. Se recomienda un visor de vídeo a la vez. El panel renueva la conexión automáticamente; si abres el flujo directo, termina a los 35 segundos y debes volver a abrirlo.

Para una comprobación técnica por USB, escribe **t** en el monitor serie: ejecuta una prueba HTTP interna de ocho segundos, consulta el estado durante el vídeo y muestra un resultado `SELFTEST`. Escribe **r** para probar relevos de conexiones, clientes que dejan de leer y una sesión larga; tarda aproximadamente 100 segundos y muestra `RECONNECTTEST` y `RECONNECTHEAP`. Haz las pruebas sin otro visor abierto; no modifican la calibración ni cambian la red del computador.

## Referencias oficiales

- [Seeed: XIAO ESP32S3 Sense y memoria](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/).
- [Seeed: cámara, pines y PSRAM](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/).
- [Espressif: esp32-camera, JPEG y buffers](https://github.com/espressif/esp32-camera).
- [Espressif: pines XIAO en CameraWebServer](https://github.com/espressif/arduino-esp32/blob/3.3.2/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h).
- [Espressif: peticiones HTTP asíncronas](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/protocols/esp_http_server.html).
