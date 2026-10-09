# ESP32-CAM AI Thinker · Pelaje naranja y gris

Las rutas y los comandos de este manual parten de la raíz del repositorio.

Proyecto autónomo para **ESP32-CAM AI Thinker con cámara OV2640 y PSRAM**. Captura imágenes, aprende el aspecto del pelaje naranja y gris, y muestra el resultado en una página alojada en la propia placa. No necesita computador, servidor ni internet después de cargar el programa. Un teléfono sirve para ver la cámara y calibrar.

**Alcance:** es un clasificador de color calibrado para una escena fija, no un detector de la especie «gato». Supone que un solo gato entra en la zona de análisis; una mano, tela u objeto parecido puede coincidir. Tampoco identifica individuos de igual color. Para usarlo, por ejemplo, junto a un comedero, coloca la cámara de forma que el torso del gato ocupe la zona marcada.

## Inicio rápido con Arduino IDE

1. Abre `firmware/ESP32CAMGatos/ESP32CAMGatos.ino`. Mantén todos los archivos de esa carpeta juntos.
2. Instala **esp32 by Espressif Systems** en el gestor de placas. El proyecto se verifica con la versión **3.3.2**.
3. Selecciona **AI Thinker ESP32-CAM**, flash de **4 MB** y una partición de aplicación de **3 MB / Huge APP** si el menú permite elegirla. Mantén la **PSRAM habilitada**; algunas versiones la fijan al seleccionar esta placa.
4. Selecciona el puerto del adaptador USB y carga el programa siguiendo las conexiones de abajo. Si cuesta cargarlo, usa 115200 baudios.
5. Quita el puente GPIO0–GND y reinicia la placa.
6. Conecta el teléfono a la red **ESP32CAM-Gatos**, contraseña **gatos2026**. Aunque el teléfono indique «sin internet», conserva esa conexión.
7. Abre **http://192.168.4.1** y registra, en este orden: **fondo vacío → gato naranja → gato gris**.

La dirección efectiva y los resultados también aparecen por el puerto serie a **115200 baudios**. Cambia la contraseña de la red en `config.local.h` si la vas a usar de forma permanente.

## Conexiones para cargar el programa

Necesitas la ESP32-CAM, un adaptador USB–UART con **señales de 3,3 V** o una base ESP32-CAM-MB, y una alimentación de 5 V estable. Una fuente de 5 V / 1 A es una opción práctica; la corriente que suministran algunos adaptadores es insuficiente.

| Adaptador o fuente | ESP32-CAM |
|---|---|
| TX, señal de 3,3 V | U0R / GPIO3 |
| RX | U0T / GPIO1 |
| GND | GND |
| Alimentación de 5 V | 5V |
| Puente para programar | GPIO0 a GND |

Con alimentación externa, une las tierras y usa una sola fuente para la línea de 5 V. No conectes señales UART de 5 V a los pines de datos. Pon GPIO0 a GND y pulsa RESET antes de cargar; retira ese puente y pulsa RESET después. La base ESP32-CAM-MB puede gestionar este paso según su modelo.

## Calibración y uso

- Fija la cámara y usa iluminación blanca, constante y suficiente. Evita contraluz y luz naranja intensa. El flash blanco de la placa permanece apagado.
- Ajusta el rectángulo verde para que cubra principalmente el pelaje, evitando la cara, el suelo y las paredes. Al guardar ajustes se borra la calibración porque cambia la región comparada.
- **Registrar fondo:** deja la zona vacía unos cuatro segundos. Se promedian ocho capturas. Registrar otro fondo borra los perfiles naranja y gris.
- **Aprender naranja / Aprender gris:** coloca el pelaje correspondiente en la zona. El sistema aprende la etiqueta que seleccionas, incluido el naranja pálido; no exige un tono predefinido. Cada registro añade ocho capturas válidas. Las imágenes malas se descartan conservando las buenas, durante un máximo de 40 intentos o 25 segundos. Evita introducir manos o ropa en el rectángulo.
- Si faltan capturas, el panel indica el motivo: poca luz, pelaje fuera de la zona o reflejos. Ajusta el encuadre según ese mensaje. Los perfiles similares se guardan, pero se advierte que necesitan mejores muestras antes de poder distinguirlos.
- Añade varios registros por gato con posiciones habituales, incluyendo sus rayas y zonas claras. No es necesario que el gato permanezca en movimiento durante el reconocimiento.
- La calibración se guarda en la memoria NVS y se recupera al reiniciar. Cambiar la posición de la cámara o la iluminación requiere volver a calibrar.

| Resultado | Significado |
|---|---|
| Pelaje naranja | Coincide con el perfil naranja durante tres imágenes consecutivas |
| Pelaje gris | Coincide con el perfil gris durante tres imágenes consecutivas |
| Zona vacía | La diferencia respecto del fondo ocupa menos del área mínima |
| Indeterminado | Color desconocido, poca evidencia, empate o resultado aún provisional |
| Falta calibración | Faltan muestras del fondo o de algún color |
| Muy poca luz | La imagen es demasiado oscura para analizarla |
| Error de cámara | No se pudo capturar o convertir la imagen |

El intervalo de captura objetivo es 500 ms. La velocidad real depende de la placa y de las consultas al panel. La salida no conserva una identificación anterior cuando la escena pasa a vacía, desconocida o error.

## Wi-Fi existente, opcional

Copia `config.local.example.h` como `config.local.h`, dentro de la carpeta del firmware:

```cpp
#pragma once
#define WIFI_SSID "TuRed2.4GHz"
#define WIFI_PASSWORD "TuClave"
#define AP_SSID "ESP32CAM-Gatos"
#define AP_PASSWORD "gatos2026"
```

