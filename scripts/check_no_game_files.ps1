# Guarda local: falha se encontrar arquivos do jogo ou binários proibidos
# prestes a serem commitados. Rodar SEMPRE antes de `git add`/`git commit`.
# Uso: powershell -ExecutionPolicy Bypass -File scripts/check_no_game_files.ps1
# (roda no PowerShell 5.1 do Windows; nao precisa de pwsh instalado)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$forbiddenNames = @(
    'Megabonk.exe', 'UnityCrashHandler64.exe',
    'GameAssembly.dll', 'UnityPlayer.dll', 'baselib.dll',
    'D3D12Core.dll', 'globalgamemanagers', 'level0', 'level1', 'level2',
    'sharedassets0.assets', 'sharedassets1.assets', 'sharedassets2.assets',
    'resources.assets', 'il2cpp_data'
)

# Extensões de dados do jogo que nunca entram no repo
$forbiddenExt = @('.vpk', '.unity3d', '.asset', '.resS', '.resources', '.bundle')

$found = @()

# Respeita o .gitignore: pastas listadas la sao ignoradas (nosso .gitignore
# exclui a pasta do jogo, que fica local e nunca eh commitada).
$ignoredDirs = @()
Get-Content (Join-Path $root '.gitignore') -ErrorAction SilentlyContinue |
    ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith('#') -and $line.EndsWith('/')) {
            $ignoredDirs += $line.TrimEnd('/')
        }
    }

function Test-Ignored([string]$fullName) {
    foreach ($d in $ignoredDirs) {
        if ($fullName -like "*\$d\*") { return $true }
    }
    return $false
}

$allFiles = Get-ChildItem -Path $root -Recurse -File -Force |
    Where-Object { $_.FullName -notmatch '\\\.git\\' }

# 1. Nomes/extensoes proibidos FORA das pastas ignoradas (ou seja: coisas que
#    iriam para o commit)
$allFiles | Where-Object { -not (Test-Ignored $_.FullName) } | ForEach-Object {
    if ($forbiddenNames -contains $_.Name) { $found += $_.FullName }
    elseif ($forbiddenExt -contains $_.Extension.ToLower()) { $found += $_.FullName }
}

# 2. Pasta do jogo precisa continuar ignorada (se alguem remover do .gitignore)
if (-not (Get-Content (Join-Path $root '.gitignore') -ErrorAction SilentlyContinue |
          Select-String -SimpleMatch 'Megabonk/' -Quiet)) {
    $found += (Join-Path $root '.gitignore') + '  (perdeu a linha Megabonk/ — restaure!)'
}

# 3. Arquivos grandes suspeitos (> 10 MB) fora do ignorado
$allFiles | Where-Object { -not (Test-Ignored $_.FullName) -and $_.Length -gt 10MB } |
    ForEach-Object { $found += "$($_.FullName)  ($([math]::Round($_.Length/1MB,1)) MB)" }

if ($found.Count -gt 0) {
    Write-Host ''
    Write-Host '=== BLOQUEADO: conteudo proibido encontrado ===' -ForegroundColor Red
    $found | Sort-Object -Unique | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    Write-Host ''
    Write-Host 'Remova esses arquivos do stage antes de commitar.' -ForegroundColor Yellow
    exit 1
}

Write-Host 'OK: nenhum arquivo do jogo ou binario grande encontrado.' -ForegroundColor Green
exit 0
