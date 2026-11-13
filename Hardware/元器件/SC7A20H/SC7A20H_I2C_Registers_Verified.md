# SC7A20H I²C 读写指令及寄存器配置（核对版）

> 适用器件：士兰微电子 **SC7A20H / SC7A20HTR**  
> 基准资料：用户提供的《SC7A20H 说明书》**V0.7**（31 页）  
> 交叉核对：《SC7A20H 说明书》**V1.1**（公开流通版本，31 页）  
> 核对日期：2026-11-13  
> 适用项目：nRF52832 + nRF Connect SDK / Zephyr 的三轴加速度采集

**核对结论**：本文中标注为“已核对”的 I²C 地址、通信帧格式、主要控制位、基础写入值和数据寄存器位置，已与 V0.7、V1.1 对照。**这属于文档层面的核对，不代表已经在实际硬件上完成 I²C 波形、功耗、FIFO 或中断验证。** 两版手册自身存在不一致之处，已明确列为“待确认”，而非强行得出确定结论。

资料对应章节：V0.7 第 8–9、12–19、21–22、26–28 页；V1.1 第 8–9、12–19、26–28 页。

---

## 1. I²C 接口基本参数

| 项目 | 数值 / 说明 | 状态 |
|---|---|---|
| 总线角色 | SC7A20H 为 I²C 从设备 | 已核对 |
| 总线速率 | 标准模式 ≤100 kHz；快速模式 ≤400 kHz | 已核对 |
| `CS` (Pin 10) | 高电平选择 I²C，芯片带内部上拉 | 已核对 |
| `SDA` (Pin 2) | I²C 数据线 | 已核对 |
| `SCL` (Pin 12) | I²C 时钟线 | 已核对 |
| `SDO` (Pin 1) | I²C 地址选择 | 已核对 |
| SDA/SCL 外部上拉 | 典型电路推荐各 4.7 kΩ 到 VDDIO | 已核对 |
| VDD / VDDIO 去耦 | 典型电路推荐各 100 nF | 已核对 |
| VDD | 1.71–3.6 V | 已核对 |
| VDDIO | 1.71 V 至 VDD+0.1 V | 已核对 |

### 1.1 I²C 从设备地址

| SDO 接法 | 7 位地址（MCU API 使用） | 总线写地址（8 位） | 总线读地址（8 位） | 注意事项 |
|---|---:|---:|---:|---|
| 悬空 / 逻辑高 | `0x19` | `0x32` | `0x33` | **推荐**；避免 SDO 接地导致上拉漏电 |
| 逻辑低 | `0x18` | `0x30` | `0x31` | 存在 SDO 内部上拉电流问题 |

**注意**：nRF Connect SDK / Zephyr 的 `i2c_write()`、`i2c_write_read()` 使用 **7 位地址**（通常为 `0x19`），不要传入 `0x32`/`0x33`。

手册定义：

```c
#define SC7A20H_I2C_ADDR  0x19u  // SDO 悬空或接高电平
// #define SC7A20H_I2C_ADDR 0x18u // SDO 接低电平
```

为了降低电流，建议优先使用 `0x19`。不应仅为消除 SDO 内部上拉电流而贸然写 `0x57` 寄存器：见第 8 节的“保留地址冲突”。

---

## 2. I²C 通信指令（重点）

说明：`S`=START、`Sr`=重复 START、`P`=STOP；`W`=写方向、`R`=读方向；`ACK` 和 `NACK` 是第 9 个时钟周期的应答。下列总线字节示例均按设备 **7 位地址 0x19** 展开。

### 2.1 单字节写寄存器

**命令格式**：

```text
S → SAD+W → ACK → SUB → ACK → DATA → ACK → P
```

**示例：向 `CTRL_REG1(0x20)` 写 `0x57`（100 Hz、XYZ 开启、正常模式）**：

```text
S → 0x32 → ACK → 0x20 → ACK → 0x57 → ACK → P
```

对应 MCU 调用：`write_reg(0x20, 0x57)`。

### 2.2 单字节读寄存器

**命令格式**：