Si `WIFI_SSID` está vacío, la placa crea su red propia. Si hay credenciales, intenta conectarse durante 15 segundos y, si falla, crea la red local. Consulta la IP por el monitor serie. El análisis funciona en la placa aunque no haya ningún navegador abierto. Las credenciales locales están excluidas de Git.

## PlatformIO, alternativa

Abre esta carpeta como proyecto de PlatformIO:

```powershell
pio run
pio run --target upload
pio device monitor
```

`platformio.ini` fija **espressif32 6.13.0**, placa `esp32cam` y framework Arduino. Esta plataforma utiliza Arduino-ESP32 **2.0.17**. No agregues una segunda biblioteca `esp32-camera`: ya forma parte del framework. La compilación de esta alternativa depende de tener PlatformIO y sus paquetes instalados; la validación local del firmware se realiza con Arduino-ESP32 3.3.2.

## Cómo funciona

1. Captura JPEG de 320 × 240 y lo convierte a RGB565 en PSRAM.
2. Muestrea una cuadrícula de 40 × 30 puntos dentro de la región elegida.
3. Compara cada punto con el fondo calibrado y conserva el primer plano. Ignora sombras profundas y reflejos saturados.
4. Extrae un histograma de tonos ponderado por saturación, conservando el color del pelaje pálido, además de brillo y proporciones de color.
5. Compara esas características con los promedios aprendidos para las etiquetas seleccionadas, exige separación y rechaza coincidencias débiles. Si ambos perfiles se parecen demasiado, muestra indeterminado y permite repetir las muestras.
6. Confirma una clase únicamente tras tres capturas consecutivas.

No hay un modelo neuronal preentrenado ni una estimación estadística de confianza. Los umbrales de rechazo son reglas prácticas que deben validarse con tus gatos y tu montaje. Un fondo de color parecido al gato puede ocultar parte del pelaje; reduce la zona, cambia el fondo o ajusta la sensibilidad.

## Actualización desde la primera versión

La versión `2.0-calibracion` corrige el registro demasiado estricto del pelaje naranja. Para una placa que ya tiene este proyecto, escribe únicamente `dist/ESP32CAMGatos-app.bin` desde `0x10000`, siguiendo `dist/README.md`. Así se conserva la memoria NVS. La actualización recupera el fondo y la zona anteriores; vuelve a registrar ambos gatos porque cambió el histograma de color. La imagen completa de 4 MB sigue disponible para una instalación nueva.

## API local

| Método y ruta | Uso |
|---|---|
| `GET /` | Panel de cámara y calibración |
| `GET /capture` | Última captura JPEG analizada |
| `GET /api/status` | Resultado, área ocupada, antigüedad de imagen, muestras y ajustes |
| `POST /api/learn?label=fondo` | Registrar fondo y borrar los perfiles anteriores al terminar |
| `POST /api/learn?label=naranja` | Añadir muestras naranjas |
| `POST /api/learn?label=gris` | Añadir muestras grises |
| `POST /api/settings` | Guardar zona y sensibilidad; cuerpo de formulario |
| `POST /api/reset` | Borrar calibración |

Los valores de `label` en `/api/status` son `naranja`, `gris`, `sin_objeto`, `indeterminado`, `sin_calibrar`, `poca_luz` y `error_camara`. Las acciones de calibración responden `202` y avanzan en la placa; consulta `busy`, `progress`, `total` y `message` para conocer el resultado. El panel está pensado para una red local de confianza.

## Pruebas y comprobación con los gatos

Las pruebas nativas usan el mismo `classifier.cpp` que el firmware. Con g++ en PATH o Developer PowerShell for Visual Studio:

```powershell
.\tests\run-tests.ps1
```

Comprueban escenas sintéticas: naranja/gris, variaciones de brillo, exclusión del fondo, fondo gris vacío, colores desconocidos, sombras/reflejos, calibración incompleta, datos inválidos y estabilidad temporal. **Estas pruebas no miden precisión con gatos reales.**

Después de calibrar, prueba posiciones que no hayas usado para aprender: al menos diez escenas por gato y diez escenas vacías. Revisa también manos y objetos de colores similares. Registra los resultados en `docs/validacion.csv`; si hay confusiones, mejora iluminación y encuadre y vuelve a calibrar.

## Archivos principales

- `firmware/ESP32CAMGatos/app.cpp`: cámara, Wi-Fi, servidor, calibración y memoria.
- `firmware/ESP32CAMGatos/classifier.cpp`: extracción de características y clasificación.
- `firmware/ESP32CAMGatos/web_ui.h`: interfaz local sin dependencias externas.
- `firmware/ESP32CAMGatos/config.h`: valores predeterminados de red y captura.
- `tests/classifier_test.cpp`: pruebas del algoritmo.

## Referencias técnicas

- [Driver oficial esp32-camera](https://github.com/espressif/esp32-camera): captura JPEG y procesamiento con PSRAM.
- [Conversión oficial de JPEG a RGB565](https://github.com/espressif/esp32-camera/blob/v2.0.4/conversions/to_bmp.c): disposición de los bytes del formato decodificado.
- [Ejemplo oficial CameraWebServer y pines AI Thinker](https://github.com/espressif/arduino-esp32/tree/2.0.17/libraries/ESP32/examples/Camera/CameraWebServer).
- [Placa AI Thinker ESP32-CAM en PlatformIO](https://docs.platformio.org/en/stable/boards/espressif32/esp32cam.html).
- [Versiones de la plataforma Espressif32](https://github.com/platformio/platform-espressif32/releases).
