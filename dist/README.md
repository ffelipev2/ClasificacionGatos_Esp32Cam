# Firmware compilado: 2.0-calibracion

`ESP32CAMGatos-4MB.bin` contiene cargador de arranque, tabla de particiones y aplicación. Está compilado para **ESP32-CAM AI Thinker, ESP32 clásico, OV2640, flash de 4 MB y PSRAM**. Usa los valores predeterminados: red `ESP32CAM-Gatos`, contraseña `gatos2026` y panel `http://192.168.4.1`.

Para cambiar credenciales o adaptar otra placa, modifica el código y compila siguiendo el README principal.

## Actualizar una placa que ya tiene este proyecto

`ESP32CAMGatos-app.bin` contiene solo la aplicación. Con esptool 5.1.0 y el puerto correcto, por ejemplo COM17:

```powershell
python -m esptool --chip esp32 --port COM17 --baud 460800 write-flash 0x10000 .\dist\ESP32CAMGatos-app.bin
```

Esta opción conserva el fondo y la zona guardados en NVS. Al arrancar por primera vez, la nueva versión borra los dos perfiles antiguos de pelaje, porque cambió el análisis de color; registra ambos gatos otra vez. Usa esta imagen únicamente si la placa ya tiene el cargador y las particiones de este proyecto.

## Carga opcional con esptool 5.1.0

La imagen integrada se escribe desde `0x0` y reemplaza todo el contenido de los 4 MB de flash, incluida una calibración anterior. Coloca GPIO0 a GND, reinicia para entrar al modo de programación y sustituye `COM5` por tu puerto real:

```powershell
python -m pip install esptool==5.1.0
python -m esptool --chip esp32 --port COM5 --baud 115200 write-flash 0x0 .\dist\ESP32CAMGatos-4MB.bin
```

Después, quita el puente GPIO0–GND y reinicia. También puedes usar Arduino IDE y cargar desde el código fuente; no necesitas este binario para hacerlo.

`SHA256SUMS.txt` permite comprobar la integridad de ambas imágenes. La versión 2 se cargó en COM17 escribiendo solo la aplicación; esptool verificó los datos escritos. Tras reiniciar, la placa anunció `2.0-calibracion`, recuperó la calibración de NVS y produjo 18 capturas consecutivas sin errores de cámara. Detalles en `docs/correccion-calibracion.md`.

