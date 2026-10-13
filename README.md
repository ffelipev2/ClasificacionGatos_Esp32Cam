# Clasificación de pelaje naranja y gris con ESP32

Proyecto autónomo para distinguir el pelaje de un gato naranja y uno gris mediante una cámara. El análisis y la calibración funcionan en la placa; un teléfono permite ver la imagen y registrar las muestras, sin computador, servidor ni internet durante el uso.

Hay dos proyectos independientes. Elige el correspondiente a tu placa:

| Placa | Versión | Proyecto | Instrucciones |
|---|---|---|---|
| ESP32-CAM AI Thinker, OV2640 y PSRAM | `2.0-calibracion` | [firmware/ESP32CAMGatos](firmware/ESP32CAMGatos) | [Manual ESP32-CAM](docs/esp32cam.md) |
| Seeed XIAO ESP32S3 Sense, 8 MB de flash y PSRAM OPI | `3.5-envio-fluido` | [seeed-s3](seeed-s3) | [Manual XIAO Sense](seeed-s3/README.md) |

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

Sustituye `COM30` por el puerto de tu placa. Para una instalación nueva, usa el mismo comando sin `-SoloAplicacion`. El script verifica chip, flash y hashes de los binarios, respalda NVS y carga el firmware. La actualización de **3.0–3.4 a 3.5 conserva los registros de calibración**.

## Vista estable en la versión S3

La versión `3.5-envio-fluido` optimiza el envío al teléfono y conserva las mejoras de fluidez y recuperación:

- Captura JPEG continuamente, mientras otra tarea analiza la última imagen disponible cada 125 ms.
- Reserva unos 96 KB en PSRAM por framebuffer de cámara, en lugar de 15 KB, manteniendo la imagen en 320 × 240. Si una captura contiene varias imágenes JPEG, envía y analiza solo la última completa, con dimensiones verificadas.
- Usa seis buffers de vista en PSRAM para evitar sobrescribir imágenes que se están leyendo o enviando, también durante una reconexión.
- El panel recibe JPEG completos al **puerto 81**, con un objetivo máximo de **30 imágenes/s**, limitado por el sensor, Wi-Fi y la tablet. Hay una sola transferencia HTTP activa.
- Recibe la siguiente foto mientras decodifica y dibuja la anterior. Guarda únicamente la foto pendiente más reciente y adapta el ritmo al tiempo de decodificación.
- Agrupa la cabecera HTTP de cada foto y activa TCP_NODELAY para evitar esperas de envío de segmentos cortos.
- Muestra los tiempos de recepción y dibujo, además de la captura en la placa, para localizar el límite de fluidez.
- Pide un número de imagen posterior al último recibido; el servidor espera brevemente una captura nueva y evita enviar duplicados.
- Dibuja cada imagen válida en un canvas y conserva la anterior mientras recibe la siguiente. Las imágenes incompletas, los cortes de red y los errores de estado no vacían la vista.
- Mantiene el panel y la calibración en el **puerto 80**. Una consulta de estado que tarda 10 segundos no detiene las imágenes.
- Pausa al ocultar la página y vuelve a pedir imágenes al regresar. Ya no renueva periódicamente la conexión de vista del panel.

En 3.4, la prueba interna de un minuto recibió **1 549 imágenes únicas, 25,8 FPS**, frente a **14,9 FPS** de la versión 3.3: aproximadamente **73 % más**. El reconocimiento mantuvo **8 análisis/s**, con cero fallos de imagen y calibración conservada. El navegador siguió conservando la imagen ante consultas de estado bloqueadas, JPEG corruptos y transferencias parciales. Ver la [verificación del envío de 3.5](seeed-s3/docs/envio-fluido.md) y la [verificación de 3.4](seeed-s3/docs/vista-fluida.md). La prueba interna usa TCP local; el resultado visible en la tablet depende de su Wi-Fi y navegador.

