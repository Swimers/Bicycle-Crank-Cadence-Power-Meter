# CadenceSensor核心板：外设、时钟与引脚配置参考

整理日期：2026-11-02。适用于本目录的 **CadenceSensor电池供电核心板**，用于后续板级定义、驱动初始化与低功耗配置。

**核心配置：nRF52832-QFAA；VOUT标称3.0V；SC7A20H走I²C；ADS1220走SPI Mode 1；电池电压走SAADC AIN0；P0.22请求整板关机。不能直接套用带排针开发板的SPI引脚和电源配置。**

## 1. 资料依据与版本边界

- 连接与数值主要依据：[网络表](Netlist_PCB1_2026-11-01.tel)。与[Gerber制造包](Gerber_PCB1_2026-11-01.zip)内飞针数据交叉比较，网络表列出的260个元器件焊盘的网络归属一致（将网络名中的`/`按飞针导出规则转换为`_`后比较）；飞针文件另列22个未接网络的器件焊盘。
- 器件型号参考：[PCB图纸及BOM](PCB_PCB1_2026-11-01.pdf)；工程文件为`CadenceSensor.eprj2`。未将工程文件当作已经完整解析的结构化原理图。
- 文件并非同时导出：PDF约09:25，网络表约09:40，Gerber约09:52，工程文件约19:22，均为2026-11-01。PDF中的L2封装与网络表不同，不能把PDF全部料号直接认定为最终贴装BOM。本文GPIO映射以相互一致的网络表、Gerber为准；如工程随后改线，须重新导出核对。
- **P0.xx是GPIO编号，U3.n是QFN48物理脚号，H3.n等是连接器焊盘号。** 表中“未外接”表示导出网络仅有MCU自身焊盘，不表示已有可用排针。
- 接线事实与建议参数分开描述；建议参数尚未代表实板测量结果。本文不修改现有固件。

## 2. MCU与外设分配

U3为 **nRF52832-QFAA，QFN48，Cortex-M4F，最高64MHz，512kB Flash、64kB RAM**。实际应用空间需扣除蓝牙栈、Bootloader和存储区，烧录前核对芯片标识及链接脚本。

| 功能 | 板上连接 | MCU资源/建议实例 | 关键配置 |
| --- | --- | --- | --- |
| 三轴加速度 | U1 SC7A20HTR | TWIM0；GPIO/GPIOTE | SDA=P0.14，SCL=P0.15，INT1=P0.13；7位地址0x19 |
| 应变采集 | U4 ADS1220IPWR | SPIM1；GPIO/GPIOTE | SCK=P0.16，MOSI=P0.17，MISO=P0.18，CS=P0.19，DRDY=P0.20 |
| 电池电压 | Q1、R6/R7 | SAADC，软件通道可选CH0 | 输入P0.02/AIN0；P0.06高有效使能检测分压 |
| 电源模式 | U2 nPM1100 MODE | GPIO输出 | P0.07：低为自动模式，高为强制PWM |
| USB输入限流 | U2 nPM1100 ISET | GPIO输出 | P0.08；初始化低；不是充电电流设置 |
| 整板关机 | U2 SHPACT，经R8/D1 | GPIO输出 | P0.22高有效请求Ship模式 |
| 蜂鸣器 | MLT-5020、Q3、R28 | PWM0单通道 | P0.23；低为关闭，约4kHz方波起调 |
| 用户按键 | SW5、R13 | GPIO/GPIOTE或PORT事件 | P0.11，输入上拉，按下为低 |
| 硬件复位 | SW4、C19 | UICR.PSELRESET | P0.21，低有效 |
| 时钟 | X1、X2 | CLOCK/HFXO/LFXO；RTC | 高频晶体32MHz；低频晶体32.768kHz |
| BLE | U3 ANT、C17/L5、L4天线 | RADIO及蓝牙控制器/协议栈 | 无额外SPI蓝牙模块或天线开关GPIO |
| 下载调试 | H3 | 专用SWD | SWDCLK=U3.25；SWDIO=U3.26 |