```text
S → SAD+W → ACK → SUB → ACK → Sr → SAD+R → ACK → DATA → NACK → P
```

**示例：读取 `WHO_AM_I(0x0F)`**：

```text
S → 0x32 → ACK → 0x0F → ACK → Sr → 0x33 → ACK → [0x11] → NACK → P
```

预期返回值：`WHO_AM_I=0x11`。另可读 `VERSION(0x70)`，手册给出的默认值为 `0x28`，建议作为辅助识别值而非无条件硬编码为唯一合法版本。

### 2.3 多字节写寄存器

在寄存器子地址的 **Bit7 设置为 1**，启动地址自增；低 7 位仍是实际寄存器地址：

```text
SUB = (起始寄存器地址 & 0x7F) | 0x80

S → SAD+W → ACK → SUB → ACK → DATA0 → ACK → DATA1 → ACK → … → P
```

示例：从 `0x20` 开始写三个相邻寄存器，发送子地址 `0xA0`。**仅在确认范围内全是目标、可写寄存器时才这样操作**，不要跨越保留区域或把读写属性不同的寄存器当作连续配置区。

### 2.4 多字节读寄存器

```text
S → SAD+W → ACK → (REG | 0x80) → ACK
  → Sr → SAD+R → ACK
  → DATA0 → ACK → DATA1 → ACK → … → DATA_LAST → NACK → P
```

**示例：连续读 `0x28` 至 `0x2D` 的 XYZ 数据（6 字节）**：

```text
S → 0x32 → ACK → 0xA8 → ACK → Sr → 0x33 → ACK
  → X_L → ACK → X_H → ACK
  → Y_L → ACK → Y_H → ACK
  → Z_L → ACK → Z_H → NACK → P
```

返回字节顺序：`X_L, X_H, Y_L, Y_H, Z_L, Z_H`（要求 `CTRL_REG4.BLE=0`，即以下推荐配置）。

**结论**：`0xA8` 是“起始地址 `0x28` + 自增位 `0x80`”组成的 **通信子地址**，不是一个真实的独立寄存器。

---

## 3. 最常用寄存器速查表

| 地址 | 寄存器 | 属性 | 说明 |
|---|---|---|---|
| `0x0F` | `WHO_AM_I` | R | 芯片标识，手册值 `0x11` |
| `0x1F` | `CTRL_REG0` | R/W | OSR、DLPF 高位、HR 工作模式 |
| `0x20` | `CTRL_REG1` | R/W | ODR、LPen、X/Y/Z 使能 |
| `0x21` | `CTRL_REG2` | R/W | 高通滤波及数据源 |
| `0x22` | `CTRL_REG3` | R/W | INT1 事件路由、FIFO 8/12 位输出选择 |
| `0x23` | `CTRL_REG4` | R/W | BDU、BLE、量程、自测试、DLPF 低位 |
| `0x24` | `CTRL_REG5` | R/W | FIFO 使能、AOI 使能及锁存 |
| `0x25` | `CTRL_REG6` | R/W | INT2 事件路由、中断输出极性/类型 |
| `0x27` | `DRDY_STATUS_REG` | R | 新数据就绪、覆盖状态 |
| `0x28`/`0x29` | `OUT_X_L` / `OUT_X_H` | R | X 轴数据低/高字节 |
| `0x2A`/`0x2B` | `OUT_Y_L` / `OUT_Y_H` | R | Y 轴数据低/高字节 |
| `0x2C`/`0x2D` | `OUT_Z_L` / `OUT_Z_H` | R | Z 轴数据低/高字节 |
| `0x2E` | `FIFO_CTRL_REG` | R/W | FIFO 模式、FIFO 水位阈值 |
| `0x2F` | `FIFO_SRC_REG` | R | FIFO 水位/溢出/空/数量标志 |
| `0x30`/`0x31` | `AOI1_CFG` / `AOI1_SRC` | R/W / R | AOI1 事件配置 / 状态 |
| `0x32`/`0x33` | `AOI1_THS` / `AOI1_DURATION` | R/W | AOI1 阈值 / 持续时间 |
| `0x34`/`0x35` | `AOI2_CFG` / `AOI2_SRC` | R/W / R | AOI2 事件配置 / 状态 |
| `0x36`/`0x37` | `AOI2_THS` / `AOI2_DURATION` | R/W | AOI2 阈值 / 持续时间 |
| `0x38`–`0x3D` | `CLICK_*` | R/W、R | 敲击功能配置/状态/系数 |
| `0x57` | `DIG_CTRL` | **有争议** | 细节章节定义，但寄存器总表将 `0x40–0x5F` 标为保留 |
| `0x61`–`0x66` | `OUT_*_New` | R | 另一组三轴实时输出；字节顺序描述矛盾 |
| `0x68` | `SOFT_RESET` | R/W | 写入 `0xA5` 软复位 |
| `0x69` | `FIFO_DATA` | R | FIFO 数据读取口；读取次数说明有歧义 |
| `0x6F` | `I2C_CTRL` | R/W | I²C 关闭控制；**一般不要改动** |
| `0x70` | `VERSION` | R | 手册默认 `0x28`，配合 `0x0F` 识别版本 |

