# CadenceSensor nRF52832 开发板

原理图依据：`SCH_Schematic1_1-P1_2026-11-14.svg`，单页 P1。此板直接继承
`nrf52832_qfaa.dtsi`（512 KiB Flash、64 KiB RAM），不继承 nRF52 DK。

| 功能 | MCU 连接 | 当前用途 |
| --- | --- | --- |
| SC7A20H I2C | SCL=P0.15、SDA=P0.14 | TWIM0，100 kHz，地址 0x19 |
| SC7A20H INT1 | P0.13 | 记录连线；现有驱动仍轮询 |
| SC7A20H INT2 | 仅有 ACC_INT2 标签 | 未发现 MCU 连接，不分配 GPIO |
| UART0 | TX=P0.25、RX=P0.26 | 排针引脚，沿用项目分配，115200，无流控 |
| ADS1220 SPI | CS=P0.06、SCK=P0.07、MOSI=P0.08、MISO=P0.09 | SPI1，暂禁用 |
| ADS1220 DRDY | P0.10，低有效 | 暂禁用，尚无 ADC 软件驱动 |
| 用户按键 SW5 | P0.11，经 1 kΩ 接地 | 上拉、低有效 |
| RESET SW4 / H3 | P0.21 | nRESET，不作为软件按键 |
| 调试 | SWDIO、SWDCLK、VOUT、GND | H3，使用外部调试器 |

X1 为 32 MHz 晶振，X2 为 32.768 kHz 晶振。SoC DTS 中 HFXO 的 64 MHz
是时钟输出描述，不应替换成晶体的 32 MHz。板上有 DCC/DEC4 电感电容，启用片上 DC/DC。
P0.09/P0.10 已通过 UICR 配置为 GPIO，NFC 禁用。HR1117 的 3.3 V VOUT 是固定硬件电源，
没有可供 MCU 配置的电源管理总线。USB Type-C 仅供电，不建立 USB 数据设备。

板级 DTS 描述物理硬件，`app.overlay` 只选择 BLE HCI 和 NUS printk 控制台。
SC7A20H 的自定义应用驱动从 `accel0` 别名读取总线和地址；本地 binding 不等于
注册了 Zephyr Sensor API 驱动。ADS1220 binding 也仅记录连线，不能直接调用 ADC API；
其预留 1 MHz / SPI mode 1 参数应随后续驱动进行硬件验证。

当前固件不启用 SPI/ADC、PWM、NFC、片上温度和看门狗；未添加不存在的 LED、蜂鸣器、
Arduino 接口或 MCUboot 分区。SPI0/I2C1/SPI2 保持 SoC 默认禁用。

## 构建

在已经加载 nRF Connect SDK 工具链的 PowerShell 中，从应用目录执行：

```powershell
west build -b cadence_sensor/nrf52832 -d build_cadence_sensor . -- "-DBOARD_ROOT=$PWD" "-DDTS_ROOT=$PWD"
```

VS Code 中选择板 `cadence_sensor/nrf52832`，构建目录使用 `build_cadence_sensor`。
旧 nRF52 DK 的 `build` 目录及构建配置已删除，仅保留 `build_cadence_sensor`。
板根目录已写入仓库 `.vscode/settings.json`，构建目录已加入忽略规则。
当前不声明 OTA/Bootloader 布局。
