param(
    [Parameter(Mandatory = $true)]
    [string]$AppPath,
    [string]$BuildDir,
    [int]$WaitSeconds = 15
)

$ErrorActionPreference = 'Stop'

$asioEnabled = $false
if (-not [string]::IsNullOrEmpty($BuildDir)) {
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if (Test-Path $cache) {
        $asioEnabled = Select-String -Path $cache -Pattern 'MILODIKFX_ENABLE_ASIO:BOOL=ON' -SimpleMatch -Quiet
    }
}

if (-not $asioEnabled) {
    Write-Host "ASIO build not enabled. Skipping ASIO smoke test."
    exit 0
}

$asioRegPaths = @(
    'HKLM:\SOFTWARE\ASIO',
    'HKLM:\SOFTWARE\WOW6432Node\ASIO'
)

$asioDrivers = @()
foreach ($path in $asioRegPaths) {
    if (Test-Path $path) {
        $asioDrivers += Get-ChildItem -Path $path -ErrorAction SilentlyContinue
    }
}

if ($asioDrivers.Count -eq 0) {
    Write-Host "ASIO drivers not found. Skipping ASIO smoke test."
    exit 0
}

if (-not (Test-Path $AppPath)) {
    Write-Error "App not found: $AppPath"
    exit 1
}

$settingsPath = Join-Path $env:APPDATA 'MilodikFX\MilodikFX.settings'

if (Test-Path $settingsPath) {
    Remove-Item -Force $settingsPath -ErrorAction SilentlyContinue
}

$proc = Start-Process -FilePath $AppPath -PassThru

try {
    Start-Sleep -Seconds $WaitSeconds
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -ErrorAction Stop
    }
} finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue
    }
}

Start-Sleep -Seconds 1

if (-not (Test-Path $settingsPath)) {
    Write-Error "Settings file not created: $settingsPath"
    exit 1
}

$hasAsioType = Select-String -Path $settingsPath -Pattern 'deviceType="ASIO"' -SimpleMatch -Quiet

if (-not $hasAsioType) {
    Write-Error "ASIO device type not selected. Ensure ASIO device is available and JUCE_ASIO is enabled."
    exit 1
}

Write-Host "ASIO smoke test passed"
