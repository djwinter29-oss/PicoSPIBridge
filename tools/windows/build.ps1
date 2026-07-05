param(
    [string]$FirmwareBuildDir = "build/firmware",
    [string]$TestBuildDir = "build/tests"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
    cmake -S firmware -B $FirmwareBuildDir
    cmake --build $FirmwareBuildDir

    cmake -S firmware/tests -B $TestBuildDir
    cmake --build $TestBuildDir
}
finally {
    Pop-Location
}