# nRF52832开发板：外设、时钟与引脚配置参考

整理日期：2026-11-02。适用于本目录的 **nRF52832带排针开发板**，不适用于CadenceSensor主板的旧引脚分配。

## 1. 依据及使用边界

- 接线依据：[网络表](Netlist_PCB1_2026-11-02.tel)、[PCB图纸及BOM](PCB_PCB1_2026-11-02.pdf)、`NRF52832.eprj2`。
- 交叉检查：[HR1117制造包](NRF52832-HR1117.zip)、[AMS1117制造包](NRF52832-AMS1117.zip)中的飞针数据。U3、U2、U4及H1～H5共130个相关引脚的网络映射一致，因此本文映射适用于两包。稳压器型号及封装仍以实际选用版本为准。
- 下表的 **P0.xx是GPIO编号**，**U3脚号是QFN48物理脚号**，**Hn.m是排针焊盘编号**，三者不能混用；排针观察方向以EDA焊盘1标记为准。
- “建议配置”是固件起调方案，并不代表已有固件或实测结果。总线实例、协议栈、RTOS和Bootloader资源最终以使用的SDK为准。

**写配置前必须确认：**

1. **H1.5未连接P0.03。** 网络表中`P0.03 ; H1.5`与`P0.03/AIN1 ; U3.5`是两个网络；两个制造包均保留此问题。软件改引脚不能修复断线，需改板或飞线。
2. **H1/H2/H4/H5没有GND脚。** H1.3、H2.8、H4.1、H5.6～10目前为NC，不能当作GND。现有调试排针地为H3.1。
3. **P0.09/P0.10已接ADC。** 必须关闭默认NFC复用后作GPIO；P0.21作硬件复位也需配置UICR。
4. H1.1/2为约3.3V，H5.11/12为VBUS（USB供电时约5V）；所有GPIO均按3.3V逻辑使用，不接5V信号。

## 2. MCU与外设总体分配

主控U3：**nRF52832-QFAA，QFN48，Cortex-M4F，最高64MHz，512kB Flash、64kB RAM**。这些是器件物理容量，应用可用空间需扣除协议栈、Bootloader和配置区；烧录时核对实际芯片标识。

| 功能 | 板上器件/连接 | MCU资源 | 状态/建议 |
| --- | --- | --- | --- |
| 三轴加速度 | U2 SC7A20HTR | TWIM/TWI、GPIO/GPIOTE | SDA=P0.14，SCL=P0.15，INT1=P0.13 |
| 应变ADC | U4 ADS1220IPWR | SPIM/SPI、GPIO/GPIOTE | CS=P0.06，SCK=P0.07，MOSI=P0.08，MISO=P0.09，DRDY=P0.10 |
| 用户按键 | SW5、R13=1kΩ | GPIO，可用GPIOTE/PORT事件 | P0.11，按下为低 |
| 硬件复位 | SW4、C19=100nF | UICR.PSELRESET | P0.21，按下为低 |
| 下载调试 | H3及H5部分引脚 | SWD | 专用SWDCLK/SWDIO，不是P0.25/P0.26 |
| 高频时钟 | X1、C10/C15 | CLOCK/HFXO | 32MHz无源晶体，供片内64MHz晶振时钟系统使用 |
| 低频时钟 | X2、C12/C22 | CLOCK/LFXO、RTC | 32.768kHz晶体，占用P0.00/P0.01 |
| BLE无线 | U3 ANT、C17/L5、L4 | RADIO、SDK蓝牙控制器/协议栈 | 没有额外的射频GPIO或外接BLE模块 |
| 片内DC/DC | L1、L3、C21 | POWER/DCDCEN | 外围已配置，可在电源验证后按SDK接口使能 |
| 扩展模拟输入 | 部分排针GPIO | SAADC（可选） | AIN0～7是MCU模拟通道；板载应变采集走ADS1220，并非SAADC |
| 时间戳、看门狗等 | 无需外接引脚 | RTC/TIMER/WDT/PPI（可选） | 由应用分配，不是板上额外器件 |

