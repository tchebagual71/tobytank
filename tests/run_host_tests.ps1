# Runs every TobyTank host test: repository contract checks, the pure-C canvas
# test, and the aquarium environment and rendering test. No board is required.
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

Write-Host "All host tests passed."
