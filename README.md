# Clasificación de pelaje naranja y gris con ESP32

Proyecto autónomo para distinguir el pelaje de un gato naranja y uno gris mediante una cámara. El análisis y la calibración funcionan en la placa; un teléfono permite ver la imagen y registrar las muestras, sin computador, servidor ni internet durante el uso.

Hay dos proyectos independientes. Elige el correspondiente a tu placa:

| Placa | Versión | Proyecto | Instrucciones |
|---|---|---|---|
| ESP32-CAM AI Thinker, OV2640 y PSRAM | `2.0-calibracion` | [firmware/ESP32CAMGatos](firmware/ESP32CAMGatos) | [Manual ESP32-CAM](docs/esp32cam.md) |
| Seeed XIAO ESP32S3 Sense, 8 MB de flash y PSRAM OPI | `3.1-video` | [seeed-s3](seeed-s3) | [Manual XIAO Sense](seeed-s3/README.md) |

El sistema aprende colores dentro de una zona fija de la imagen. Supone que un solo gato ocupa esa zona; una mano, tela u objeto de color parecido también puede coincidir. No incorpora un detector neuronal de gatos ni identifica individuos de igual color.

## Inicio rápido: XIAO ESP32S3 Sense

1. Instala **esp32 by Espressif Systems 3.3.2** en Arduino IDE.
2. Abre [XIAOS3Gatos.ino](seeed-s3/firmware/XIAOS3Gatos/XIAOS3Gatos.ino) y selecciona **XIAO_ESP32S3**, CPU **240 MHz**, flash **8 MB**, **OPI PSRAM**, partición **Default 8 MB**, USB **Hardware CDC and JTAG** y **USB CDC On Boot habilitado**. Las opciones completas están en el [manual](seeed-s3/README.md#carga-con-arduino-ide).
3. Carga por USB-C, conecta el teléfono a **XIAO-S3-Gatos**, contraseña **gatos2026**, y abre **http://192.168.4.1**.
4. Fija la cámara, ajusta la zona verde y registra **fondo vacío → gato naranja → gato gris** con iluminación blanca y constante.
5. Después de una actualización, **recarga la página** para recibir la interfaz nueva.

Cada registro reúne ocho imágenes válidas; las malas se descartan conservando las buenas. La calibración se guarda en NVS y se recupera al reiniciar.

### Actualizar una XIAO que ya tiene el proyecto

Desde la raíz de este repositorio, con las herramientas ESP32 de Arduino instaladas:

```powershell
.\seeed-s3\cargar.ps1 -Port COM30 -SoloAplicacion
```

Sustituye `COM30` por el puerto de tu placa. Para una instalación nueva, usa el mismo comando sin `-SoloAplicacion`. El script verifica chip, flash y hashes de los binarios, respalda NVS y carga el firmware. La actualización de **3.0 a 3.1 conserva los registros de calibración**.

## Vídeo continuo en la versión S3

La versión `3.1-video` corrige la vista entrecortada de las fotos solicitadas cada 200 ms, que tenía un máximo teórico de cinco imágenes por segundo:

- Captura JPEG continuamente, mientras otra tarea analiza la última imagen disponible cada 125 ms.
- Usa cinco buffers de vista en PSRAM para evitar sobrescribir imágenes que se están leyendo o enviando.
- Mantiene una conexión **MJPEG en el puerto 81** y atiende el panel y la calibración en el **puerto 80**.
- Muestra velocidades separadas del vídeo y del reconocimiento, reintenta la conexión y pausa el vídeo cuando se oculta la pestaña.

En la prueba HTTP interna realizada en una XIAO conectada se recibieron **207 imágenes únicas en ocho segundos: 25,9 FPS**, con **8,0 análisis por segundo**, respuesta de la API y cero errores de cámara. Esa prueba usa TCP local dentro de la placa; la fluidez visible en el teléfono también depende del Wi-Fi y del navegador. El [registro de verificación](seeed-s3/docs/video-fluido.md) explica el método y sus límites.

El flujo directo está en **http://192.168.4.1:81/stream**. Se recomienda un visor de vídeo a la vez.

## ESP32-CAM AI Thinker

La versión `2.0-calibracion` corrige el aprendizaje demasiado estricto del naranja pálido y permite reunir imágenes válidas aunque haya capturas malas entre ellas. Su intervalo objetivo de análisis es **500 ms**.

Abre [ESP32CAMGatos.ino](firmware/ESP32CAMGatos/ESP32CAMGatos.ino), selecciona **AI Thinker ESP32-CAM**, flash **4 MB** y partición **Huge APP**. Después de cargar, conecta el teléfono a **ESP32CAM-Gatos**, contraseña **gatos2026**, y abre **http://192.168.4.1**.

Las conexiones GPIO0–GND para programar, alimentación, calibración y actualización están en el [manual completo](docs/esp32cam.md). Los binarios de cada placa son distintos: usa la carpeta correspondiente.

## Descargas y archivos

| Contenido | ESP32-CAM AI Thinker | XIAO ESP32S3 Sense |
|---|---|---|
| Proyecto completo | [ESP32CAM-Gatos.zip](ESP32CAM-Gatos.zip) | [XIAO-ESP32S3-Sense-Gatos.zip](XIAO-ESP32S3-Sense-Gatos.zip) |
| Binarios e instrucciones de carga | [dist](dist/README.md) | [seeed-s3/dist](seeed-s3/dist/README.md) |
| Configuración PlatformIO | [platformio.ini](platformio.ini) | [seeed-s3/platformio.ini](seeed-s3/platformio.ini) |

La compilación verificada usa **Arduino-ESP32 3.3.2**. Las configuraciones PlatformIO incluyen su versión de plataforma y sus límites de comprobación en cada manual. Para usar una red Wi-Fi propia, copia el `config.local.example.h` del firmware elegido como `config.local.h`; las credenciales locales están excluidas de Git.

## Pruebas

Con g++ en PATH o desde Developer PowerShell for Visual Studio:

```powershell
# ESP32-CAM: clasificador
.\tests\run-tests.ps1

# XIAO Sense: clasificador y protección de buffers
.\seeed-s3\tests\run-tests.ps1
```

Las **85 comprobaciones del clasificador** cubren naranja pálido y brillante, gris, fondos, colores desconocidos, poca luz, muestras intercaladas y estabilidad temporal. Las pruebas de la S3 comprueban también lectores simultáneos y recuperación de buffers. Son pruebas sintéticas: la precisión con gatos reales debe comprobarse en el montaje, con posiciones distintas de las usadas para aprender.

Los registros de ejecución se incluyen en los ZIP de cada versión. Los respaldos NVS y las credenciales locales quedan fuera del repositorio y de esos paquetes.

## Referencias

- [Seeed: XIAO ESP32S3 Sense y cámara](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/).
- [Espressif: driver esp32-camera](https://github.com/espressif/esp32-camera).
- [Espressif: ejemplo CameraWebServer](https://github.com/espressif/arduino-esp32/tree/3.3.2/libraries/ESP32/examples/Camera/CameraWebServer).
