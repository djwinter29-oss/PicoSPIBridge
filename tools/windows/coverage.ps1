param(
    [string]$CoverageBuildDir = "build/tests-coverage",
    [string]$CoverageOutputDir = "build/coverage"
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

    $coverageCompiler = Get-Command clang -ErrorAction SilentlyContinue
    if (-not $coverageCompiler) {
        $coverageCompiler = Get-Command gcc -ErrorAction SilentlyContinue
    }

    if (-not $coverageCompiler) {
        throw "Coverage on Windows requires a GCC- or Clang-based host compiler. The current Visual Studio/MSVC test setup can run tests, but it cannot produce gcov coverage output by itself."
    }

    $gcovrArgs = @(
        "--root", $repoRoot,
        "--filter", "firmware/src",
        "--filter", "firmware/tests",
        "--exclude", "firmware/tests/.*/CMakeFiles",
        "--txt", (Join-Path $CoverageOutputDir "coverage.txt"),
        "--html-details", (Join-Path $CoverageOutputDir "coverage.html"),
        $CoverageBuildDir
    )

    if (-not (Get-Command gcovr -ErrorAction SilentlyContinue)) {
        throw "gcovr is required for coverage reporting. Install it with 'pip install gcovr'."
    }

    Invoke-NativeCommand "Coverage configure" { cmake -G Ninja -S firmware/tests -B $CoverageBuildDir -DBRIDGE_ENABLE_COVERAGE=ON -DCMAKE_C_COMPILER=$coverageCompiler.Source }
    Invoke-NativeCommand "Coverage build" { cmake --build $CoverageBuildDir --config Debug }
    Invoke-NativeCommand "Coverage test run" { ctest --test-dir $CoverageBuildDir -C Debug --output-on-failure }

    New-Item -ItemType Directory -Force -Path $CoverageOutputDir | Out-Null

    & gcovr @gcovrArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Coverage report generation failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}