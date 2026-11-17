# LSM6DSRTR I²C 寄存器配置与驱动开发规范

> **用途**：供后续 Coding Agent 实现 nRF52832 / Zephyr 下的 LSM6DSRTR 驱动。  
> **唯一芯片依据**：用户提供的 ST《LSM6DSR Datasheet – production data》，**DocID030186 Rev 1，March 2019，166 页**。LSM6DSRTR 仅为卷带包装型号。  
> **范围**：Mode 1 下主 I²C 接口、基础六轴采样、温度、DRDY 中断、FIFO、运动唤醒与低功耗。未覆盖 Sensor Hub/OIS/FSM/复杂压缩解码的完整驱动。  
> **标注原则**：**[手册]** 为手册定义；**[项目建议]** 为针对自行车曲柄踏频/功率计的设计选择；**[注意]** 为开发边界条件。不得将项目建议误当作芯片默认值。

## 1. 硬件条件与 I²C 通信

- **[手册 §5.1, pp.35–37]**：CS(12) 接 VDDIO 以启用 I²C；SDA(14)、SCL(13) 外部上拉到 VDDIO；I²C 支持标准模式与 400 kHz Fast Mode。
- **[项目电路]**：SA0/SDO(1) 接 GND，故 7 位 I²C 地址为 **`0x6A`**；若接 VDDIO 则为 `0x6B`。
- 7 位地址 `0x6A` 的总线地址字节：写 `0xD4`，读 `0xD5`；地址 `0x6B` 则写 `0xD6`，读 `0xD7`。Zephyr `reg = <0x6a>` 和 `i2c_*` API 均使用**7 位地址**，**不可**填 `0xD4`。
- **[手册 §5.3, p.44]**：上电时 INT1 的电平参与 I²C/I3C 共存选择：INT1 悬空（内部下拉）允许 I²C/I3C，INT1 被硬连 VDDIO 则仅 I3C。主控侧不得在传感器上电时主动将 INT1 驱动为高电平。
- **[手册 §7.1, p.57]**：Mode 1 的辅助 SDx(2)、SCx(3) 接 VDDIO 或 GND；NC 引脚保持符合官方参考连接；VDD(8)、VDDIO(5) 各配置近端 100 nF 去耦。
- **[项目建议]**：初期使用 SDA/SCL 各 4.7 kΩ 上拉，验收总线波形、功耗后可调整；使用短走线 400 kHz。

### 1.1 单字节写入

`START -> SAD+W -> ACK -> 8-bit register address (SUB) -> ACK -> DATA -> ACK -> STOP`

示例向 `CTRL1_XL(0x10)` 写 `0x48`：`S -> 0xD4 -> 0x10 -> 0x48 -> P`。

Zephyr：`i2c_reg_write_byte_dt(&imu, 0x10, 0x48)`。

### 1.2 单字节读取

`START -> SAD+W -> ACK -> SUB -> ACK -> REPEATED START -> SAD+R -> ACK -> DATA -> NACK -> STOP`

例如 `WHO_AM_I(0x0F)`：预期读到 `0x6B`。Zephyr：`i2c_reg_read_byte_dt(&imu, 0x0F, &id)`。

### 1.3 连续读取

- **[手册 §5.1, p.36; §9.17 p.76]**：`CTRL3_C.IF_INC` (bit2) = 1，启用连续传输期间地址自动递增，**上电默认值 = 1**。
- 建议 `i2c_burst_read_dt(&imu, 0x22, buf, 12)` 一次读取陀螺仪和加速度计；所有 API 必须检查返回值。
- **[注意]**：不能将该芯片的普通连续读与 FIFO 连续读混为一谈；FIFO 的 `0x78...0x7E` 代表弹出的记录，其 TAG 必须单独解析。

## 2. 主寄存器速查（主寄存器 Bank）

