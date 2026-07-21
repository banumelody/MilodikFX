<#
.SYNOPSIS
    Builds a shippable MilodikFX: frontend, native Release exe, and installer.

.DESCRIPTION
    The frontend must be built before CMake configures, because the UI bundle is
    embedded into the executable at configure time. This script enforces that
    order so the result is a genuinely standalone exe rather than one that needs
    a resources folder beside it.

.PARAMETER SkipInstaller
    Build the exe only. Without Inno Setup installed the installer step is
    skipped automatically anyway.

.PARAMETER AsioSdkPath
    Path to the Steinberg ASIO SDK. Supplying it enables ASIO, which is the
    lowest-latency driver path on Windows.
#>
param(
    [switch]$SkipFrontend,
    [switch]$SkipInstaller,
    [string]$AsioSdkPath,
    [string]$Generator = "Visual Studio 17 2022"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Definition) '..')).Path
$frontendDir = Join-Path $repoRoot 'frontend'
$buildDir = Join-Path $repoRoot 'build'
$distDir = Join-Path $repoRoot 'dist'

function Log($message) { Write-Host "[build] $message" -ForegroundColor Cyan }

# Read the version straight from CMakeLists so it can never drift.
$cmakeText = Get-Content (Join-Path $repoRoot 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(MilodikFX\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not read the project version from CMakeLists.txt'
}
$version = $Matches[1]
Log "Version $version"

if (-not $SkipFrontend) {
    Log 'Building the frontend...'
    Push-Location $frontendDir
    try {
        if (Test-Path (Join-Path $frontendDir 'node_modules')) { npm install } else { npm ci }
        if ($LASTEXITCODE -ne 0) { throw 'npm install failed' }

        npm run build
        if ($LASTEXITCODE -ne 0) { throw 'frontend build failed' }
    } finally {
        Pop-Location
    }
}

$distIndex = Join-Path $frontendDir 'dist\index.html'
if (-not (Test-Path $distIndex)) {
    throw "frontend/dist is missing. Run without -SkipFrontend, or build it manually first."
}

if ($AsioSdkPath) {
    if (-not (Test-Path (Join-Path $AsioSdkPath 'common\asio.h'))) {
        throw "ASIO SDK not found at $AsioSdkPath (expected common\asio.h)"
    }
    $env:ASIOSDK_DIR = $AsioSdkPath
    Log "ASIO SDK: $AsioSdkPath"
}

Log 'Configuring...'
# Force a fresh configure so the embedded bundle picks up the frontend we just built.
cmake -S $repoRoot -B $buildDir -G $Generator -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }

Log 'Building Release...'
cmake --build $buildDir --config Release --target MilodikFX --parallel
if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }

$exePath = Join-Path $buildDir 'MilodikFX_artefacts\Release\MilodikFX.exe'
if (-not (Test-Path $exePath)) { throw "Executable not produced: $exePath" }

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
Copy-Item $exePath (Join-Path $distDir "MilodikFX-$version.exe") -Force

$sizeMb = [math]::Round((Get-Item $exePath).Length / 1MB, 1)
Log "Standalone executable: dist\MilodikFX-$version.exe ($sizeMb MB)"

if ($SkipInstaller) {
    Log 'Installer skipped (-SkipInstaller)'
    exit 0
}

$iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
if (-not $iscc) {
    foreach ($candidate in @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe")) {
        if (Test-Path $candidate) { $iscc = Get-Item $candidate; break }
    }
}

if (-not $iscc) {
    Log 'Inno Setup (ISCC.exe) not found - skipping the installer.'
    Log 'Install it from https://jrsoftware.org/isdl.php to produce MilodikFX-setup.exe.'
    exit 0
}

Log 'Building the installer...'
& $iscc.FullName "/DMyAppVersion=$version" (Join-Path $repoRoot 'installer\MilodikFX.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed' }

Log "Installer: dist\MilodikFX-$version-setup.exe"
