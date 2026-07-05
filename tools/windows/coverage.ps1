param(
    [string]$CoverageBuildDir = "build/tests-coverage",
    [string]$CoverageOutputDir = "build/coverage"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
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

    cmake -S firmware/tests -B $CoverageBuildDir -DBRIDGE_ENABLE_COVERAGE=ON
    cmake --build $CoverageBuildDir
    ctest --test-dir $CoverageBuildDir --output-on-failure

    New-Item -ItemType Directory -Force -Path $CoverageOutputDir | Out-Null

    & gcovr @gcovrArgs
}
finally {
    Pop-Location
}