| 地址 | 名称 | 属性 | 作用 | 手册页 |
|---|---|---|---|---|
| `0x07` | `FIFO_CTRL1` | R/W | FIFO 水位 WTM[7:0] | 68 |
| `0x08` | `FIFO_CTRL2` | R/W | WTM[8]、压缩与 STOP_ON_WTM | 68 |
| `0x09` | `FIFO_CTRL3` | R/W | Gyro / XL 批量采样率 BDR | 69 |
| `0x0A` | `FIFO_CTRL4` | R/W | FIFO 模式 | 70 |
| `0x0D` | `INT1_CTRL` | R/W | INT1 数据就绪/FIFO 中断路由 | 72 |
| `0x0E` | `INT2_CTRL` | R/W | INT2 数据就绪/FIFO 中断路由 | 73 |
| `0x0F` | `WHO_AM_I` | R | 固定 `0x6B` | 73 |
| `0x10` | `CTRL1_XL` | R/W | 加速度 ODR、FS、LPF2 | 74 |
| `0x11` | `CTRL2_G` | R/W | 陀螺仪 ODR、FS | 75 |
| `0x12` | `CTRL3_C` | R/W | SW_RESET、BDU、IF_INC、INT 引脚电气特性 | 76 |
| `0x13` | `CTRL4_C` | R/W | 陀螺仪 Sleep、I²C 禁用、滤波/中断选项 | 77 |
| `0x15` | `CTRL6_C` | R/W | XL_HM_MODE、GY LPF1 FTYPE | 79 |
| `0x16` | `CTRL7_G` | R/W | G_HM_MODE、GY HPF | 80 |
| `0x1A` | `ALL_INT_SRC` | R | 综合事件来源 | 84 |
| `0x1B` | `WAKE_UP_SRC` | R | 唤醒事件来源 | 84 |
| `0x1E` | `STATUS_REG` | R | TDA/GDA/XLDA | 86 |
| `0x20–0x21` | `OUT_TEMP_L/H` | R | 温度，16 位补码 | 86 |
| `0x22–0x27` | `OUTX/Y/Z_L/H_G` | R | 陀螺仪三轴，16 位补码 | 87–88 |
| `0x28–0x2D` | `OUTX/Y/Z_L/H_A` | R | 加速度三轴，16 位补码 | 88–89 |
| `0x3A` | `FIFO_STATUS1` | R | 未读 FIFO 记录数低 8 位 | 92 |
| `0x3B` | `FIFO_STATUS2` | R | 计数高 2 位/满/溢出/阈值 | 92 |
| `0x5B` | `WAKE_UP_THS` | R/W | 唤醒阈值 | 97 |
| `0x5C` | `WAKE_UP_DUR` | R/W | 唤醒持续时间、阈值权重 | 97 |
| `0x5E` | `MD1_CFG` | R/W | 唤醒、自由落体等事件到 INT1 | 99 |
| `0x5F` | `MD2_CFG` | R/W | 事件到 INT2 | 100 |
| `0x78` | `FIFO_DATA_OUT_TAG` | R | FIFO 当前记录 TAG | 108 |
| `0x79–0x7E` | `FIFO_DATA_OUT_X/Y/Z_L/H` | R | FIFO 当前记录的 6 字节数据 | 109 |

**[注意]**：寄存器映射是主 Bank 地址；嵌入式功能 Bank、Sensor Hub Bank 可能使用相同的数字地址，不得混用。只有使用 BANK 切换 API 之后，才能访问相应 Bank。此文档的基本驱动默认访问主 Bank。

## 3. 核心控制寄存器逐位定义

### 3.1 `WHO_AM_I` (`0x0F`, R)

- **[手册 §9.14 p.73]**：返回固定 `0x6B`。
- 测试顺序：供电正常 -> 检查总线 ready -> 读 `0x0F` -> 值必须为 `0x6B`。
- **注意**：读值错误和 I²C 通信错误应该有不同的错误码/日志。

### 3.2 `CTRL3_C` (`0x12`, R/W)

| bit | 名称 | 0 / 1 含义 |
|---|---|---|
| 7 | `BOOT` | 1：重载内部存储器内容，自动清零 |
| 6 | `BDU` | 0：连续更新；1：输出数据 MSB、LSB 均被读取前不更新该输出字 |
| 5 | `H_LACTIVE` | 0：中断高有效；1：低有效 |
| 4 | `PP_OD` | 0：推挽；1：开漏 |
| 3 | `SIM` | SPI 4-wire/3-wire，I²C 通常保持 0 |
| 2 | `IF_INC` | 0：连续访问地址不递增；1：地址递增（默认 1） |
| 1 | Reserved | 始终保留为 0 |
| 0 | `SW_RESET` | 1：软件复位，自动清零 |