**建议实例分配：TWIM0用于SC7A20H，SPIM1用于ADS1220。** GPIO经PSEL路由，接线不会把总线固定到某个实例。SPI/TWI同ID实例可能共享寄存器与资源，不能把TWIM0和SPIM0同时当作独立外设启用；SDK启用项需相互排除。不要占用蓝牙栈/RTOS已保留的RTC、TIMER和中断资源。[Nordic TWIM](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/twim.html)、[SPIM及共享资源](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/spim.html)。

## 3. 全部GPIO映射

| GPIO | U3物理脚 | 实际连接 | 固件用途及初始状态 |
| --- | ---: | --- | --- |
| P0.00 / XL1 | 2 | X2.2、C22；未引至排针 | LFXO，占用；不用普通GPIO |
| P0.01 / XL2 | 3 | X2.1、C12；未引至排针 | LFXO，占用；不用普通GPIO |
| P0.02 / AIN0 | 4 | H1.4 | 可选GPIO或SAADC AIN0；默认输入断开、无上下拉 |
| P0.03 / AIN1 | 5 | 仅U3自身网络；H1.5另网 | 芯片脚可用，但排针连接待修复 |
| P0.04 / AIN2 | 6 | H1.6 | 可选GPIO或SAADC AIN2 |
| P0.05 / AIN3 | 7 | H1.7 | 可选GPIO或SAADC AIN3 |
| P0.06 | 8 | H1.8 → R19=47Ω → U4.2 CS | ADC片选，输出，初始化先置高 |
| P0.07 | 9 | H1.9 → R18=47Ω → U4.1 SCLK | SPI时钟，Mode 1，空闲低 |
| P0.08 | 10 | H1.10 → R20=47Ω → U4.16 DIN | SPI MOSI输出，空闲可低 |
| P0.09 / NFC1 | 11 | H1.11 ← R21=47Ω ← U4.15 DOUT/DRDY | SPI MISO输入，无上下拉；先解除NFC复用 |
| P0.10 / NFC2 | 12 | H1.12 ← R17=47Ω ← U4.14 DRDY | ADC独立数据就绪输入，下降沿；先解除NFC复用 |
| P0.11 | 14 | H4.2 → R13=1kΩ → SW5 → GND | 用户按键，输入上拉，低有效，软件消抖 |
| P0.12 | 15 | H4.3 | 空闲扩展GPIO；没有板载LED |
| P0.13 | 16 | H4.4 ← U2.5 INT1 | 加速度中断输入；极性/边沿与传感器寄存器一致 |
| P0.14 | 17 | H4.5 ↔ U2.2 SDX，R2=4.7kΩ上拉 | I²C SDA，开漏兼容，外部上拉已存在 |
| P0.15 | 18 | H4.6 ↔ U2.12 SCX，R1=4.7kΩ上拉 | I²C SCL，开漏兼容，外部上拉已存在 |
| P0.16 | 19 | H4.7 | 空闲扩展GPIO |
| P0.17 | 20 | H4.8 | 空闲扩展GPIO |
| P0.18 | 21 | H4.9 | 空闲扩展GPIO |
| P0.19 | 22 | H4.10 | 空闲扩展GPIO |
| P0.20 | 23 | H4.11 | 空闲扩展GPIO |
| P0.21 / nRESET | 24 | H4.12、H3.3、SW4、C19 | 配为硬件RESET后保留，勿作普通输出 |
| P0.22 | 27 | H5.3 | 空闲扩展GPIO；本板不是POWER_OFF |
| P0.23 | 28 | H5.4 | 空闲扩展GPIO；本板没有蜂鸣器连接 |
| P0.24 | 29 | H5.5 | 空闲扩展GPIO |
| P0.25 | 37 | H2.1 | 空闲扩展GPIO，不是专用SWDCLK |
| P0.26 | 38 | H2.2 | 空闲扩展GPIO，不是专用SWDIO |
| P0.27 | 39 | H2.3 | 空闲扩展GPIO |
| P0.28 / AIN4 | 40 | H2.4 | 可选GPIO或SAADC AIN4 |
| P0.29 / AIN5 | 41 | H2.5 | 可选GPIO或SAADC AIN5 |
| P0.30 / AIN6 | 42 | H2.6 | 可选GPIO或SAADC AIN6 |
| P0.31 / AIN7 | 43 | H2.7 | 可选GPIO或SAADC AIN7 |

