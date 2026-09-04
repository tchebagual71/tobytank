# Compiles and runs the touch, IMU, and aquarium interaction host test.
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "host_cc.ps1")

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root "main"

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "interactions_host_test.c"),
    (Join-Path $main "aquarium\environment.c"),
    (Join-Path $main "aquarium\interactions.c"),
    (Join-Path $main "aquarium\lifecycle.c"),
    (Join-Path $main "fish\behavior.c"),
    (Join-Path $main "fish\genome.c"),
    (Join-Path $main "fish\genome_validate.c"),
    (Join-Path $main "fish\identity.c"),
    (Join-Path $main "fish\motion.c"),
    (Join-Path $main "fish\prng.c"),
    (Join-Path $main "input\motion_filter.c"),
    (Join-Path $main "render\background.c"),
    (Join-Path $main "render\canvas.c"),
    (Join-Path $main "render\dither.c"),
    (Join-Path $main "render\effects.c"),
    (Join-Path $main "sim\snapshot.c")
) -IncludeDirs @($main, $root, (Join-Path $root "tools\preview")) -ExeName "interactions_host_test.exe"

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "interactions_host_test failed (exit $LASTEXITCODE)"
}