- **[项目建议]**：复位后写 `0x44`（BDU=1、IF_INC=1，INT 推挽高有效）。
- 正确复位操作：写 `0x01` 到 `0x12` -> 轮询 bit0 直到为 0（加超时） -> 再写 `0x44` -> 再写其余配置。
- **[注意]**：`BDU=1` 仅保证同一 16 位寄存器对的高低字节一致；不保证六轴完全硬件同步。

### 3.3 `CTRL1_XL` (`0x10`, R/W)

```text
bits 7:4  ODR_XL[3:0]
bits 3:2  FS_XL[1:0]
bit  1    LPF2_XL_EN
bit  0    Reserved (0)
```

**加速度计量程编码 [手册 p.74]**：

| FS_XL[1:0] | g | 灵敏度 mg/LSB |
|---|---:|---:|
| `00` | ±2 | 0.061 |
| `01` | ±16 | 0.488 |
| `10` | ±4 | 0.122 |
| `11` | ±8 | 0.244 |

**加速度计 ODR 编码 [手册 Table 51 p.74]**：

| ODR[3:0] | `XL_HM_MODE=0`（高性能允许） | `XL_HM_MODE=1`（高性能禁止） |
|---|---|---|
| `0x0` | Power-down | Power-down |
| `0x1` | 12.5 Hz（高性能） | 12.5 Hz（低功耗） |
| `0x2` | 26 Hz（高性能） | 26 Hz（低功耗） |
| `0x3` | 52 Hz（高性能） | 52 Hz（低功耗） |
| `0x4` | 104 Hz（高性能） | 104 Hz（普通） |
| `0x5` | 208 Hz（高性能） | 208 Hz（普通） |
| `0x6` | 416 Hz | 416 Hz（高性能） |
| `0x7` | 833 Hz | 833 Hz（高性能） |
| `0x8` | 1.66 kHz | 1.66 kHz |
| `0x9` | 3.33 kHz | 3.33 kHz |
| `0xA` | 6.66 kHz | 6.66 kHz |
| `0xB–0xF` | **不允许** | **不允许** |

> **重要更正**：先前对话中曾把 `0xB` 解释为低功耗 1.6 Hz。对照本手册 **Table 51**，该编码 **不允许**；本 Rev 1 手册并未给出以 `0xB` 实现 1.6 Hz 的方法。驱动不得写 `0xB0` 作为常规低功耗配置。

`LPF2_XL_EN=1`：选择 LPF2 二级滤波输出；其带宽还需配合 `CTRL8_XL` 的设置，不应只设置此位就假定特定截止频率。

**[项目建议]**：初期 `0x48` = 104 Hz、±4g、未开 LPF2。

### 3.4 `CTRL2_G` (`0x11`, R/W)

```text
bits 7:4  ODR_G[3:0]
bits 3:2  FS_G[1:0]
bit  1    FS_125
bit  0    FS_4000
```

**陀螺仪 ODR [手册 Table 54 p.75]**：`0x0` 关，`0x1` 12.5 Hz，`0x2` 26 Hz，`0x3` 52 Hz，`0x4` 104 Hz，`0x5` 208 Hz，`0x6` 416 Hz，`0x7` 833 Hz，`0x8` 1.66 kHz，`0x9` 3.33 kHz，`0xA` 6.66 kHz；`0xB` 不可用，不要擅自使用其他保留编码。`G_HM_MODE=1` 下 12.5/26/52Hz 为低功耗、104/208Hz 为普通模式。

**量程编码 [手册 p.75]**：

| 条件 | 量程 dps | 灵敏度 mdps/LSB |
|---|---:|---:|
| FS_125=1, FS_4000=0 | ±125 | 4.375 |
| FS_125=0, FS_4000=0, FS_G=00 | ±250 | 8.75 |
| FS_125=0, FS_4000=0, FS_G=01 | ±500 | 17.50 |
| FS_125=0, FS_4000=0, FS_G=10 | ±1000 | 35 |
| FS_125=0, FS_4000=0, FS_G=11 | ±2000 | 70 |
| FS_4000=1 | ±4000 | 140 |

