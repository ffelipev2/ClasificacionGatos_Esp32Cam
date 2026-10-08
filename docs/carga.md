# Carga en la ESP32-CAM conectada

- Puerto: **COM17**, adaptador USB-SERIAL CH340.
- Chip confirmado: ESP32-D0WD, revisión 1.0.
- Memoria flash detectada: **4 MB**.
- Firmware: `dist/ESP32CAMGatos-4MB.bin`.
- SHA-256: `3feb45a1e4a53a89d1de04251e5ed116338061a15c0bca828e42357c196ea481`.
- Escritura con esptool 5.1.0 a 460800 baudios, desde `0x0`.
- Resultado: carga completa y **hash de los datos escritos verificado**.

Tras el reinicio se observó el puerto serie a 115200 baudios durante diez segundos. La placa anunció:

```text
Red: ESP32CAM-Gatos
Panel: http://192.168.4.1
Registra el fondo vacío y después ambos gatos.
```

Se recibieron **18 capturas analizadas consecutivas**, todas en estado `sin_calibrar`. No hubo errores de cámara ni reinicios repetidos durante esa observación. El aviso inicial de NVS `state NOT_FOUND` es esperado al no existir aún una calibración guardada.

Para usarla, conecta el teléfono a `ESP32CAM-Gatos` con la contraseña `gatos2026`, abre `http://192.168.4.1` y registra el fondo vacío, el pelaje naranja y el gris, en ese orden. La precisión de la clasificación y la persistencia de esas muestras aún requieren comprobarse con el montaje real.

