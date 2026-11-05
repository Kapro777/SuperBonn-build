# Build desktop multi-módulo do guest (host mingw, sem device, sem CI).
# Uso: powershell scripts\rt\build-desktop.ps1
param(
    [string]$ExePE  = "Megabonk\Megabonk.exe",
    [string]$UpPE   = "Megabonk\UnityPlayer.dll"
)

$ErrorActionPreference = 'Continue'
Set-Location (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

$gxx = "C:\Users\USER\bonk\tools\mingw64\mingw64\bin\g++.exe"
$py  = "C:\Users\USER\bonk\tools\python\python.exe"

$out   = "build\desktop"
$exeG  = "lift\work\gen_exe_fork"       # exe (prefixo nenhum)
$upG   = "lift\work\gen_unityplayer_up" # UnityPlayer com prefixo UP

# 0. namespaces + contagens
& $py scripts\lift\namespace-gen.py $upG "" $upG 2>$null | Out-Null
if (-not (Test-Path "$upG\recomp_dispatch.c")) {
    New-Item -ItemType Directory -Force -Path $upG | Out-Null
    Copy-Item "lift\work\gen_unityplayer\*" $upG -Recurse -Force
    & $py scripts\lift\namespace-gen.py $upG "UP"
}

& $py -c @"
import re, pathlib
def count(path):
    n = 0
    for l in open(path, encoding='utf-8', errors='replace'):
        if re.match(r'\s*\{\s*0x', l):
            n += 1
    return n
exe = count('lift/work/gen_exe_fork/recomp_imports.c')
up  = count('$upG/recomp_imports.c')
hdr = '#pragma once\nconst uint32_t SB_EXE_IMPORTS = %d;\nconst uint32_t SB_UP_IMPORTS = %d;\n' % (exe, up)
pathlib.Path('$out/sb_counts.h').write_text(hdr, encoding='utf-8')
print('contagens: exe %d, UP %d' % (exe, up))
"@

# 1. registro de shims
& $py scripts\rt\gen-shim-registry.py runtime\src\shim\kernel32_shims.h "$out\shim_registry.c"
if (-not (Test-Path "$out\shim_registry.c")) { Write-Host "FALHA gerando registro"; exit 1 }

$inc = @("-I", "tools\pcrecomp\runtime\recomp64_cpu",
         "-I", "$upG", "-I", $exeG,
         "-I", $out,
         "-I", "runtime\src\shim", "-I", "runtime\src\desktop")

$objs = @()
$std = @("-std=c++20", "-w", "-fpermissive")

function Compile($src, $extra) {
    $o = "$out\" + [IO.Path]::GetFileNameWithoutExtension($src) + ".o"
    if ($src -like "*$upG*") { $name = "UP_" + [IO.Path]::GetFileNameWithoutExtension($src) }
    else { $name = [IO.Path]::GetFileNameWithoutExtension($src) }
    $o = "$out\$name.o"
    & $gxx -c $src -o $o -O1 -w -g0 $inc $extra 2>&1 | ForEach-Object { Write-Host $_ }
    if (Test-Path $o) { $script:objs += $o } else { Write-Host "FALHOU: $src" -ForegroundColor Red; $script:fail = 1 }
}

$script:fail = 0

# C do guest: exe (sem prefixo) + UnityPlayer (prefixo UP)
foreach ($f in (Get-ChildItem "$exeG\*.c")) { Compile $f.FullName $std }
foreach ($f in (Get-ChildItem "$upG\*.c"))  { Compile $f.FullName $std }

Compile "tools\pcrecomp\runtime\recomp64_cpu\eh64.c" $std
Compile "runtime\src\desktop\pe_load.c" $std
Compile "runtime\src\desktop\multimod.c" $std
Compile "runtime\src\desktop\desktop_main.c" $std
Compile "$out\shim_registry.c" $std

Compile "runtime\src\shim\kernel32_memory.cpp" $std
Compile "runtime\src\shim\kernel32_file.cpp" $std
Compile "runtime\src\shim\kernel32_thread.cpp" @($std + "-pthread")
Compile "runtime\src\shim\kernel32_misc.cpp" $std
Compile "runtime\src\shim\game_entry.cpp" $std
Compile "runtime\src\desktop\desktop_host.cpp" $std

if ($script:fail) { Write-Host "compilacao falhou"; exit 1 }

& $gxx -static -static-libgcc -static-libstdc++ -pthread -o "$out\sb_desktop.exe" $objs 2>&1 |
    ForEach-Object { Write-Host $_ }
if (-not (Test-Path "$out\sb_desktop.exe")) { Write-Host "LINK FALHOU" -ForegroundColor Red; exit 1 }
Write-Host "build ok: $out\sb_desktop.exe" -ForegroundColor Green
