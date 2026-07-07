param(
    [string]$FirmwareBuildDir = "build/firmware",
    [string]$TestBuildDir = "build/tests"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot

Push-Location $repoRoot
try {
    Initialize-BuildEnvironment

    Invoke-NativeCommand "Firmware configure" { cmake -S firmware -B $FirmwareBuildDir }
    Invoke-NativeCommand "Firmware build" { cmake --build $FirmwareBuildDir }

    Invoke-NativeCommand "Host test configure" { cmake -S firmware/tests -B $TestBuildDir }
    Invoke-NativeCommand "Host test build" { cmake --build $TestBuildDir }
}
finally {
    Pop-Location
}