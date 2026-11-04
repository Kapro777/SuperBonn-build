# Compila e roda os testes dos shims no host mingw (sem NDK, sem jogo).
# Uso: powershell scripts\rt\test-shims.ps1
$ErrorActionPreference = 'Continue'
Set-Location (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

$gxx = "C:\Users\USER\bonk\tools\mingw64\mingw64\bin\g++.exe"
if (-not (Test-Path $gxx)) { Write-Host "gcc portatil ausente em tools/mingw64"; exit 1 }

$out = "build\shimtest"
New-Item -ItemType Directory -Force -Path $out | Out-Null

& $gxx -std=c++20 -O0 -g -DSB_TEST_HOST -static -static-libgcc -static-libstdc++ -pthread `
    -I runtime/src/shim -I runtime/src/shim/test `
    runtime/src/shim/kernel32_memory.cpp `
    runtime/src/shim/test/test_memory.cpp `
    -o "$out\test_memory.exe" 2>&1 |
    ForEach-Object { Write-Host $_ }

if (Test-Path "$out\test_memory.exe") {
    & "$out\test_memory.exe"
    Write-Host "exit=$LASTEXITCODE"
} else {
    Write-Host "compilacao falhou"
    exit 1
}
