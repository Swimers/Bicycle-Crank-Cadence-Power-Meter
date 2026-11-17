# ADS1220 SPI通信总结

> 导出自 ChatGPT，时间：2026/11/17 15:44:28

# 消息 1

## 用户

如上文件是ADS1220技术手册，请你根据手册内容，总结在SPI通信模式下相关寄存器的设置、读取以及采样数据的读取方法，为后续驱动开发做准备

## 助手

# **ADS1220 SPI 通信与寄存器配置开发指南**

依据：TI ADS1220 数据手册 ZHCSBH5D，2026 年 5 月修订版（78 页）

根据你上传的 ADS1220 技术手册，我重点分析了第 8.5 节（SPI 编程）、第 8.6 节（寄存器映射）、第 9.1.6 节（通信伪代码）和第 9.2.3 节（电阻式电桥测量）。

对于你的 **nRF52832 + Zephyr 踏频功率计项目**，可以将 ADS1220 驱动开发分为四部分：

1. **SPI 通信层**：发送命令、写寄存器、读寄存器。
2. **ADC 配置层**：选择输入通道、PGA 增益、采样速率和基准电压。
3. **ADC 采样层**：启动转换、检测 DRDY、读取 24 位数据。
4. **数据处理层**：符号扩展、电压换算、零点校准、应变与扭矩计算。

下面先总结数据手册规定的通信协议，再给出实际操作与驱动代码。

## **一、SPI 接口基本配置**

ADS1220 采用兼容 SPI 的串行接口，**只支持 SPI Mode 1**。

| SPI 参数 | 设置 | 说明 |
| --- | --- | --- |
| CPOL | `0` | 空闲时 SCLK 为低电平 |
| CPHA | `1` | 第二个时钟边沿采样 |
| 数据长度 | 8 bit | 命令和配置寄存器均为 8 位 |
| 位顺序 | MSB First | 高位先传输 |
| CS | 低电平有效 | 一次完整通信期间保持低电平 |
| DIN | MCU → ADC | 输入命令、寄存器配置 |
| DOUT/DRDY | ADC → MCU | 输出寄存器和转换数据 |
| DRDY | 低电平有效 | 指示一次 ADC 转换完成 |

根据手册第 9 页时序要求，SCLK 最小周期为 150 ns，理论上对应约 6.67 MHz 的时钟上限，但还需要满足高低电平及建立保持时间要求。

建议初期硬件验证使用 **1 MHz SPI 时钟**。

### **SPI 接线关系**

> [推荐使用独立 DRDY 引脚，通过 GPIO 中断获取 ADC 转换完成事件。]

推荐使用独立 DRDY 引脚，通过 GPIO 中断获取 ADC 转换完成事件。需要注意：

- 如果 CS 拉高，DOUT/DRDY 进入高阻态，但独立 DRDY 仍然能够指示转换完成。
- 仅使用 DOUT/DRDY 检测就绪时，需要配置 `DRDYM=1`，并正确处理片选。
- 如果使用独立 DRDY 引脚，可以保持 `DRDYM=0`。

参考：数据手册第 9 页、第 36 页。

## **二、ADS1220 的 6 条 SPI 控制命令**

这是后续驱动中最需要掌握的内容。

### **2.1 命令总表**

| 命令 | 二进制编码 | 常用十六进制值 | 功能 |
| --- | --- | --- | --- |
| `RESET` | `0000 011x` | `0x06` | 复位芯片 |
| `START/SYNC` | `0000 100x` | `0x08` | 开始或重新开始转换 |
| `POWERDOWN` | `0000 001x` | `0x02` | 进入低功耗模式 |
| `RDATA` | `0001 xxxx` | `0x10` | 读取最近的 ADC 转换结果 |
| `RREG` | `0010 rrnn` | 动态生成 | 读取配置寄存器 |
| `WREG` | `0100 rrnn` | 动态生成 | 写入配置寄存器 |

其中：

- `rr`：寄存器起始地址，范围 `0~3`。
- `nn`：访问的寄存器数量减 1。
- `x`：不关心的位。