空闲GPIO不必逐个设成输出低。未使用时通常保持输入缓冲断开、无上下拉；作为SAADC输入时也需关闭数字输入缓冲和上下拉。表中外设脚已接板载器件，从排针再接外设时应避免驱动冲突。

## 4. 时钟、电源与非GPIO引脚

### 4.1 时钟配置

| 项目 | 本板硬件 | 固件配置 |
| --- | --- | --- |
| 高频晶体HFXO | X1=`X322532MOB4SI`，32MHz；U3.34=XC1接X1.3，U3.35=XC2接X1.1；C15/C10各18pF | 晶体模式，非外部有源时钟旁路；无线使用前由SDK/协议栈请求HFXO |
| CPU系统时钟 | 片内HFINT/HFXO时钟系统，最高64MHz | 不把CPU频率误填为外部晶体的32MHz；无需照搬STM32的HSE/PLL配置流程 |
| 低频晶体LFXO | X2=`SF32WK32768D31T002`，32.768kHz、CL=12.5pF；C12/C22各20pF | LFCLK选外部晶体XTAL；P0.00/P0.01不再配置GPIO |
| 低频RC备选 | SoC内部LFRC | 仅在明确改用RC时选择，并按蓝牙栈要求校准，不能把RC配置与外部晶体精度混用 |
| RTC | 基于LFCLK | 实例、预分频与中断资源由RTOS/协议栈共同分配 |
| ADS1220采样时钟 | U4.3 CLK接GND | ADC使用自身内部振荡器，无需nRF输出MCK |

X1的厂家CL为12pF；当前18pF/20pF是两侧外接电容值，不等于晶体等效负载。频偏与起振仍需结合实际寄生验证。LFCLK精度参数应计入初始误差、温漂、负载误差及老化，不能直接把晶体室温±20ppm当作整机全温范围精度。[Nordic CLOCK说明](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/clock.html)。

### 4.2 电源、去耦与射频脚

| U3物理脚 | 功能 | 板上连接 / 软件处理 |
| --- | --- | --- |
| 13、36、48 | VDD | 接VOUT，约3.3V；不是GPIO |
| 31、45、EP（网络表49） | VSS / GND | 公共地；31为RF邻近地 |
| 1 | DEC1 | C20=100nF到GND，不作为供电输出或GPIO |
| 33 | DEC3 | C13=100pF到GND |
| 46 | DEC4 | C21=1µF，连接L3支路 |
| 47 | DCC | L1=10µH → L3=15nH → DEC4；可按SDK电源接口使能DC/DC |
| 30 | ANT | C17=0.8pF、L5=3.9nH、L4=AN2051-245；由RADIO使用，不配置GPIO |
| 32、44 | NC | 保持未用，不配置为外部功能 |
| 34、35 | XC1、XC2 | 高频晶体专用脚，见上表 |
| 25、26 | SWDCLK、SWDIO | 专用调试脚，见第7节 |

板上DC/DC电感型号为L1=`LQM18FN100M00D`，与L3/C21构成片内降压外围。使能DC/DC应使用所选SDK/协议栈的电源管理接口，避免绕过协议栈直接改寄存器。先确认供电和外围，再验证启用前后的电流、纹波与采集噪声。

## 5. 板载传感器配置

### 5.1 SC7A20H（U2）

| 信号 | 传感器脚 | nRF脚 | 排针 |
| --- | ---: | --- | --- |
| SDA / SDX | 2 | P0.14，U3.17 | H4.5 |
| SCL / SCX | 12 | P0.15，U3.18 | H4.6 |
| INT1 | 5 | P0.13，U3.16 | H4.4 |
| INT2 | 6 | **未接MCU/排针**，仅有ACC_INT2网络名 | 无 |
| SDO / 地址选择 | 1 | 硬件接VOUT | 无，地址固定由硬件决定 |
| CS / 接口选择 | 10 | 硬件接VOUT | 无，使用I²C |
| VDD / VDDIO | 7 / 3 | VOUT约3.3V | — |

