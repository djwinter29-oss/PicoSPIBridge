param(
    [string]$FirmwareBuildDir = "build/firmware",
    [string]$TestBuildDir = "build/tests"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
    Import-VsDevEnvironment

    cmake -S firmware -B $FirmwareBuildDir
    cmake --build $FirmwareBuildDir

    cmake -S firmware/tests -B $TestBuildDir
    cmake --build $TestBuildDir
}
finally {
    Pop-Location
}