param(
    [Parameter(Mandatory = $true)]
    [string]$AppPath,
    [int]$WaitSeconds = 12
)

$ErrorActionPreference = 'Stop'

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

$hasAudioKey = Select-String -Path $settingsPath -Pattern 'audio.deviceStateXml' -SimpleMatch -Quiet

if (-not $hasAudioKey) {
    Write-Error "audio.deviceStateXml not found in settings file"
    exit 1
}

Write-Host "Smoke test passed"
