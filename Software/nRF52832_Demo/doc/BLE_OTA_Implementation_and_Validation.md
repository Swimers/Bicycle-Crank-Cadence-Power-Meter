# MCUboot 单业务槽 OTA：实现与实机验证

历史实机验证日期：2026-11-15，环境为 NCS v3.4.1 / Zephyr 4.4.2。工程：`Software/nRF52832_Demo`。

2026-11-16 起，项目构建入口已统一为 NCS 3.2.4 / `build`，同时启用 ANT、BLE、OTA。以下操作命令已同步到当前路径；原有实机结果和占用数据是历史记录，不代表已重新验证当前固件的无线功能。当前构建方式见 [项目说明](../README.md)。

## 结果

已实现并通过 fireDAP 烧录 MCUboot、独立 BLE Loader 和业务应用；在本机 Windows BLE 适配器上完成真实 SMP OTA，将业务固件从 **1.0.0 更新为 1.0.1**。异常测试后，已再次通过 BLE 恢复有效的 1.0.1，开发板最终运行正常业务应用。

当前为开发验证配置：使用 SDK 自带 ECDSA P-256 测试密钥，SMP 连接未要求配对加密。发布前需要配置自有签名密钥，并确定升级入口的访问控制。

## 实际实现

- `Kconfig.sysbuild` 注册工程内 `ble_loader`，源码位于 `loader/`；不修改 SDK 源码。
- `sysbuild.conf` 选择 firmware updater 模式，统一构建 MCUboot、业务应用和 Loader。
- 板级公共分区文件 `cadence_sensor_partitions.dtsi` 定义固定 Flash 布局和 GPREGRET 启动标志。
- `sysbuild/mcuboot.conf` 启用完整镜像验证、按键入口、业务镜像无效时进入 Loader。
- `loader/` 使用 SoftDevice Controller 和 SMP over BLE；启用 ZCBOR、Flash/image management、分包重组。
- Loader 在启动 BLE 前设置 BPROT，保护 MCUboot、Loader、参数区；仅业务槽允许写入。
- Loader 拦截所有独立 image erase 命令；业务上传使用渐进擦除。已实测对 slot1 的擦除请求返回错误。
- 业务应用增加 `VERSION`、`OTA ENTER` 行命令，原文本回显、NUS 和传感器采集保留。
- `VERSION` 定义当前版本为 1.0.1。
- 板级增加 pyOCD runner；`scripts/probe.py` 提供备份、按页烧录、读回比对和运行状态检查。

| 分区 | 地址范围，结束地址不含 | 大小 |
|---|---|---:|
| MCUboot | `0x00000–0x10000` | 64 KB |
| 业务 slot0 | `0x10000–0x4C000` | 240 KB |
| Loader slot1 | `0x4C000–0x7C000` | 192 KB |
| 参数区 | `0x7C000–0x80000` | 16 KB |

初始预算已能容纳实际镜像。本轮保留该布局，没有为增加业务容量再次调整地址。

| 镜像 | 链接器 Flash 占用 | 静态 RAM 占用 |
|---|---:|---:|
| MCUboot | 23,396 B | 13,200 B |
| 业务应用 | 111,740 B | 25,352 B |
| BLE Loader | 114,808 B | 31,628 B |

静态 RAM 不代表运行峰值。三个程序分阶段运行，不相加计算 RAM。业务签名 BIN 为 111,891 B。

## 编译与 SWD 烧录

以下命令在仓库根目录的 PowerShell 中执行：

```powershell
Set-Location D:\Documents\GitHub\CadenceSensor
& .\Software\nRF52832_Demo\scripts\build.ps1 -Pristine
```

`build.ps1` 默认 SDK 为 `D:/ncs/v3.2.4`，工具链为 `C:/ncs/toolchains/fd21892d0f`，另行加载 ANT add-on 2.1.1；可用 `-SdkRoot`、`-ToolchainRoot`、`-AntModuleDir` 覆盖。后续增量构建省略 `-Pristine`。

统一 sysbuild 在所有镜像编译后运行 `package.py`（脚本及 IDE 均执行），检查最终有效 Kconfig，包括 MCUboot 验签、Loader 的 SMP/ZCBOR 和保护回调，合并三个 HEX，并拒绝地址内容冲突。

| 输出 | 用途 |
|---|---|
| `build/merged.hex` | 初次 SWD 烧录整个启动链 |
| `build/dfu_application.zip` | SDK 生成的业务更新包；本轮未实测手机 ZIP 导入流程 |
| `build/nRF52832_Demo/zephyr/zephyr.signed.bin` | 本机已验证的 SMP 业务 OTA 镜像 |
| `build/ota_artifacts.json` | 各镜像 HEX 地址段、文件 SHA-256 |

通过已连接 fireDAP 烧录并读回比对：

```powershell
$ncsPython = 'C:/ncs/toolchains/fd21892d0f/opt/bin/python.exe'
& $ncsPython Software/nRF52832_Demo/scripts/probe.py flash --hex Software/nRF52832_Demo/build/merged.hex
```

本机探针 ID 为 `0123456789abcdef`，其他探针使用 `--probe <ID>`。脚本选择 `nrf52832`，SWD 1 MHz，禁用自动解锁，按页擦除，不整片擦除。首次烧录前已备份原 512 KB Flash 和 UICR 至 `tmp/ota-validation/before/`。

开发中不要把业务裸 HEX 烧录到地址 0，也不要使用整片擦除破坏固定 Loader 和参数。

## BLE 使用

PC 工具为 `Tools/BLE_OTA/ota.py`，依赖见同目录 `requirements.txt`。本轮使用现有 BLE_NUS 隔离环境，额外安装 cbor2：

