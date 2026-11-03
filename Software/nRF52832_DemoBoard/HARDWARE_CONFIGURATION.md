# NRF52832 排针开发板硬件配置

本工程仅适用于 2026-11-02 的 3.3V nRF52832 排针开发板，唯一构建目标为 `nrf52832_proto/nrf52832`。配置覆盖时钟、引脚复用、通信外设、输入 GPIO、片内稳压模式及 SWD/RTT。适配本机 NCS 3.4.1（Zephyr 4.4.2），目前完成板级初始化，传感器寄存器设置、采样流、算法与 BLE 服务待实现。

## 硬件依据

主要依据 [开发板硬件配置参考](../../Hardware/PCB/NRF52832/NRF52832_Firmware_Hardware_Config.md) 与 [2026-11-02 网络表](../../Hardware/PCB/NRF52832/Netlist_PCB1_2026-11-02.tel)。主控为 nRF52832-QFAA，QFN48，CPU 64 MHz，Flash 512 KiB，RAM 64 KiB；板上稳压输出约 3.3 V。

网络表 SHA256：`d6ebf0e690eb8f2bc06a1a27d0a0b29375ee698ab20fe9dedb74614866c13679`。后续硬件连接变更时，应同步修改本工程。

## 外设和引脚

P0.xx 为 GPIO 编号，U3.x 为芯片物理脚号，Hn.x 为排针脚号。

| 功能 | GPIO | U3 物理脚 | 排针 | 配置 |
| --- | --- | --- | --- | --- |
| ADS1220 CS | P0.06 | 8 | H1.8 | GPIO 片选，低有效，启动置高 |
| ADS1220 SCK | P0.07 | 9 | H1.9 | SPIM1 时钟，空闲低 |
| ADS1220 MOSI | P0.08 | 10 | H1.10 | SPIM1 输出 |
| ADS1220 MISO | P0.09 | 11 | H1.11 | SPIM1 输入，无上下拉；NFC 转 GPIO |
| ADS1220 DRDY | P0.10 | 12 | H1.12 | 输入，无上下拉，低有效；NFC 转 GPIO |
| 用户按键 SW5 | P0.11 | 14 | H4.2 | 输入上拉，低有效 |
| SC7A20H INT1 | P0.13 | 16 | H4.4 | 输入，无上下拉，极性按传感器配置匹配 |
| SC7A20H SDA | P0.14 | 17 | H4.5 | TWIM0 SDA |
| SC7A20H SCL | P0.15 | 18 | H4.6 | TWIM0 SCL |
| XL1 / XL2 | P0.00 / P0.01 | 2 / 3 | 未引出 | 外部 32.768 kHz 晶振专用 |
| RESET | P0.21 | 24 | H3.3 / H4.12 | UICR 映射硬件复位，低有效 |
| SWDCLK | 专用调试脚 | 25 | H3.4 / H5.1 | SWD，不能与 P0.25 混用 |
| SWDIO | 专用调试脚 | 26 | H3.5 / H5.2 | SWD，不能与 P0.26 混用 |

空闲扩展脚保持 SoC 默认的输入缓冲断开、无上下拉状态，不配置成输出低：P0.02～P0.05、P0.12、P0.16～P0.20、P0.22～P0.31。其中 P0.03 芯片脚与 H1.5 在当前网络表中断开，软件无法修复该连接。

H1.1/2 为 3.3 V，H5.11/12 为 USB VBUS，不能接至 GPIO。H1/H2/H4/H5 的 NC 脚不能当作公共地；外设公共地可从 H3.1 取得。板载传感器引脚同时引到排针，扩展接线时应避免多个输出相互驱动。

### SC7A20H

使用 TWIM0，100 kHz，7 位地址 `0x19`。SDA/SCL 已有 4.7 kΩ 硬件上拉，不再启用内部上拉；不使用与 TWIM0 共享实例的 SPI0。INT2 未接 MCU，不配置。

