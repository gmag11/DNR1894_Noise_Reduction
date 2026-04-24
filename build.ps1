param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [string]$BuildDir = "build",

    [switch]$SkipConfigure
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildPath = Join-Path $repoRoot $BuildDir

Push-Location $repoRoot

try {
    if (-not $SkipConfigure) {
        Write-Host "Configuring CMake in '$BuildDir'..."
        cmake -S . -B $BuildDir
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed with exit code $LASTEXITCODE."
        }
    }

    Write-Host "Building VST3 and Standalone ($Configuration)..."
    cmake --build $BuildDir --config $Configuration --target DNR1894_VST3 DNR1894_Standalone
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    $standalonePath = Join-Path $buildPath "DNR1894_artefacts/$Configuration/Standalone/DNR1894.exe"
    $vst3Path = Join-Path $buildPath "DNR1894_artefacts/$Configuration/VST3/DNR1894.vst3"

    Write-Host "Build completed successfully."

    if (Test-Path $standalonePath) {
        Write-Host "Standalone: $standalonePath"
    }

    if (Test-Path $vst3Path) {
        Write-Host "VST3: $vst3Path"
    }
}
finally {
    Pop-Location
}