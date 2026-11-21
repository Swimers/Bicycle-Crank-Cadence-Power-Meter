# Cadence Sensor V1.0 固件

目标板为 `cadence_sensor/nrf52832`。使用 NCS 3.2.4 通过 Sysbuild 生成 MCUboot、`ble_loader` 和业务应用。

业务以 100 Hz 读取 SC7A20H，对三轴做滤波和周期检测，识别连续转动后报告整圈事件。对确认后的圈周期做低通滤波，并将估计的事件时间约束在检测时刻之前最多 80 ms。停转或采样故障时保持最后一次事件，重新识别整圈后重新初始化滤波。将传感器固定在曲柄上使用，检测范围约 20–240 rpm。

在码表或手机应用中连接 BLE 踏频传感器 `CadenceSensor`，订阅 CSC 服务 `0x1816` 的测量特征 `0x2A5B`。订阅后，固件每 100 ms 尝试发送一次通知，实际接收间隔取决于对端协商的连接参数和链路。测量包带曲柄标志 `0x02`、累计圈数和最后一圈事件时间。后两个字段各为 16 位小端整数，时间单位为 1/1024 秒。接收端用相邻有效事件计算 rpm。

## 电量与低电保护

上电或复位时测一次电池电压，之后每 60 秒测一次。采样时短暂打开电池分压，丢弃首个转换并平均随后的 8 次读数，退出采样后关闭分压。BLE 电池服务 `0x180F` 的 Battery Level 特征 `0x2A19` 每轮有效采样通知一次，并可随时读取最近一次有效估计。首次成功采样前读取返回错误。

电量由单节 4.2 V 锂聚合物电池的电压曲线估算，范围为 1–100%。曲线以 4.20 V 对应 100%、3.60 V 对应 20%、3.50 V 对应 10%，可按实测电芯和负载修正。

启动时，电量至少 20% 响两声短音，10–19% 响三声短音，低于 10% 响一声长音后通过 P0.22 请求 nPM1100 进入 Ship mode。运行中每分钟判断一次低电量，10–19% 响三声，低于 10% 响一声长音并请求关机。无效读数跳过低电告警和关机，启动仍提示两声。Ship mode 依赖 VBUS 断开，接入 USB 可退出。

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

运行新版业务固件时，可用 [BLE OTA 工具](../../Tools/BLE_OTA/README.md) 的“进入 Loader”按钮，或执行 `python ota.py enter --address <设备地址>`。设备只支持一个 BLE 连接，操作前先断开码表等正在连接的设备。

从通用 GATT 客户端操作时，连接 `CadenceSensor`，在升级服务 `85b8f1a0-c8ed-4be7-a48f-47d72e308066` 的控制特征 `85b8f1a1-c8ed-4be7-a48f-47d72e308066` 中，用 **Write Request（有响应写入）**发送完整 ASCII 字符串 `OTA ENTER`，不带换行。写入成功后约 500 ms 重启。该特征无需配对，已连接的客户端可发起请求。

Loader 以 `CadenceDFU` 广播 SMP，上传 `build/nRF52832_CadanceSensor/zephyr/zephyr.signed.bin` 后复位。系统覆盖单一业务槽，MCUboot 在启动业务前校验签名。当前构建使用 SDK 开发签名密钥。