`FS_4000` 在 OIS 链开启时必须为 0，参考手册 §9.16。不要同时激活有冲突的 FS 位。

**[项目建议]**：`0x48` = 104 Hz、±1000dps；极端高踏频下可切到 ±2000dps（`0x4C`）。曲柄 120RPM = 720dps，故 ±250dps **不足以覆盖** 120RPM 轴向转动。

### 3.5 `CTRL4_C` (`0x13`, R/W)

| bit | 字段 | 说明 |
|---|---|---|
| 6 | `SLEEP_G` | 1：使能陀螺仪 sleep |
| 5 | `INT2_on_INT1` | 1：将 INT2 可用事件 OR 到 INT1 |
| 3 | `DRDY_MASK` | 1：在滤波器建立阶段屏蔽 XL/G DRDY |
| 2 | `I2C_disable` | **必须保持 0 才能使用 I²C** |
| 1 | `LPF1_SEL_G` | 1：开启陀螺仪 LPF1，带宽结合 CTRL6_C FTYPE |
| 7,4,0 | Reserved | 保留 0 |

**关键防错要求**：自编驱动禁止把 `I2C_disable` 置 1，否则可能立即丢失 I²C 通信。

### 3.6 `CTRL6_C` (`0x15`) 与 `CTRL7_G` (`0x16`)

- `CTRL6_C.bit4 XL_HM_MODE`：0 高性能允许；1 高性能禁止，12.5/26/52Hz 可进入低功耗。
- `CTRL6_C.bits[2:0] FTYPE`：陀螺仪 LPF1 带宽编码。若要启用，须配合 `CTRL4_C.LPF1_SEL_G=1`，依手册 Table 66 选型。
- `CTRL7_G.bit7 G_HM_MODE`：0 高性能允许；1 高性能禁止。低 ODR 可进入低功耗。
- `CTRL7_G.bit6 HP_EN_G`：陀螺仪数字高通滤波，在相应模式下使用；`bits5:4 HPM_G` 截止选择。
- `CTRL7_G.bit3` 是保留位，手册明确要求为 **0**。
- **[项目建议]**：基础调试阶段，保留上述两个寄存器复位值，先获得正确读数；低功耗优化时进行读-改-写，避免覆盖其他滤波和 OIS 设置。

## 4. 数据就绪及输出寄存器

### 4.1 `STATUS_REG` (`0x1E`, R)

| bit | 位名 | 掩码 | 说明 |
|---|---|---|---|
| 2 | TDA | `0x04` | 温度新数据 |
| 1 | GDA | `0x02` | 角速度新数据 |
| 0 | XLDA | `0x01` | 加速度新数据 |

- 检查单项：`status & 0x01` / `status & 0x02`。
- 检查双项：`(status & 0x03) == 0x03`。
- **[注意]**：必须把“尚无新数据”与“读 I²C 失败”分别处理；轮询要带延时和超时。

### 4.2 三轴输出地址布局

| 地址 | 内容 | 地址 | 内容 |
|---|---|---|---|
| `0x20` | TEMP_L | `0x21` | TEMP_H |
| `0x22` | G_X_L | `0x23` | G_X_H |
| `0x24` | G_Y_L | `0x25` | G_Y_H |
| `0x26` | G_Z_L | `0x27` | G_Z_H |
| `0x28` | XL_X_L | `0x29` | XL_X_H |
| `0x2A` | XL_Y_L | `0x2B` | XL_Y_H |
| `0x2C` | XL_Z_L | `0x2D` | XL_Z_H |

**[手册 pp.86–89]**：各轴是 **16 位二进制补码**，低地址为低字节（little-endian byte order）。一次从 `0x22` 连续读 12 字节，顺序为 `GX_L, GX_H, GY_L, GY_H, GZ_L, GZ_H, AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H`。

可靠的解码（不依赖实现定义的无符号超范围转换）：

```c
static int16_t lsm6dsr_unpack_s16(const uint8_t *p)
{
    uint16_t u = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    int32_t s = (u & 0x8000u) ? (int32_t)u - 65536 : (int32_t)u;
    return (int16_t)s;
}
```

### 4.3 物理量换算

**[手册 Table 3, p.26]**：