**严禁写入**手册明确定义的保留地址（尤其 `0x00–0x0B` 及有冲突的 `0x40–0x5F`）。也不要修改工厂校准数据。V1.1 建议“配置完寄存器后延时 1 ms 再进行寄存器读取操作”。

---

## 4. 关键寄存器位定义与常用配置值

### 4.1 `CTRL_REG0 (0x1F)`

```text
Bit7    Bit6     Bit5     Bit4     Bit3  Bit2  Bit1     Bit0
  --    OSR[2]   OSR[1]   OSR[0]    --    --   DLPF[1]    HR
```

- `OSR[2:0]`：`000`=ODR，`001`=ODR/2，`010`=ODR/4，`011`=ODR/8，`100`=ODR/16，`101`–`111`=ODR/32。
- `DLPF[1]`：数字低通滤波设置的高位（低位在 `0x23.Bit3`）。
- `HR`：`0`=正常或低功耗；`1`=高性能或增强模式。
- **常用值：`0x00`**（不分频、默认低通配置、HR=0）。

### 4.2 `CTRL_REG1 (0x20)`

```text
Bit7  Bit6  Bit5  Bit4  Bit3   Bit2 Bit1 Bit0
ODR3  ODR2  ODR1  ODR0  LPen   Zen  Yen  Xen
```

- `ODR[3:0]`：输出数据率；`0000` 为停止采样/电源关断模式。
- `LPen=0`：正常模式（当 HR=0）；`LPen=1`：低功耗模式（当 HR=0）。
- `Zen/Yen/Xen`：各轴 `1`=使能；全开为 `0b111`。

在 `0x1F.HR=0` 且 XYZ 全开的前提下：

| 输出率 | ODR 位 | 正常模式写入 `0x20` | 低功耗模式写入 `0x20` |
|---|---|---|---|
| 停止采样 | `0000` | `0x07` | — |
| 1.56 Hz | `0001` | `0x17` | `0x1F` |
| 12.5 Hz | `0010` | `0x27` | `0x2F` |
| 25 Hz | `0011` | `0x37` | `0x3F` |
| 50 Hz | `0100` | `0x47` | `0x4F` |
| **100 Hz** | `0101` | **`0x57`** | **`0x5F`** |
| 200 Hz | `0110` | `0x67` | `0x6F` |
| 400 Hz | `0111` | `0x77` | `0x7F` |
| 800 Hz | `1000` | `0x87` | 需按模式限制验证 |

> 注意：手册对 1.48 kHz、2.66 kHz、4.434 kHz 定义为高性能模式。需要结合 `0x1F.HR=1` 设置，不应只改 `0x20` 的 ODR 位。表中的“低功耗”是配置逻辑，不保证每个 ODR 的实际功耗表现。

### 4.3 `CTRL_REG2 (0x21)`

```text
Bit7  Bit6  Bit5   Bit4   Bit3  Bit2       Bit1   Bit0
 --   HDS   HPCF2  HPCF1  FDS   HP_RESET   HPIS2  HPIS1
```

