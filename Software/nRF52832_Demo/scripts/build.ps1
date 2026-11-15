param(
    [string]$BuildDir = 'build_ota',
    [string]$SdkRoot = 'D:/ncs/v3.4.1',
    [string]$ToolchainRoot = 'C:/ncs/toolchains/4f5b6ad6dd',
    [switch]$Pristine,
    [string[]]$ExtraCmakeArgs = @()
)
$ErrorActionPreference = 'Stop'
$appRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.Replace('\', '/')
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
$buildPath = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { "$appRoot/$BuildDir" }
$buildArgs = @('build', '--sysbuild', '-b', 'cadence_sensor/nrf52832', '-d', $buildPath, '-o=-j4')
if ($Pristine) { $buildArgs += @('-p', 'always') }
$buildArgs += @($appRoot, '--', "-DBOARD_ROOT=$appRoot", "-DDTS_ROOT=$appRoot")
$buildArgs += $ExtraCmakeArgs
& "$ToolchainRoot/opt/bin/python.exe" -m west @buildArgs
if ($LASTEXITCODE -ne 0) { throw "west build failed ($LASTEXITCODE)" }
& "$ToolchainRoot/opt/bin/python.exe" "$appRoot/scripts/package.py" $buildPath
if ($LASTEXITCODE -ne 0) { throw "OTA packaging failed ($LASTEXITCODE)" }
