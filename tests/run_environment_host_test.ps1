# Compiles and runs the aquarium environment and rendering host test.
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "host_cc.ps1")

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root "main"
$preview = Join-Path $root "tools\preview"

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "environment_host_test.c"),
    (Join-Path $main "aquarium\environment.c"),
    (Join-Path $main "render\canvas.c"),
    (Join-Path $main "render\background.c"),
    (Join-Path $main "render\effects.c"),
    (Join-Path $main "render\particles.c"),
    (Join-Path $preview "ppm.c")
) -IncludeDirs @($main, $preview) -ExeName "environment_host_test.exe"

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "environment_host_test failed (exit $LASTEXITCODE)"
}
