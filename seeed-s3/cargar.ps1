param(
  [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]+$')][string]$Port,
  [string]$Esptool = "",
  [ValidateRange(115200,921600)][int]$Baud = 460800,
  [switch]$SoloAplicacion
)
$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
if (!$Esptool) {
  $installed = Get-Command esptool.exe -ErrorAction SilentlyContinue
  if ($installed) { $Esptool = $installed.Source }
  else {
    $toolRoot = Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools\esptool_py"
    if (Test-Path -LiteralPath $toolRoot) {
      $candidates = Get-ChildItem -LiteralPath $toolRoot -Directory |
        Sort-Object { [version]$_.Name } -Descending
      foreach ($candidate in $candidates) {
        $exePath = Join-Path $candidate.FullName "esptool.exe"
        if (Test-Path -LiteralPath $exePath) { $Esptool = $exePath; break }
      }
    }
  }
}
if (!$Esptool -or !(Test-Path -LiteralPath $Esptool)) {
  throw "No se encontró esptool.exe. Instala esp32 3.3.2 en Arduino IDE o usa -Esptool con su ruta."
}
# Se valida todo antes de escribir en la placa.
$dist = Join-Path $projectRoot "dist"
$files = @("XIAOS3Gatos-app.bin")
if (!$SoloAplicacion) { $files += @("bootloader.bin", "partitions.bin", "boot_app0.bin") }
$hashLines = Get-Content -LiteralPath (Join-Path $dist "SHA256SUMS.txt")
foreach ($file in $files) {
  $filePath = Join-Path $dist $file
  $hash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
  $expected = $hashLines | Where-Object { $_ -match ('^[a-f0-9]{64}  ' + [regex]::Escape($file) + '$') }
  if (!$expected -or $expected.Substring(0,64) -ne $hash) { throw "Hash inválido: $file" }
}
$common = @("--chip", "esp32s3", "--port", $Port, "--baud", "$Baud", "--connect-attempts", "3")
$preflight = $common + @("--after", "no-reset")
# --chip esp32s3 rechaza chips ESP32 clásicos antes de escribir.
$chip = & $Esptool @preflight chip-id 2>&1
$chipCode = $LASTEXITCODE
$chip | Out-Host
if ($chipCode -ne 0) { throw "No se pudo identificar una ESP32-S3. Revisa puerto y BOOT/RESET." }
$flash = & $Esptool @preflight flash-id 2>&1
$flashCode = $LASTEXITCODE
$flash | Out-Host
if ($flashCode -ne 0 -or (($flash -join "`n") -notmatch 'Detected flash size:\s*(8|16|32)MB')) {
  throw "No se confirmó una flash de al menos 8 MB. No se cargó el firmware."
}
$backupDir = Join-Path $projectRoot ".backups"
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$backup = Join-Path $backupDir ((Get-Date -Format "yyyyMMdd-HHmmss") + "-$Port-nvs.bin")
& $Esptool @preflight read-flash 0x9000 0x5000 $backup
if ($LASTEXITCODE -ne 0) { throw "No se pudo respaldar NVS. Se detuvo la carga." }
if ($SoloAplicacion) {
  & $Esptool @common write-flash 0x10000 (Join-Path $dist "XIAOS3Gatos-app.bin")
} else {
  & $Esptool @common write-flash --flash-mode dio --flash-freq 80m --flash-size 8MB `
    0x0 (Join-Path $dist "bootloader.bin") `
    0x8000 (Join-Path $dist "partitions.bin") `
    0xe000 (Join-Path $dist "boot_app0.bin") `
    0x10000 (Join-Path $dist "XIAOS3Gatos-app.bin")
}
if ($LASTEXITCODE -ne 0) { throw "Falló la escritura. Revisa el mensaje de esptool." }
Write-Host "Carga terminada. Respaldo NVS: $backup"
Write-Host "Pulsa RESET y abre http://192.168.4.1 en la red XIAO-S3-Gatos."