- 总线初调用 **100kHz、7位地址`0x19`**，稳定后可评估400kHz；常规I²C API中不填8位读写地址`0x32/0x33`。
- 读`WHO_AM_I`（`0x0F`）预期`0x11`，并结合`VERSION`（`0x70`，当前手册缺省`0x28`）核对实际版本。不要套用LIS3DH的`0x33`身份值。
- R1/R2各4.7kΩ已上拉至3.3V；TWIM按开漏兼容方式配置，通常无需再启用内部上拉。
- 配置ODR、量程、输出格式和中断路由后，再使能P0.13中断。INT1的推挽/开漏、极性、脉冲/锁存模式应与GPIO边沿和拉电阻匹配，不能仅凭导线推定固定边沿。
- 可将100Hz作为ODR调试起点；量程需按曲柄半径、最高转速的向心加速度及冲击预算选择，避免高踏频时削顶，不直接固定为±2g或±4g。中断服务中只通知任务，寄存器读取放到任务/工作队列。

依据：[本地SC7A20H手册](../../元器件/SC7A20H/B4EB8A0DB32006C0C2DCBF47B8DC5BCF.pdf)的I²C地址表及`WHO_AM_I`/`VERSION`寄存器说明。

### 5.2 ADS1220（U4）

| 信号 | ADC脚 | 串联器件 | nRF脚 / 排针 | 方向（以nRF为准） |
| --- | ---: | --- | --- | --- |
| CS，低有效 | 2 | R19=47Ω | P0.06 / H1.8 | 输出，空闲高 |
| SCLK | 1 | R18=47Ω | P0.07 / H1.9 | 输出，空闲低 |
| DIN | 16 | R20=47Ω | P0.08 / H1.10 | MOSI输出 |
| DOUT/DRDY | 15 | R21=47Ω | P0.09 / H1.11 | MISO输入 |
| DRDY，低有效 | 14 | R17=47Ω | P0.10 / H1.12 | 独立输入，下降沿 |
| CLK | 3 | 直接接GND | 无 | 使用ADC内部振荡器 |

