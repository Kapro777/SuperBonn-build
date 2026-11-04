# Roda os lifts completos COM o fork aplicado, medindo os RECOMP_TODO.
# Uso: powershell scripts\lift\run-lifts-with-fork.ps1
$ErrorActionPreference = 'Continue'
Set-Location (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

$py = "C:\Users\USER\bonk\tools\python\python.exe"

& $py scripts\lift\apply-fork.py | Out-Null

# exe (gate rapido)
& $py tools\pcrecomp\tools\lift\generate64.py Megabonk\Megabonk.exe `
    lift\work\disasm\funcs_exe.txt lift\work\gen_exe_fork --split 100 2>&1 |
    Select-String "recomp_todo_sites|lift_errors"
$depois = (Select-String -Path "lift\work\gen_exe_fork\*.c" -Pattern "RECOMP_TODO" -AllMatches | Measure-Object).Count
Write-Host "TODO exe com fork: $depois (original: 172)"

# GameAssembly.dll
& $py tools\pcrecomp\tools\lift\generate64.py Megabonk\GameAssembly.dll `
    lift\work\disasm\funcs_gameassembly.txt lift\work\gen_gameassembly_fork --split 400 2>&1 |
    Out-File lift\work\gen_ga_fork.log -Encoding utf8
Write-Host "=== GameAssembly COM fork ==="
Select-String -Path lift\work\gen_ga_fork.log -Pattern "COMPLETE|functions |lift_errors|recomp_todo" | ForEach-Object { $_.Line }

# UnityPlayer.dll
& $py tools\pcrecomp\tools\lift\generate64.py Megabonk\UnityPlayer.dll `
    lift\work\disasm\funcs_unityplayer.txt lift\work\gen_unityplayer_fork --split 400 2>&1 |
    Out-File lift\work\gen_up_fork.log -Encoding utf8
Write-Host "=== UnityPlayer COM fork ==="
Select-String -Path lift\work\gen_up_fork.log -Pattern "COMPLETE|functions |lift_errors|recomp_todo" | ForEach-Object { $_.Line }

Write-Host "TODOS CONCLUIDOS"