因此，`RREG` 和 `WREG` 命令不仅包含操作类型，还包含寄存器地址和读取或写入的数量。

参考：手册第 38 页表 8-7。

### **2.2 RESET：复位芯片**

发送：

```
MCU → ADS1220

CS   : LOW
DIN  : 0x06
CS   : HIGH
```

芯片复位后：

- 4 个配置寄存器恢复为 `0x00`。
- 转换配置恢复默认值。
- 需要等待至少 `50 µs + 32 × tCLK` 才能发送后续命令。

实际驱动中可以在发送 `0x06` 后等待 100 µs，前提是系统时钟为手册规定的常用频率。

### **2.3 START/SYNC：启动转换**

发送 `0x08`。

对于单次转换模式（`CM=0`），每发送一次 START/SYNC，芯片执行一次转换，完成后返回低功耗状态。

对于连续转换模式（`CM=1`），发送一次 START/SYNC 后，芯片持续进行 ADC 转换，按配置的采样速率更新结果。

**注意：仅把 **`CM`** 设置为 1，并不等于已经启动连续转换。还需要发送 START/SYNC。**

### **2.4 POWERDOWN：低功耗模式**

发送 `0x02`。

芯片将关闭模拟电路、关闭 IDAC、断开内部低侧开关，但保留配置寄存器的数值。后续发送 `0x08` 可以恢复转换。

对于电池供电的功率计，这个命令适合在设备静止、停止测量或准备休眠时使用。

### **2.5 RREG：读取配置寄存器**

命令构成：

```
RREG = 0x20 | (reg_addr << 2) | (count - 1);
```

例如：

| 操作 | 命令 |
| --- | --- |
| 读取 REG0 | `0x20` |
| 读取 REG1 | `0x24` |
| 读取 REG2 | `0x28` |
| 读取 REG3 | `0x2C` |
| 连续读取 REG0～REG3 | `0x23` |

假设读取全部四个寄存器：

```
              CMD    REG0   REG1   REG2   REG3
MCU  DIN :   0x23   0x00   0x00   0x00   0x00
ADC DOUT :   xxxx   0x3E   0x04   0x98   0x00
```

这是一组示例读回值，代表寄存器内容为 `{0x3E, 0x04, 0x98, 0x00}`。

重点：**第一个接收字节是发送命令时同步收到的内容，不属于寄存器数据。真正的数据从第二个字节开始。**

读取全部四个寄存器时，需要在同一次片选有效期间提供 40 个 SCLK。

### **2.6 WREG：写入配置寄存器**

命令构成：

```
WREG = 0x40 | (reg_addr << 2) | (count - 1);
```

常用命令：

| 操作 | 命令 |
| --- | --- |
| 写入 REG0 | `0x40` |
| 写入 REG1 | `0x44` |
| 写入 REG2 | `0x48` |
| 写入 REG3 | `0x4C` |
| 连续写入 REG0～REG3 | `0x43` |

例如，向 REG0 写入 `0x3E`：

```
CS  : LOW
DIN : 0x40 0x3E
CS  : HIGH
```

一次写入全部四个配置寄存器：

```
CS  : LOW
DIN : 0x43 0x3E 0x04 0x98 0x00
CS  : HIGH
```

寄存器的更新发生在完整写入操作最后一个 SCLK 下降沿之后。

**驱动中的重要原则：** 发送 RREG/WREG 命令和后续寄存器数据必须属于同一次完整 SPI 事务。如果 Zephyr 自动控制 CS，就不要简单地先调用一次 `spi_write_dt()`，再调用一次 `spi_read_dt()`，让 CS 在两次调用之间拉高。

## **三、ADS1220 四个配置寄存器详解**

ADS1220 只有 4 个 8 位配置寄存器，复位值全部为 `0x00`。

### **寄存器分布总览**

