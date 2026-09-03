# Compiles and runs the pure-C canvas host test.
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "host_cc.ps1")

$root = Split-Path -Parent $PSScriptRoot
$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "canvas_host_test.c"),
    (Join-Path $root "main\render\canvas.c")
) -IncludeDirs @((Join-Path $root "main")) -ExeName "canvas_host_test.exe"

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "canvas_host_test failed (exit $LASTEXITCODE)"
}
