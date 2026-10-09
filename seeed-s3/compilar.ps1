param([string]$ArduinoCli = "arduino-cli")
$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
$outDir = Join-Path $projectRoot ".build\arduino"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$fqbn = "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,FlashSize=8M,FlashMode=qio,PartitionScheme=default_8MB,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=default,LoopCore=1,EventsCore=0"
& $ArduinoCli compile --fqbn $fqbn --output-dir $outDir (Join-Path $projectRoot "firmware\XIAOS3Gatos")
if ($LASTEXITCODE -ne 0) { throw "Falló la compilación. Instala esp32:esp32 3.3.2 y revisa Arduino CLI." }
Write-Host "Compilación terminada: $outDir"
Write-Host "Los binarios de dist son los entregados; esta compilación queda en .build/arduino."
