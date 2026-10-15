# Compile only; uses the source-compatible isolated V1.4 toolchain.
param([int]$Jobs = 8)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$workspace = Split-Path -Parent $root
$environment = Join-Path $workspace 'vendor\clock_rom'
$config = Join-Path $environment 'arduino-cli.yaml'
if (-not (Test-Path $config)) { throw 'Run py -3 .\clock_rom\prepare.py first.' }
$buildRoot = Join-Path $workspace 'build\clock_rom'
$output = Join-Path $buildRoot ('run_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fffffff'))
$logs = Join-Path $workspace 'build\logs\clock_rom'
New-Item -ItemType Directory -Force -Path $output,$logs | Out-Null
$log = Join-Path $logs ('compile_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.log')
$fqbn = 'esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio'
# Old GCC is not long-path aware. Give only our isolated environment a short,
# temporary drive alias; never replace a user's mapping. Remove it in finally.
$drive = @('Z:','Y:','X:','W:','V:','U:') | Where-Object { -not (Test-Path "$_\") } | Select-Object -First 1
if (-not $drive) { throw 'No unused temporary drive letter available.' }
& subst.exe $drive $environment
if ($LASTEXITCODE -ne 0) { throw 'Cannot create temporary compiler path alias.' }
try {
  $shortConfig = Join-Path $logs ('cli_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.yaml')
  @("directories:","  data: '$drive/data'","  downloads: '$drive/downloads'","  user: '$drive/user'") | Set-Content -LiteralPath $shortConfig -Encoding ASCII
  & py -3 (Join-Path $root 'embed_page.py')
  $ErrorActionPreference = 'Continue'
  & 'C:\Program Files\Arduino CLI\arduino-cli.exe' --config-file $shortConfig compile --fqbn $fqbn --libraries "$drive/libraries" --build-path $output --jobs $Jobs $root 2>&1 | Tee-Object -FilePath $log
  if ($LASTEXITCODE -ne 0) { throw "Compile failed; preserve log: $log" }
} finally {
  & subst.exe $drive /d
}
& py -3 (Join-Path $root 'verify.py') --build-dir $output
if ($LASTEXITCODE -ne 0) { throw 'Offline image verification failed. Do not flash.' }
Write-Host 'Compile passed. Nothing uploaded; this script does not change the running firmware.'