- 加速度 `a_g = raw * sensitivity_mg_per_LSB / 1000`。
- 加速度 SI 单位 `a_m_s2 = a_g * 9.80665`。
- 角速度 `omega_dps = raw * sensitivity_mdps_per_LSB / 1000`。
- 角速度弧度单位 `omega_rad_s = omega_dps * π/180`。
- 自行车曲柄轴向角速度换算为踏频（仅传感器测量轴确实与旋转轴对齐时）：`RPM = abs(omega_dps)/6`。**需扣除零偏、校正方向和安装倾角**。
- 陀螺仪灵敏度见 §3.4，XL 灵敏度见 §3.3。物理量缩放必须按**当前实际配置量程**选择，不得将 `0.035` 或 `0.000122` 写死应用于所有量程。

### 4.4 温度

**[手册 Table 5 p.30]**：典型 25°C 时 0 LSB，灵敏度 256 LSB/°C。

`temperature_C = 25.0 + raw_temp / 256.0`。

手册规定温度 offset 存在差异，不能把该结果当成高精度环境温度计。

## 5. 中断配置

### 5.1 `INT1_CTRL` (`0x0D`, R/W)

| bit | 名称 | 掩码 |
|---|---|---|
| 7 | DEN_DRDY_flag | `0x80` |
| 6 | INT1_CNT_BDR | `0x40` |
| 5 | INT1_FIFO_FULL | `0x20` |
| 4 | INT1_FIFO_OVR | `0x10` |
| 3 | INT1_FIFO_TH | `0x08` |
| 2 | INT1_BOOT | `0x04` |
| 1 | INT1_DRDY_G | `0x02` |
| 0 | INT1_DRDY_XL | `0x01` |

- `0x01`：XL DRDY 到 INT1；`0x02`：Gyro DRDY 到 INT1；`0x03`：两种 DRDY OR 到 INT1；`0x08`：FIFO 水位中断到 INT1。
- **[注意]**：`0x03` 表示逻辑 OR，并不保证两类数据同时有效，ISR 唤醒线程后仍应查 `STATUS_REG`。

### 5.2 `INT2_CTRL` (`0x0E`, R/W)

`bit7=reserved`；bit6 CNT_BDR；bit5 FIFO_FULL；bit4 FIFO_OVR；bit3 FIFO_TH；bit2 DRDY_TEMP；bit1 DRDY_G；bit0 DRDY_XL。

- 中断针脚默认由 `CTRL3_C.H_LACTIVE=0` 配为高有效，`PP_OD=0` 为推挽。
- 建议使用 GPIO 中断 **仅发信号量**，I²C 数据读取在 Zephyr 线程/工作队列上下文执行；不要在 GPIO ISR 中使用阻塞式 I²C。
- 运动唤醒事件的路由不是 `INT1_CTRL` 位，而是后述 `MD1_CFG/MD2_CFG`。

### 5.3 运动唤醒与 `MD1_CFG`

- **[手册 §9.49–9.50 p.97]**：`WAKE_UP_THS(0x5B).bits[5:0] WK_THS` 为唤醒阈值；`WAKE_UP_DUR(0x5C).bits[6:5] WAKE_DUR` 为持续时间，bit4 `WAKE_THS_W` 控制阈值的 LSB 权重（0: FS_XL / 2^6；1: FS_XL / 2^8）。`WAKE_DUR` 的 1 LSB 对应一个 ODR 时间。
- **[手册 §9.52 p.99]**：`MD1_CFG(0x5E).bit5 INT1_WU` 将 wake-up 事件路由到 INT1；bit7 activity/inactivity、bit6 single tap、bit4 free-fall、bit3 double tap、bit2 6D、bit1 embedded function、bit0 sensor hub。
- `MD2_CFG(0x5F)` 类似地负责 INT2 路由，**位布局必须依据手册第 100 页，不可假定与 MD1 完全一致**。
- 对唤醒功能还应结合 `TAP_CFG0/1/2` 和滤波路径设置；**不能仅写 `MD1_CFG=0x20` 就宣称可靠唤醒配置完成**。阈值须实测标定。

## 6. FIFO：第二阶段驱动

**[手册 §6.6 pp.53–56]**：物理 FIFO 3 KB；开启压缩后等效最多 9 KB。每条**非压缩** FIFO word 为 7 字节：`TAG(0x78) + X_L/H + Y_L/H + Z_L/H (0x79–0x7E)`。FIFO 保存历史样本，区别于 `0x22–0x2D` 的最新输出。

