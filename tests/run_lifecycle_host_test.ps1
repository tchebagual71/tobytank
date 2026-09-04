# Compiles and runs the visitor lifecycle and motion host test.
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "host_cc.ps1")

$root = Split-Path -Parent $PSScriptRoot
$main = Join-Path $root "main"

$exe = Invoke-HostBuild -Sources @(
    (Join-Path $PSScriptRoot "lifecycle_host_test.c"),
    (Join-Path $main "aquarium\lifecycle.c"),
    (Join-Path $main "fish\behavior.c"),
    (Join-Path $main "fish\genome.c"),
    (Join-Path $main "fish\genome_validate.c"),
    (Join-Path $main "fish\identity.c"),
    (Join-Path $main "fish\motion.c"),
    (Join-Path $main "fish\prng.c"),
    (Join-Path $main "sim\snapshot.c")
) -IncludeDirs @($main) -ExeName "lifecycle_host_test.exe"

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "lifecycle_host_test failed (exit $LASTEXITCODE)"
}