- `FDS=0`：数据输出绕过内部高通滤波器；`FDS=1`：高通数据输出到数据寄存器/FIFO。
- `HPCF[1:0]`：高通截止频率选择（随 ODR 改变）。
- `HPIS1/HPIS2`：AOI1/AOI2 的高通滤波使能。
- **基础采样写入：`0x00`**，以保留重力分量。

在 ODR=100 Hz 时，`HPCF=00/01/10/11` 的高通截止频率分别为约 8/4/0.5/0.2 Hz（见手册第 16 页）。

### 4.4 `CTRL_REG3 (0x22)`

```text
Bit7      Bit6     Bit5     Bit4      Bit3   Bit2    Bit1       Bit0
I1_CLICK  I1_AOI1  I1_AOI2  I1_DRDY    --    I1_WTM  I1_OVERRUN FIFO_MODE
```

- 基础轮询：`0x00`。
- DRDY 映射到 INT1：`0x10`。
- FIFO 水位中断映射到 INT1：`0x04`。
- `FIFO_MODE`：`0`=FIFO 12 位数据模式；`1`=FIFO 8 位数据模式。**该位与 `0x2E` 的 FIFO 工作模式字段不同。**

### 4.5 `CTRL_REG4 (0x23)`

```text
Bit7  Bit6  Bit5  Bit4  Bit3     Bit2 Bit1 Bit0
BDU   BLE   FS1   FS0   DLPF[0]  ST1  ST0  SIM
```

- `BDU=1`：等待高低字节均被读取后才更新输出寄存器；建议使能。
- `BLE=0`：低字节位于低地址，适用于 `0x28–0x2D` 的读取顺序。
- `FS[1:0]`：`00`=±2g；`01`=±4g；`10`=±8g；`11`=±16g。
- `ST1:ST0=00`：关闭自测试。
- `SIM=0`：SPI 四线模式标志；I²C 使用时保持 0 即可。

| 量程 | FS 位 | `BDU=1`、`BLE=0`、自测试关闭时写入 `0x23` | 手册标称灵敏度 |
|---|---|---|---|
| ±2g | `00` | `0x80` | 1 mg/digit |
| ±4g | `01` | `0x90` | 2 mg/digit |
| **±8g** | `10` | **`0xA0`** | 4 mg/digit |
| ±16g | `11` | `0xB0` | 8 mg/digit |

> 该表还假定 `DLPF[0]=0`。不要把 `0x23.Bit3` 当成 HR 位；**HR 位在 `0x1F.Bit0`**。

### 4.6 `CTRL_REG5 (0x24)`

```text
Bit7  Bit6     Bit5    Bit4 Bit3      Bit2      Bit1      Bit0
BOOT  FIFO_EN  AOI_EN   --  LIR_INT1  D4D_INT1  LIR_INT2  D4D_INT2
```

- `FIFO_EN=1`：使能 FIFO；只使能 FIFO 时为 `0x40`。
- `BOOT=1`：重新载入内部修调值；平常不要主动操作。
- `AOI_EN` 在手册中的语义是 `0`=使能 AOI 中断，`1`=禁止 AOI 中断；不要按字段名称直觉理解。
- 不使用 FIFO、AOI 锁存时，写 `0x00`。

### 4.7 `CTRL_REG6 (0x25)`

```text
Bit7      Bit6     Bit5     Bit4     Bit3      Bit2       Bit1        Bit0
I2_CLICK  I2_AOI1  I2_AOI2  I2_BOOT  I2_DRDY   CS_PU_EN  H_LACTIVE   INT_PP_OD
```

- 关闭 INT2 功能并保持默认输出极性：`0x00`。
- DRDY 映射到 INT2：`0x08`。
- `H_LACTIVE=0`：中断有效时高电平；`1`：有效时低电平。
- `INT_PP_OD=0`：推挽输出；`1`：开漏输出。
- `CS_PU_EN`（Bit2）手册标注为 `0`=CS 上拉使能、`1`=禁止；更改前确保 CS 外部电平确定。

### 4.8 `DRDY_STATUS_REG (0x27)`