### 6.1 FIFO 控制与状态

- `FIFO_CTRL1(0x07)`：WTM[7:0]；`FIFO_CTRL2(0x08).bit0`：WTM[8]。单位是 **1 条 FIFO word（7 字节）**，不是按裸字节计数。
- `FIFO_CTRL2.bit7 STOP_ON_WTM`：1 时在水位限定深度；`bit6 FIFO_COMPR_RT_EN`：压缩运行时使能；`bit4 ODRCHG_EN`：将 ODR 变化元数据纳入 FIFO；`bits2:1 UNCOPTR_RATE`：非压缩参考记录间隔。
- `FIFO_CTRL3(0x09)`：`bits7:4 BDR_GY`、`bits3:0 BDR_XL`，分别配置陀螺仪、加速度计 FIFO batching rate。例 `0x44` 为两者 104Hz BDR（需相应传感器输出速率已配置）。
- `FIFO_CTRL4(0x0A).bits[2:0] FIFO_MODE`：`000` Bypass、`001` FIFO、`011` Continuous-to-FIFO、`100` Bypass-to-Continuous、`110` Continuous、`111` Bypass-to-FIFO。**不是所有 3bit 编码都有效**。Continuous 可用 `0x06`（前提是其他位保持需要的值）。
- `FIFO_STATUS1(0x3A)` 与 `FIFO_STATUS2(0x3B).bits[1:0]`：拼成 `DIFF_FIFO[9:0]`，表示 **未读 FIFO 记录数量**。
- `FIFO_STATUS2.bit7 FIFO_WTM_IA`、bit6 `FIFO_OVR_IA`、bit5 `FIFO_FULL_IA`、bit4 `COUNTER_BDR_IA`、bit3 `FIFO_OVR_LATCHED`；部分状态位读取后清除，具体见 p.92。

计算未读记录数量：

```c
uint16_t words = (uint16_t)status1 |
                 (((uint16_t)status2 & 0x03u) << 8);
```

### 6.2 FIFO TAG 编码

`TAG_SENSOR = tag_byte >> 3`（bits 7:3）；低 3 位为 `TAG_CNT[1:0]` 和 `TAG_PARITY`，**不得将完整 TAG 字节直接与 0x01/0x02 比较**。

| TAG_SENSOR | 记录类型（手册 Table 174 p.108） |
|---|---|
| `0x01` | Gyroscope NC（非压缩） |
| `0x02` | Accelerometer NC（非压缩） |
| `0x03` | Temperature |
| `0x04` | Timestamp |
| `0x05` | CFG_Change |
| `0x06–0x0D` | 加速度/陀螺仪压缩差分或参考记录 |
| `0x0E–0x11` | Sensor Hub Slave 0–3 |
| `0x12` | Step Counter |
| `0x19` | Sensor Hub Nack |

**初版 FIFO 驱动禁用压缩**；解析 TAG 仅处理 `0x01/0x02`，其他 TAG 记录要安全跳过并计数，不能当作原始六轴数据进行换算。

### 6.3 FIFO 读取流程

1. 初始化 IMU 且确认 ODR / BDR 配置一致。
2. 置 Bypass 清空旧 FIFO，再设置期望的 FIFO 模式（例如 Continuous）。
3. 读 `FIFO_STATUS1/2` 得出可读 **word 数**，并检查溢出标志。
4. 对每条可读 word，从 `0x78` 连读 7 字节，解析 `TAG_SENSOR` 后按记录类型处理。
5. 必要时开启 INT1 FIFO_WTM（`INT1_CTRL.bit3`）减少轮询；发生溢出应统计丢失情况。
6. 不要假设每一个 XL word 与相邻的 G word 一定有严格同时的采样时刻；需要同步时应基于 ODR 与时间戳重建。

## 7. 驱动开发配置方案

### 7.1 Profile A：初始 I²C 连通性与六轴采样

**[项目建议]**：

