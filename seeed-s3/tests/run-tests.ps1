$ErrorActionPreference = "Stop"
$projectRoot = Split-Path $PSScriptRoot -Parent
$outDir = Join-Path $projectRoot ".build\tests"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Push-Location $outDir
try {
  $classifierTest = Join-Path $PSScriptRoot "classifier_test.cpp"
  $classifier = Join-Path $projectRoot "firmware\XIAOS3Gatos\classifier.cpp"
  $poolTest = Join-Path $PSScriptRoot "preview_pool_test.cpp"
  if (Get-Command g++ -ErrorAction SilentlyContinue) {
    & g++ -std=c++14 -O2 -Wall -Wextra $classifierTest $classifier -o classifier_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Falló la compilación del clasificador." }
    & g++ -std=c++14 -O2 -Wall -Wextra $poolTest -o preview_pool_test.exe
  } elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    & cl.exe /nologo /std:c++14 /EHsc /W4 $classifierTest $classifier /Fe:classifier_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Falló la compilación del clasificador." }
    & cl.exe /nologo /std:c++14 /EHsc /W4 $poolTest /Fe:preview_pool_test.exe
  } else { throw "Abre Developer PowerShell for Visual Studio o instala g++." }
  if ($LASTEXITCODE -ne 0) { throw "Falló la compilación del pool." }
  & .\classifier_test.exe
  if ($LASTEXITCODE -ne 0) { throw "Fallaron las pruebas del clasificador." }
  & .\preview_pool_test.exe
  if ($LASTEXITCODE -ne 0) { throw "Fallaron las pruebas del pool." }
} finally { Pop-Location }