工程自带 `silan,sc7a20h` 原始 I²C 端点绑定，不实例化 Zephyr sensor 驱动。后续使用 `I2C_DT_SPEC_GET(DT_NODELABEL(sc7a20h))` 访问。身份验证为 `WHO_AM_I(0x0F)=0x11`；VERSION 寄存器为 `0x70`，当前手册缺省值 `0x28`，需结合实际芯片版本核对。

启动仅配置 INT1 输入，中断保持禁用。设备树暂以高有效描述端点，后续必须先确定传感器的中断极性、推挽/开漏与锁存模式，再配置匹配的 GPIO 触发方式。按键消抖、量程、ODR 和运动中断路由均由后续应用实现。

### ADS1220

使用 SPIM1，1 MHz、8 位、MSB first、Mode 1（CPOL=0，CPHA=1）。CS 为 GPIO 片选，独立 DRDY 使用 P0.10。ADC 的 CLK 接地，使用自身内部振荡器，不配置 MCU 时钟输出。开启 `CONFIG_NRF52_ANOMALY_58_WORKAROUND=y`，处理 nRF52832 SPIM 短传输勘误。

工程自带 `cadence,ads1220-raw` 原始 SPI 绑定，`cadence` 仅为项目绑定前缀。应用使用 `SPI_DT_SPEC_GET(DT_NODELABEL(ads1220), SPI_WORD_SET(8) | SPI_TRANSFER_MSB)`，宏自动从设备树取得 `spi-cpha`。模式与速率在第一次实际传输时由驱动应用；一条完整操作应在同一次 SPI 事务内保持 CS。

启动只将 CS 置高并检查总线就绪，不执行 ADC RESET、寄存器写入或转换。后续需依照手册完成上电等待、RESET、配置、回读和启动转换，再使能 DRDY 下降沿中断。

AIN0 接精密补桥中点，AIN1 接外接应变片中点。外部 REFP/REFN 未接有效参考源，不可选择悬空参考对；内部 2.048 V 或 AVDD–AVSS 比例式参考应与 PGA、共模限制及码值换算配套。板载应变采集不使用 MCU SAADC；当前 SAADC 保持禁用，扩展模拟输入待应用明确需求后配置。

两片 350 Ω 应变片接在 3.3 V—中点、中点—GND 两段：H2.10 为激励正，H2.9 为激励负，H2.11/12 均为同一个应变中点。连续激励总电流约 9.43 mA，ADC 休眠不会切断桥路供电。

## 时钟和系统配置

| 项目 | 配置 | 说明 |
| --- | --- | --- |
| 高频时钟 | 保留 SoC 的 64 MHz 时钟域定义 | X1 为 32 MHz 无源晶体；不将 CPU/HFXO 时钟域错误填写为晶体标称频率 |
| 低频时钟 | `CONFIG_CLOCK_CONTROL_NRF_K32SRC_XTAL=y` | X2 为 32.768 kHz，P0.00/P0.01 专用 |
| 低频精度 | 暂按 `CONFIG_CLOCK_CONTROL_NRF_K32SRC_500PPM=y` | 保守起调值；应按装配晶体全温误差及实测调整 |
| 片内 DC/DC | `regulator-initial-mode = NRF5X_REG_MODE_DCDC` | 板上 L1/L3/C21 外围已存在；需上板核对电流与纹波 |
| NFC 复用 | `&uicr { nfct-pins-as-gpios; };` | 释放 P0.09/P0.10 |
| 硬件复位 | `&uicr { gpio-as-nreset; };` | SDK 启动代码处理 P0.21 复位映射 |
| 日志 | SEGGER RTT，非阻塞跳过模式 | 经外部 J-Link/SWD 查看 |
| UART / USB | 不启用 | USB 仅供电，没有板载 USB 串口或调试器 |
| SAADC / PWM | 不启用 | 没有配置扩展模拟采样和 PWM 输出 |
| Flash | 应用从 0 开始，512 KiB | 无 MCUboot、OTA 或存储分区 |

