# Verificación de la versión XIAO S3

Fecha: 9 de octubre de 2026. Modelo confirmado por el usuario: XIAO ESP32S3 Sense.

## Comprobaciones realizadas

- Compilación y enlace completos para **ESP32-S3** con las herramientas instaladas de Arduino-ESP32 **3.3.2**, SDK ESP-IDF 5.5.1 y GCC Xtensa 14.2.0. Se compilaron de nuevo el core, las bibliotecas y la aplicación para S3; no se reutilizaron objetos del ESP32 clásico.
- Se ejecutaron directamente las recetas del compilador y enlazador oficial. Arduino CLI no pudo resolver sus directorios en este entorno restringido; `compilar.ps1` queda como alternativa para un entorno normal con ese core instalado.
- Pines contrastados con el ejemplo oficial `CAMERA_MODEL_XIAO_ESP32S3`. Configuración de flash de 8 MB, PSRAM OPI y USB Hardware CDC.
- **85 comprobaciones del clasificador**: naranja, naranja pálido y brillante, gris, fondo vacío, otros colores, poca luz, imágenes inválidas, muestras válidas intercaladas con malas, agotamiento de intentos y estabilidad temporal.
- **Pool de imágenes**: lector conservado durante varias publicaciones, agotamiento sin sobrescritura, reciclaje, aborto de escritura e invalidación y recuperación de capturas.
- **11 comprobaciones del JavaScript del panel** en un DOM simulado: falta de calibración, botones, FPS, progreso, motivos de espera, naranja reconocido, cambio de zona y desconexión. También se verificó su sintaxis. No equivale a una inspección visual en navegador real.
- Sintaxis PowerShell de los tres scripts sin errores.
- Script de carga ejecutado con una herramienta simulada: un chip incompatible y flash de 4 MB detienen la carga antes de escribir; instalación y actualización de solo aplicación usan las regiones esperadas; un binario alterado se rechaza antes de acceder al puerto. La simulación no accedió a hardware.
- Binarios de aplicación y bootloader generados con esptool 5.1.0. `image-info` verifica chip ESP32-S3, flash de 8 MB, checksum y hash de validación.

La aplicación conserva la lógica de calibración que el usuario confirmó operativa en la ESP32-CAM. La nueva placa requiere registrar su propio fondo y ambos gatos.

## Carga y comprobación en la placa

La placa apareció en **COM30** y esptool confirmó **ESP32-S3 QFN56 revisión 0.2**, PSRAM integrada de **8 MB**, flash de **8 MB**, USB Serial/JTAG y MAC `10:b4:1d:e8:23:84`. Los registros están en `placa-identificada.txt` y `carga-COM30.log`.

Se respaldaron los 20 480 bytes de NVS antes de escribir. El respaldo queda en `../.backups/` y no se incluye en el ZIP. Se cargaron bootloader, particiones y aplicación; esptool verificó sus hashes.

La comprobación USB posterior registró siete mediciones durante 14 segundos, con **8,0 análisis/s** y **cero errores de captura** en todas ellas. Decodificación: 62,5–62,9 ms; análisis: 3,8–3,9 ms. El contador pasó de la imagen 176 a la 272, avanzando 16 imágenes por cada intervalo de dos segundos. Ver `arranque-COM30.log`. Las capturas válidas también comprueban que la cámara y la memoria de imágenes se inicializaron.

El resultado `sin_calibrar` es el esperado en una placa nueva. No se aprendieron etiquetas con imágenes arbitrarias: corresponde registrar el fondo y los gatos reales.

La aplicación 3.0 añadió PSRAM e IP al diagnóstico USB y evitó compartir el mensaje del clasificador con el arranque de Wi-Fi. Se recompiló, se actualizó solo la aplicación y se verificó nuevamente su hash (`carga-final-COM30.log`). Esa comprobación registró seis mediciones durante 12 segundos: **8,0 análisis/s**, **PSRAM 8,0 MB**, **IP 192.168.4.1**, **cero errores**, decodificación de 62,3–63,0 ms y análisis de 3,8–3,9 ms. Los datos históricos están en `diagnostico-final-COM30.log` y `mediciones-finales.json`. La versión actual del ZIP es **3.1-video**; ver `video-fluido.md`.

## Comprobaciones pendientes con el montaje

Después de cargar:

1. Abrir el panel en la red XIAO-S3-Gatos y comprobar vista y zona.
2. Registrar fondo, naranja y gris, incluyendo posiciones que no se usaron para aprender.
3. Revisar FPS, tiempos y `cameraErrors`, primero sin navegador y luego con el panel abierto. Sin navegador se puede consultar el registro USB, que se emite cada dos segundos cuando el monitor está disponible.
4. Reiniciar y comprobar que recupera la calibración.
5. Comparar con la ESP32-CAM usando el mismo encuadre y luz. Medir velocidad y aciertos con gatos reales; los datos sintéticos no prueban esa precisión.
