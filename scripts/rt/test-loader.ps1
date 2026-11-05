# test-loader.ps1 — teste local do loader multi-modulo (host mingw).
# Complica exe + baselib (pequenos), exercita: load com relocacao, exports,
# IAT com resolucao cruzada e o registro de modulos.
$ErrorActionPreference = 'Continue'
Set-Location (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

$gxx = "C:\Users\USER\bonk\tools\mingw64\mingw64\bin\g++.exe"
$py  = "C:\Users\USER\bonk\tools\python\python.exe"
$out = "build\desktop"
New-Item -ItemType Directory -Force -Path $out | Out-Null

# 1. namespace do baselib (prefixo BL) + contagens do teste
New-Item -ItemType Directory -Force -Path "lift\work\gen_baselib_bl" | Out-Null
Copy-Item "lift\work\gen_baselib\*" "lift\work\gen_baselib_bl" -Recurse -Force
& $py scripts\lift\namespace-gen.py lift\work\gen_baselib_bl BL | Out-Null
& $py -c "
import re
lines = ['#pragma once']
for tag, path in (('SB_TEST_IMPORTS_A', 'lift/work/gen_exe_fork/recomp_imports.c'),
                  ('SB_TEST_IMPORTS_B', 'lift/work/gen_baselib_bl/recomp_imports.c')):
    n = sum(1 for l in open(path, encoding='utf-8', errors='replace') if re.match(r'\s*\{\s*0x', l))
    lines.append('const uint32_t %s = %d;' % (tag, n))
open('build/sb_test_counts.h', 'w').write(chr(10).join(lines) + chr(10))
"

# 2. registro de shims
& $py scripts\rt\gen-shim-registry.py runtime\src\shim\kernel32_shims.h "$out\shim_registry.c" | Out-Null

# 3. compila (exe sem prefixo, baselib com BL)
$inc = @("-I", "tools\pcrecomp\runtime\recomp64_cpu",
         "-I", "lift\work\gen_exe_fork", "-I", "lift\work\gen_baselib_bl",
         "-I", "build", "-I", $out,
         "-I", "runtime\src\shim", "-I", "runtime\src\desktop")

$objs = @()
function Co($src, $tag) {
    $o = "$out\load_$tag.o"
    & $gxx -c $src -o $o -O1 -w -std=c++20 -fpermissive $inc 2>&1 | ForEach-Object { Write-Host $_ }
    if (Test-Path $o) { $script:objs += $o } else { $script:fail = 1 }
}

$script:fail = 0
foreach ($f in (Get-ChildItem "lift\work\gen_exe_fork\*.c"))      { Co $f.FullName "e" }
foreach ($f in (Get-ChildItem "lift\work\gen_baselib_bl\*.c"))    { Co $f.FullName "b" }
Co "tools\pcrecomp\runtime\recomp64_cpu\eh64.c" "eh"
Co "runtime\src\desktop\pe_load.c" "pe"
Co "runtime\src\desktop\multimod.c" "mm"
Co "runtime\src\desktop\test\test_loader.cpp" "t"
Co "$out\shim_registry.c" "sr"
Co "runtime\src\desktop\desktop_host.cpp" "dh"
foreach ($f in @("kernel32_memory", "kernel32_file", "kernel32_thread", "kernel32_misc", "game_entry")) {
    Co "runtime\src\shim\$f.cpp" "s"
}

if ($script:fail) { Write-Host "compilacao falhou"; exit 1 }

& $gxx -static -static-libgcc -static-libstdc++ -pthread -o "build\test_loader.exe" $objs 2>&1 |
    ForEach-Object { Write-Host $_ }
if (-not (Test-Path "build\test_loader.exe")) { Write-Host "LINK FALHOU"; exit 1 }

& "build\test_loader.exe" Megabonk\Megabonk.exe Megabonk\baselib.dll
if ($LASTEXITCODE -ne 0) { Write-Host "TESTE FALHOU" -ForegroundColor Red; exit 1 }
Write-Host "loader multi-modulo ok" -ForegroundColor Green