En **3.5**, la prueba interna recibió **1 605 imágenes únicas, 26,7 FPS**, sin fallos y con calibración conservada. Este dato usa TCP local. En el teléfono del montaje, el usuario reportó **15–21 FPS** tras actualizar a 3.5, frente a **7–14 FPS** con 3.4. Los indicadores «Recepción» y «Dibujo» permiten seguir comprobando el rendimiento.

La [verificación de 3.2](seeed-s3/docs/recuperacion-video.md) conserva el registro de la recuperación del servidor MJPEG.

Para el uso habitual abre **http://192.168.4.1** y comprueba que el pie diga **Versión 3.5**. La foto directa está en **http://192.168.4.1:81/frame**. Se conserva el flujo MJPEG en `/stream` para diagnóstico; esa sesión termina después de 35 segundos y requiere volver a abrirla. Se recomienda un visor a la vez.

## ESP32-CAM AI Thinker

La versión `2.0-calibracion` corrige el aprendizaje demasiado estricto del naranja pálido y permite reunir imágenes válidas aunque haya capturas malas entre ellas. Su intervalo objetivo de análisis es **500 ms**.

Abre [ESP32CAMGatos.ino](firmware/ESP32CAMGatos/ESP32CAMGatos.ino), selecciona **AI Thinker ESP32-CAM**, flash **4 MB** y partición **Huge APP**. Después de cargar, conecta el teléfono a **ESP32CAM-Gatos**, contraseña **gatos2026**, y abre **http://192.168.4.1**.

Las conexiones GPIO0–GND para programar, alimentación, calibración y actualización están en el [manual completo](docs/esp32cam.md). Los binarios de cada placa son distintos: usa la carpeta correspondiente.

## Descargas y archivos

| Contenido | ESP32-CAM AI Thinker | XIAO ESP32S3 Sense |
|---|---|---|
| Proyecto completo | [ESP32CAM-Gatos.zip](ESP32CAM-Gatos.zip) | [XIAO-ESP32S3-Sense-Gatos.zip](XIAO-ESP32S3-Sense-Gatos.zip) |
| Binarios e instrucciones de carga | [dist](dist/README.md) | [seeed-s3/dist](seeed-s3/dist/README.md) |
| Configuración PlatformIO | [platformio.ini](platformio.ini) | [Referencia histórica; usa Arduino IDE/CLI](seeed-s3/platformio.ini) |

La compilación verificada usa **Arduino-ESP32 3.3.2**. Para la XIAO utiliza Arduino IDE o CLI; su configuración PlatformIO antigua necesita migrarse al core actual, como se explica en el [manual](seeed-s3/README.md#compilación-y-pruebas). Para usar una red Wi-Fi propia, copia el `config.local.example.h` del firmware elegido como `config.local.h`; las credenciales locales están excluidas de Git.

## Pruebas

Con g++ en PATH o desde Developer PowerShell for Visual Studio:

```powershell
# ESP32-CAM: clasificador
.\tests\run-tests.ps1

# XIAO Sense: clasificador, protección de buffers y validación JPEG
.\seeed-s3\tests\run-tests.ps1

# XIAO Sense: recuperación del panel (requiere Node.js)
node .\seeed-s3\tests\web_ui_test.cjs
```

Las **85 comprobaciones del clasificador** cubren naranja pálido y brillante, gris, fondos, colores desconocidos, poca luz, muestras intercaladas y estabilidad temporal. Las pruebas de la S3 comprueban también lectores simultáneos y recuperación de buffers. Son pruebas sintéticas: la precisión con gatos reales debe comprobarse en el montaje, con posiciones distintas de las usadas para aprender.

Los registros de ejecución se incluyen en los ZIP de cada versión. Los respaldos NVS y las credenciales locales quedan fuera del repositorio y de esos paquetes.

## Referencias

- [Seeed: XIAO ESP32S3 Sense y cámara](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/).
- [Espressif: driver esp32-camera](https://github.com/espressif/esp32-camera).
- [Espressif: ejemplo CameraWebServer](https://github.com/espressif/arduino-esp32/tree/3.3.2/libraries/ESP32/examples/Camera/CameraWebServer).
