# Compila el informe con Tectonic y deja main.pdf en esta carpeta.
# Uso (desde PowerShell):  .\informe\compilar.ps1
$tectonic = Join-Path $env:LOCALAPPDATA 'Programs\tectonic\tectonic.exe'
if (-not (Test-Path $tectonic)) {
    Write-Error "No se encontró Tectonic en $tectonic. Descargarlo de https://github.com/tectonic-typesetting/tectonic/releases"
    exit 1
}
Push-Location $PSScriptRoot
try {
    & $tectonic -X compile main.tex --synctex
    if ($LASTEXITCODE -eq 0) { Write-Host "OK: $PSScriptRoot\main.pdf" }
} finally {
    Pop-Location
}
