param(
    [string]$FirmwareBuildDir = "build/firmware",
    [string]$OpenOcdExe = "openocd",
    [int]$AdapterSpeedKhz = 5000,
    [switch]$SkipBuild
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
$elfPath = Join-Path $repoRoot "$FirmwareBuildDir/pico_spi_bridge.elf"

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        Import-VsDevEnvironment

        if (-not $env:PICO_SDK_PATH) {
            throw "PICO_SDK_PATH is not set. Set it before running tools/windows/load.ps1, or pass -SkipBuild to program an existing ELF."
        }

        Invoke-NativeCommand "Firmware configure" { cmake -S firmware -B $FirmwareBuildDir }
        Invoke-NativeCommand "Firmware build" { cmake --build $FirmwareBuildDir }
    }

    if (-not (Get-Command $OpenOcdExe -ErrorAction SilentlyContinue)) {
        throw "OpenOCD executable not found: $OpenOcdExe"
    }

    if (-not (Test-Path $elfPath -PathType Leaf)) {
        throw "ELF not found at $elfPath"
    }

    Invoke-NativeCommand "Firmware program" {
        & $OpenOcdExe `
            -f interface/cmsis-dap.cfg `
            -f target/rp2040.cfg `
            -c "adapter speed $AdapterSpeedKhz" `
            -c "program $elfPath verify reset exit"
    }

    Write-Host "Programmed $elfPath over Debug Probe"
}
finally {
    Pop-Location
}