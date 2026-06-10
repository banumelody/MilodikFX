param(
    [switch]$Build,
    [switch]$NoServer,
    [string]$Config = "Debug"
)

Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
Push-Location $repoRoot

$frontendDir = Join-Path $repoRoot "frontend"
$exePath = Join-Path $repoRoot ("build\MilodikFX_artefacts\" + $Config + "\MilodikFX.exe")

function Log($m) { Write-Host "[run-local-e2e] $m" }

# Build frontend + exe if requested
if ($Build) {
    Log "Building frontend..."
    Push-Location $frontendDir
    npm ci
    npm run build
    Pop-Location

    Log "Configuring and building native project..."
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config $Config --parallel
}

if ($NoServer) {
    Log "Skipping server start as requested (-NoServer)"
    exit 0
}

if (!(Test-Path $exePath)) {
    Write-Error "Executable not found: $exePath"
    exit 1
}

Log "Starting MilodikFX exe: $exePath"
$proc = Start-Process -FilePath $exePath -PassThru -WindowStyle Hidden

# Wait for readiness (index.html must be served)
$ready = $false
for ($i=0; $i -lt 60; $i++) {
    try {
        $r = Invoke-WebRequest -Uri "http://localhost:3000/index.html" -UseBasicParsing -TimeoutSec 5 -ErrorAction Stop
        if ($r.StatusCode -eq 200 -and $r.Headers['Content-Type'] -match "text/html") { $ready = $true; break }
    } catch {
        # ignore
    }
    Start-Sleep -Seconds 1
}

if (-not $ready) {
    Write-Error "Server not ready after timeout"
    try { Stop-Process -Id $proc.Id -Force } catch {}
    exit 2
}

Log "Server ready. Running smoke checks..."
try {
    Invoke-RestMethod -Uri "http://localhost:3000/api/levels" -Method GET -TimeoutSec 10 | Out-Null
    Log "GET /api/levels OK"

    $getParam = Invoke-RestMethod -Uri "http://localhost:3000/api/parameters/master-volume" -Method GET -TimeoutSec 10
    Log "GET /api/parameters/master-volume OK"

    $body = @{ value = 0.5 } | ConvertTo-Json
    Invoke-RestMethod -Uri "http://localhost:3000/api/parameters/master-volume" -Method PUT -Body $body -ContentType "application/json" -TimeoutSec 10
    Log "PUT /api/parameters/master-volume OK"
} catch {
    Write-Error "Smoke check failed: $_"
    try { Stop-Process -Id $proc.Id -Force } catch {}
    exit 3
}

# Teardown
try { Stop-Process -Id $proc.Id -Force } catch {}
Log "Smoke checks passed"
exit 0