```text
Bit7   Bit6 Bit5 Bit4 Bit3    Bit2 Bit1 Bit0
ZYXOR  ZOR  YOR  XOR  ZYXDA   ZDA  YDA  XDA
```

- `(status & 0x08) != 0`：XYZ 新数据均已准备好。
- `(status & 0x80) != 0`：存在至少一个轴的数据覆盖。
- 获取完整一组数据时，建议先看 Bit3，再一次性读 6 字节。

---

## 5. 经过核对的初始化读写指令

面向 nRF52832 踏频计的**首轮硬件验证**，建议 **100 Hz + ±8g + 正常模式 + BDU + 不使用 FIFO/中断**：

| 顺序 | 操作 | 寄存器地址 | 数据值 | 目的 |
|---:|---|---|---|---|
| 1 | 读 | `0x0F` | 预期 `0x11` | 校验设备 ID |
| 2 | 读 | `0x70` | 手册默认 `0x28` | 辅助校验版本 |
| 3 | 写 | `0x20` | `0x07` | 停止采样，方便修改配置 |
| 4 | 写 | `0x1F` | `0x00` | HR=0，OSR=0，DLPF[1]=0 |
| 5 | 写 | `0x21` | `0x00` | 不把高通结果作为原始输出 |
| 6 | 写 | `0x22` | `0x00` | 关闭 INT1 路由 |
| 7 | 写 | `0x23` | **`0xA0`** | ±8g、BDU=1、BLE=0 |
| 8 | 写 | `0x24` | `0x00` | 关闭 FIFO 和中断锁存 |
| 9 | 写 | `0x25` | `0x00` | 关闭 INT2 路由 |
| 10 | 写 | `0x20` | **`0x57`** | 100 Hz 正常采样，XYZ 全开 |
| 11 | 延时 | — | **建议 20 ms** | 等待至少一轮采样（工程建议） |
| 12 | 读 | `0x27` | 检查 Bit3 | 新三轴数据就绪 |
| 13 | 连续读 | `0x28–0x2D` | 子地址 `0xA8` | XYZ 各 2 字节 |

上表中“建议 20 ms”是根据 100 Hz 采样周期给出的工程等待时间，**不是数据手册保证的最大起振/采样稳定时间**。V1.1 还建议在配置寄存器后等 1 ms 再读回寄存器。

相应的核心写命令为：

```c
write_reg(0x20, 0x07); // 暂停采样
write_reg(0x1F, 0x00); // HR/OSR 配置
write_reg(0x21, 0x00); // 高通配置
write_reg(0x22, 0x00); // INT1 关闭
write_reg(0x23, 0xA0); // BDU=1, ±8g
write_reg(0x24, 0x00); // FIFO 关闭
write_reg(0x25, 0x00); // INT2 关闭
write_reg(0x20, 0x57); // 100Hz/XYZ 使能
```

### 5.1 快速切换命令

| 目标 | I²C 寄存器写入 | 前提 |
|---|---|---|
| 100 Hz 正常模式 | `0x1F←0x00`，`0x20←0x57` | XYZ 全开 |
| 100 Hz 低功耗模式 | `0x1F←0x00`，`0x20←0x5F` | XYZ 全开 |
| 50 Hz 正常模式 | `0x20←0x47` | `HR=0` |
| 200 Hz 正常模式 | `0x20←0x67` | `HR=0` |
| 停止采样 | `0x20←0x07` | 只停止采样，不切断 VDD |
| 量程 ±2g | `0x23←0x80` | BDU=1、BLE=0、DLPF[0]=0 |
| 量程 ±4g | `0x23←0x90` | 同上 |
| 量程 ±8g | `0x23←0xA0` | 同上 |
| 量程 ±16g | `0x23←0xB0` | 同上 |
| 将 DRDY 路由到 INT1 | `0x22←0x10` | 会替换 `0x22` 其他位；需按需组合位掩码 |
| 将 DRDY 路由到 INT2 | `0x25←0x08` | 会替换 `0x25` 其他位；需按需组合位掩码 |
| 软件复位 | `0x68←0xA5` | 复位后重新初始化；再次访问需等待稳定 |

