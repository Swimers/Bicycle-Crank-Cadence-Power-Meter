# CadenceSensor BLE/NUS 上位机

基于 Tkinter + Bleak 的 Windows 桌面文本终端。通过系统 BLE 适配器连接 MCU，自动订阅 NUS TX 通知，接收 `printk()` 文本，向 NUS RX 写入 UTF-8 文本。

## 启动

需要 Windows 11、Python 3.10+（包含 Tk）、已安装驱动并开启的 BLE 适配器。当前主机使用 Python 3.14；依赖安装在本目录 `.venv`，不修改系统 Python。

在 PowerShell 中运行：

```powershell
cd D:\Documents\GitHub\CadenceSensor\Tools\BLE_NUS
.\start.ps1
```

也可直接双击本目录的 `start.cmd`。

首次运行会创建虚拟环境并从 PyPI 安装 `requirements.txt`。若 PowerShell 策略不允许运行脚本，可手动运行：

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe app.py
```

## 收发验证

1. MCU 烧录支持 NUS 控制台的固件并上电。关闭手机上占用该 MCU 连接的 App；当前固件只支持一个连接。
2. 点击“扫描设备”，选择 `CadenceSensor`，点击“连接”。扫描也显示其他 BLE 设备；广播 NUS 的设备优先。
3. 状态显示“已连接 · Notify 已订阅”后，接收区会显示 MCU 缓存的启动日志以及后续 `printk()`。
4. 输入 `hello`，选择 LF 后点击发送或按 Enter。当前 MCU 回显固件应返回 `hello` 和换行；TX 日志仅表示 GATT 写入成功，回显才是 MCU 接收并处理的证据。
5. “保存接收数据”按原始字节保存最近最多 2 MiB；界面按 UTF-8 增量解码，支持中文跨 BLE 数据包。发送文本最多 512 字节，按 20 字节分包，支持无换行、LF、CRLF。

| 通道 | UUID | 方向 |
| --- | --- | --- |
| NUS 服务 | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | 服务发现 |
| RX | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | 上位机写入 MCU |
| TX | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | MCU Notify 到上位机 |

无需在系统设置配对，不使用 COM 串口，也不需要设置 UART 波特率。物理 UART 的 115200 设置不影响 BLE。Windows 自动选择可用的 BLE 无线电；本工具不提供多个适配器的显式选择。

## 常见问题与限制

- 扫描不到：检查 Windows 蓝牙开关、USB 适配器驱动、MCU 供电和广播；设备被手机连接后可能停止广播。
- 找不到 NUS 服务：检查烧录固件；固件服务表改变后，可能需要在 Windows 删除旧设备记录并重新扫描。
- 已连接但无数据：本工具自动订阅通知，但 MCU 必须持续调用 `printk()`；连接前超过 MCU 2 KiB 缓存的日志不能恢复。
- 蓝牙操作失败：界面底部显示错误类型和原因。不会因为名称相同自动连接另一台设备，断连后需要手动重连。
- 本工具用于文本调试，不保证无损高速采集。接收队列过载会显示丢弃字节数；发送后不提供应用层 ACK 或重试，以免重复执行命令。连接默认不配对、不加密，仅用于本地调试。
- 关闭窗口会取消扫描/连接任务并尝试释放 BLE 连接，最长等待约 12 秒。

## 开发验证

```powershell
.\.venv\Scripts\python.exe -m unittest -v
```

测试覆盖 UTF-8 数据分包、通知接收、服务缺失时清理连接、断连后拒绝发送和写入异常传播。需要真实设备验证 RF 连接和 MCU 回显。

实现依据：[Bleak 客户端文档](https://bleak.readthedocs.io/en/stable/api/client.html)、[Windows 后端](https://bleak.readthedocs.io/en/stable/backends/windows.html)。
