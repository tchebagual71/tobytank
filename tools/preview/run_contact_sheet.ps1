# Renders a host fish contact sheet to tools/preview/out/fish_contact_sheet.ppm.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $root "tests\host_cc.ps1")

$main = Join-Path $root "main"
$outDir = Join-Path $PSScriptRoot "out"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$outFile = Join-Path $outDir "fish_contact_sheet.ppm"

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "contact_sheet_main.c"),
    (Join-Path $PSScriptRoot "ppm.c"),
    (Join-Path $main "fish\genome.c"),
    (Join-Path $main "fish\genome_validate.c"),
    (Join-Path $main "fish\identity.c"),
    (Join-Path $main "fish\portrait.c"),
    (Join-Path $main "fish\prng.c"),
    (Join-Path $main "render\canvas.c"),
    (Join-Path $main "render\composite.c"),
    (Join-Path $main "render\dither.c"),
    (Join-Path $main "render\fish_cache.c"),
    (Join-Path $main "render\fish_rasterizer.c")
) -IncludeDirs @($main, $PSScriptRoot) -ExeName "tobytank_fish_contact_sheet.exe"

& $exe $outFile
if ($LASTEXITCODE -ne 0) {
    throw "contact sheet failed (exit $LASTEXITCODE)"
}

Write-Host "Fish contact sheet written to $outFile"