**注意**：上述 `0x20←0x57` 表示“**寄存器地址 0x20、数据 0x57**”，完全不同于“写入寄存器地址 0x57”；后者属于手册有争议的地址。

---

## 6. 三轴数据拼接与换算

默认 `BLE=0` 时，读取 `0x28–0x2D` 的顺序为 `XL, XH, YL, YH, ZL, ZH`。手册给出的是**16 位寄存器容器中的左对齐 12 位二进制补码**。

```c
// 高低字节拼接，提取高 12 位，并手动符号扩展；不依赖负数右移行为。
static int16_t sc7_decode12(uint8_t lo, uint8_t hi)
{
    uint16_t word = ((uint16_t)hi << 8) | lo;
    int16_t value = (int16_t)(word >> 4); // 0..4095
    if (value & 0x0800) {
        value -= 4096;
    }
    return value;                        // -2048..2047
}
```

采用手册**第 19 页示例表所定义的理想比例**：

```text
加速度 (g) = raw12 * 正向量程(g) / 2048
```

| 量程 | 按手册示例表换算 (g/count) | 手册标称灵敏度 (mg/digit) |
|---|---:|---:|
| ±2g | 2/2048 = 0.0009765625 | 约 1 |
| ±4g | 4/2048 = 0.001953125 | 约 2 |
| ±8g | 8/2048 = 0.00390625 | 约 4 |
| ±16g | 16/2048 = 0.0078125 | 约 8 |

**验算例子（手册第 19 页）**：

- `H=0x40,L=0x00` → `word=0x4000` → `raw12=+1024` → 在 ±2g 下 `+1.0g`。
- `H=0xE0,L=0x00` → `word=0xE000` → `raw12=-512` → 在 ±2g 下 `-0.5g`。
- `H=0xC0,L=0x00` → `word=0xC000` → `raw12=-1024` → 在 ±8g 下 `-4.0g`。

这些计算解释了 V0.7/V1.1 手册“1/2/4/8 mg/digit”与示例表之间的小幅数值差异：前者是约数或标称值，后者相当于按 ±FS/2048 映射。在实物上仍应通过静置重力方向、反向翻转和必要的标定来确定工程换算值。

---

## 7. 可选功能命令：中断与 FIFO

### 7.1 数据就绪中断（DRDY）

| 功能 | 寄存器 | 数据 |
|---|---|---|
| DRDY → INT1 | `0x22` | `0x10` |
| DRDY → INT2 | `0x25` | `0x08` |
| 关闭对应映射 | 该寄存器对应 Bit 清零 | 推荐**读-改-写**，避免覆盖其他中断功能 |

默认中断为高电平有效、推挽输出（`0x25` 的 Bit1=0、Bit0=0）。连接 nRF52832 GPIO 时，检查中断配置与引脚电气匹配。

### 7.2 FIFO 基础配置（仅作为文档级示例）

| 功能 | 寄存器 | 字段 |
|---|---|---|
| FIFO 使能 | `0x24.Bit6` | `FIFO_EN=1` |
| FIFO 工作模式 | `0x2E.Bit7:6` | `00` Bypass；`01` FIFO；`10` Stream；`11` Trigger |
| 水位阈值 | `0x2E.Bit4:0` | `FTH[4:0]` |
| FIFO 状态 | `0x2F` | WTM(7)、OVER(6)、EMPTY(5)、FSS(4:0) |
| FIFO 数据位宽 | `0x22.Bit0` | `0`=12 位；`1`=8 位 |
| FIFO 水位中断 → INT1 | `0x22.Bit2` | `I1_WTM=1` |
| FIFO 读数据口 | `0x69` | **需单独核实读取方法** |

一个仅用于展示字段组合的例子：

```c
write_reg(0x24, 0x40);  // FIFO_EN=1
write_reg(0x2E, 0x90);  // FM=10(Stream), FTH原始编码=0x10
write_reg(0x22, 0x04);  // INT1 FIFO水位中断，FIFO_MODE=0
```

