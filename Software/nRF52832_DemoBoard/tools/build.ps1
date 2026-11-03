param(
    [string]$SdkRoot = 'D:/ncs/v3.4.1',
    [string]$ToolchainRoot = 'C:/ncs/toolchains/4f5b6ad6dd'
)
$boardName = 'nrf52832_proto'
$ErrorActionPreference = 'Stop'
$applicationRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$toolchainEnvironment = Get-Content -LiteralPath (Join-Path $ToolchainRoot 'environment.json') -Raw | ConvertFrom-Json
foreach ($entry in $toolchainEnvironment.env_vars) {
    if ($entry.type -eq 'relative_paths') {
        $value = ($entry.values | ForEach-Object { Join-Path $ToolchainRoot $_ }) -join ';'
    } else { $value = $entry.value }
    if ($entry.existing_value_treatment -eq 'prepend_to') {
        $value += ';' + [Environment]::GetEnvironmentVariable($entry.key, 'Process')
    }
    [Environment]::SetEnvironmentVariable($entry.key, $value, 'Process')
}
$env:ZEPHYR_BASE = Join-Path $SdkRoot 'zephyr'
$pythonExe = Join-Path $ToolchainRoot 'opt/bin/python.exe'
Push-Location $SdkRoot
try {
    & $pythonExe -m west build --no-sysbuild -b "$boardName/nrf52832" -d (Join-Path $applicationRoot 'build') $applicationRoot -- -DBOARD_ROOT="$applicationRoot" -DDTS_ROOT="$applicationRoot"
    if ($LASTEXITCODE -ne 0) { throw "west build failed: $LASTEXITCODE" }
} finally { Pop-Location }
