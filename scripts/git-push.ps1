# Push sem prompt de credencial: usa o token do gh (ja autenticado) numa URL
# temporaria e volta a URL limpa. NUNCA escreve o token em arquivo.
# Uso: powershell scripts\git-push.ps1 ["mensagem de commit"]
param([string]$Message = "")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$gh = "C:\Users\USER\bonk\tools\gh\bin\gh.exe"
$git = "C:\Users\USER\bonk\tools\mingit\cmd\git.exe"

# commit pendente?
if ($Message) {
    & $git commit -q -am $Message
    if ($LASTEXITCODE -eq 0) { Write-Host "commit: $Message" }
}

$tok = & $gh auth token
if (-not $tok) { Write-Host "gh nao autenticado. Rode: gh auth login --web"; exit 1 }

& $git push "https://Kapro777:$tok@github.com/Kapro777/SuperBonn.git" main 2>&1 | Out-String | Write-Host
& $git remote set-url origin https://github.com/Kapro777/SuperBonn.git
& $git status -sb | Write-Host
