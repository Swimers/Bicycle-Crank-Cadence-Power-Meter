# nice!nano ANT+ USB 接收器技术方案

版本：0.1（2026-11-15）  
状态：已添加实验性 ANT 接收、解码和 USB CDC 输出及上位机串口后端；单个功率设备和 USB 输出已实测，多设备接收尚待验证。本文件后续章节保留最初的方案和协议草案，不能把草案中的所有命令当作已实现功能。

当前实现使用 ANT Add-on 2.1.1 / NCS 3.2.4，编译时额外加载 `ant.conf`；默认 `prj.conf` 不启用 ANT。串口支持 `PING`、`STATUS`、`BOOTLOADER` 行命令，启动后尝试自动扫描；测量以 JSON 行输出，字段见 `src/ant_receiver.c`。现仅支持心率、踏频、功率常用数据页，其他页仅输出原始数据。接收队列和设备槽位分别为 32 和 24；串口背压和完整丢包统计尚未实现。评估密钥仅供评估，发布前须另行处理授权。ANT 扩展的后台搜索与真实多设备兼容性尚未验证。

本机验证记录（2026-11-15）：UF2 镜像的 ELF 链接及写入范围均以 `0x26000` 开始，位于应用分区；解码单元测试通过。修复 CDC 接收端点的固件已刷入并枚举为 COM7，`STATUS` 返回 ANT 启动错误码 0。唤醒功率设备后，实测收到设备号 1、类型 11 的广播，并通过串口及上位机后端解析出 250 W、90 rpm。此测试覆盖单个设备；心率、踏频传感器和多个设备同时接收仍待实测。

## 1. 目标与边界

使用 nice!nano 的 nRF52840 同时接收多个 ANT+ 运动传感器，识别设备并解码心率、踏频和功率，通过 USB CDC ACM 串口发送到 Windows；上位机通过同一串口管理扫描和选择。首版以只读广播为主，不承诺运动手表配对、可靠控制或 ANT USB Stick 二进制协议兼容。

`Tools/ANT_Receiver` 已添加独立 CDC 后端，原有 ANT USB 和 BLE 后端仍保留；界面区分数据来源。

## 2. 已验证的本地基础

| 项 | 现状 |
| --- | --- |
| 板卡目标 | NCS 3.4.1 的 `promicro_nrf52840/nrf52840/uf2`，板级描述基于 nice!nano；实物版本及 Bootloader 待核实。 |
| USB | Zephyr USB device-next、CDC ACM 类及开机自动初始化已由板级配置启用；`zephyr,console` 指向 CDC UART。 |
| 时钟 | Kconfig 为 32.768 kHz 外部晶振、50 ppm；nice!nano 官方资料列有低频晶振。需核实具体实物和晶振规格。 |
| Flash | 当前生成设备树采用 S140 v6 UF2 布局：应用 `0x26000` 至 `0xEBFFF`、存储 `0xEC000` 至 `0xF3FFF`、bootloader 区 `0xF4000` 至 `0xFFFFF`；不得据此认定每块板的实装 bootloader 都相同。 |
| ANT | 默认 `prj.conf` 未启用 ANT；需通过 `ant.conf` 和兼容的 ANT Add-on 构建接收器。USB 本身不实现 ANT。 |

## 3. 依赖与版本决策

ANT Wireless 官方兼容表 2.1.1 列出 `sdk-ant 2.1.0/2.1.1 + sdk-nrf 3.2.4` 支持 nRF52840；表中没有 3.4.1。这不等于证明 3.4.1 不能使用，但不能把它当作受支持组合。优先在独立目录安装 ANT add-on v2.1.1 所绑定的 NCS 3.2.4，核对清单锁定的 SDK 和工具链版本；**不要覆盖当前 NCS 3.4.1 安装**。在新 SDK 中重新确认 `promicro_nrf52840/nrf52840/uf2` 板目标是否存在、CDC 自动初始化行为及最终分区，不存在时再选择匹配板定义或移植。