| 顺序 | 操作 | 寄存器/值 | 意义 |
|---|---|---|---|
| 1 | 读取 | `WHO_AM_I(0x0F)` -> `0x6B` | 确认芯片身份 |
| 2 | 写/等待 | `CTRL3_C(0x12)=0x01`，轮询 bit0 清零 | 软件复位 |
| 3 | 写 | `CTRL3_C(0x12)=0x44` | BDU+地址自动递增 |
| 4 | 写 | `CTRL1_XL(0x10)=0x48` | XL:104Hz, ±4g |
| 5 | 写 | `CTRL2_G(0x11)=0x48` | G:104Hz, ±1000dps |
| 6 | 读取 | `STATUS_REG(0x1E)` | 查询 XLDA/GDA |
| 7 | 连读 | `0x22` 起共 `12` 字节 | 6轴原始采样 |
| 8 | 换算 | XL `0.000122 g/LSB`; G `0.035 dps/LSB` | 输出物理量 |

- `CTRL4_C.I2C_disable` 必须为 0。上电复位默认满足；不要盲目写不必要寄存器。
- 初始化写入后给滤波/输出建立时间，随后等待 DRDY；不要以固定 sleep 时间替代所有 DRDY 检测。
- `IF_INC` 和 `BDU` 应显式设置；完成配置后建议回读关键寄存器核验。
- 如果 `WHO_AM_I` 不匹配，立即退出而非继续配置。

### 7.2 Profile B：曲柄量程选择

曲柄理想轴向角速度 `dps=RPM*6`。举例 120RPM=720dps，200RPM=1200dps。

- 上限在约 160RPM 内且有余量要求时，可测试 ±1000dps（`CTRL2_G=0x48`）。
- 需覆盖 200RPM、冲刺或其他扰动时，优先测试 ±2000dps（104Hz 时 `CTRL2_G=0x4C`），对应灵敏度 **0.07 dps/LSB**。
- 量程不能只按平均踏频设置；须留突然加速/震动和传感器安装角度的余量。

### 7.3 Profile C：静止省电与运动唤醒（后续）

- 关闭陀螺仪：`CTRL2_G.ODR_G=0000`；让 XL 在 12.5/26Hz 的合适低功耗模式运行，配合 wake-up 事件设置。
- 运动触发时恢复测量 ODR、量程并等待输出稳定。
- 由 `WAKE_UP_THS(0x5B)`、`WAKE_UP_DUR(0x5C)`、`MD1_CFG(0x5E)`、相关 TAP_CFG/滤波寄存器共同配置。
- **不是**仅凭 ODR/INT1 的某个固定写值就能保证稳定的静止/骑行状态机；需实测阈值，防止颠簸误唤醒。

## 8. Zephyr 驱动接口与模块交付约定

### 8.1 建议 API

```c
int lsm6dsr_init(void);
int lsm6dsr_read_reg(uint8_t reg, uint8_t *val);
int lsm6dsr_write_reg(uint8_t reg, uint8_t val);
int lsm6dsr_update_reg(uint8_t reg, uint8_t mask, uint8_t value);
int lsm6dsr_read_regs(uint8_t start, uint8_t *buf, size_t len);
int lsm6dsr_set_accel(uint8_t odr_code, uint8_t fs_code);
int lsm6dsr_set_gyro(uint8_t odr_code, uint8_t fs_code);
int lsm6dsr_read_raw(struct lsm6dsr_raw *out);
int lsm6dsr_read_temperature(float *temperature_c);
int lsm6dsr_set_drdy_interrupt(bool xl, bool gyro);
int lsm6dsr_fifo_setup(/* explicit cfg */);
int lsm6dsr_fifo_read(/* output records, cap */);
```

建议在头文件为原始值定义 `int16_t ax, ay, az, gx, gy, gz`，转换数据的单位在字段名中明确为 `_g`, `_dps`, `_ms2`, `_rad_s`。由驱动层返回 `0` 成功、负 `errno` 错误（`-ENODEV`, `-EIO`, `-ETIMEDOUT`, `-EAGAIN` 等）。

### 8.2 Zephyr 最小访问示例

```c
#include <zephyr/drivers/i2c.h>
#include <zephyr/devicetree.h>

#define IMU_NODE DT_NODELABEL(lsm6dsr)
static const struct i2c_dt_spec imu = I2C_DT_SPEC_GET(IMU_NODE);

/* prerequisite: valid devicetree node with reg=<0x6a> */
uint8_t id;
int ret = i2c_reg_read_byte_dt(&imu, 0x0f, &id);
if (ret == 0 && id == 0x6b) {
    /* device identified */
}
```

