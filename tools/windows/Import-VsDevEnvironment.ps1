function Import-VsDevEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $needsVsDevEnvironment = (-not (Get-Command cmake -ErrorAction SilentlyContinue)) -or (-not (Get-Command cl -ErrorAction SilentlyContinue))
    $vsInstallPath = $null
    $vsDevCmd = $null

    if ($needsVsDevEnvironment -and -not (Test-Path $vswhere)) {
        throw "The Visual Studio developer environment is required, but Visual Studio Installer metadata was not found."
    }

    if ($needsVsDevEnvironment) {
        $vsInstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $vsInstallPath) {
            throw "No Visual Studio installation with C++ build tools was found."
        }

        $vsDevCmd = Join-Path $vsInstallPath "Common7\Tools\VsDevCmd.bat"
        if (-not (Test-Path $vsDevCmd)) {
            throw "VsDevCmd.bat was not found under '$vsInstallPath'."
        }

        $envDump = & cmd /c "call `"$vsDevCmd`" -host_arch=x64 -arch=x64 >nul && set"
        foreach ($line in $envDump) {
            $separator = $line.IndexOf('=')
            if ($separator -lt 1) {
                continue
            }

            $name = $line.Substring(0, $separator)
            $value = $line.Substring($separator + 1)
            Set-Item -Path ("Env:{0}" -f $name) -Value $value
        }
    }
}