param(
    [string]$TestBuildDir = "build/tests"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
    if (-not (Test-Path $TestBuildDir)) {
        cmake -S firmware/tests -B $TestBuildDir
        cmake --build $TestBuildDir
    }

    ctest --test-dir $TestBuildDir --output-on-failure
}
finally {
    Pop-Location
}