$ErrorActionPreference = 'Stop'
try {
    $script = Join-Path $PSScriptRoot 'server.py'
    $pattern = '(?i)(?:"' + [regex]::Escape($script) + '"|(?<!\S)' + [regex]::Escape($script) + '(?=\s|$))'
    $owned = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^python(?:w|\d+(?:\.\d+)*)?\.exe$' -and $_.CommandLine -match $pattern
    })
    foreach ($process in $owned) {
        $current = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.ProcessId)"
        if ($current -and $current.CommandLine -match $pattern -and $current.CreationDate -eq $process.CreationDate) {
            Stop-Process -Id $process.ProcessId -ErrorAction Stop
            Wait-Process -Id $process.ProcessId -Timeout 10 -ErrorAction SilentlyContinue
            Write-Host "Stopped local quota service PID $($process.ProcessId)"
        }
    }
    if (-not $owned.Count) { Write-Host 'Local quota service is already stopped.' }
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