**重要限制**：不能直接认定 `FTH=0x10` 必然意味着“16 组三轴完整样本”。V1.1 在 FIFO 阈值和数量描述中新增“1 对应 2 Byte”的说法，与其“按组数统计”和 `0x69` 的读出次数描述并不完全一致。**FIFO 数据取出次数、阈值实际单位、连续读的子地址行为需要示波器/逻辑分析仪或真实采集验证，不建议现阶段直接纳入正式踏频算法。**

### 7.3 AOI 运动阈值中断

- `0x30/0x34`：AOI1/AOI2 事件模式及轴选择。
- `0x32/0x36`：阈值，使用低 7 位；±2/4/8/16g 下分别为 16/32/64/128 mg 每 LSB。
- `0x33/0x37`：持续时间，步进以 ODR 为基准。
- `0x31/0x35`：中断事件状态。
- `0x22/0x25`：将 AOI1/AOI2 中断路由到 INT1/INT2。

只设置阈值不能保证实现正确的运动唤醒；还需验证高通滤波、事件 AND/OR、锁存、轴使能与极性。初始驱动建议暂不启用 AOI。

---

## 8. 文档差异、潜在错误和使用限制

以下内容**没有被两个版本彻底消除歧义**，因此不能标记为“经确认无误”：

1. **`0x57` 地址冲突**：V0.7 与 V1.1 的寄存器总表均将 `0x40–0x5F` 标为 Reserved（不应修改），但细节章节又给出 `DIG_CTRL(0x57)`，包含 SDO、SDA/SCL 内部上拉控制。**规避建议：不要写地址 `0x57`，优先 SDO 悬空/接高电平使用 `0x19`；如必须关闭内部上拉，应向原厂确认器件版本及安全操作。**
2. **`0x61–0x66` 字节顺序冲突**：寄存器总表命名 `0x61=X_L,0x62=X_H`；同一手册细节章节写 `0x61=XH,0x62=XL`（Y/Z 同类）。**规避建议：优先通过 `0x28–0x2D` 获取加速度。**
3. **`0x69` FIFO 数据次数歧义**：细节章节一方面写“组数×3 次读取”，另一方面说 12 位模式每组相当于 `0x28–0x2D` 共 6 个字节，8 位模式每组 3 个字节。V1.1 对 FIFO 数量和阈值又有追加描述。**规避建议：FIFO 读取前必须做实测。**
4. **`0x6F.I2C_UN` 的关闭流程**：手册指出 Bit2 可以关闭 I²C，且有特殊生效前置条件。**常规 I²C 驱动不要设置此位**，否则可能丢失通信通道。
5. **数据缩放约数**：标称灵敏度 1/2/4/8 mg/digit 与手册 12 位示例的精确比例有差距。**算法先按示例表的 ±FS/2048 计算，再做实物标定。**
6. **写入与读回延时**：V1.1 比 V0.7 明确新增“配置完寄存器后建议延时 1 ms 再读取”。**调试时按 V1.1 建议执行**。

这些事项不影响本文给出的基础链路（`WHO_AM_I → CTRL_REG0/1/2/3/4/5/6 → STATUS → 0x28–0x2D`）。对于没有手册内部冲突的寄存器，其位定义及上述字节值已逐一核对。

---

## 9. Zephyr 最小读写函数（可直接整合到工程）

> 前提：`CONFIG_I2C=y`，设备树 `i2c0` 已启用且 pinctrl 映射正确。若工程使用其他 I²C 控制器，应相应修改 `DT_NODELABEL(i2c0)`。

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#define SC7_ADDR 0x19u
#define SC7_BUS  DEVICE_DT_GET(DT_NODELABEL(i2c0))

static int sc7_write_reg(uint8_t reg, uint8_t val)
{
    const struct device *bus = SC7_BUS;
    uint8_t frame[] = {reg, val};
    if (!device_is_ready(bus)) return -ENODEV;
    return i2c_write(bus, frame, sizeof(frame), SC7_ADDR);
}

static int sc7_read_reg(uint8_t reg, uint8_t *buf, size_t len)
{
    const struct device *bus = SC7_BUS;
    if (!device_is_ready(bus)) return -ENODEV;
    if (buf == NULL || len == 0 || reg > 0x7Fu) return -EINVAL;
    uint8_t sub = reg | ((len > 1) ? 0x80u : 0x00u);
    return i2c_write_read(bus, SC7_ADDR, &sub, 1, buf, len);
}