按官方流程获取 ANT add-on、接受 Adopter 条款，启用 ANT 库并配置适用的评估授权。协议库授权密钥、ANT+ 网络密钥以及 USB VID/PID 是不同事项；不得将个人授权密钥提交仓库或写进日志。评估授权只适用于其条款规定范围；产品发布需单独核实授权和 ANT+ Profile 要求。ANT 栈要求 LFCLK 精度不差于 ±50 ppm，当前 Kconfig 只是声明值，需由实物晶振及负载设计确认。

## 4. 数据流和并发

```text
ANT+ devices -> ANT stack / scan -> bounded raw event queue
                                  -> per-device state / decode
                                  -> bounded TX queue -> USB CDC -> PC
PC -> USB CDC RX -> line framing -> command parser -> state machine
```

ANT 回调仅检查消息类型、复制 8 字节 payload、扩展设备标识和接收时间，再使用非阻塞队列投递；禁止在无线事件回调中等待 USB 或执行格式化打印。处理线程维护设备状态、解码并投递 TX；独立串口发送逻辑负责处理 USB 未打开、短写及背压。所有队列设定容量；满时丢弃并累计计数，向 PC 报告，不伪称无损。

首阶段验证连续扫描模式支持的设备类型、扩展标识及 ANT add-on 的具体 API。连续扫描适合 USB 供电、多设备广播采集，但不能假定与独立通道并行；满负载无线接收会丢包。需要可靠定向交互、校准或设备控制时，再设计扫描与指定设备通道切换、通道数量上限和超时恢复。设备身份采用 `(device_number, device_type, transmission_type)`；身份可能重用或碰撞，允许上位机提示并人工选择，发现不是认证。

## 5. Profile 解码边界

| ANT+ 类型 | 初期覆盖 | 待确认与扩展 |
| --- | --- | --- |
| 120 心率 | 常用心率数据页、心率 bpm 和事件计数 | 无效值、扩展页、发射器停更；须按正式 Profile 测试 |
| 121 速度/踏频、122 踏频 | 曲柄事件时间和累计转数，计算 rpm | 首包无速率；16 位回绕、重复事件、长时间静止与丢包；类型 121 的轮速需轮周长，首版不计算 |
| 11 功率 | 标准功率页 `0x10` 的瞬时功率和有效踏频 | 扭矩/曲柄扭矩、左右平衡、校准及其他数据页另行实现；不支持页不得填假数据 |

踏频示意：两个有效事件之间 `rpm = 60 × Δ转数 × 1024 / Δ事件时间`，按字段宽度做模运算。仅当转数和时间均有效且间隔合理时更新；重复事件不刷新运动时间。停止阈值和掉线阈值分开管理，阈值应根据设备发送周期实测，超时分别标记 `stopped` / `stale` / `lost`，不能一直显示旧数值为实时值。严禁用主机接收时间替代 Profile 事件时间而不标注。

## 6. 后续 CDC 协议草案（未实现）

固定 UTF-8、每行一个 JSON 对象，换行 `\n`；限制单行长度（建议 512 字节），校验协议版本和类型，逐字节累积以处理 USB 分片及粘包；超长行丢弃至换行再返回错误。字段明确单位，缺测用 `null` 而非 0。以下均为设计示例，并非当前固件支持的命令：

```json
{"v":1,"type":"cmd","id":1,"cmd":"scan_start"}
{"v":1,"type":"ack","id":1,"ok":true}
{"v":1,"type":"device","device_id":12345,"device_type":120,"transmission_type":1,"state":"seen"}
{"v":1,"type":"measurement","device_id":12345,"device_type":120,"transmission_type":1,"uptime_ms":123456,"heart_rate_bpm":72,"cadence_rpm":null,"power_w":null,"raw":"0000000000000048"}
{"v":1,"type":"stats","rx_dropped":0,"tx_dropped":0}
```

拟定控制：`scan_start`、`scan_stop`、`select`、`unselect`、`status`，每条命令必须附请求 ID 并应答；先声明具体命令的前置状态、重复命令和错误码。选择仅控制本接收器筛选，不代表控制对端。JSON 与日志不得混杂为未标记的 `printk()` 文本；保留带 `type=log` 的结构化日志或采用独立调试通道。USB CDC 的主机设置波特率是串口参数，不决定 USB 无线吞吐。

