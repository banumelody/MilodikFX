# ==============================================================================
# MilodikFX Frontend E2E Test Runner (Windows PowerShell)
# ==============================================================================
#
# This script reproduces the cloud agent E2E testing workflow locally on Windows.
# Designed to match the copilot-setup-steps.yml configuration.
#
# Usage:
#   .\run-e2e-tests.ps1 [-Build] [-NoServer] [-Watch] [-Help]
#
# Options:
#   -Build        Force rebuild of frontend (default: only if dist/ missing)
#   -NoServer     Skip preview server (assume it's already running)
#   -Watch        Run Cypress in watch mode (interactive)
#   -Help         Show this help message
#
# Exit Codes:
#   0 - All tests passed
#   1 - Build failed
#   2 - Server failed to start or health check failed
#   3 - Cypress tests failed
#   4 - Invalid arguments
#
# Requirements:
#   - PowerShell 5.0+ (or PowerShell Core)
#   - Node.js 18+ and npm
#   - curl (available in Windows 10+)
#
# Examples:
#   .\run-e2e-tests.ps1                    # Run full E2E test suite
#   .\run-e2e-tests.ps1 -Build             # Rebuild frontend then run E2E
#   .\run-e2e-tests.ps1 -NoServer          # Skip server startup
#   .\run-e2e-tests.ps1 -Watch             # Run in watch/interactive mode
#
# ==============================================================================

param(
    [switch]$Build,
    [switch]$NoServer,
    [switch]$Watch,
    [switch]$Help
)

# Enable strict error handling
$ErrorActionPreference = "Stop"
$VerbosePreference = "Continue"

# ==============================================================================
# Configuration
# ==============================================================================

$FRONTEND_DIR = "frontend"
$PREVIEW_PORT = 5173
$PREVIEW_URL = "http://localhost:${PREVIEW_PORT}"
$MAX_WAIT_TIME = 60
$FORCE_BUILD = $Build
$SKIP_SERVER = $NoServer
$HEADLESS = -not $Watch
$PREVIEW_PROCESS = $null

# ==============================================================================
# Color Output Functions
# ==============================================================================

function Write-Info {
    param([string]$Message)
    Write-Host "ℹ  $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "✓  $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "⚠  $Message" -ForegroundColor Yellow
}

function Write-Error-Custom {
    param([string]$Message)
    Write-Host "✗  $Message" -ForegroundColor Red
}