| 寄存器 | 地址 | 主要作用 |
| --- | --- | --- |
| Configuration Register 0 | `0x00` | 输入通道、PGA 增益 |
| Configuration Register 1 | `0x01` | 数据速率、工作模式、转换模式 |
| Configuration Register 2 | `0x02` | 基准电压、滤波、激励电流 |
| Configuration Register 3 | `0x03` | IDAC 引脚路由、数据就绪配置 |

### **3.1 REG0：输入通道与 PGA 增益**

> [Chart or card]
>
> REG0 — 地址 0x00 默认 0x00 7 6 5 4 3 2 1 0 MUX[3:0] GAIN[2:0] BYPASS

**MUX[3:0]：Bit 7～4，选择模拟输入通道。**

| MUX 编码 | 正输入 AINP | 负输入 AINN |
| --- | --- | --- |
| `0000` | AIN0 | AIN1 |
| `0001` | AIN0 | AIN2 |
| `0010` | AIN0 | AIN3 |
| `0011` | AIN1 | AIN2 |
| `0100` | AIN1 | AIN3 |
| `0101` | AIN2 | AIN3 |
| `0110` | AIN1 | AIN0 |
| `0111` | AIN3 | AIN2 |
| `1000`～`1011` | AIN0～AIN3 | AVSS |
| `1100` | 外部基准电压 / 4 | 内部监测 |
| `1101` | 模拟电源电压 / 4 | 内部监测 |
| `1110` | 内部输入短接 | 内部零点校准 |
| `1111` | 保留 | 不使用 |

其中 `1100`、`1101` 是内部监测通道，并非普通外部差分输入。使用连接 AVSS 的单端模式时，必须旁路 PGA。

**GAIN[2:0]：Bit 3～1。**

| 编码 | 增益 |
| --- | --- |
| `000` | 1 |
| `001` | 2 |
| `010` | 4 |
| `011` | 8 |
| `100` | 16 |
| `101` | 32 |
| `110` | 64 |
| `111` | 128 |

**PGA****_****BYPASS：Bit 0。**

- `0`：启用 PGA。
- `1`：禁用 PGA、旁路放大器，只适合增益 1、2、4。
- 增益为 8～128 时，无论此位如何设置，PGA 都会启用。

例如，假设应变电桥输出连接到 AIN1 和 AIN2，使用 128 倍增益：

```
MUX        = 0011
GAIN       = 111
PGA_BYPASS = 0

REG0 = 0011 1110 = 0x3E
```

手册来源：第 43 页表 8-10。

### **3.2 REG1：采样速率与转换模式**

> [Chart or card]
>
> REG1 — 地址 0x01 默认 0x00 7 6 5 4 3 2 1 0 DR MODE CM TS BCS

**DR[2:0]：Bit 7～5，数据速率。**

同一个 DR 编码，在不同工作模式下对应不同 SPS。

| DR 编码 | Normal | Duty-cycle | Turbo |
| --- | --- | --- | --- |
| `000` | 20 | 5 | 40 |
| `001` | 45 | 11.25 | 90 |
| `010` | 90 | 22.5 | 180 |
| `011` | 175 | 44 | 350 |
| `100` | 330 | 82.5 | 660 |
| `101` | 600 | 150 | 1200 |
| `110` | 1000 | 250 | 2000 |
| `111` | 保留 | 保留 | 保留 |

数据单位为 SPS（每秒转换次数）；表中速率基于内部振荡器或 4.096 MHz 外部时钟。**MODE[1:0]：Bit 4～3。**

| 编码 | 模式 |
| --- | --- |
| `00` | 正常模式 |
| `01` | 占空比模式，降低平均功耗 |
| `10` | Turbo，高速采样 |
| `11` | 保留 |

其余控制位：

| 位 | 名称 | 0 | 1 |
| --- | --- | --- | --- |
| Bit 2 | CM | 单次转换 | 连续转换 |
| Bit 1 | TS | 正常 ADC 测量 | 内部温度传感器 |
| Bit 0 | BCS | 关闭烧毁电流源 | 开启 10 µA 检测电流源 |

例如，配置正常模式、90 SPS、连续转换：

```
DR   = 010
MODE = 00
CM   = 1
TS   = 0
BCS  = 0

REG1 = 0100 0100 = 0x44
```