实例分配是建议，不是硬件固定要求；PSEL可路由这些总线GPIO。TWIM0与SPIM0等相同ID外设共享资源，不能同时作为独立总线启用。RTC、TIMER、PPI、GPIOTE通道及中断优先级应避开协议栈/RTOS保留资源；SPIM使用EasyDMA时缓冲区放RAM，并按芯片修订版及SDK处理相关勘误。[Nordic SPIM说明](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/spim.html)。

## 3. 全部GPIO对应关系

| GPIO | U3物理脚 | 网络表网络名 | 实际连接及建议初始状态 |
| --- | ---: | --- | --- |
| P0.00 / XL1 | 2 | `P0.00/XL1` | X2.2、C22；LFXO专用，勿作普通GPIO |
| P0.01 / XL2 | 3 | `P0.01/XL2` | X2.1、C12；LFXO专用，勿作普通GPIO |
| P0.02 / AIN0 | 4 | `BAT_MON` | R6/R7中点；SAADC单端输入，关闭数字输入及上下拉 |
| P0.03 / AIN1 | 5 | `P0.03/AIN1` | 未外接；默认输入断开、无上下拉 |
| P0.04 / AIN2 | 6 | `P0.04/AIN2` | 未外接；默认输入断开、无上下拉 |
| P0.05 / AIN3 | 7 | `P0.05/AIN3` | 未外接；默认输入断开、无上下拉 |
| P0.06 | 8 | `BAT_MON_EN` | Q1.2、R3=1MΩ下拉；输出低关闭，采样前置高 |
| P0.07 | 9 | `MODE` | U2.7；输出低，允许nPM1100自动选择工作模式 |
| P0.08 | 10 | `ISET` | U2.5；输出低，按USB输入限流规则控制 |
| P0.09 / NFC1 | 11 | `P0.09` | 未外接；默认NFC复用，无需为本板ADC改UICR |
| P0.10 / NFC2 | 12 | `P0.10` | 未外接；默认NFC复用，无需为本板ADC改UICR |
| P0.11 | 14 | `P0.11` | R13=1kΩ → SW5 → GND；输入上拉、低有效、软件消抖 |
| P0.12 | 15 | `P0.12` | 未外接；不是板载LED |
| P0.13 | 16 | `ACC_INT1` | U1.5 INT1；输入，极性及边沿与传感器配置一致 |
| P0.14 | 17 | `ACC_SDA` | U1.2 SDA，R2=4.7kΩ上拉至VOUT；I²C开漏兼容 |
| P0.15 | 18 | `ACC_SCL` | U1.12 SCL，R1=4.7kΩ上拉至VOUT；I²C开漏兼容 |
| P0.16 | 19 | `ADS_SCK` | R18=47Ω → U4.1 SCLK；SPI Mode 1，空闲低 |
| P0.17 | 20 | `ADS_MOSI` | R20=47Ω → U4.16 DIN；SPI输出 |
| P0.18 | 21 | `ADS_MISO` | R21=47Ω ← U4.15 DOUT/DRDY；SPI输入，无上下拉 |
| P0.19 | 22 | `ADS_CS` | R19=47Ω → U4.2 CS；输出，初始化先置高 |
| P0.20 | 23 | `ADS_DRDY` | R17=47Ω ← U4.14 DRDY；输入，下降沿通知就绪 |
| P0.21 / nRESET | 24 | `P0.21` | H3.3、SW4、C19=100nF；UICR配置为硬件复位后保留 |
| P0.22 | 27 | `POWER_OFF` | R8=1kΩ → D1阳极，D1阴极 → SHPACT；R9=100kΩ下拉；输出初始低 |
| P0.23 | 28 | `P0.23` | R28=100Ω → Q3栅极；R30=1MΩ下拉控制端；PWM输出，初始低 |
| P0.24 | 29 | `P0.24` | 未外接；默认输入断开、无上下拉 |
| P0.25 | 37 | `P0.25` | 未外接；不是SWDCLK |
| P0.26 | 38 | `P0.26` | 未外接；不是SWDIO |
| P0.27 | 39 | `P0.27` | 未外接；默认输入断开、无上下拉 |
| P0.28 / AIN4 | 40 | `P0.28/AIN4` | 未外接；默认输入断开、无上下拉 |
| P0.29 / AIN5 | 41 | `P0.29/AIN5` | 未外接；默认输入断开、无上下拉 |
| P0.30 / AIN6 | 42 | `P0.30/AIN6` | 未外接；默认输入断开、无上下拉 |
| P0.31 / AIN7 | 43 | `P0.31/AIN7` | 未外接；默认输入断开、无上下拉 |