## 7. 当前可用的 Bootloader 命令

当前阶段仅支持如下**纯文本**命令；不是上节 JSON 协议：

| 主机发送（大小写敏感） | 响应 | 行为 |
| --- | --- | --- |
| `PING\n` | `PONG\r\n` | 确认 CDC 通道 |
| `BOOTLOADER\n` | `OK BOOTLOADER\r\n` | 约 300 ms 后写 GPREGRET=0x57 并软件复位，请求 Adafruit USB UF2/CDC 模式 |

也接受 CRLF，行最长 31 字节（含可选 CR，不含 LF）；含 NUL、内嵌 CR、未知或过长的命令返回 `ERR COMMAND\r\n`。空行忽略。命令无身份验证，允许访问串口的主机可以触发复位；连接不可信电脑前应考虑访问控制。确认响应为尽力发送，不保证 USB 主机在复位前一定收到。输出先经 `uart_poll_out` 进入 CDC TX 缓冲区，不等于已发送到主机。

该路径针对支持 `0x57` 的 Adafruit nRF52 Bootloader；官方 0.6.0 源码包含此模式，**板上实际 Bootloader、USB 枚举和进入效果尚未验证**。请先使用现有 UF2 方法烧录包含命令的应用，再在串口工具发送 `PING` 测试；确认硬件复位/双击 RST 可用后发送 `BOOTLOADER`，观察应用 COM 口断开、UF2 驱动器出现。若无法响应，检查实际 bootloader 版本及串口；通过官方双击 RST 进入 bootloader 作为恢复方式。不要用全片擦除替代正常 UF2 更新。

## 8. 实施和验收

1. **串口和 Bootloader**：当前应用完成编译后，在实际板上验证 PING、LF/CRLF、分片、错误/超长行、连续输入及 UF2 进入、断电重连与双复位恢复；ACK 缺失不代表复位失败。
2. **版本和授权**：在隔离 SDK 安装 add-on；记录 west manifest 修订、编译器版本及评估许可状态。确认 UF2 内存布局和生成文件后再刷机。
3. **单设备收包**：优先运行官方 ANT 心率接收示例，校验初始化、LFCLK、设备身份、原始页和心率；使用第二台设备作参照。
4. **多设备与解码**：同时测试至少 1 个心率带、1 个踏频器、1 个功率计；覆盖不同身份、事件回绕、重复事件、静止、关机和重新出现，比较商用品读数及原始包。
5. **CDC 与 UI**：实现版本化协议、错误响应、队列丢包计数、断线重连、串口选择、设备筛选；验证粘包、拆包、过长输入及慢主机。
6. **稳定性**：记录固定距离、设备数量、无线环境、运行时长、各设备期望与接收消息数及队列丢包数；持续性和可接受丢包阈值应由实际产品需求制定，不能根据编译成功推断。

## 参考资料

- [ANT for NCS 兼容矩阵及 LFCLK 要求](https://ant-nrfconnect.github.io/doc/compatibility.html)
- [ANT add-on 安装与条款](https://ant-nrfconnect.github.io/doc/getting_started.html)
- [ANT 集成和授权](https://ant-nrfconnect.github.io/doc/integration_notes.html)
- [ANT 连续扫描和消息协议](https://www.thisisant.com/assets/resources/ANT%20Protocol/D00000652_ANT_Message_Protocol_and_Usage_Rev_5.1.pdf)
- [ANT 心率接收示例](https://ant-nrfconnect.github.io/samples/ant_plus/ant_hrm/hrm_rx/README.html)
- [nice!nano 硬件及 Bootloader](https://nicekeyboards.com/docs/nice-nano/)
- [Adafruit Bootloader 0.6.0 模式与 GPREGRET](https://github.com/adafruit/Adafruit_nRF52_Bootloader/blob/0.6.0/src/main.c)