```powershell
$blePython = 'Tools/BLE_NUS/.venv/Scripts/python.exe'
& $blePython -m pip install -r Tools/BLE_OTA/requirements.txt
& $blePython Tools/BLE_OTA/ota.py scan
```

本机开发板 BLE 地址为 `E6:BD:02:05:A6:FE`。工具要求显式指定地址，避免连接同名设备。板卡只允许一个连接，执行测试前关闭占用连接的终端或手机。

```powershell
# 查询业务版本，同时接收传感器日志。
& $blePython Tools/BLE_OTA/ota.py nus --address E6:BD:02:05:A6:FE

# NUS 发送 OTA ENTER；业务回复后延迟 500 ms 复位。
& $blePython Tools/BLE_OTA/ota.py enter --address E6:BD:02:05:A6:FE

# 设备改为 CadenceDFU，广播 SMP 服务。查询模式和镜像列表。
& $blePython Tools/BLE_OTA/ota.py status --address E6:BD:02:05:A6:FE

# 直接覆盖唯一业务槽，完成后请求复位。
& $blePython Tools/BLE_OTA/ota.py upload --address E6:BD:02:05:A6:FE --image Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/zephyr.signed.bin --reset

# 重连业务应用核对版本。
& $blePython Tools/BLE_OTA/ota.py nus --address E6:BD:02:05:A6:FE
```

Loader 在上传后不执行 swap 的 test/confirm。上传成功只是数据传输成功，MCUboot 在复位后独立验签。

SW5（P0.11）已配置为 `mcuboot-button0`，按住并复位应进入 Loader；本轮未进行人工按键操作验证。业务完全失效时，镜像无效自动进入 Loader 的路径已实机验证。

## 实机证据

硬件：Embedfire FireFlasherMini CMSIS-DAP / nRF52832；BLE：Windows BARROT Bluetooth Adapter。

| 检查 | 结果与证据 |
|---|---|
| SWD 识别容量 | FICR：页大小 `0x1000`，128 页，共 512 KB |
| 首次三镜像烧录 | 合并 HEX 所有地址段读回一致 |
| MCUboot 启动业务 1.0.0 | NUS 返回 `CadenceSensor firmware 1.0.0`；VTOR=`0x10200` |
| 业务传感器 | SC7A20H 地址 `0x19`、ID=`0x11`、版本=`0x28`、初始化 `err=0`；持续输出采样 |
| NUS 软件升级入口 | 返回 `OTA: entering CadenceDFU in 500 ms`，重启后广播 `CadenceDFU` |
| Loader 模式 | SMP bootloader mode=`7`；slot1 为 active；VTOR=`0x4C200` |
| 正常 OTA | 真实 BLE 上传 111,891 B，将 1.0.0 更新到 1.0.1，重启并重连成功 |
| slot1 擦除保护 | `image erase {slot:1}` 返回 `rc=8`，之后仍可查询 Loader |
| 超大镜像 | 声明长度超过业务槽，上传请求被拒绝 |
| 中断上传 + 复位 | 向 1.0.1 业务槽上传 1.0.0 的前 8,448 B 后断开，再复位；自动进入 Loader，可再次上传 |
| 损坏签名 | 翻转签名 BIN 最后一个字节，完整通过 BLE 上传并复位；MCUboot 拒绝业务镜像，进入 Loader |
| 最终恢复 | 再次通过 BLE 上传有效 1.0.1，NUS 版本、传感器初始化和采样正常 |
| 最终业务读回 | 与签名 BIN 的全部 111,891 B 逐字节一致；imgtool 独立验签通过 |
| 固定区完整性 | 初次烧录后与异常测试结束后的 MCUboot、Loader、参数区 SHA-256 一致 |

最终业务签名 BIN SHA-256：

```text
d27fde297e06a064c4fd8dad4f6bd51a45566a26210acd3dd9e7cfe5a208d535
```

固定区 SHA-256（针对完整分区，而非 HEX 文件）：

```text
MCUboot aad1ba3a50800ec95bcc08d14501072bde732a2bcdadf7dc0e76d6c9ef7151bb
Loader  1f036ed84b729ad83eff76d3ae7803b3f1087e1ce52c0d3fb42269519b58b645
Storage 0fbba07a833d4dcfc7024eaf313661a0ba8f80a05c6d29b8801c612e10e60dee
```

Loader 运行时读取的 BPROT 寄存器组为 `0000ffff / 00000000 / fffff000 / ffffffff`，与 boot、Loader 和参数区对应。擦除命令测试验证了软件拦截；未通过故意执行受保护地址的原始 Flash 写入测试硬件故障行为。

原始记录保存在仓库忽略的 `tmp/ota_*.log`、`tmp/ota_*inspect.json`、`tmp/ota_loader_before.json`，初始 1.0.0 镜像在 `tmp/ota-validation/v1.0.0/`。构建和验证脚本保留在工程内。

## 验证边界与后续工作

- 已完成软件复位场景下的中断恢复；未远程控制物理断电，真实掉电恢复仍需人工执行。
- 未实测手机 Device Manager 的界面流程、SW5 实体按键和长期稳定性。
- 保留当前保守分区；未来可根据增长需求缩小固定区，但发布后不能随意变更布局。
- 无业务回滚。有效签名但运行卡死的固件需要按键进入 Loader 修复；本轮未实现启动失败计数。
- 原固件基线与新固件均有较高的 SC7 overrun 计数，本轮验证采集通路，不将无丢样或数据质量作为已通过项目。
- SDK 默认 MCUboot ECDSA 实现仍有 TinyCrypt 弃用提示，NUS UART 为实验性功能；没有编译或链接错误，不代表所有 SDK 提示已消除。
- 当前为开发测试密钥，尚未实施发布密钥管理、升级授权或防降级策略。
