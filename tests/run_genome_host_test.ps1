# Compiles and runs the identity, PRNG, and genome host test.
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "host_cc.ps1")

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root "main"

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "genome_host_test.c"),
    (Join-Path $main "fish\prng.c"),
    (Join-Path $main "fish\identity.c"),
    (Join-Path $main "fish\genome.c"),
    (Join-Path $main "fish\genome_validate.c")
) -IncludeDirs @($main) -ExeName "genome_host_test.exe"

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "genome_host_test failed (exit $LASTEXITCODE)"
}
