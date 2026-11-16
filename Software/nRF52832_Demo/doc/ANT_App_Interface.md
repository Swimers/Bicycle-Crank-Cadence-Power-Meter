# ANT Bicycle Power 固定数据测试

`src/app/ant_app.c/.h` 接入官方 ANT 协议栈，上电初始化后持续发送 `0x10` Power Only，瞬时功率固定 **250 W**、踏频固定 **90 rpm**，踏板功率比例为 `0xFF`（不可用）。BLE NUS 文本、`printk` 输出及进入 OTA 的指令仍由原有蓝牙模块处理。射频时序由 ANT/MPSL 调度，主循环不通过延时模拟无线发送周期。

## 环境与构建

项目统一使用 NCS 3.2.4（`D:/ncs/v3.2.4`）、匹配工具链 `C:/ncs/toolchains/fd21892d0f`，另行加载 ANT add-on 2.1.1 模块 `D:/ncs/ant-v2.1.1/ant`。不使用 ANT 安装工作区旁的 SDK 副本。

在应用目录执行统一入口：

```powershell
.\scripts\build.ps1 -Pristine
# 后续增量构建：
.\scripts\build.ps1
```

可通过 `-SdkRoot`、`-ToolchainRoot`、`-AntModuleDir` 指定安装位置。唯一顶层构建目录是 `build`，内部构建业务、BLE loader、MCUboot 三个镜像：

- OTA 业务固件：`build/nRF52832_Demo/zephyr/zephyr.signed.bin`。
- 完整 SWD 镜像：`build/merged.hex`。

ANT 配置已并入 `prj.conf`，业务镜像默认同时启用 ANT 和 BLE；Loader 与 MCUboot 不启用 ANT。应用 CMake 自动加载 ANT 模块。`sysbuild/CMakeLists.txt` 自动运行配置校验及 HEX 合并，脚本与 IDE 的顶层构建行为一致。完整入口见 [项目说明](../README.md)。

分区使用 `fixed-partitions` 并关闭 Partition Manager；地址及容量保持原值。`cmake/sign-loader-dts.cmake` 从实际 DTS 读取 Loader 分区大小，补全 NCS 3.2.4 签名参数，不修改 SDK 源码。ANT 无线部分无需新建 UART/SPI 或修改设备树 RADIO 节点。

使用官方 `CONFIG_ANT_EVALUATION_KEY=y`，限非商业评估开发。发布商业产品前需取得并配置正式授权。ANT+ 网络密钥由 SDK key manager 管理，应用仓库不保存网络密钥明文。

## 通道及数据

| 参数 | 值 |
| --- | --- |
| 通道 / 本地网络索引 | 0 / 0 |
| 通道类型 | 双向主通道 `0x10` |
| 设备类型 | Bicycle Power `0x0B` |
| 传输类型 | `0x05` |
| 设备编号 | 1（开发默认值，量产须唯一且非零） |
| RF | 57，即 2457 MHz |
| 周期 | 8182 / 32768 秒，约 4.005 Hz |

测试模式在每个 ANT TX 事件生成一次新的固定测量值，为下一帧递增事件计数、累计功率；计数分别按 8 位、16 位自然回绕。功率和踏频不随 SC7A20H 采样改变。首帧及随后帧的八字节负载示例：

```text
10 00 FF 5A 00 00 FA 00
10 01 FF 5A FA 00 FA 00
10 02 FF 5A F4 01 FA 00
```

多字节字段显式按小端编码，不能直接发送结构体内存。`0x12` Crank Torque 也提供发送接口，但默认不会发送该页；累计曲柄周期单位 1/2048 秒，累计扭矩单位 1/32 Nm。

## 业务接口

`ant_app_init()` 在 BLE 初始化前调用，配置并打开通道；`ant_app_process()` 仅每秒通过现有 `printk` 打印状态。初始化失败返回 `-EIO`，原始栈错误显示在 `stack`；重复初始化返回 `-EALREADY`，参数非法返回 `-EINVAL`。底层未启用 ANT 时返回 `-ENOTSUP`；当前统一构建强制校验 ANT 已启用。

`ant_app_send_power_only()`、`ant_app_send_crank_torque()` 接收一个完整采样快照，线程安全地替换待发送数据，并退出固定测试模式。此后业务层负责事件计数及所有累计量；没有新快照时继续重发当前值，不增加计数。返回 0 仅表示本地接收，非接收端确认；空指针/页号错误返回 `-EINVAL`，通道未打开返回 `-ENODEV`。

```c
struct ant_app_power_only_data data = {
    .page_number = ANT_APP_PAGE_POWER_ONLY,
    .update_event_count = event_count,
    .pedal_power = ANT_APP_PEDAL_POWER_UNAVAILABLE,
    .instantaneous_cadence = cadence_rpm,
    .accumulated_power = accumulated_power,
    .instantaneous_power = power_watts,
};
int err = ant_app_send_power_only(&data);
```

## 实机验证与范围

此前已完成 ANT 三镜像构建、签名、配置校验和 HEX 合并；2026-11-16 将构建入口统一为 NCS 3.2.4 / `build`。ANT 业务镜像 Flash 使用 150224 B、RAM 使用 32968 B，签名 BIN 约 147 KiB，未超过 240 KiB 业务分区。业务镜像和 loader 均通过原 SDK 的 imgtool 签名验证，镜像头、向量地址和分区边界也已校验。尚未烧录或验证实际无线收发。

烧录后，通过 BLE NUS 订阅文本输出，可看到周期状态：

```text
ANT ch=0 open=1 tx=... errors=0 stack=0x0 page=0x10 fixed=1
```

`tx` 表示协议栈的本地 TX 事件数，不能证明接收端已经收到。使用 ANT+ 码表或 USB ANT 接收器按 Bicycle Power 搜索设备号 1，验证 250 W / 90 rpm，同时保持手机 BLE NUS 连接并观察文本输出。手机系统蓝牙列表不能接收 ANT 数据。

当前是 `0x10` 固定值联调，实现不包含完整产品所需的校准应答、通用信息页交织、采样功率算法或 ANT+ 认证，不能据此宣称完整 ANT+ 互操作合规。

依据：

- [官方版本兼容表](https://ant-nrfconnect.github.io/doc/compatibility.html)。
- [ANT/BLE 共存及授权说明](https://ant-nrfconnect.github.io/doc/integration_notes.html)。
- [官方 Bicycle Power 发送示例](https://ant-nrfconnect.github.io/samples/ant_plus/ant_bpwr/bpwr_tx/README.html)。
- [Bicycle Power Profile 5.1](https://forums.garmin.com/cfs-file/__key/communityserver-discussions-components-files/402/D00001086_5F00_ANT_2B005F00_Device_5F00_Profile_5F002D005F00_Bicycle_5F00_Power_5F00_Rev_5F00_5.1.pdf)。
