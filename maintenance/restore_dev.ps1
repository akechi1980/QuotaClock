# Recreate only the isolated Arduino toolchain; never connect to or flash a device.
param([switch]$InstallCore)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$environment = Join-Path $workspace 'vendor\clock_rom'
$libraries = Join-Path $environment 'libraries'
if (-not (Test-Path -LiteralPath $libraries)) { throw 'Extract the bundled library sources first.' }
$data = Join-Path $environment 'data'
$downloads = Join-Path $environment 'downloads'
$user = Join-Path $environment 'user'
foreach ($dir in @($data, $downloads, $user)) { [IO.Directory]::CreateDirectory($dir) | Out-Null }
$config = Join-Path $environment 'arduino-cli.yaml'
$settings = [ordered]@{
    directories = [ordered]@{ data = $data; downloads = $downloads; user = $user }
    board_manager = @{ additional_urls = @('https://espressif.github.io/arduino-esp32/package_esp32_index.json') }
}
# JSON is valid YAML; regenerate machine-specific paths instead of copying old ones.
$settings | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $config -Encoding UTF8
Write-Host "Isolated configuration ready: $config"
if ($InstallCore) {
    $cli = 'C:\Program Files\Arduino CLI\arduino-cli.exe'
    if (-not (Test-Path -LiteralPath $cli)) { throw 'Install Arduino CLI at C:\Program Files\Arduino CLI (the build script uses this path).' }
    & $cli --config-file $config core update-index
    if ($LASTEXITCODE -ne 0) { throw 'Cannot update the isolated package index.' }
    & $cli --config-file $config core install 'esp32:esp32@2.0.12'
    if ($LASTEXITCODE -ne 0) { throw 'Cannot install the isolated core.' }
    Write-Host 'Isolated core 2.0.12 installed; global Arduino cores untouched. Nothing flashed.'
}
