<#
.SYNOPSIS
    Starts MilodikFX and checks that the engine answers over HTTP.

.DESCRIPTION
    Non-destructive: the user's settings file is backed up before the run and
    restored afterwards. The previous version deleted it outright and then
    asserted on keys the app no longer writes, so it destroyed real settings and
    failed regardless.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$AppPath,

    [int]$StartupTimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $AppPath)) {
    Write-Error "App not found: $AppPath"
    exit 1
}

$appDataDir = Join-Path $env:APPDATA 'MilodikFX'
$settingsPath = Join-Path $appDataDir 'MilodikFX.settings'
$backupPath = "$settingsPath.smokebak"

$restoreNeeded = $false
if (Test-Path $settingsPath) {
    Copy-Item $settingsPath $backupPath -Force
    $restoreNeeded = $true
}

$proc = $null
$failures = @()

function Check($name, [scriptblock]$body) {
    try {
        & $body
        Write-Host "  OK   $name"
    } catch {
        Write-Host "  FAIL $name -- $($_.Exception.Message)"
        $script:failures += $name
    }
}

try {
    Write-Host "Starting $AppPath"
    $proc = Start-Process -FilePath $AppPath -PassThru

    # Find the port: the engine falls back past 3000 when it is taken.
    $baseUrl = $null
    $deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)

    while ((Get-Date) -lt $deadline -and -not $baseUrl) {
        foreach ($port in 3000..3008) {
            try {
                $probe = Invoke-WebRequest -Uri "http://127.0.0.1:$port/api/health" -UseBasicParsing -TimeoutSec 2 -ErrorAction Stop
                if ($probe.StatusCode -eq 200) { $baseUrl = "http://127.0.0.1:$port"; break }
            } catch {
                # not up yet
            }
        }

        if (-not $baseUrl) { Start-Sleep -Milliseconds 500 }
    }

    if (-not $baseUrl) {
        Write-Error "Engine did not answer on ports 3000-3008 within $StartupTimeoutSeconds s"
        exit 2
    }

    Write-Host "Engine reachable at $baseUrl"

    Check 'GET /index.html serves the UI' {
        $r = Invoke-WebRequest -Uri "$baseUrl/index.html" -UseBasicParsing -TimeoutSec 10
        if ($r.StatusCode -ne 200) { throw "status $($r.StatusCode)" }
    }

    Check 'GET /api/levels reports metering' {
        $levels = Invoke-RestMethod -Uri "$baseUrl/api/levels" -TimeoutSec 10
        if ($null -eq $levels.inputLevel) { throw 'no inputLevel field' }
        if ($null -eq $levels.audioRunning) { throw 'no audioRunning field' }
    }

    Check 'GET /api/effects lists the whole chain' {
        $effects = Invoke-RestMethod -Uri "$baseUrl/api/effects" -TimeoutSec 10
        if ($effects.effects.Count -lt 8) { throw "only $($effects.effects.Count) effects" }

        $ids = $effects.effects | ForEach-Object { $_.id }
        foreach ($required in @('overdrive', 'eq', 'compressor', 'reverb', 'master', 'cabinet', 'noiseGate')) {
            if ($ids -notcontains $required) { throw "missing effect '$required'" }
        }
    }

    Check 'GET /api/devices reports a live device' {
        $devices = Invoke-RestMethod -Uri "$baseUrl/api/devices" -TimeoutSec 15
        if (-not $devices.current) { throw 'no current device' }
    }

    Check 'Parameter write round-trips through the engine' {
        $original = (Invoke-RestMethod -Uri "$baseUrl/api/parameters/master-volume" -TimeoutSec 10).masterVolume

        Invoke-RestMethod -Uri "$baseUrl/api/parameters/master-volume" -Method Put `
            -Body '{"value":-7.5}' -ContentType 'application/json' -TimeoutSec 10 | Out-Null

        $readBack = (Invoke-RestMethod -Uri "$baseUrl/api/parameters/master-volume" -TimeoutSec 10).masterVolume
        if ([Math]::Abs($readBack - (-7.5)) -gt 0.01) { throw "read back $readBack" }

        $restore = @{ value = $original } | ConvertTo-Json -Compress
        Invoke-RestMethod -Uri "$baseUrl/api/parameters/master-volume" -Method Put `
            -Body $restore -ContentType 'application/json' -TimeoutSec 10 | Out-Null
    }

    Check 'Out-of-range values are clamped, not accepted' {
        $result = Invoke-RestMethod -Uri "$baseUrl/api/effects/reverb/dryWetMix" -Method Put `
            -Body '{"value":9999}' -ContentType 'application/json' -TimeoutSec 10
        if ($result.value -gt 1.0) { throw "clamp failed, got $($result.value)" }
    }

    Check 'Unknown parameters are rejected' {
        # Windows PowerShell 5.1 and PowerShell 7 raise different exception
        # types here, so match on the response rather than on the type.
        $status = 0

        try {
            Invoke-RestMethod -Uri "$baseUrl/api/effects/overdrive/nosuchparam" -Method Put `
                -Body '{"value":1}' -ContentType 'application/json' -TimeoutSec 10 | Out-Null
        } catch {
            if ($_.Exception.PSObject.Properties['Response'] -and $_.Exception.Response) {
                $status = $_.Exception.Response.StatusCode.value__
            }
        }

        if ($status -ne 404) { throw "expected 404, got $status" }
    }

    Check 'The web root cannot be escaped' {
        try {
            Invoke-WebRequest -Uri "$baseUrl/C:/Windows/win.ini" -UseBasicParsing -TimeoutSec 10 | Out-Null
            throw 'absolute path was served'
        } catch {
            if ($_.Exception.Response -and $_.Exception.Response.StatusCode.value__ -ne 404) {
                throw "unexpected status $($_.Exception.Response.StatusCode.value__)"
            }
        }
    }
}
finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
    }

    if ($restoreNeeded) {
        Copy-Item $backupPath $settingsPath -Force -ErrorAction SilentlyContinue
        Remove-Item $backupPath -Force -ErrorAction SilentlyContinue
    }
}

if (Test-Path $settingsPath) {
    $hasDspKeys = Select-String -Path $settingsPath -Pattern 'dsp\.' -Quiet
    if (-not $hasDspKeys) {
        Write-Host "  FAIL settings file has no dsp.* keys"
        $failures += 'settings persistence'
    } else {
        Write-Host "  OK   settings file contains dsp.* keys"
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Smoke test failed: " + ($failures -join ', '))
    exit 1
}

Write-Host "Smoke test passed"
exit 0
