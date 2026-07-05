param(
    [string]$TestBuildDir = "build/tests"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
    Import-VsDevEnvironment

    cmake -S firmware/tests -B $TestBuildDir
    cmake --build $TestBuildDir --config Debug

    ctest --test-dir $TestBuildDir -C Debug --output-on-failure
}
finally {
    Pop-Location
}