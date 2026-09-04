# Compiles and runs the procedural fish rasterizer host test.
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "host_cc.ps1")

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root "main"

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "fish_rasterizer_host_test.c"),
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
) -IncludeDirs @($main) -ExeName "fish_rasterizer_host_test.exe"

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "fish_rasterizer_host_test failed (exit $LASTEXITCODE)"
}

