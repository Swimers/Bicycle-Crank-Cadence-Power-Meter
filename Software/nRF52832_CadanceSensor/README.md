# Cadence Sensor V1.0 固件

目标板为 `cadence_sensor/nrf52832`。使用 NCS 3.2.4 通过 Sysbuild 生成 MCUboot、`ble_loader` 和业务应用。上电后蜂鸣器用 4 kHz 发出两声 100 ms 的提示音，间隔 100 ms。

业务以 100 Hz 读取 SC7A20H，对三轴做滤波和周期检测，识别连续转动后报告整圈事件。将传感器固定在曲柄上使用。当前检测范围约 20–240 rpm，停转或采样故障时保持最后一次事件。

在码表或手机应用中连接 BLE 踏频传感器 `CadenceSensor`，订阅 CSC 服务 `0x1816` 的测量特征 `0x2A5B`。订阅后，固件每 100 ms 尝试发送一次通知，实际接收间隔取决于对端协商的连接参数和链路。测量包带曲柄标志 `0x02`、累计圈数和最后一圈事件时间。后两个字段各为 16 位小端整数，时间单位为 1/1024 秒。接收端用相邻有效事件计算 rpm。

## 构建与烧录

在此项目目录下运行 PowerShell。SDK 和工具链路径可用 `-SdkRoot`、`-ToolchainRoot` 指定。

```powershell
.\scripts\build.ps1
$ncsPython = 'C:/ncs/toolchains/fd21892d0f/opt/bin/python.exe'
& $ncsPython -m pyocd list --probes
& $ncsPython .\scripts\probe.py flash --probe <探针ID> --hex .\build\merged.hex
```

将 `<探针ID>` 替换为实际 ID。`flash` 按页擦除、烧录并读回校验。MCUboot 日志走 RTT。

## 启动与升级

MCUboot 位于 `0x00000`，业务槽位于 `0x10000`，常驻 Loader 位于 `0x4C000`，参数区位于 `0x7C000`。按住 SW5 并复位可手动进入 Loader。业务镜像无效时，MCUboot 自动进入 Loader。

Loader 以 `CadenceDFU` 广播 SMP，上传 `build/nRF52832_CadanceSensor/zephyr/zephyr.signed.bin` 后复位。系统覆盖单一业务槽，MCUboot 在启动业务前校验签名。当前构建使用 SDK 开发签名密钥。
