# Renders host preview frames of the empty aquarium to tools/preview/out/*.ppm.
# Nothing is flashed and no board is needed.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $root "tests\host_cc.ps1")

$main = Join-Path $root "main"
$outDir = Join-Path $PSScriptRoot "out"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "preview_main.c"),
    (Join-Path $PSScriptRoot "ppm.c"),
    (Join-Path $main "aquarium\environment.c"),
    (Join-Path $main "render\canvas.c"),
    (Join-Path $main "render\background.c"),
    (Join-Path $main "render\effects.c"),
    (Join-Path $main "render\particles.c")
) -IncludeDirs @($main, $PSScriptRoot) -ExeName "tobytank_preview.exe"

& $exe $outDir
if ($LASTEXITCODE -ne 0) {
    throw "preview failed (exit $LASTEXITCODE)"
}

Write-Host "Preview frames written to $outDir"