**设备树注意**：必须按当前 Zephyr/NCS 版本核对 `compatible` 和 binding。不可仅因型号相似，就把 LSM6DSR 随意声明为其他型号并让该型号的内置驱动自动初始化；使用自建 binding 时，须确保 Zephyr DTS 构建正确拾取它，且不和系统 Sensor 驱动重复操作硬件。

### 8.3 Agent 实现的测试/验收清单

- [ ] I²C 总线 ready；`WHO_AM_I = 0x6B`。
- [ ] 软件复位后的 SW_RESET bit 自动清零；失败时超时退出。
- [ ] 写 `CTRL3_C = 0x44` 后读回正确；确认 I²C_disable 未打开。
- [ ] 写入 XL/G ODR/FS 后读回，并验证数据变化周期与大致量程。
- [ ] 使用 DATA_READY 轮询+超时；连续读取 `0x22–0x2D` 正确解包有符号数据。
- [ ] 静止时 `sqrt(ax²+ay²+az²) ≈ 1g`；陀螺仪三轴在零附近（有噪声/零偏）。
- [ ] 绕主要旋转轴缓慢旋转，角速度方向、幅值正确；变更量程后使用对应灵敏度。
- [ ] INT1 GPIO 上电状态不破坏 I²C/I3C 选择；GPIO ISR 内无阻塞 I²C。
- [ ] FIFO 连续模式、TAG 解码、DIFF_FIFO 计数和溢出处理通过测试；未支持压缩模式时明确报错。
- [ ] 保存驱动实测日志：芯片 ID、初始化寄存器读回、采样时间、raw 数据与物理量。

## 9. 信息优先级与不允许的假设

1. **手册中的 bit 编码、默认值、寄存器地址优先于示例代码。** 若与本指南冲突，以所附原版手册为准。
2. 本指南**不声称覆盖全部** 166 页的所有寄存器；其范围是主 I²C 驱动及紧密相关功能。其它功能（FSM、OIS、Sensor Hub、计步、复杂滤波/压缩）应另立专题并逐位依据手册实现。
3. 不得把 `CTRL1_XL=0xB0` 当作低功耗 1.6Hz：本手册 ODR 表不支持。
4. 不得把 `CTRL2_G=0x40`（±250dps）误用于覆盖 120RPM 曲柄旋转。
5. 不得把 `INT1_CTRL=0x03` 解释成“两个传感器同时同步采样”。
6. 不得忽略 `CTRL4_C.I2C_disable`、`CTRL7_G` 保留位限制、复位位自清零、`BDU` 读数一致性要求。
7. 不得直接把 FIFO 压缩记录当作三轴普通原始数据。
8. 不得臆造 Zephyr `compatible`；应先验证当前 SDK/绑定。

## 10. 原始数据手册索引

- §4.1 p.26：加速度计/陀螺仪灵敏度；§4.3 p.30：温度灵敏度。
- §5.1.1 pp.35–37：I²C 传输、地址、ACK 和重复 START。
- §5.3 p.44：INT1 上电连接与 I²C/I3C 共存。
- §6.6 pp.53–56：FIFO 模式与读取协议。
- §7.1 p.57：Mode 1 电路连接。
- §8 pp.63–65：主寄存器地址映射。
- §9.12–9.24 pp.72–83：控制、中断、滤波、功耗寄存器。
- §9.29–9.36 pp.86–89：状态、温度、三轴输出。
- §9.41–9.42 p.92：FIFO 状态；§9.49–9.53 pp.97–100：唤醒阈值与事件路由。
- §9.65–9.68 pp.108–109：FIFO TAG、FIFO word 数据。

---

**Agent 首个里程碑**：实现并验证 `WHO_AM_I`、软复位、`CTRL3_C/CTRL1_XL/CTRL2_G`、`STATUS_REG`、12 字节六轴读取与动态灵敏度换算。之后依次实现 DRDY 中断、FIFO、低功耗唤醒。每个阶段必须具备超时、错误码、关键寄存器回读和可复现硬件测试日志。