HFXO 由时钟驱动按客户端请求管理，后续无线交由协议栈请求并等待稳定。未强制高频晶体始终运行，当前初始化也不代表已经实测起振或频偏。

UICR 为非易失配置，首次配置可能触发启动代码复位；删除设备树属性不会自动恢复已写入值。恢复 NFC/复位复用需按 Nordic 流程操作。本次工作只生成和构建固件，不执行擦除或烧录。

提供 default/sleep 两套 pinctrl 状态，但未启用设备自动挂起；主线程休眠不会自动切换所有外设到 sleep 状态。稳压器、应变电桥等仍耗电，当前配置不是已验证的整板低功耗方案。

## 构建和烧录

本工程为单应用构建，不使用 sysbuild；Applications 保留应用和 build 两层，不生成独立子镜像上下文。目录更名后刷新 Applications 即可识别新路径。所有源文件由仓库根 Git 管理。

在 VS Code nRF Connect 插件中新建构建配置：SDK 选 3.4.1，Board root 选本应用目录，目标选 `nrf52832_proto/nrf52832`，输出目录选 `build`，关闭 sysbuild（不勾选 Build with sysbuild）。不复用 `nrf52dk/nrf52832` 缓存，CMake 会拒绝其他板目标。

普通 PowerShell 从仓库根目录运行：

```powershell
./Software/nRF52832_DemoBoard/tools/build.ps1
```

脚本固定选择当前开发板；SDK 和工具链路径可通过 `-SdkRoot`、`-ToolchainRoot` 指定，默认分别为 `D:/ncs/v3.4.1`、`C:/ncs/toolchains/4f5b6ad6dd`。

也可在插件提供的对应 SDK Terminal 中，从本应用目录执行：

```powershell
west build --no-sysbuild -b nrf52832_proto/nrf52832 -d build . -- -DBOARD_ROOT=. -DDTS_ROOT=.
```

修改板定义或绑定后，可添加 `-p always` 重新生成缓存。产物在 `build/zephyr/`，包含 `zephyr.hex`、`zephyr.elf`、最终 `zephyr.dts` 与 `.config`。

硬件独立供电，外部 J-Link 连接 H3：1=GND、2=3.3 V VTref、3=RESET、4=SWDCLK、5=SWDIO。使用插件烧录，或在 SDK Terminal 执行 `west flash -d build --runner jlink`。打开 RTT 查看 `Board: nrf52832_proto/nrf52832; init: 0`。该结果仅代表控制器和 GPIO 初始化成功，传感器通信仍需实际事务验证。

## 文件与验证范围

- `boards/cadence/nrf52832_proto/nrf52832_proto_nrf52832.dts`：当前开发板全部引脚及外设定义。
- 同目录的 `board.yml`、`Kconfig.nrf52832_proto`、defconfig 和 `board.cmake`：QFAA 目标与调试器定义。
- `dts/bindings/`：SC7A20H、ADS1220 原始通信端点绑定。
- `prj.conf`：时钟、通信驱动、RTT 与 SPIM 勘误规避。
- `src/main.c`：CS 置高、输入配置、中断禁用及控制器就绪检查。
- `tools/build.ps1`：当前开发板构建入口。

上板验收顺序：电源/SWD/RESET → 晶振 → SC7A20H 身份/版本 → ADS1220 寄存器回读和 DRDY → 电桥零点和已知负载。当前未提供测量、BLE、按键业务逻辑或传感器中断服务。

本次验证：NCS 3.4.1 编译及链接通过，无构建警告，已生成 HEX/ELF；Flash 使用 28,296 B，RAM 使用 6,872 B。已检查最终设备树中的全部已用 GPIO、总线速率/模式、UICR、低频时钟及未使用外设禁用状态。未进行烧录或硬件实测。
