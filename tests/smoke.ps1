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

$requiredKeys = @(
    'audio.deviceStateXml',
    'dsp.cleanBoost.enabled',
    'dsp.cleanBoost.gainDb',
    'dsp.overdrive.enabled',
    'dsp.overdrive.drivePct',
    'dsp.overdrive.levelPct',
    'dsp.eq.enabled',
    'dsp.eq.bassDb',
    'dsp.eq.midDb',
    'dsp.eq.trebleDb',
    'ui.preset.selectedName'
)

foreach ($k in $requiredKeys) {
    $hasKey = Select-String -Path $settingsPath -Pattern $k -SimpleMatch -Quiet
    if (-not $hasKey) {
        Write-Error "$k not found in settings file"
        exit 1
    }
}

$presetDir = Join-Path $env:APPDATA 'MilodikFX\Presets'
$defaultPreset = Join-Path $presetDir 'Default Clean.json'
if (-not (Test-Path $defaultPreset)) {
    Write-Error "Default preset not created: $defaultPreset"
    exit 1
}

Write-Host "Smoke test passed"