“输入断开”指关闭GPIO输入缓冲、无上下拉的低功耗配置，不是让已使用的中断输入失效。P0.09/P0.10仅在以后改为普通GPIO时才需解除NFC复用；本板没有连接NFC天线。

## 4. 时钟、电源与复位

| 项目 | 硬件配置 | 固件配置要点 |
| --- | --- | --- |
| HFXO | X1=32MHz，BOM型号X322532MOB4SI；U3.34/XC1、U3.35/XC2；C15/C10各18pF | 使用外部晶体模式；CPU最高64MHz，不是将CPU配置为32MHz；BLE由SDK申请高频晶体时钟 |
| LFXO | X2=32.768kHz，BOM型号SF32WK32768D31T002；P0.00/P0.01；C22/C12各20pF | LFCLK选择XTAL，非外部有源时钟/bypass；RTC与BLE低频精度按晶体、温漂、老化及负载误差总预算设置 |
| MCU供电 | U3.13/36/48接VOUT，U3.31/45及EP49接GND | 逻辑电平按标称3.0V设计；GPIO不接5V |
| MCU内部DC/DC | L1=10µH、L3=15nH、C21=1µF，连接DCC/DEC4 | 外围已布置，首板确认后可通过SDK电源接口使能；与nPM1100外部BUCK的MODE控制是两件事 |
| nPM1100外部BUCK | U2.2/3均接DEC，选择脚为高/高；L2=2.2µH | VOUT标称3.0V，由硬件固定，不能用I²C改为3.3V；低电池电压下应考虑压差导致的输出下降 |
| 硬件复位 | P0.21、SW4、H3.3；C19接地 | 通过SDK/烧录配置将UICR.PSELRESET[0]与[1]均映射到P0.21；改UICR后按SDK流程复位生效 |

