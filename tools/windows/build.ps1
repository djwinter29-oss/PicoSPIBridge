param(
    [string]$FirmwareBuildDir = "build/firmware",
    [string]$TestBuildDir = "build/tests"
)

$ErrorActionPreference = "Stop"

function Invoke-NativeCommand {
    param(
        [string]$Description,
        [scriptblock]$Command
    )

    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

. (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
    Import-VsDevEnvironment

    Invoke-NativeCommand "Firmware configure" { cmake -S firmware -B $FirmwareBuildDir }
    Invoke-NativeCommand "Firmware build" { cmake --build $FirmwareBuildDir }

    Invoke-NativeCommand "Host test configure" { cmake -S firmware/tests -B $TestBuildDir }
    Invoke-NativeCommand "Host test build" { cmake --build $TestBuildDir }
}
finally {
    Pop-Location
}