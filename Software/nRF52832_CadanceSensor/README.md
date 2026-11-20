# Cadence Sensor V1.0 固件

目标板为 `cadence_sensor/nrf52832`。使用 NCS 3.2.4 和 ANT add-on 2.1.1，通过 Sysbuild 生成 MCUboot、`ble_loader` 和业务应用。业务保留 SC7A20H 采样、BLE NUS 和 ANT Bicycle Power 固定值测试。

## 构建与烧录

在此项目目录下运行 PowerShell。SDK、工具链和 ANT 模块路径可用 `-SdkRoot`、`-ToolchainRoot`、`-AntModuleDir` 指定。

```powershell
.\scripts\build.ps1
$ncsPython = 'C:/ncs/toolchains/fd21892d0f/opt/bin/python.exe'
& $ncsPython -m pyocd list --probes
& $ncsPython .\scripts\probe.py flash --probe <探针ID> --hex .\build\merged.hex
```

将 `<探针ID>` 替换为实际 ID。`flash` 按页擦除、烧录并读回校验。MCUboot 日志走 RTT，业务日志走 BLE NUS。

## 启动与升级

MCUboot 位于 `0x00000`，业务槽位于 `0x10000`，常驻 Loader 位于 `0x4C000`，参数区位于 `0x7C000`。SW5 按住复位、业务镜像无效或通过 NUS 发送 `OTA ENTER` 都可进入 Loader。

Loader 以 `CadenceDFU` 广播 SMP，上传 `build/nRF52832_CadanceSensor/zephyr/zephyr.signed.bin` 后复位。系统覆盖单一业务槽，MCUboot 在启动业务前校验签名。当前构建使用 SDK 开发签名密钥和 ANT 评估授权。

ADS1220、电池采样、蜂鸣器和 nPM1100 信号由板级设备树描述。现有业务采样和 ANT 测试数据沿用 Demo，ANT 当前发送固定的 250 W 和 90 rpm。
