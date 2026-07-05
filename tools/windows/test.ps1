param(
    [string]$FirmwareBuildDir = "build/firmware",
    [string]$TestBuildDir = "build/tests",
    [switch]$SkipFirmwareBuild
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

    if (-not $SkipFirmwareBuild) {
        if (-not $env:PICO_SDK_PATH) {
            throw "PICO_SDK_PATH is not set. Set it before running tools/windows/test.ps1, or pass -SkipFirmwareBuild to run only the host-side tests."
        }

        Invoke-NativeCommand "Firmware configure" { cmake -S firmware -B $FirmwareBuildDir }
        Invoke-NativeCommand "Firmware build" { cmake --build $FirmwareBuildDir --config Debug }
    }

    Invoke-NativeCommand "Host test configure" { cmake -S firmware/tests -B $TestBuildDir }
    Invoke-NativeCommand "Host test build" { cmake --build $TestBuildDir --config Debug }

    Invoke-NativeCommand "Host test run" { ctest --test-dir $TestBuildDir -C Debug --output-on-failure }
}
finally {
    Pop-Location
}