这里的 90 SPS 指的是 ADC 每秒最多提供约 90 次新转换数据，而不是 SPI 通信速率。

参考：第 44 页表 8-11、表 8-12。

### **3.3 REG2：基准电压、滤波与激励电流**

> [Chart or card]
>
> REG2 — 地址 0x02 默认 0x00 7 6 5 4 3 2 1 0 VREF 50/60 PSW IDAC

**VREF[1:0]：Bit 7～6，选择参考电压。**

| 编码 | 参考电压源 |
| --- | --- |
| `00` | 内部 2.048 V |
| `01` | 外部 REFP0、REFN0 |
| `10` | 外部 REFP1、REFN1（复用 AIN0、AIN3） |
| `11` | AVDD − AVSS 模拟电源 |

对于惠斯通电桥，如果 ADC 参考电压与电桥激励电压来自同一电压源，便可以采用**比例式测量**，减少激励电压变化引起的测量误差。

**50/60[1:0]：Bit 5～4，工频滤波。**

| 编码 | 滤波功能 |
| --- | --- |
| `00` | 不使用专门的 50/60 Hz 抑制设置 |
| `01` | 同时抑制 50 Hz 和 60 Hz |
| `10` | 仅抑制 50 Hz |
| `11` | 仅抑制 60 Hz |

特别注意：这两位仅适用于正常模式的 20 SPS 或占空比模式的 5 SPS。其他采样速率应设置为 `00`。

**PSW：Bit 3。**

- `0`：内部低侧开关始终断开。
- `1`：START/SYNC 后闭合，POWERDOWN 后断开。

该开关位于 AIN3/REFN1 与 AVSS 之间。只有硬件确实采用该开关控制电桥低侧供电时，才能启用它。

**IDAC[2:0]：Bit 2～0。**

| 编码 | 激励电流 |
| --- | --- |
| `000` | 关闭 |
| `001` | 10 µA |
| `010` | 50 µA |
| `011` | 100 µA |
| `100` | 250 µA |
| `101` | 500 µA |
| `110` | 1000 µA |
| `111` | 1500 µA |

这些电流源主要用于 RTD 等电阻测量方案。对于外部电压激励的普通惠斯通电桥，通常可以关闭 IDAC。

例如：

```
VREF  = 11    // AVDD - AVSS
50/60 = 00
PSW   = 0
IDAC  = 000

REG2  = 1100 0000 = 0xC0
```

参考：第 45 页表 8-13。

### **3.4 REG3：电流源输出通道及数据就绪模式**

寄存器布局：

```
Bit :    7 6 5 | 4 3 2 | 1 | 0
         I1MUX | I2MUX | DRDYM | RESERVED
```

**I1MUX 和 I2MUX：**

| 编码 | 电流源输出位置 |
| --- | --- |
| `000` | 关闭 |
| `001` | AIN0/REFP1 |
| `010` | AIN1 |
| `011` | AIN2 |
| `100` | AIN3/REFN1 |
| `101` | REFP0 |
| `110` | REFN0 |
| `111` | 保留 |

两个电流源分别通过 I1MUX 与 I2MUX 选择输出引脚。

**DRDYM：Bit 1。**

- `0`：仅独立 DRDY 引脚用于表示数据就绪。
- `1`：DRDY 和 DOUT/DRDY 均提供就绪指示。

Bit 0 为保留位，必须写 `0`。

对于具有独立 DRDY 引脚、不使用内部激励电流源的方案：

```
I1MUX = 000
I2MUX = 000
DRDYM = 0
RESERVED = 0

REG3 = 0x00
```

参考：第 46 页表 8-14。

## **四、如何读取 ADS1220 的 24 位采样数据？**

这部分是驱动的核心。

ADS1220 有两种不同的数据读取方式。

### **4.1 方式一：DRDY 就绪后直接读取（推荐）**

这是 TI 手册第 39 页图 8-25～图 8-27 介绍的方式。

工作流程：

**发送 START/SYNC（0x08）**