时钟起振须设超时并检查状态；晶体负载电容不是可由软件修正的配置项。使用蓝牙栈/RTOS时由其时钟管理接口申请与释放时钟，避免直接停止其正在使用的时钟。参考：[Nordic CLOCK](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/clock.html)、[nPM1100 BUCK](https://docs.nordicsemi.com/r/bundle/ps_npm1100/page/chapters/core_components/buck/doc/frontpage.html)。

## 5. 传感器与模拟采集配置

### 5.1 SC7A20H加速度计

U1的VDD（7脚）、VDDIO（3脚）、CS（10脚）及SDO（1脚）均接VOUT，因此采用 **I²C，7位地址0x19**。不要向要求7位地址的SDK传入左移后的0x32。SDA=P0.14，SCL=P0.15，已有4.7kΩ外部上拉，一般不再启用GPIO内部上拉。

建议从100kHz开始，读`WHO_AM_I(0x0F)`应为`0x11`，确认器件后再设置三轴使能、输出数据率、量程、BDU与FIFO。输出数据率可从100Hz起调；量程结合安装半径和最高转速确定，旋转造成的向心加速度可能使±2g不足，不能只按静止重力选量程。需要时再验证400kHz总线。

INT1接P0.13；INT2仅有U1自身网络，未接MCU。配置INT1的数据就绪或运动唤醒功能时同步配置电平极性、锁存/脉冲行为及MCU触发方式；锁存中断按手册读取相应源寄存器清除。轴方向需按实际安装姿态标定，不能由网络表推断。寄存器以本地[SC7A20H手册](../../元器件/SC7A20H/B4EB8A0DB32006C0C2DCBF47B8DC5BCF.pdf)为准，不套用其他相似型号的完整寄存器表。

### 5.2 ADS1220应变ADC

| 项目 | 实际连接/起调建议 |
| --- | --- |
| 总线 | SPI Mode 1（CPOL=0、CPHA=1），MSB first，8bit帧；先用1MHz验证 |
| 片选与就绪 | CS=P0.19，空闲高；DRDY=P0.20，下降沿触发读取；完整命令/数据事务期间保持CS有效 |
| 时钟 | U4.3 CLK接GND，使用ADS1220内部振荡器，不由MCU输出MCK |
| 电源 | AVDD（12）、DVDD（13）接VOUT；AVSS（5）、DGND（4）接GND |
| 差分输入 | AIN+=R23=1kΩ→U4.11 AIN0；AIN−=R22=1kΩ→U4.10 AIN1；C7=100nF跨接两个ADC输入 |
| MUX | 正输入AIN0、负输入AIN1；输入对换会反转输出符号 |
| 参考 | 起调采用内部2.048V参考；需要抑制桥激励波动时，可验证选择AVDD−AVSS作为参考的比例测量方案 |
| 未用引脚 | REFP0/REFN0（9/6脚）、AIN2（8脚）、AIN3/REFN1（7脚）未外接；不能选择悬空的外部参考输入 |
| 增益/采样率 | 先低增益确认零点与满量程，再选增益；静态验证可用20SPS，动态采集按带宽选100/330SPS等手册支持档位 |
| 其他功能 | 初始关闭IDAC、烧断检测电流源及低侧开关功能；本板没有用这些功能给电桥供电 |

初始化可按“等待电源及上电复位完成 → 软件RESET命令0x06并等待手册要求时间 → 写寄存器并回读 → START/SYNC → 等待DRDY → 读24bit数据”执行。24bit结果做二进制补码符号扩展，再按实际参考、增益及标定系数换算；不要当作无符号24bit数。ADS1220没有可照搬SC7A20H方式读取的WHO_AM_I寄存器。

高增益需同时检查差分满量程与PGA共模范围，不能只计算`VREF/GAIN`。20SPS下可按手册配置50/60Hz抑制；换用较高采样率时不能继续假定具有同样工频抑制。读数异常时保存原始码、寄存器回读和DRDY时序，便于区分接线、时序、过载与算法问题。[TI ADS1220手册](https://www.ti.com/lit/ds/symlink/ads1220.pdf)，亦有[本地副本](../../元器件/ADS1220/ads1220.pdf)。

**电桥接线及待机限制：** R4=350Ω从VOUT接到AIN+，R16=350Ω从AIN+接到GND，形成固定半桥。外部两片标称350Ω应变片分别接SG1两脚、SG2两脚，组成另半桥；SG1.1与SG2.2已在板上相连为AIN−。两个连接器不是两个独立ADC通道，也不是可直接插入任意全桥四线传感器的标准接口。机械贴片位置、受拉/受压方向决定灵敏度与符号，需做零点、已知载荷及温漂标定。

两侧均为350Ω+350Ω时，等效负载约350Ω，3.0V下桥电流约8.57mA、总功耗约25.7mW。本板没有独立电桥断电GPIO；只让ADS1220休眠或MCU进入System OFF，电桥仍耗电。ADS1220内部温度不能直接代表应变片或电池温度。

### 5.3 电池电压SAADC

P0.06控制Q1高侧检测开关：输出高打开分压，低关闭；R3=1MΩ将控制端下拉，R5=1MΩ将P管栅极拉回VBAT。Q1两管的连接为N管S/G/D=1/2/6脚，P管S/G/D=4/5/3脚，贴装时仍应核对实际料号引脚定义。R6=1.5MΩ为上臂，R7=220kΩ为下臂，中点接P0.02/AIN0。

```text
Vadc = Vbat × 220 / (1500 + 220)
Vbat = Vadc × 7.8181818
Vbat=4.2V时，Vadc≈0.5372V
分压等效源阻抗≈191.9kΩ
```

建议SAADC单端、AIN0、内部0.6V参考、增益1/1、12bit、无输入电阻上下拉，采集时间20µs或40µs；禁止沿用低阻源的3µs设置。先拉高使能并等待开关/输入稳定，起调可预留毫秒量级等待，再以实测缩短；丢弃初次不稳定样本，做平均/适当过采样，结束后拉低P0.06。上电及明显温度变化后按SDK流程进行偏移校准。[Nordic SAADC](https://docs.nordicsemi.com/r/bundle/ps_nrf52832/page/saadc.html)。

换算必须使用真实的SAADC参考、增益和分辨率，不能把3.0V直接当作ADC参考。可选软件通道CH0，但**软件通道号、AIN0编号和SDK输入枚举不是同一个数值概念**。实板检查使能前后电压、4.2V点及低电压点，校准电阻误差；电压估算SOC还需考虑负载、电池曲线与充电状态。

## 6. 电源管理、按键与蜂鸣器

### 6.1 nPM1100的控制范围

nPM1100不是I²C/SPI寄存器型PMIC；当前板级控制是MODE、ISET、SHPACT三路GPIO，电压和充电电流由固定接线决定。

| 项目 | 本板配置/固件含义 |
| --- | --- |
| 输出电压 | VOUTBSET1/0高/高，VOUT标称3.0V |
| 充电终止电压 | U2.9 VTERMSET接VSYS；按BOM标准电压型号NPM1100-QDAA-R为4.2V |
| 充电电流 | U2.12 ICHG经R11=30kΩ接GND，标称20mA；不能把ISET设高理解为提高该充电电流 |
| 电池温度 | U2.14 NTC经固定R12=10kΩ接GND，符合当前不实现温度检测的需求；没有真实电池温度输入 |
| MODE=P0.07 | 初始低为自动模式；必要时精密采样阶段置高强制PWM，比较噪声与耗电后决定是否采用 |
| ISET=P0.08 | 初始及USB拔出后保持低；见下文USB输入电流限制 |
| 状态灯 | LED4由VBUS经R10供电；LED5接CHG；LED6接ERR。均不是MCU用户LED |
| 状态读取 | CHG、ERR、VBUS没有连接MCU检测GPIO，不能直接用软件读取充电、错误或USB插入状态 |

当前USB D+/D−接nPM1100进行端口检测，不接nRF52832；nRF52832也没有USB设备控制器。对SDP，ISET低对应100mA输入限流，只有完成适用的USB协商后才可置高切换到500mA；本板MCU不能执行该USB枚举协商，因此默认保持低。检测到DCP/CDP时PMIC自行使用500mA限流，ISET不参与该结果。Type-C的5.1kΩ下拉本身不能替代SDP的USB协商。[nPM1100 SYSREG](https://docs.nordicsemi.com/r/bundle/ps_npm1100/page/chapters/core_components/sysreg/doc/frontpage.html)。

固定10kΩ不提供电池温度保护，片内过热保护检测的是芯片自身温度；该配置不应在软件中被标记为“已测得电池温度正常”。充电参数按实际电池规格确认。[nPM1100 CHARGER](https://docs.nordicsemi.com/r/bundle/ps_npm1100/page/chapters/charger.html)。

### 6.2 关机、开机与普通按键

| 操作 | 硬件路径 | 配置/行为 |
| --- | --- | --- |
| 普通用户按键 | SW5 → R13 → P0.11 | 输入上拉、按下低，建议10～30ms消抖后生成应用事件 |
| 硬件复位 | SW4 → P0.21及H3.3 | 按下低；需要UICR映射；不是电源关机 |
| 按键关机 | SW2将VBAT接到SHPACT | 持续高至少200ms，并满足Ship进入条件；未接MCU按键输入，固件不能拦截其关机 |
| GPIO关机 | P0.22 → R8 → D1 → SHPACT | 初始低；置高保持至少200ms，建议为请求预留250～300ms以上，不在进入前提前拉低 |
| 按键开机 | SW3将SHPHLD拉至GND；R25=100kΩ上拉VBAT | 持续低至少200ms退出Ship；不是MCU GPIO唤醒按键 |
| USB开机 | 接入有效VBUS | PMIC退出Ship，重新给系统供电 |

**进入Ship的条件：VBUS已断开并放电至手册要求的低电平，SHPHLD保持高，SHPACT高持续满足保持时间。** SHPACT另有R24=500kΩ下拉。USB仍供电时不能承诺按SW2或拉高P0.22就使整板断电；还应注意SHPACT高会禁用充电器，不能把关机请求长期挂在高电平当作USB待机模式。[nPM1100 Ship模式及200ms保持要求](https://docs.nordicsemi.com/r/bundle/ps_npm1100/page/product_overview.html)。

建议软件关机流程：停止新采集和蜂鸣器 → 保存必要标定/状态并等待Flash操作结束 → 结束BLE与传感器业务 → 将P0.06拉低、ADC和加速度计置低功耗 → 保持SHPHLD未被按低 → P0.22置高并等待断电。不要先让MCU进入System OFF再期望它输出关机脉冲；电源消失后流程不会继续执行。由于没有VBUS检测GPIO，无法事先可靠判断是否能进入Ship；若超时仍运行，应撤销P0.22高电平并进入明确的待机状态，避免反复关机/复位。

手动SW2可能直接切电，因此重要参数宜在正常运行时以可恢复方式保存，不能只依赖“收到关机事件后再写Flash”。SW3未接MCU，不能把它作为运行态软件按键读取；若希望普通SW5长按关机，可以在应用中识别长按后走P0.22关机流程，但Ship后仍需SW3或USB开机。

**System OFF与整板Ship不同：** MCU System OFF保留外部3.0V供电，电桥仍耗电；配置合适时可用SW5或SC7A20H INT1唤醒MCU。Ship关闭系统供电后加速度计也断电，不能再通过运动唤醒，只能使用SHPHLD按键或USB退出Ship。

### 6.3 蜂鸣器

P0.23经R28=100Ω驱动Q3 AO3400A栅极，Q3低侧开关驱动MLT-5020；蜂鸣器另一端接VOUT，D2跨接负载。固件用PWM驱动，先以BOM标称4kHz、50%占空比短鸣起调，关闭时输出低；持续高电平不是正常的无源蜂鸣器音频驱动。

PMIC的BUCK供给MCU、BLE、传感器、电桥及蜂鸣器的总负载，额定输出能力为150mA；短鸣、无线发射与采样并发时须测供电压降。精密采样尽量与鸣叫错开，停止PWM后按噪声/稳定时间验证再采样。PWM卸载或休眠时确保P0.23保持低，不只停止计数器。

## 7. 外部接口及调试配置

| 接口 | 焊盘 | 网络/用途 |
| --- | --- | --- |
| H3 | 1 | GND |
| H3 | 2 | VOUT，标称3.0V，可作调试器VTref参考；默认由板上电源供电 |
| H3 | 3 | P0.21 / nRESET |
| H3 | 4 | SWDCLK，U3.25专用脚 |
| H3 | 5 | SWDIO，U3.26专用脚 |
| Battery | 1 / 2 | VBAT / GND，单节可充锂电池接口 |
| SG1 | 1 / 2 | AIN− / VOUT，外部上桥臂 |
| SG2 | 1 / 2 | GND / AIN−，外部下桥臂 |
| USB2 | VBUS、D+/D−、CC | 供电、PMIC端口检测及Type-C受电配置；不是MCU USB通信接口 |

连接器方向以EDA焊盘编号及实板1脚标记为准。H3不是标准排列的10pin Cortex调试接口，需按上表制作转接线。不要默认从调试器向H3.2反向灌电，否则会影响关机及电流测试。没有板载UART接口，建议优先用SWD/RTT日志；如需UART，另选空闲GPIO并提供实际接线。

## 8. 板级宏与固件移植要点

以下为SDK无关的GPIO编号示例，只表示P0端口内的引脚号；不是QFN物理脚号，也不是可直接复制到任意设备树属性的格式。

```c
#define BOARD_BAT_MON_PIN          2u
#define BOARD_BAT_MON_EN_PIN       6u
#define BOARD_PMIC_MODE_PIN        7u
#define BOARD_PMIC_ISET_PIN        8u
#define BOARD_USER_BUTTON_PIN     11u
#define BOARD_ACC_INT1_PIN        13u
#define BOARD_ACC_SDA_PIN         14u
#define BOARD_ACC_SCL_PIN         15u
#define BOARD_ADS_SCK_PIN         16u
#define BOARD_ADS_MOSI_PIN        17u
#define BOARD_ADS_MISO_PIN        18u
#define BOARD_ADS_CS_PIN          19u
#define BOARD_ADS_DRDY_PIN        20u
#define BOARD_RESET_PIN           21u
#define BOARD_POWER_OFF_PIN       22u
#define BOARD_BUZZER_PIN          23u

#define BOARD_ACC_I2C_ADDR      0x19u /* 7-bit address */
#define BOARD_BAT_MON_SCALE    (1720.0f / 220.0f)
/* BAT_MON uses AIN0; translate AIN0 to the selected SDK's input enum. */
```

上电顺序建议：先设置P0.22低、P0.23低、P0.06低、MODE低、ISET低、ADS_CS高，再初始化时钟与总线；读回加速度计标识、ADC配置并做电池检测，最后启用中断、周期任务和BLE。输出脚先预置输出寄存器，再切到输出模式，降低初始化毛刺风险。

仓库已有[开发板固件工程](../../../Software/nRF52832_DemoBoard/README.md)，其板级定义面向带排针开发板。移植核心板应创建独立板级配置：更新SPI的pinctrl、CS/DRDY；增加SAADC、PMIC控制及PWM；保持LFXO XTAL；配置P0.21复位；取消“P0.09/P0.10因ADC需要改GPIO”的前提。Zephyr/NCS需对应更新设备树、GPIO极性、ADC通道及Kconfig；nRF5 SDK需对应更新板级宏、实例使能和时钟配置。具体符号以工程锁定的SDK版本为准。

| 与带排针开发板的差异 | CadenceSensor核心板 | NRF52832开发板 |
| --- | --- | --- |
| 系统电源 | nPM1100，标称3.0V | LDO，约3.3V |
| ADS1220 SPI | SCK16、MOSI17、MISO18、CS19、DRDY20 | SCK7、MOSI8、MISO9、CS6、DRDY10 |
| P0.06/07/08 | 电池检测使能 / MODE / ISET | ADC CS / SCK / MOSI |
| P0.09/10 | 未外接，无需为ADC解除NFC复用 | ADC MISO/DRDY，需解除NFC复用 |
| P0.22/23 | 整板关机 / 蜂鸣器 | 空闲扩展GPIO |
| 加速度计位号 | U1 | U2；同样SDA14/SCL15/INT1=13 |
| 外设扩展 | 主要为H3、Battery、SG1/SG2 | H1～H5排针；不能套用其排针表 |

## 9. 首板配置验证与文件指纹

建议一次完成以下验证并保存测量值：VOUT、复位与双晶体起振；I²C标识及三轴方向；SPI回读、DRDY周期、ADC零点/已知载荷；电池检测开关极性和比例；蜂鸣器PWM及供电跌落；SW5消抖、SW4复位；无USB时SW2/P0.22关机与SW3开机、USB接入退出Ship。低功耗电流分别测正常采集、MCU待机、System OFF与Ship，并断开可能反向供电的调试器。低电量关机阈值结合带载VOUT和ADC最低工作电压确定，不能仅用空载电池电压判断。

本次依据文件SHA-256如下，后续更新导出文件后应重新核对映射：

| 文件 | SHA-256 |
| --- | --- |
| `Netlist_PCB1_2026-11-01.tel` | `5e07a45edf990e26538b7e540163d77a26084e57f6b0e52caed3d5bb47084b09` |
| `Gerber_PCB1_2026-11-01.zip` | `df4be3c1a24a9ee02a36c7732f70c1c334041d7bbf109386673a8aebfc2c347d` |
| `PCB_PCB1_2026-11-01.pdf` | `10dedb45f34f100da7fbdcfbefc9fe783bf6301355a65963590bc92844e673d3` |
| `CadenceSensor.eprj2` | `45bcfd742f3160bd2113c6208eb536a6171bd8ec550bebb7fcdf632796937052` |
