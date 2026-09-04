# Runs every TobyTank host test: repository contract checks, the canvas test,
# the aquarium environment and rendering test, the identity and genome test,
# the fish rasterizer test, the visitor lifecycle test, and the interaction
# test. No board is required.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

Write-Host "== repository contract tests =="
Push-Location $root
try {
    & python -m unittest discover -s tests -p "test_*.py"
} finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) {
    throw "repository contract tests failed (exit $LASTEXITCODE)"
}

Write-Host "== canvas host test =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_canvas_host_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "canvas host test failed (exit $LASTEXITCODE)"
}

Write-Host "== environment host test =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_environment_host_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "environment host test failed (exit $LASTEXITCODE)"
}

Write-Host "== genome host test =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_genome_host_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "genome host test failed (exit $LASTEXITCODE)"
}

Write-Host "== fish rasterizer host test =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_fish_rasterizer_host_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "fish rasterizer host test failed (exit $LASTEXITCODE)"
}

Write-Host "== visitor lifecycle host test =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_lifecycle_host_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "visitor lifecycle host test failed (exit $LASTEXITCODE)"
}

Write-Host "== interactions host test =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_interactions_host_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "interactions host test failed (exit $LASTEXITCODE)"
}

Write-Host "All host tests passed."
