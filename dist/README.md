# Firmware compilado

`ESP32CAMGatos-4MB.bin` contiene cargador de arranque, tabla de particiones y aplicación. Está compilado para **ESP32-CAM AI Thinker, ESP32 clásico, OV2640, flash de 4 MB y PSRAM**. Usa los valores predeterminados: red `ESP32CAM-Gatos`, contraseña `gatos2026` y panel `http://192.168.4.1`.

Para cambiar credenciales o adaptar otra placa, modifica el código y compila siguiendo el README principal.

## Carga opcional con esptool 5.1.0

La imagen integrada se escribe desde `0x0` y reemplaza todo el contenido de los 4 MB de flash, incluida una calibración anterior. Coloca GPIO0 a GND, reinicia para entrar al modo de programación y sustituye `COM5` por tu puerto real:

```powershell
python -m pip install esptool==5.1.0
python -m esptool --chip esp32 --port COM5 --baud 115200 write-flash 0x0 .\dist\ESP32CAMGatos-4MB.bin
```

Después, quita el puente GPIO0–GND y reinicia. También puedes usar Arduino IDE y cargar desde el código fuente; no necesitas este binario para hacerlo.

`SHA256SUMS.txt` permite comprobar la integridad del archivo. El binario se cargó en la ESP32 de COM17: esptool verificó la escritura y el registro serie confirmó el arranque, la red local y las capturas de cámara. Falta calibrarlo y evaluar el reconocimiento con los gatos reales.