**SPI参数：主机、Mode 1（CPOL=0、CPHA=1）、MSB first、每字节8bit；初调1MHz。** SCLK上升沿更新数据、下降沿采样。CS为独立GPIO，在一条完整命令/寄存器操作或一次完整数据读取期间保持低，再按时序释放。SPIM本身不直接产生片选，驱动框架的GPIO CS可完成此工作。[TI ADS1220数据手册](https://www.ti.com/lit/ds/symlink/ads1220.pdf)。

- 上电等待电源稳定及规定的复位时间，发送RESET（`0x06`），等待完成，再写配置并回读；不要把未等待的首帧数据当作有效采样。
- 板上应变正中点经R23接 **AIN0（U4.11）**，外接应变片中点经R22接 **AIN1（U4.10）**；建议MUX选AINP=AIN0、AINN=AIN1。实际受力正负号在标定时确定。
- AVDD/DVDD接3.3V；REFP0（U4.9）/REFN0（U4.6）未接外部参考，不能配置使用该悬空参考对。AIN0/REFP1已作信号输入、AIN3/REFN1接地，也不能随意选作桥路参考。
- 起调可用内部2.048V参考；如采用电源比例测量，可配置AVDD–AVSS为参考。参考选择、PGA增益、共模限制及ADC码值换算必须保持一致，不能把3.3V当成所有配置下的固定满量程。
- 初调从较低增益验证零点，再根据桥路失衡及最大负载选择增益；20SPS适于静态噪声/工频抑制验证，动态应变采样再按需要评估更高数据率。
- DRDY中断通知任务读取完整24位转换结果；做24位有符号补码符号扩展，不把原始数据当无符号数，也不把“24位”当作24位有效精度。
- 板上C9=100nF为差分输入滤波，不是MCU SAADC输入。ADC内部温度传感器可用于诊断，不能直接代替曲柄应变片温度补偿。

### 5.3 外接应变片端子

| 排针 | 网络名 | 经0Ω器件后的实际节点 |
| --- | --- | --- |
| H2.10 | STRAIN_P_1 | R6 → 3.3V激励正 |
| H2.9 | STRAIN_P_2 | R7 → GND激励负，**不是正电源** |
| H2.11 | STRAIN_N_2 | R3 → AIN−中点 |
| H2.12 | STRAIN_N_1 | R5 → 同一个AIN−中点 |

两片350Ω应变片分别接在“3.3V—中点”和“中点—GND”之间；R4/R16各350Ω补成另一桥臂。正常平衡时两中点约1.65V，连续激励总电流约9.43mA。当前没有桥路供电使能GPIO；让ADC休眠并不会切断电桥耗电。

## 6. UICR、按键和启动顺序

### 6.1 必需的复用设置

| 设置 | 目标 | 原因 |
| --- | --- | --- |
| `UICR.NFCPINS.PROTECT` | `Disabled`，值0，按SDK烧录/启动流程生效 | P0.09/P0.10必须改作MISO/DRDY |
| `UICR.PSELRESET[0]` | PIN=21、CONNECT=Connected | 让SW4及H3.3成为硬件复位 |
| `UICR.PSELRESET[1]` | 与`[0]`相同 | 两寄存器不一致时复位映射无效 |

UICR是非易失配置，不是普通GPIO初始化。使用SDK提供的板级/烧录机制设置，并遵守擦写及复位要求，不在循环中重复写；保留其他UICR字段。选定SDK版本后再确定Kconfig、启动宏或烧录脚本写法，本文不提供跨版本通用的整字寄存器写入代码。[Nordic UICR](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/uicr.html)。

SW5（P0.11）输入上拉，按下产生下降沿，可先采用约20ms软件消抖后读取确认。SW4与用户按键分开，P0.21配置复位后不当作普通按键扫描或LED输出。

### 6.2 建议启动顺序

1. 选择nRF52832-QFAA目标、正确Flash/RAM布局及协议栈；配置NFC GPIO和RESET的UICR策略。
2. 按SDK初始化时钟，LFCLK使用外部32.768kHz晶体；无线需要HFXO时交由协议栈/时钟驱动请求并等待稳定。
3. ADC CS先拉高，SCLK初始低；配置SW5上拉，DRDY与INT1输入；中断源暂不使能。
4. 初始化TWIM0，读SC7A20H身份及版本，配置量程、ODR和INT1路由。
5. 初始化SPIM1，ADS1220复位、配置、回读、启动转换，再使能DRDY及INT1中断。
6. 验证样本后启动BLE服务、定时/时间戳及低功耗策略；GPIO中断只通知任务。
7. 最后验证DC/DC、电流、时钟精度、BLE与ADC并发噪声，再决定生产配置。

## 7. 排针速查

| 脚号 | H1 | H2 | H4 | H5 |
| ---: | --- | --- | --- | --- |
| 1 | 3.3V | P0.25 | NC | SWDCLK |
| 2 | 3.3V | P0.26 | P0.11 / SW5 | SWDIO |
| 3 | NC | P0.27 | P0.12 | P0.22 |
| 4 | P0.02 / AIN0 | P0.28 / AIN4 | P0.13 / INT1 | P0.23 |
| 5 | **P0.03断开** | P0.29 / AIN5 | P0.14 / SDA | P0.24 |
| 6 | P0.04 / AIN2 | P0.30 / AIN6 | P0.15 / SCL | NC |
| 7 | P0.05 / AIN3 | P0.31 / AIN7 | P0.16 | NC |
| 8 | P0.06 / CS | NC | P0.17 | NC |
| 9 | P0.07 / SCK | 应变激励−（GND，经R7） | P0.18 | NC |
| 10 | P0.08 / MOSI | 应变激励+（3.3V，经R6） | P0.19 | NC |
| 11 | P0.09 / MISO | 应变中点（经R3） | P0.20 | VBUS |
| 12 | P0.10 / DRDY | 应变中点（经R5） | P0.21 / RESET | VBUS |

| H3调试脚 | 连接 | U3物理脚 / 用法 |
| ---: | --- | --- |
| 1 | GND | 与调试器共地 |
| 2 | VOUT约3.3V | 调试器VTref电平参考；不是默认供电输入 |
| 3 | P0.21 / RESET | 24，UICR配置后低有效 |
| 4 | SWDCLK | 25，专用调试脚 |
| 5 | SWDIO | 26，专用调试脚 |

USB Type-C用于供电，D+/D−未接USB串口或调试芯片；不能按USB串口板配置日志、USB DFU或烧录。可用外接SWD/RTT；如另加UART日志，可自行选择空闲GPIO并外接3.3V USB-UART，本文不预设TX/RX脚。

本板没有板载状态LED、蜂鸣器、nPM1100、电池分压采样或Ship控制，因此不沿用主项目的`POWER_OFF`、`BAT_MON`等定义。HR1117/AMS1117稳压器也没有MCU使能控制连接，MCU System OFF不等于关闭板上LDO或应变桥。

## 8. 可提取的引脚常量

以下为SDK无关的C常量示例，数值是P0端口GPIO编号，不是QFN脚号。不包含驱动调用或UICR写入；使用pinctrl/设备树的工程应将同一映射转入其板级配置，不能重复抢占引脚。

```c
#define BOARD_ADS_CS_PIN       6u
#define BOARD_ADS_SCK_PIN      7u
#define BOARD_ADS_MOSI_PIN     8u
#define BOARD_ADS_MISO_PIN     9u   /* Requires NFC -> GPIO configuration. */
#define BOARD_ADS_DRDY_PIN    10u   /* Active low; falling-edge event. */
#define BOARD_USER_BUTTON_PIN 11u  /* Active low; input pull-up. */
#define BOARD_ACC_INT1_PIN    13u  /* Polarity follows SC7A20H configuration. */
#define BOARD_ACC_SDA_PIN     14u
#define BOARD_ACC_SCL_PIN     15u
#define BOARD_RESET_PIN       21u  /* Reserved after PSELRESET configuration. */
#define BOARD_ACC_I2C_ADDR    0x19u /* 7-bit address. */
#define BOARD_ACC_WHO_AM_I_REG 0x0Fu
#define BOARD_ACC_WHO_AM_I_VAL 0x11u
#define BOARD_ADS_SPI_MODE     1u
#define BOARD_ADS_SPI_HZ       1000000u /* Initial bring-up choice. */
#define BOARD_ACC_I2C_HZ       100000u  /* Initial bring-up choice. */
```

## 9. 版本指纹与参考

后续改网名、补P0.03、给NC脚接GND或调整GPIO分配时，应同时更新本文。本文记录的文件SHA256如下：

| 输入 | SHA256 |
| --- | --- |
| `Netlist_PCB1_2026-11-02.tel` | `d6ebf0e690eb8f2bc06a1a27d0a0b29375ee698ab20fe9dedb74614866c13679` |
| `PCB_PCB1_2026-11-02.pdf` | `26e00f3772df4a5541ab971ed2ade8d44373995440231f9749571a121a4e0fcd` |
| `NRF52832.eprj2` | `30d39a2929e3b018136e0598ee21d9fa21333b3f777efca47ea97bbfc904e1b8` |
| `NRF52832-HR1117.zip` | `ab59ddf9232c95700d292be340ab271220ef303ca5d86b758c936c6d6e24dd09` |
| `NRF52832-AMS1117.zip` | `e8093b1312c66e0d61ea2dd7a7f9436109facb9de5c61f4aca6b5154399f0cd1` |

其他核对入口：[Nordic QFN48引脚定义](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/pin.html)、[Flash/RAM规格](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/memory.html)、[本地ADS1220手册](../../元器件/ADS1220/ads1220.pdf)。本文的配置建议不替代射频、供电和整机功能验证。
