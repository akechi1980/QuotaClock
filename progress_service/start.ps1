param(
    [string]$BindAddress = '0.0.0.0',
    [ValidateRange(1, 65535)][int]$Port = 8766,
    [switch]$SetupOnly
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$script = Join-Path $root 'server.py'
$config = Join-Path $root 'config.private.json'
$python = Join-Path $root '.venv\Scripts\python.exe'

try {
    if (-not (Test-Path -LiteralPath $python)) {
        Write-Host 'Creating local Python virtual environment...'
        if (Get-Command py -ErrorAction SilentlyContinue) {
            & py -3 -m venv (Join-Path $root '.venv')
        } elseif (Get-Command python -ErrorAction SilentlyContinue) {
            & python -m venv (Join-Path $root '.venv')
        } else { throw 'Install Python 3 (with venv support) first.' }
        if ($LASTEXITCODE -ne 0) { throw 'Virtual environment creation failed.' }
    }
    & $python -c 'import sys; assert sys.version_info >= (3, 9)'
    if ($LASTEXITCODE -ne 0) { throw 'Python environment check failed.' }
    if ($SetupOnly) { Write-Host 'Environment ready (stdlib only; no pip dependencies).'; exit 0 }
    if (-not (Test-Path -LiteralPath $config)) {
        [System.IO.File]::Copy((Join-Path $root 'config.example.json'), $config, $false)
        throw 'Created config.private.json. Fill provider tokens locally, then run again.'
    }
    # Check syntax/presence only; never print private configuration or secrets.
    @'
import json, sys
try:
    with open(sys.argv[1], encoding='utf-8-sig') as f:
        c = json.load(f)
    assert c['openai']['access_token'].strip() and c['cursor']['session_token'].strip()
except Exception:
    sys.exit(1)
'@ | & $python - $config
    if ($LASTEXITCODE -ne 0) { throw 'Private configuration invalid or tokens empty.' }

    # Match the full absolute script path, not just python.exe or the port number.
    $pattern = '(?i)(?:"' + [regex]::Escape($script) + '"|(?<!\S)' + [regex]::Escape($script) + '(?=\s|$))'
    $owned = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^python(?:w|\d+(?:\.\d+)*)?\.exe$' -and $_.CommandLine -match $pattern
    })
    $listeners = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)
    foreach ($listener in $listeners) {
        if ($listener.OwningProcess -notin @($owned.ProcessId)) {
            throw "Port $Port belongs to an unrecognized process. Stop that service manually; nothing was killed."
        }
    }
    foreach ($process in $owned) {
        $current = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.ProcessId)"
        if ($current -and $current.CommandLine -match $pattern -and $current.CreationDate -eq $process.CreationDate) {
            Stop-Process -Id $process.ProcessId -ErrorAction Stop
            Wait-Process -Id $process.ProcessId -Timeout 10 -ErrorAction SilentlyContinue
            Write-Host "Stopped previous local service PID $($process.ProcessId)"
        }
    }
    Write-Host "Starting real quota service on ${BindAddress}:$Port (trusted LAN only)."
    $pythonw = Join-Path $root '.venv\Scripts\pythonw.exe'
    if (-not (Test-Path -LiteralPath $pythonw)) { throw 'pythonw.exe missing; recreate the virtual environment.' }
    $runtime = Join-Path $root '.runtime'
    [System.IO.Directory]::CreateDirectory($runtime) | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fffffff'
    $stdout = Join-Path $runtime "$stamp.stdout.log"
    $stderr = Join-Path $runtime "$stamp.stderr.log"
    # Credentials remain in the private file, never in arguments or launcher logs.
    if ($BindAddress -notmatch '^[a-zA-Z0-9.:-]+$') { throw 'Invalid bind address.' }
    $arguments = '-u "' + $script + '" --host ' + $BindAddress + ' --port ' + $Port + ' --config "' + $config + '"'
    $child = Start-Process -FilePath $pythonw -ArgumentList $arguments -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    Start-Sleep -Milliseconds 800
    $child.Refresh()
    if ($child.HasExited) { throw "Service exited during startup. See $stderr" }
    Write-Host 'Started in background. This window can close; use stop.cmd to stop. No firewall changes.'
    Write-Host "Logs: $runtime"
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