ADS1220 执行 ADC 转换**DRDY 变为低电平**

ADC 数据已准备就绪

CS 拉低，读取 3 字节CS 拉高，组合成 24 位数据连续转换模式下等待下一次 DRDY

假设 ADC 当前转换结果为 `0x123456`，SPI 传输过程如下：

```
CS   : LOW

SCLK : 24 clock cycles

DIN  : 0x00   0x00   0x00
DOUT : 0x12   0x34   0x56

CS   : HIGH
```

这里 MCU 发送的 `0x00` 只是为了产生 SPI 时钟，不是 RDATA 命令。

**优势：** 不需要额外发送命令，特别适合使用 DRDY 中断触发采样的应用。

### **4.2 方式二：使用 RDATA 命令读取**

如果没有使用 DRDY 信号，也可以主动发送 `0x10`，读取数据缓冲区中最近的 ADC 结果。

```
CS   : LOW

DIN  : 0x10   0x00   0x00   0x00
DOUT : xxxx   0x12   0x34   0x56

CS   : HIGH
```

其中 `xxxx` 是发送命令字节期间收到的无效或无关数据。

因此：

| 读取方式 | 发送字节数 | 有效接收数据 |
| --- | --- | --- |
| DRDY 后直接读 | 3 | 第 1～3 字节 |
| 先发 RDATA | 4 | 第 2～4 字节 |

需要特别注意，**RDATA 读取的是已经存储的转换结果，并不主动启动一次新的转换。** 如果读取频率高于 ADC 实际采样频率，就可能重复读取相同的数据。

此外，手册第 39～40 页还说明了 RDATA 命令与新转换同时发生时的 DRDY 行为，因此使用 RDATA 轮询读取时，不能仅凭 SPI 通信成功就认定获得了新样本。

### **4.3 24 位 ADC 数据的符号转换**

ADS1220 的输出是 **24 位二进制补码（Two's Complement）**，具有正负值。

| 原始值 | 有符号十进制值 | 含义 |
| --- | --- | --- |
| `0x000000` | 0 | 理想零差分输入 |
| `0x000001` | 1 | 正向 1 LSB |
| `0x7FFFFF` | 8388607 | 正满量程 |
| `0xFFFFFF` | -1 | 负向 1 LSB |
| `0x800000` | -8388608 | 负满量程 |

不能直接将三个字节组合成普通的无符号整数后用于电压计算。

C 语言可以这样处理：

```
int32_t ads1220_decode24(const uint8_t data[3])
{
    uint32_t raw =
        ((uint32_t)data[0] << 16) |
        ((uint32_t)data[1] << 8)  |
        (uint32_t)data[2];

    // 24位补码转32位有符号整数
    if (raw & 0x800000U) {
        return (int32_t)raw - 0x1000000;
    }

    return (int32_t)raw;
}
```

这样能够正确处理传感器输出的正负差分电压。

### **4.4 原始 ADC 数据转换为电压**

手册第 37 页给出的 LSB 计算公式为：

$$
V_{LSB}=\frac{2V_{REF}}{Gain\times 2^{24}}
=\frac{V_{REF}}{Gain\times 2^{23}}
$$

因此输入差分电压：

$$
\boxed{V_{IN}=\frac{ADC_{signed}\times V_{REF}}{Gain\times 8388608}}
$$

其中：

- `ADC_signed` 是符号扩展后的 24 位 ADC 数值。
- `VREF` 是 ADC 实际采用的参考电压。
- `Gain` 为 PGA 配置的总增益。

可以实现为：

```
double ads1220_raw_to_voltage(
    int32_t raw,
    double vref,
    uint16_t gain)
{
    return ((double)raw * vref) /
           ((double)gain * 8388608.0);
}
```

> [Chart or card]
>
> 交互式 ADC 数据换算 有符号 ADC 原始值 参考电压 VREF（V） PGA 增益 16 32 64 128 计算出的输入差分电压 3.222656 mV 对应 3222.66 µV

