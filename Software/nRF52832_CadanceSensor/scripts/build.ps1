param(
    [string]$SdkRoot = 'D:/ncs/v3.2.4',
    [string]$ToolchainRoot = 'C:/ncs/toolchains/fd21892d0f',
    [switch]$Pristine,
    [string[]]$ExtraCmakeArgs = @()
)
$ErrorActionPreference = 'Stop'
$appRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.Replace('\', '/')
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path.Replace('\', '/')
$ToolchainRoot = (Resolve-Path -LiteralPath $ToolchainRoot).Path.Replace('\', '/')
if ((Get-Content -LiteralPath "$SdkRoot/nrf/VERSION" -Raw).Trim() -ne '3.2.4') {
    throw 'This project requires NCS 3.2.4.'
}
$envSpec = Get-Content (Join-Path $ToolchainRoot 'environment.json') -Raw | ConvertFrom-Json
foreach ($entry in $envSpec.env_vars) {
    if ($entry.type -eq 'relative_paths') {
        $value = ($entry.values | ForEach-Object { Join-Path $ToolchainRoot $_ }) -join ';'
        if ($entry.existing_value_treatment -eq 'prepend_to') {
            $value += ';' + [Environment]::GetEnvironmentVariable($entry.key, 'Process')
        }
    } else { $value = $entry.value }
    [Environment]::SetEnvironmentVariable($entry.key, $value, 'Process')
}
$env:ZEPHYR_BASE = "$SdkRoot/zephyr"
$env:PYTHONUTF8 = '1'
$buildPath = "$appRoot/build"
$buildArgs = @('build', '--sysbuild', '-b', 'cadence_sensor/nrf52832', '-d', $buildPath, '-o=-j4')
if ($Pristine) { $buildArgs += @('-p', 'always') }
$buildArgs += @($appRoot, '--', "-DBOARD_ROOT=$appRoot", "-DDTS_ROOT=$appRoot")
$buildArgs += $ExtraCmakeArgs
& "$ToolchainRoot/opt/bin/python.exe" -m west @buildArgs
if ($LASTEXITCODE -ne 0) { throw "west build failed ($LASTEXITCODE)" }
