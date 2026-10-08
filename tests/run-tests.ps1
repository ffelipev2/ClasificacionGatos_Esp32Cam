$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot '.build\tests'
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$source = Join-Path $PSScriptRoot 'classifier_test.cpp'
$classifier = Join-Path $projectRoot 'firmware\ESP32CAMGatos\classifier.cpp'
$executable = Join-Path $buildRoot 'classifier_test.exe'

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    & g++ -std=c++11 -Wall -Wextra -Werror $source $classifier -o $executable
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    Push-Location $buildRoot
    try { & cl /nologo /std:c++14 /EHsc /W4 $source $classifier "/Fe:$executable" } finally { Pop-Location }
} else {
    throw 'Usa g++ en PATH o abre Developer PowerShell for VS para disponer de cl.exe.'
}
if ($LASTEXITCODE -ne 0) { throw 'La compilación de las pruebas falló.' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Fallaron las pruebas.' }

