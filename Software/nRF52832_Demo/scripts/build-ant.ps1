param(
    [string]$AntSdkRoot = 'D:/ncs/ant-v2.1.1',
    [string]$ToolchainRoot = 'D:/ncs/toolchains/toolchains/fd21892d0f',
    [switch]$Pristine
)
$ErrorActionPreference = 'Stop'
$appRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.Replace('\', '/')
$sdkRoot = (Resolve-Path -LiteralPath $AntSdkRoot).Path.Replace('\', '/')
if (-not (Test-Path "$sdkRoot/ant/zephyr/module.yml")) {
    throw 'Install the official ANT add-on v2.1.1 workspace first.'
}
$buildParams = @{
    BuildDir = 'build_ant'
    SdkRoot = $sdkRoot
    ToolchainRoot = $ToolchainRoot
    ExtraCmakeArgs = @(
        "-DZEPHYR_EXTRA_MODULES=$sdkRoot/ant",
        "-DnRF52832_Demo_EXTRA_CONF_FILE=$appRoot/ant.conf"
    )
}
if ($Pristine) { $buildParams.Pristine = $true }
& "$PSScriptRoot/build.ps1" @buildParams
