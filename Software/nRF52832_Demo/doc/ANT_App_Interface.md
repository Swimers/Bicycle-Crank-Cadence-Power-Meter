# ANT Bicycle Power 接口占位

本次范围为已确认的 `0x10` Power Only、`0x12` Crank Torque 两页声明及发送接口，
不实现采样算法、计数累计、编码、通道调度或射频发送。两页都携带踏频字段。

`src/app/ant_app.c/.h` 已加入业务镜像；主程序初始化并周期调用处理钩子。
合法参数调用返回 `-ENOTSUP`，空指针、错误页号或零设备编号返回 `-EINVAL`；
`ant_app_process()` 为空操作。BLE/NUS/OTA 保持现有实现，不生成模拟运动数据。

通道参数声明使用双向主通道 `0x10`、设备类型 `0x0B`、传输类型 `0x05`、
RF=57（2457 MHz）、周期 8182/32768 秒。通道及本地网络索引为 0。
设备号 1 仅为开发默认值，接入协议栈时须换成设备唯一非零编号，并另外配置合法的 ANT+ 网络密钥。

结构体包含完整八字节字段布局，多字节字段为本机整数；后续必须显式编码为小端，不能直接发送结构体内存。
`0x10` 提供事件计数、踏频、累计及瞬时功率；`0x12` 提供事件计数、累计曲柄转数、踏频、累计周期及扭矩。
累计周期单位为 1/2048 秒，扭矩单位为 1/32 Nm。累计量必须按各自位宽回绕，
不能每次无线重发就当作新采样增加计数；无有效踏频时使用 `0xFF`。

业务层未来调用示意（当前会返回 `-ENOTSUP`，不是实际发送示例）：

```c
struct ant_app_power_only_data power = {
    .page_number = ANT_APP_PAGE_POWER_ONLY,
    .pedal_power = ANT_APP_PEDAL_POWER_UNAVAILABLE,
    .instantaneous_cadence = ANT_APP_CADENCE_UNAVAILABLE,
};
/* 业务采样更新各字段后调用，必须检查返回值。 */
int err = ant_app_send_power_only(&power);

struct ant_app_crank_torque_data crank = {
    .page_number = ANT_APP_PAGE_CRANK_TORQUE,
    .instantaneous_cadence = ANT_APP_CADENCE_UNAVAILABLE,
};
err = ant_app_send_crank_torque(&crank);
```

本机 NCS 3.4.1 未安装 ANT 栈，SDK 文档也注明不支持 ANT，因此未添加不存在的
`CONFIG_ANT` 开关或修改设备树 RADIO。nRF52832 芯片具备 ANT 能力并不意味着当前 SDC 已包含 ANT。
后续必须先确认支持此芯片的协议栈及 BLE 共存方案；不能把 nRF5 SDK 的 S332 直接链接进现有 Zephyr/SDC 固件。
实际产品还需要通用信息页、校准应答、页面交织、网络和互操作验证；两个结构体不代表完整 ANT+ 兼容实现。

依据：

- [Bicycle Power Profile 5.1](https://forums.garmin.com/cfs-file/__key/communityserver-discussions-components-files/402/D00001086_5F00_ANT_2B005F00_Device_5F00_Profile_5F002D005F00_Bicycle_5F00_Power_5F00_Rev_5F00_5.1.pdf)：5.2、8、10 节。
- [Nordic nRF52 协议支持](https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/app_dev/device_guides/nrf52/features.html)。
