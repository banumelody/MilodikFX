<#
.SYNOPSIS
    Runs the Cypress suite against a real MilodikFX process.

.DESCRIPTION
    The UI has no meaning without the engine behind it -- every control is bound
    to a live DSP parameter -- so this starts the exe, waits for it to serve the
    bundle, points Cypress at it, and shuts it down again.
#>
param(
    [switch]$Build,
    [string]$Config = "Debug",
    [int]$StartupTimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$frontendDir = Join-Path $repoRoot 'frontend'
$exePath = Join-Path $repoRoot "build\MilodikFX_artefacts\$Config\MilodikFX.exe"

function Log($message) { Write-Host "[e2e] $message" }

if ($Build) {
    Log 'Building frontend...'
    Push-Location $frontendDir
    try {
        npm ci
        npm run build
    } finally {
        Pop-Location
    }

    Log 'Configuring and building the native project...'
    cmake -S $repoRoot -B (Join-Path $repoRoot 'build') -G "Visual Studio 17 2022" -A x64
    cmake --build (Join-Path $repoRoot 'build') --config $Config --target MilodikFX --parallel
}

if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found: $exePath (run with -Build first)"
    exit 1
}

# A second instance is refused by the single-instance guard, so Cypress would
# silently talk to whichever engine was already running. Match on the executable
# path, not the process name: a copy named MilodikFX-0.9.0.exe reports a
# different process name and slipped past a name-based check, which meant a
# whole test session ran against a stale build.
$existing = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ExecutablePath -like '*MilodikFX*.exe' }

if ($existing) {
    Log "Stopping $($existing.Count) already running MilodikFX process(es)..."
    $existing | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 3
}

Log "Starting $exePath"
$proc = Start-Process -FilePath $exePath -PassThru

$baseUrl = $null
$exitCode = 0

try {
    $deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)

    while ((Get-Date) -lt $deadline -and -not $baseUrl) {
        foreach ($port in 3000..3008) {
            try {
                $probe = Invoke-WebRequest -Uri "http://127.0.0.1:$port/index.html" -UseBasicParsing -TimeoutSec 2 -ErrorAction Stop
                if ($probe.StatusCode -eq 200) { $baseUrl = "http://127.0.0.1:$port"; break }
            } catch {
                # not up yet
            }
        }

        if (-not $baseUrl) { Start-Sleep -Milliseconds 500 }
    }

    if (-not $baseUrl) {
        Write-Error "Engine did not serve the UI within $StartupTimeoutSeconds s"
        exit 2
    }

    Log "Engine ready at $baseUrl"

    Push-Location $frontendDir
    try {
        $env:MILODIKFX_URL = $baseUrl
        npx cypress run --spec 'cypress/e2e/engine.cy.ts'
        $exitCode = $LASTEXITCODE
    } finally {
        Remove-Item Env:\MILODIKFX_URL -ErrorAction SilentlyContinue
        Pop-Location
    }
} finally {
    if ($proc -and -not $proc.HasExited) {
        Log 'Stopping MilodikFX...'
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}

if ($exitCode -ne 0) {
    Write-Error "Cypress reported failures (exit $exitCode)"
    exit $exitCode
}

Log 'E2E passed'
exit 0