static int sc7_init_basic(void)
{
    int ret;
    uint8_t id, version;

    ret = sc7_read_reg(0x0F, &id, 1);
    if (ret) return ret;
    if (id != 0x11) return -ENODEV;

    ret = sc7_read_reg(0x70, &version, 1);
    if (ret) return ret;
    /* version 的手册默认值为 0x28；可打印观察，不强制拒绝其他版本 */
    (void)version;

    static const uint8_t config[][2] = {
        {0x20, 0x07},
        {0x1F, 0x00},
        {0x21, 0x00},
        {0x22, 0x00},
        {0x23, 0xA0},
        {0x24, 0x00},
        {0x25, 0x00},
        {0x20, 0x57},
    };
    for (size_t i = 0; i < sizeof(config) / sizeof(config[0]); ++i) {
        ret = sc7_write_reg(config[i][0], config[i][1]);
        if (ret) return ret;
    }
    k_msleep(20); /* 工程等待值：100Hz 下预留约两个采样周期 */
    return 0;
}

static int sc7_read_xyz_raw(int16_t out[3])
{
    if (out == NULL) return -EINVAL;
    uint8_t status, data[6];
    int ret = sc7_read_reg(0x27, &status, 1);
    if (ret) return ret;
    if ((status & 0x08u) == 0) return -EAGAIN;

    ret = sc7_read_reg(0x28, data, sizeof(data)); /* SUB=0xA8 */
    if (ret) return ret;

    for (int axis = 0; axis < 3; ++axis) {
        uint16_t word = ((uint16_t)data[2 * axis + 1] << 8)
                      | data[2 * axis];
        int16_t raw = (int16_t)(word >> 4);
        if (raw & 0x0800) raw -= 4096;
        out[axis] = raw;
    }
    return 0;
}

/* 当前 ±8g 配置，按手册示例表的理想换算关系：
 * float ax_g = (float)raw[0] * (8.0f / 2048.0f);
 */
```

说明：这里没有自动操作存在争议的 `0x57`、`0x61–0x66`、`0x69`、`0x6F`。如果 MCU 软件的 I²C 驱动出现超时、NACK 或数据全 0，应优先检查供电、CS/SDO 电平、SCL/SDA 上拉和设备树引脚，而不是盲目写保留寄存器。

---

## 10. 建议的板级验证清单

- [ ] 测量 VDD 与 VDDIO，确认位于允许范围内。
- [ ] 确认 CS 高电平，SDO 对应期望的 `0x19` 或 `0x18`。
- [ ] I²C 总线读 `0x0F` 返回 `0x11`；记录 `0x70` 读值。
- [ ] 写 `0x20=0x07` 后配置 `0x1F/21/22/23/24/25`，再写 `0x20=0x57`。
- [ ] 按 V1.1 建议延时并读回 `0x1F`、`0x20`、`0x23`。
- [ ] 查询 `0x27 & 0x08`，确认数据准备完成。
- [ ] 用 `0xA8` 连续读 XYZ 六字节；验证每次读取数据随运动变化。
- [ ] 静止时加速度矢量模约为 1g；翻转传感器验证符号和轴向。
- [ ] 测量低功耗模式 `0x20=0x5F` 下的实际传感器与整机电流。
- [ ] 最后才进行 FIFO、AOI 运动唤醒及 DRDY 中断集成。

## 参考资料

1. 用户提供：杭州士兰微电子《SC7A20H 说明书》V0.7，31 页（本次对话附件）。
2. 公开流通版本：《SC7A20H 说明书》V1.1，31 页：[PDF 链接](https://pf.unikeyic.com/datasheet/202511/59f66cd3197e2534279e18eca6a9f806.pdf)。
3. [士兰微电子官网 SC7A20H 产品页](https://www.silan.com.cn/index.php/product/details/2564.html)。官网当前公开的是简要版说明书；本文对 V1.1 的核对不等同于原厂确认了手册内部冲突。