需要注意，以上电压换算公式基于理想 ADC 转换关系。实际测量还存在偏移、增益误差以及噪声，后续要加入零点标定与灵敏度标定。

参考：第 37 页第 8.5.2 节。

## **五、针对 Zephyr 的基础 SPI 驱动实现**

下面给出适合你的 nRF52832 工程的驱动核心示例。

该代码展示如何封装底层通信，并不是完整的 Zephyr ADC 子系统驱动。它假设设备树中已经有标签为 `ads1220` 的 SPI 从设备节点，且 SPI 总线与片选配置正确。

### **5.1 SPI 初始化与统一传输**

```
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#define ADS1220_NODE DT_NODELABEL(ads1220)

#define ADS1220_CMD_RESET       0x06
#define ADS1220_CMD_START       0x08
#define ADS1220_CMD_POWERDOWN   0x02
#define ADS1220_CMD_RDATA       0x10

/* CPOL=0, CPHA=1, 8bit, MSB first */
static const struct spi_dt_spec ads_spi =
    SPI_DT_SPEC_GET(
        ADS1220_NODE,
        SPI_WORD_SET(8) | SPI_MODE_CPHA,
        0
    );

static int ads1220_transfer(
    const uint8_t *tx,
    uint8_t *rx,
    size_t len)
{
    if (tx == NULL || len == 0) {
        return -EINVAL;
    }

    struct spi_buf tx_buf = {
        .buf = (void *)tx,
        .len = len
    };

    struct spi_buf_set tx_set = {
        .buffers = &tx_buf,
        .count = 1
    };

    if (rx == NULL) {
        return spi_write_dt(&ads_spi, &tx_set);
    }

    struct spi_buf rx_buf = {
        .buf = rx,
        .len = len
    };

    struct spi_buf_set rx_set = {
        .buffers = &rx_buf,
        .count = 1
    };

    return spi_transceive_dt(
        &ads_spi,
        &tx_set,
        &rx_set
    );
}

static int ads1220_send_cmd(uint8_t cmd)
{
    return ads1220_transfer(&cmd, NULL, 1);
}
```

所有后续操作都通过 `ads1220_transfer()` 进行 SPI 通信。

这样可以保证一组命令和数据由单次 SPI 调用完成，避免自动片选造成通信帧不完整。

### **5.2 读取全部配置寄存器**

```
int ads1220_read_registers(uint8_t regs[4])
{
    uint8_t tx[5] = {
        0x23, 0x00, 0x00, 0x00, 0x00
    };

    uint8_t rx[5] = {0};

    int ret = ads1220_transfer(
        tx, rx, sizeof(tx)
    );

    if (ret < 0) {
        return ret;
    }

    memcpy(regs, &rx[1], 4);

    return 0;
}
```

这里 `0x23` 代表从 REG0 开始连续读取 4 个寄存器。

### **5.3 一次写入四个配置寄存器**

```
int ads1220_write_registers(
    const uint8_t regs[4])
{
    uint8_t tx[5] = {
        0x43,
        regs[0],
        regs[1],
        regs[2],
        regs[3]
    };

    return ads1220_transfer(
        tx, NULL, sizeof(tx)
    );
}
```

如果仅修改某一个配置参数，可以在后续增加按地址读写的函数，或者对缓存的寄存器值做位掩码操作后重新写入。

### **5.4 读取 24 位 ADC 转换结果**

采用独立 DRDY 信号，在确认新数据就绪后调用：

```
int ads1220_read_raw(int32_t *value)
{
    if (value == NULL) {
        return -EINVAL;
    }

    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};

    int ret = ads1220_transfer(
        tx, rx, sizeof(tx)
    );

    if (ret < 0) {
        return ret;
    }

    *value = ads1220_decode24(rx);

    return 0;
}
```

如果采用 RDATA 方式，只需要改成：

```
int ads1220_read_raw_rdata(int32_t *value)
{
    if (value == NULL) {
        return -EINVAL;
    }

    uint8_t tx[4] = {
        ADS1220_CMD_RDATA, 0, 0, 0
    };

    uint8_t rx[4] = {0};

    int ret = ads1220_transfer(
        tx, rx, sizeof(tx)
    );

    if (ret < 0) {
        return ret;
    }

    *value = ads1220_decode24(&rx[1]);

    return 0;
}
```