function Show-Help {
    @"
MilodikFX Frontend E2E Test Runner

Usage:
  .\run-e2e-tests.ps1 [OPTIONS]

Options:
  -Build        Force rebuild of frontend (default: only if dist/ missing)
  -NoServer     Skip preview server (assume it's already running)
  -Watch        Run Cypress in watch mode (interactive)
  -Help         Show this help message

Examples:
  .\run-e2e-tests.ps1                    # Run full E2E test suite
  .\run-e2e-tests.ps1 -Build             # Rebuild frontend then run E2E
  .\run-e2e-tests.ps1 -NoServer          # Skip server startup
  .\run-e2e-tests.ps1 -Watch             # Run in watch/interactive mode

Exit Codes:
  0 - All tests passed
  1 - Build failed
  2 - Server failed to start or health check failed
  3 - Cypress tests failed
  4 - Invalid arguments

"@
}

# ==============================================================================
# Cleanup Function
# ==============================================================================

function Cleanup {
    if ($PREVIEW_PROCESS) {
        Write-Info "Stopping preview server (PID: $($PREVIEW_PROCESS.Id))..."
        try {
            Stop-Process -Id $PREVIEW_PROCESS.Id -Force -ErrorAction SilentlyContinue
        }
        catch {
            # Process may already be stopped
        }
    }
}

# Register cleanup on script exit
$null = Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action { Cleanup }
trap { Cleanup; throw $_ }

# ==============================================================================
# Requirement Checks
# ==============================================================================

function Test-Requirements {
    Write-Info "Checking requirements..."
    
    # Check Node.js
    $nodeVersion = $null
    try {
        $nodeVersion = & node -v 2>$null
        if (-not $nodeVersion) {
            throw "Node.js not found"
        }
        $nodeMajor = [int]($nodeVersion -replace 'v(\d+)\..*', '$1')
        if ($nodeMajor -lt 18) {
            Write-Error-Custom "Node.js 18+ is required, but found: $nodeVersion"
            exit 4
        }
        Write-Success "Node.js $nodeVersion ✓"
    }
    catch {
        Write-Error-Custom "Node.js is not installed or not in PATH"
        exit 4
    }
    
    # Check npm
    try {
        $npmVersion = & npm -v 2>$null
        if (-not $npmVersion) {
            throw "npm not found"
        }
        Write-Success "npm $npmVersion ✓"
    }
    catch {
        Write-Error-Custom "npm is not installed"
        exit 4
    }
    
    # Check frontend directory
    if (-not (Test-Path $FRONTEND_DIR -PathType Container)) {
        Write-Error-Custom "Frontend directory not found: $FRONTEND_DIR"
        exit 4
    }
    Write-Success "Frontend directory found ✓"
}

# ==============================================================================
# Install Dependencies
# ==============================================================================

function Install-Dependencies {
    Write-Info "Installing frontend dependencies..."
    
    Push-Location $FRONTEND_DIR
    try {
        & npm ci
        if ($LASTEXITCODE -ne 0) {
            Write-Error-Custom "Failed to install dependencies"
            exit 1
        }
        Write-Success "Dependencies installed ✓"
    }
    finally {
        Pop-Location
    }
}

# ==============================================================================
# Build Frontend
# ==============================================================================

function Build-Frontend {
    Write-Info "Building frontend..."
    
    Push-Location $FRONTEND_DIR
    try {
        & npm run build
        if ($LASTEXITCODE -ne 0) {
            Write-Error-Custom "Frontend build failed"
            exit 1
        }
        Write-Success "Frontend built ✓"
    }
    finally {
        Pop-Location
    }
}

# ==============================================================================
# Verify Build
# ==============================================================================

function Verify-Build {
    Write-Info "Verifying build artifacts..."
    
    $distPath = Join-Path $FRONTEND_DIR "dist"
    $indexPath = Join-Path $distPath "index.html"
    
    if (-not (Test-Path $distPath -PathType Container)) {
        Write-Error-Custom "Build directory not found: $distPath"
        exit 1
    }
    
    if (-not (Test-Path $indexPath -PathType Leaf)) {
        Write-Error-Custom "index.html not found in dist"
        exit 1
    }
    
    Write-Success "Build artifacts verified ✓"
}

# ==============================================================================
# Start Preview Server
# ==============================================================================

function Start-PreviewServer {
    if ($SKIP_SERVER) {
        Write-Info "Skipping preview server (-NoServer flag set)"
        return
    }
    
    Write-Info "Starting preview server on port $PREVIEW_PORT..."
    
    Push-Location $FRONTEND_DIR
    try {
        $logFile = Join-Path $env:TEMP "vite-preview.log"
        $PREVIEW_PROCESS = Start-Process -FilePath "npx" `
            -ArgumentList "vite", "preview", "--port", $PREVIEW_PORT, "--strictPort" `
            -RedirectStandardOutput $logFile `
            -RedirectStandardError $logFile `
            -PassThru `
            -WindowStyle Hidden
        
        if (-not $PREVIEW_PROCESS) {
            Write-Error-Custom "Failed to start preview server"
            exit 2
        }
    }
    finally {
        Pop-Location
    }
    
    Wait-ForServer
}

# ==============================================================================
# Wait for Server
# ==============================================================================

function Wait-ForServer {
    Write-Info "Waiting for preview server to be ready (max ${MAX_WAIT_TIME}s)..."
    
    $elapsed = 0
    $interval = 2
    
    while ($elapsed -lt $MAX_WAIT_TIME) {
        try {
            $response = Invoke-WebRequest -Uri "$PREVIEW_URL" -ErrorAction SilentlyContinue -TimeoutSec 2
            if ($response.StatusCode -eq 200) {
                Write-Success "Preview server is ready ✓"
                Test-ServerHealth
                return
            }
        }
        catch {
            # Server not ready yet
        }
        
        Start-Sleep -Seconds $interval
        $elapsed += $interval
        Write-Host "`r  Waiting... ${elapsed}s" -NoNewline
    }
    
    Write-Host ""
    Write-Error-Custom "Preview server failed to start within ${MAX_WAIT_TIME}s"
    
    $logFile = Join-Path $env:TEMP "vite-preview.log"
    if (Test-Path $logFile) {
        Write-Error-Custom "Server logs:"
        Get-Content $logFile | ForEach-Object { Write-Host "    $_" }
    }
    
    exit 2
}

# ==============================================================================
# Server Health Check
# ==============================================================================

function Test-ServerHealth {
    Write-Info "Performing health check on preview server..."
    
    try {
        $response = Invoke-WebRequest -Uri "$PREVIEW_URL/index.html" -ErrorAction SilentlyContinue
        $httpCode = $response.StatusCode
        $contentType = $response.Headers["Content-Type"] -join "; "
        
        if ($httpCode -ne 200) {
            Write-Error-Custom "Server returned HTTP $httpCode for /index.html"
            exit 2
        }
        
        if ($contentType -match "text/html") {
            Write-Success "Server health check passed (HTTP $httpCode, Content-Type: text/html) ✓"
        }
        else {
            Write-Warning "Unexpected Content-Type: $contentType"
        }
    }
    catch {
        Write-Error-Custom "Health check failed: $_"
        exit 2
    }
}

# ==============================================================================
# Run Cypress Tests
# ==============================================================================

function Invoke-CypressTests {
    Write-Info "Running Cypress E2E tests..."
    
    Push-Location $FRONTEND_DIR
    try {
        if ($HEADLESS) {
            & npm run e2e:headless
        }
        else {
            & npm run e2e
        }
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error-Custom "Cypress tests failed (exit code: $LASTEXITCODE)"
            exit 3
        }
        Write-Success "Cypress tests passed ✓"
    }
    finally {
        Pop-Location
    }
}

