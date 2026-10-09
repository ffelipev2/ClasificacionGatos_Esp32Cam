# Binarios para XIAO ESP32S3 Sense

Firmware `3.1-video`, Arduino-ESP32 3.3.2, CPU 240 MHz, flash QIO 80 MHz y PSRAM OPI. El encabezado de la imagen usa DIO para el arranque, como la receta oficial QIO de esta placa.

| Archivo | Dirección |
|---|---|
| `bootloader.bin` | `0x0` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `XIAOS3Gatos-app.bin` | `0x10000` |
| `XIAOS3Gatos-8MB.bin` | `0x0`, imagen completa de 8 MB |

Usa `../cargar.ps1 -Port COMxx` para instalar los cuatro archivos separados y conservar NVS. Usa `-SoloAplicacion` únicamente para actualizar una placa que ya tiene este proyecto y su tabla de particiones.

La imagen completa sobrescribe toda la flash de 8 MB, incluida la calibración. `SHA256SUMS.txt` contiene los hashes de los cinco archivos. Los binarios son para ESP32-S3; el ESP32 clásico usa otro proyecto.