这两个函数的主要区别就是是否发送 RDATA 命令，以及原始采样数据从接收缓冲区哪个字节开始。

### **5.5 初始化寄存器示例**

手册第 51 页给出了 TI 官方连续转换初始化伪代码，对应配置为：

```
static const uint8_t initial_regs[4] = {
    0x08,  // REG0: AIN0-AIN1, Gain=16
    0x04,  // REG1: Normal, 20SPS, Continuous
    0x10,  // REG2: Internal VREF, 50/60Hz rejection
    0x00   // REG3: IDAC off, dedicated DRDY
};
```

示例初始化流程：

```
int ads1220_init(void)
{
    if (!spi_is_ready_dt(&ads_spi)) {
        return -ENODEV;
    }

    int ret = ads1220_send_cmd(
        ADS1220_CMD_RESET
    );

    if (ret < 0) {
        return ret;
    }

    /* Reset recovery, internal clock assumed */
    k_busy_wait(100);

    const uint8_t config[4] = {
        0x08, 0x04, 0x10, 0x00
    };

    ret = ads1220_write_registers(config);

    if (ret < 0) {
        return ret;
    }

    /* Read back and verify */
    uint8_t readback[4];

    ret = ads1220_read_registers(readback);

    if (ret < 0) {
        return ret;
    }

    if (memcmp(config, readback, 4) != 0) {
        return -EIO;
    }

    /* Start continuous conversions */
    return ads1220_send_cmd(
        ADS1220_CMD_START
    );
}
```

这里假设上电复位已经完成、电源已稳定，且 ADC 使用内部时钟。初始化函数还需要与实际的 DRDY GPIO 配置和电源控制流程配合。

**这组 **`0x08, 0x04, 0x10, 0x00`** 是手册第 51 页官方伪代码示例，不是针对你实际电桥硬件确认的最终寄存器设置。**

## **六、针对踏频功率计的采样配置建议**

这一部分是结合你的应用所做的建议，而不是手册对踏频功率计的专用规定。

### **6.1 参考 TI 官方电桥测量方案**

手册第 62～63 页的图 9-11 和表 9-6 使用 4 线称重传感器、5 V 电桥激励以及比例式测量，配置如下：

| 寄存器 | TI 示例数值 | 配置内容 |
| --- | --- | --- |
| REG0 | `0x3E` | AIN1–AIN2，增益 128 |
| REG1 | `0x04` | 20 SPS，正常模式、连续转换 |
| REG2 | `0x98` | REFP1/REFN1 外部基准、双频抑制、低侧开关 |
| REG3 | `0x00` | 关闭 IDAC，独立 DRDY |

这套配置的重点是：电桥激励电压被用作 ADC 的参考电压，使测量结果具有比例式特性。

但在你的实际 PCB 中，不能直接复制 `0x98`，除非电桥激励和 REFP1/REFN1、低侧电源开关的连接方式确实与 TI 示例一致。

### **6.2 建议的初期调试参数**

假设你的硬件满足以下条件：

- 电桥信号分别接入 AIN1、AIN2。
- 电桥与 ADS1220 AVDD 使用相同的激励/供电电压。
- 不使用 ADS1220 的内部低侧电源开关。
- 使用独立 DRDY 引脚。

那么可以先尝试下面这组参数。

> [Chart or card]
>
> 初步配置：90 SPS、增益 128、连续转换 采样速率 PGA 增益 参考电压选择 AVDD − AVSS 内部 2.048 V REFP0 / REFN0 REFP1 / REFN1 REG 0 0x3E REG 1 0x44 REG 2 0xC0 REG 3 0x00 写入帧：0x43 0x3E 0x44 0xC0 0x00

这里将 90 SPS 作为初步调试选择，是因为功率计需要获得随曲柄转动变化的扭矩信号，而不只是稳定的静态称重结果。