# ==============================================================================
# Report Artifacts
# ==============================================================================

function Report-Artifacts {
    Write-Info "Test artifacts:"
    
    $videosPath = Join-Path $FRONTEND_DIR "cypress/videos"
    if (Test-Path $videosPath -PathType Container) {
        $videoCount = @(Get-ChildItem $videosPath -Recurse -File).Count
        if ($videoCount -gt 0) {
            Write-Success "Videos: $videoCount file(s)"
        }
    }
    
    $screenshotsPath = Join-Path $FRONTEND_DIR "cypress/screenshots"
    if (Test-Path $screenshotsPath -PathType Container) {
        $screenshotCount = @(Get-ChildItem $screenshotsPath -Recurse -File).Count
        if ($screenshotCount -gt 0) {
            Write-Success "Screenshots: $screenshotCount file(s)"
        }
    }
}

# ==============================================================================
# Main Execution
# ==============================================================================

if ($Help) {
    Show-Help
    exit 0
}

Write-Host ""
Write-Info "==========================================="
Write-Info "MilodikFX Frontend E2E Test Runner"
Write-Info "==========================================="
Write-Host ""

Test-Requirements
Install-Dependencies

# Check if we need to build
if ($FORCE_BUILD -or -not (Test-Path "$FRONTEND_DIR/dist" -PathType Container)) {
    Build-Frontend
    Verify-Build
}
else {
    Write-Info "Build artifacts found, skipping build (use -Build to force rebuild)"
}

Start-PreviewServer
Invoke-CypressTests
Report-Artifacts

Write-Host ""
Write-Success "==========================================="
Write-Success "All E2E tests completed successfully! ✓"
Write-Success "==========================================="
Write-Host ""

exit 0
