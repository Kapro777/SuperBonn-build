# Compila e roda os testes dos shims no host mingw (sem NDK, sem jogo).
# Uso: powershell scripts\rt\test-shims.ps1
$ErrorActionPreference = 'Continue'
Set-Location (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

$gxx = "C:\Users\USER\bonk\tools\mingw64\mingw64\bin\g++.exe"
if (-not (Test-Path $gxx)) { Write-Host "gcc portatil ausente em tools/mingw64"; exit 1 }

$out = "build\shimtest"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$fail = 0

function Build-And-Run($name, $sources) {
    $exe = "$out\$name.exe"
    & $gxx -std=c++20 -O0 -g -DSB_TEST_HOST -static -static-libgcc -static-libstdc++ -pthread `
        -I runtime/src/shim -I runtime/src/shim/test `
        @sources -o $exe 2>&1 | ForEach-Object { Write-Host $_ }
    if (-not (Test-Path $exe)) {
        Write-Host "COMPILACAO FALHOU: $name" -ForegroundColor Red
        $script:fail = 1
        return
    }
    Write-Host "--- $name ---"
    & $exe
    if ($LASTEXITCODE -ne 0) { $script:fail = 1 }
}

Build-And-Run "test_memory" @(
    "runtime/src/shim/kernel32_memory.cpp",
    "runtime/src/shim/test/test_memory.cpp")

Build-And-Run "test_file" @(
    "runtime/src/shim/kernel32_memory.cpp",
    "runtime/src/shim/kernel32_file.cpp",
    "runtime/src/shim/test/test_file.cpp")

if ($fail) { Write-Host "`nFALHAS NOS TESTES" -ForegroundColor Red; exit 1 }
Write-Host "`ntodos os testes de shim passaram" -ForegroundColor Green