20 SPS 更偏重低噪声和工频抑制；90 SPS 或 175 SPS 可以获得更多动态采样点，但噪声也会增加。最终需要结合实际应变信号带宽、滤波和功率计算方法决定。

同时，增益必须结合电桥最大输出以及 PGA 共模输入范围校核，不能单纯认为 128 倍一定最好。

## **七、推荐的驱动模块划分**

后续正式开发时，建议不要把所有 ADS1220 操作都直接写在 `main.c` 中。

```
CadenceSensor/
├── src/
│   ├── main.c
│   └── drivers/
│       └── ads1220.c
│
├── include/
│   └── drivers/
│       └── ads1220.h
│
├── boards/
│   └── your_board.overlay
│
├── prj.conf
└── CMakeLists.txt
```

建议在 `ads1220.h` 中提供以下应用接口：

```
int ads1220_init(void);

int ads1220_reset(void);
int ads1220_start(void);
int ads1220_powerdown(void);

int ads1220_write_reg(uint8_t reg, uint8_t value);
int ads1220_read_reg(uint8_t reg, uint8_t *value);

int ads1220_read_raw(int32_t *value);

double ads1220_raw_to_voltage(
    int32_t raw,
    double vref,
    uint16_t gain
);
```

实际工程中还可以加入增益设置、采样率设置、零点校准和采样超时等接口。

推荐采用 **GPIO DRDY 中断 + Zephyr 信号量 + 采样线程** 的方案：

```
ADS1220 完成转换
        |
        v
DRDY 下降沿中断
        |
        v
k_sem_give()
        |
        v
采样线程唤醒
        |
        v
spi_transceive_dt()
        |
        v
获得 int32_t ADC 原始值
        |
        v
零点修正 / 电压换算
        |
        v
扭矩计算 / 功率计算
```

GPIO 中断回调只负责通知线程，不建议在中断中直接执行阻塞式 SPI 操作。

## **八、实际硬件验证顺序**

你当前已经进入 PCB 焊接验证和固件开发阶段，我建议按下面的顺序测试 ADS1220。

> [Chart or card]
>
> ADS1220 硬件验证清单 0 / 7 已完成 检查供电 确认 AVDD、DVDD、AVSS、DGND 以及 CLK 引脚状态正确 SPI 通信测试 发送 RESET 0x06，随后读取全部 4 个寄存器，确认默认为 0x00 寄存器写入回读 写入一组测试配置，再读回比较每一个字节 启动 ADC 采样 发送 START/SYNC 0x08，并观察 DRDY 是否周期性变化 采样数据验证 读取 3 字节数据，检查数值是否随输入改变 输入短接测试 使用 MUX=1110 进行内部零点采样，验证偏移与噪声 连接应变电桥 读取真实电桥输出，检查静态零点、加载变化和增益是否合理

这里需要强调：**能够正确读回配置寄存器，只能证明 SPI 寄存器通信基本正常，并不能证明 ADC 模拟输入和采样通道都正常。** 后续还需要通过已知电压或应变电桥加载测试验证。

## **最终总结**

对于 ADS1220 驱动开发，最重要的几条规则是：

- **SPI 必须采用 Mode 1**，发送 RREG/WREG 命令与后续数据时要维持完整的片选事务。
- **4 个配置寄存器全部为 8 位**，可以用 `0x23` 一次读回、用 `0x43` 一次写入。
- **采样结果为有符号 24 位补码**，读取后必须完成符号扩展。
- **连续转换需要 START/SYNC 启动**，DRDY 变低表示新数据已经准备好。
- **推荐独立 DRDY 中断触发读取**，避免无意义的高频轮询。
- **基准电压和 PGA 配置必须匹配实际电桥硬件**，否则即使 SPI 通信完全正确，测量结果仍可能错误。

对于你的项目，下一步最有价值的是先实现 `RESET → WREG → RREG → START/SYNC → DRDY → 24 位读取` 这一条完整通信链路。确认它工作正常后，再增加标定、滤波及踏频功率计算逻辑。

