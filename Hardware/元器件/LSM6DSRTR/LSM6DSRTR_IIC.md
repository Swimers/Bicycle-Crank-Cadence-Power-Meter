# 分析LSM6DSR方案差异

> 导出自 ChatGPT，时间：2026/11/17 15:44:54

# 消息 1

## 用户

如上文件是LSM6DSRTR技术手册，请你根据手册内容，总结在IIC通信模式下相关寄存器的设置、读取以及采样数据的读取方法，为后续驱动开发做准备

## 助手

# **LSM6DSRTR I²C 通信与寄存器驱动开发指南**

依据：STMicroelectronics《LSM6DSR Datasheet》DocID030186 Rev 1，2019 年 3 月，166 页

我已读取你上传的 LSM6DSR 技术手册，重点核对了第 5 章 Digital interfaces、第 6 章 Functionality、第 8 章 Register mapping，以及第 9 章 Register description。

针对你正在开发的 **nRF52832 + Zephyr 自行车踏频/功率计**，建议首先完成一个最小驱动：能够识别芯片、配置加速度计和陀螺仪、判断数据就绪、读取三轴加速度和三轴角速度，并转换为实际物理量。

这里需要先明确两个概念：

- **I²C 是通信方式**：决定 nRF52832 如何向 LSM6DSR 发送寄存器地址、读取和写入数据。
- **寄存器是功能配置接口**：决定传感器是否工作、采样频率、测量量程、滤波方式、数据输出和中断行为。

LSM6DSRTR 和 LSM6DSR 使用相同的寄存器定义；`TR` 表示卷带包装型号，并非另一套通信协议。

## **一、驱动开发的整体流程**

1. **初始化 I²C 外设**

nRF52832 配置 SDA、SCL，引入传感器的 7 位从机地址。
2. **读取 WHO****_****AM****_****I**

读取寄存器 `0x0F`，验证设备响应为 `0x6B`。
3. **复位并配置控制寄存器**

配置 BDU、地址自动递增、加速度计和陀螺仪的 ODR、量程及功耗模式。
4. **等待采样数据就绪**

查询 `STATUS_REG` 或通过 INT1/INT2 接收 DRDY 中断。
5. **读取六轴采样结果**

从 `0x22` 读取 6 字节陀螺仪数据，从 `0x28` 读取 6 字节加速度计数据。
6. **原始数据转换与算法处理**

将 16 位有符号整数换算为 `g` 和 `°/s`，供踏频、角速度和曲柄相位算法使用。

这是本阶段驱动的主线。FIFO、运动唤醒、中断阈值和低功耗状态切换可以在基础采样验证成功之后增加。

## **二、I²C 通信协议**

手册第 35～37 页，5.1.1 节，Table 9～15

### **2.1 从机地址**

LSM6DSR 的 I²C 地址取决于 `SDO/SA0` 引脚电平。

| SDO/SA0 电平 | 7 位地址 | 写地址字节 | 读地址字节 |
| --- | --- | --- | --- |
| GND | `0x6A` | `0xD4` | `0xD5` |
| VDDIO | `0x6B` | `0xD6` | `0xD7` |

根据你之前的原理图，SA0 接 GND，因此使用：

```
#define LSM6DSR_I2C_ADDR  0x6A
```

**特别注意：Zephyr I²C API 使用 7 位地址 **`0x6A`**，不要把 **`0xD4`** 或 **`0xD5`** 当作设备树的 **`reg`** 地址。**

此外，CS 引脚必须保持高电平以启用 I²C 通信，SDA/SCL 需要外部上拉电阻。手册明确支持标准模式和 400kHz Fast-mode I²C。

### **2.2 写入寄存器**

LSM6DSR 使用 8 位寄存器地址（SUB），一次写入可以操作单个寄存器。

例如：

```
向 CTRL1_XL（0x10）写入 0x40
```

通信顺序：

> [传输过程中，从机对地址和数据字节返回 ACK；START 和 STOP 不需要 ACK。]
>
> START 起始 0xD4 设备+W 0x10 寄存器 0x40 数据 STOP 结束 传输过程中，从机对地址和数据字节返回 ACK；START 和 STOP 不需要 ACK。

其含义是：nRF52832 首先向地址 `0x6A` 的设备发起写操作，然后告诉 LSM6DSR 要访问 `0x10` 寄存器，最后写入 `0x40`。

### **2.3 读取寄存器**

读取时要先写入目标寄存器地址，再使用重复起始条件（Repeated START）切换为读操作。

例如，读取 `WHO_AM_I（0x0F）`：

> [单字节读取的最后一个数据字节由主机发送 NACK，然后发出 STOP。]
>
> START 起始 0xD4 写地址 0x0F 寄存器 Sr 重复起始 0xD5 读地址 0x6B 返回数据 STOP 结束 单字节读取的最后一个数据字节由主机发送 NACK，然后发出 STOP。

对应 Zephyr 代码：

```
uint8_t id = 0;

int ret = i2c_reg_read_byte_dt(
    &lsm6dsr,
    0x0F,
    &id
);
```

成功时 `id` 应为 `0x6B`，通信失败则会返回负错误码。

### **2.4 连续读取多个寄存器**

LSM6DSR 支持多字节访问，只要 `CTRL3_C` 的 `IF_INC` 位为 1，就能在传输中自动增加寄存器地址。

例如读取：

```
0x28 -> X轴低字节
0x29 -> X轴高字节
0x2A -> Y轴低字节
0x2B -> Y轴高字节
0x2C -> Z轴低字节
0x2D -> Z轴高字节
```

只需要从 `0x28` 连续读取 6 字节：

```
uint8_t data[6];

int ret = i2c_burst_read_dt(
    &lsm6dsr,
    0x28,
    data,
    sizeof(data)
);
```

相比执行六次单寄存器读取，连续读取可以减少 I²C 总线操作次数，也更方便保持同组采样数据的一致性。

## **三、必须掌握的寄存器地址表**

手册第 63～65 页 Register mapping，第 66～109 页 Register description

下面按实际驱动用途整理，而不是简单按照手册地址顺序排列。

| 地址 | 寄存器 | R/W | 功能 |
| --- | --- | --- | --- |
| `0x0F` | WHO_AM_I | R | 芯片 ID |
| `0x10` | CTRL1_XL | R/W | 加速度计 ODR、量程 |
| `0x11` | CTRL2_G | R/W | 陀螺仪 ODR、量程 |
| `0x12` | CTRL3_C | R/W | 复位、BDU、自动递增 |
| `0x13` | CTRL4_C | R/W | I²C 禁用控制、滤波等 |
| `0x15` | CTRL6_C | R/W | 加速度计功耗模式 |
| `0x16` | CTRL7_G | R/W | 陀螺仪功耗模式 |
| `0x0D` | INT1_CTRL | R/W | INT1 中断源配置 |
| `0x0E` | INT2_CTRL | R/W | INT2 中断源配置 |
| `0x1E` | STATUS_REG | R | 数据就绪状态 |
| `0x20～0x21` | OUT_TEMP_L/H | R | 温度原始数据 |
| `0x22～0x27` | OUTX/Y/Z_G | R | 三轴角速度数据 |
| `0x28～0x2D` | OUTX/Y/Z_A | R | 三轴加速度数据 |
| `0x07～0x0A` | FIFO_CTRL1～4 | R/W | FIFO 采样和工作模式 |
| `0x3A～0x3B` | FIFO_STATUS1/2 | R | FIFO 未读数据及状态 |
| `0x78～0x7E` | FIFO_DATA_OUT | R | FIFO 标签和采样数据 |
| `0x5B～0x5C` | WAKE_UP_THS/DUR | R/W | 运动唤醒阈值、持续时间 |
| `0x5E～0x5F` | MD1_CFG/MD2_CFG | R/W | 运动事件中断路由 |

对于最初的 I²C 驱动，实际上只需使用 `0x0F`、`0x10`、`0x11`、`0x12`、`0x1E` 和 `0x22～0x2D` 就可以完成六轴数据采集。

## **四、关键控制寄存器与位定义**

### **4.1 CTRL3_C（0x12）：设备基础控制**

手册第 76 页，Table 55、56

这是初始化中最重要的寄存器之一。

> [位定义按 bit7 到 bit0 排列；H_L 为 H_LACTIVE，RESET 为 SW_RESET 的简写。]
>
> 7 BOOT 6 BDU 5 H_L 4 PP_OD 3 SIM 2 IF_INC 1 0 0 RESET 位定义按 bit7 到 bit0 排列；H_L 为 H_LACTIVE，RESET 为 SW_RESET 的简写。

| 位 | 名称 | 功能 |
| --- | --- | --- |
| 7 | BOOT | 重新加载内部存储器内容 |
| 6 | BDU | 阻止 16 位数据读取期间被更新 |
| 5 | H_LACTIVE | 中断有效电平，0 为高有效 |
| 4 | PP_OD | 中断输出推挽/开漏选择 |
| 3 | SIM | SPI 三线/四线选择 |
| 2 | IF_INC | 多字节寄存器地址自动递增 |
| 1 | 保留 | 保持 0 |
| 0 | SW_RESET | 软件复位，自动清零 |

推荐普通采样配置：

```
CTRL3_C = 0x44;
```

即：

```
0x44 = 0100 0100b

bit6 BDU    = 1
bit2 IF_INC = 1
其余位      = 0
```

这里 `IF_INC` 的上电默认值已经是 1，但驱动中显式配置有利于保证行为一致。

需要软件复位时设置 `SW_RESET=1`，等待其自动清零后再进行其他配置，不要在同一次写入中依赖复位前的控制位保留。

### **4.2 CTRL1_XL（0x10）：加速度计配置**

手册第 74 页，Table 49～51

寄存器格式：

```
bit7~4 : ODR_XL[3:0]
bit3~2 : FS_XL[1:0]
bit1   : LPF2_XL_EN
bit0   : 0
```

其中 ODR（Output Data Rate）表示输出数据率，FS（Full Scale）表示测量量程。

**加速度计 ODR 设置表：**

| ODR_XL | 输出频率 | 典型配置值（±2g） |
| --- | --- | --- |
| `0000` | Power-down | `0x00` |
| `0001` | 12.5 Hz | `0x10` |
| `0010` | 26 Hz | `0x20` |
| `0011` | 52 Hz | `0x30` |
| `0100` | 104 Hz | `0x40` |
| `0101` | 208 Hz | `0x50` |
| `0110` | 416 Hz | `0x60` |
| `0111` | 833 Hz | `0x70` |
| `1000` | 1.66 kHz | `0x80` |
| `1001` | 3.33 kHz | `0x90` |
| `1010` | 6.66 kHz | `0xA0` |
| `1011` | 特殊：低功耗下 1.6 Hz | `0xB0` |

其中 `0xB0` 只有在加速度计低功耗条件下才对应 1.6 Hz；高性能模式下该编码对应 12.5 Hz。其他 ODR 的实际功耗模式还取决于 `CTRL6_C.XL_HM_MODE`，不能仅凭 ODR 值判断功耗。

**加速度计量程：**

| FS_XL | 量程 | 典型灵敏度 |
| --- | --- | --- |
| `00` | ±2g | 0.061 mg/LSB |
| `01` | ±16g | 0.488 mg/LSB |
| `10` | ±4g | 0.122 mg/LSB |
| `11` | ±8g | 0.244 mg/LSB |

例如：

```
// 104 Hz, ±4g, LPF2 关闭
CTRL1_XL = 0x48;
```

解释：

```
0x48 = 0100 1000b

ODR_XL = 0100 -> 104 Hz
FS_XL  = 10   -> ±4g
LPF2   = 0    -> 未启用第二级低通滤波
```

对于自行车曲柄安装场景，我建议先使用 ±4g 或 ±8g 进行测试，观察实际峰值是否发生饱和，再确定最终量程。

### **4.3 CTRL2_G（0x11）：陀螺仪配置**

手册第 75 页，Table 52～54

寄存器格式：

```
bit7~4 : ODR_G[3:0]
bit3~2 : FS_G[1:0]
bit1   : FS_125
bit0   : FS_4000
```

陀螺仪 ODR：

| ODR_G | 频率 |
| --- | --- |
| `0000` | Power-down |
| `0001` | 12.5 Hz |
| `0010` | 26 Hz |
| `0011` | 52 Hz |
| `0100` | 104 Hz |
| `0101` | 208 Hz |
| `0110` | 416 Hz |
| `0111` | 833 Hz |
| `1000` | 1.66 kHz |
| `1001` | 3.33 kHz |
| `1010` | 6.66 kHz |

陀螺仪量程：

| FS_G | FS_125 | FS_4000 | 量程 |
| --- | --- | --- | --- |
| `00` | 0 | 0 | ±250 dps |
| `01` | 0 | 0 | ±500 dps |
| `10` | 0 | 0 | ±1000 dps |
| `11` | 0 | 0 | ±2000 dps |
| — | 1 | 0 | ±125 dps |
| — | — | 1 | ±4000 dps |

例如，配置：

```
// 104 Hz, ±250 dps
CTRL2_G = 0x40;
```

对于踏频计算，±250 dps 通常足以覆盖普通骑行。因为：

$$
\omega = \frac{\mathrm{RPM}\times360}{60}
=6\times\mathrm{RPM}
$$

例如，120 RPM 对应 `720 °/s`，这说明 **±250 dps 并不足以覆盖 120 RPM 的曲柄轴向角速度**。

因此，你的项目应优先考虑：

```
// 104 Hz, ±1000 dps
CTRL2_G = 0x48;
```

如果目标最高踏频为 200 RPM，则对应 1200 dps，需要进一步选择 ±2000 dps。这里讨论的是传感器轴与曲柄旋转轴对齐时，该轴测量到的理想旋转角速度；实际安装倾角和其他运动分量也需要纳入余量。

### **4.4 CTRL6_C / CTRL7_G：工作功耗模式**

手册第 79～80 页分别定义：

| 寄存器 | 位 | 0 | 1 |
| --- | --- | --- | --- |
| CTRL6_C `0x15` | bit4 XL_HM_MODE | 加速度计高性能使能 | 高性能禁用 |
| CTRL7_G `0x16` | bit7 G_HM_MODE | 陀螺仪高性能使能 | 高性能禁用 |

例如，在不用其他特殊功能的前提下：

```
// 加速度计允许低功耗/普通模式
CTRL6_C = 0x10;

// 陀螺仪允许低功耗/普通模式
CTRL7_G = 0x80;
```

但这些位并不表示“写 1 就一定进入最低功耗”；实际工作模式受到 ODR 等配置的共同约束。

对于初次驱动测试，建议先保留高性能默认模式，确保读数正确后，再单独测试低功耗模式。

## **五、采样数据读取：重点内容**

这一部分直接决定后续六轴驱动的数据结构和读取函数如何设计。

### **5.1 STATUS_REG（0x1E）：数据就绪检测**

手册第 86 页，Table 84、85

寄存器格式：

```
bit7~3 : 保留
bit2   : TDA
bit1   : GDA
bit0   : XLDA
```

| 标志位 | 掩码 | 说明 |
| --- | --- | --- |
| TDA | `0x04` | 新温度数据已就绪 |
| GDA | `0x02` | 新陀螺仪数据已就绪 |
| XLDA | `0x01` | 新加速度计数据已就绪 |

例如：

```
uint8_t status = 0;

i2c_reg_read_byte_dt(
    &lsm6dsr,
    0x1E,
    &status
);

if (status & 0x01) {
    // 加速度计有新数据
}

if (status & 0x02) {
    // 陀螺仪有新数据
}
```

如果需要等到两种传感器都有新数据：

```
if ((status & 0x03) == 0x03) {
    // 读取六轴数据
}
```

注意，`XLDA` 和 `GDA` 只是各自的数据就绪状态，不意味着加速度和角速度的采样时刻严格同步。若后续算法需要精确的样本时间关联，应考虑 FIFO 时间戳和数据同步策略。

### **5.2 陀螺仪原始数据寄存器**

手册第 87～88 页，9.31～9.33 节

| 地址 | 寄存器 | 内容 |
| --- | --- | --- |
| `0x22` | OUTX_L_G | X轴角速度低8位 |
| `0x23` | OUTX_H_G | X轴角速度高8位 |
| `0x24` | OUTY_L_G | Y轴角速度低8位 |
| `0x25` | OUTY_H_G | Y轴角速度高8位 |
| `0x26` | OUTZ_L_G | Z轴角速度低8位 |
| `0x27` | OUTZ_H_G | Z轴角速度高8位 |

寄存器存储的是 **16 位二进制补码有符号整数**，低地址存放低字节。

例如：

```
0x22 = 0x34
0x23 = 0x12

16位原始值 = 0x1234 = 4660
```

组合方法：

```
int16_t gyro_x =
    (int16_t)((uint16_t)data[0] |
               ((uint16_t)data[1] << 8));
```

当最高位 bit15 为 1 时，读数代表负角速度。

### **5.3 加速度计原始数据寄存器**

手册第 88～89 页，9.34～9.36 节

| 地址 | 寄存器 | 内容 |
| --- | --- | --- |
| `0x28` | OUTX_L_A | X轴加速度低8位 |
| `0x29` | OUTX_H_A | X轴加速度高8位 |
| `0x2A` | OUTY_L_A | Y轴加速度低8位 |
| `0x2B` | OUTY_H_A | Y轴加速度高8位 |
| `0x2C` | OUTZ_L_A | Z轴加速度低8位 |
| `0x2D` | OUTZ_H_A | Z轴加速度高8位 |

读取方式与陀螺仪完全相同，区别仅在于起始地址和实际物理含义。

### **5.4 连续读取六轴数据**

因为陀螺仪与加速度计的数据寄存器在地址空间中连续排列，因此可以一次读取 12 字节。

> [Chart or card]
>
> 从 0x22 连续读取 12 字节 0x22 GX_L 0x23 GX_H 0x24 GY_L 0x25 GY_H 0x26 GZ_L 0x27 GZ_H 0x28 AX_L 0x29 AX_H 0x2A AY_L 0x2B AY_H 0x2C AZ_L 0x2D AZ_H 前 6 字节：陀螺仪 后 6 字节：加速度计

对应实现：

```
uint8_t data[12];

int ret = i2c_burst_read_dt(
    &lsm6dsr,
    0x22,
    data,
    sizeof(data)
);

if (ret == 0) {
    int16_t gx = (int16_t)(
        (uint16_t)data[0] |
        ((uint16_t)data[1] << 8));

    int16_t gy = (int16_t)(
        (uint16_t)data[2] |
        ((uint16_t)data[3] << 8));

    int16_t gz = (int16_t)(
        (uint16_t)data[4] |
        ((uint16_t)data[5] << 8));

    int16_t ax = (int16_t)(
        (uint16_t)data[6] |
        ((uint16_t)data[7] << 8));

    int16_t ay = (int16_t)(
        (uint16_t)data[8] |
        ((uint16_t)data[9] << 8));

    int16_t az = (int16_t)(
        (uint16_t)data[10] |
        ((uint16_t)data[11] << 8));
}
```

建议初始化时将 `BDU=1` 和 `IF_INC=1`，避免读取同一 16 位测量值期间发生高低字节更新不一致的问题。

### **5.5 数据转换为物理量**

手册第 26 页，Table 3：Mechanical characteristics

传感器直接返回的是整数，需要根据已配置的量程进行比例换算。

**加速度：**

$$
a(g)=Raw\times\frac{Sensitivity_{mg/LSB}}{1000}
$$

例如，±4g 量程时，灵敏度为 `0.122 mg/LSB`：

```
float ax_g = ax * 0.000122f;
float ay_g = ay * 0.000122f;
float az_g = az * 0.000122f;
```

如果需要国际单位 `m/s²`：

```
float ax_ms2 = ax_g * 9.80665f;
```

**角速度：**

$$
\omega(°/s)=Raw\times\frac{Sensitivity_{mdps/LSB}}{1000}
$$

完整灵敏度速查表如下：

| 加速度量程 | g/LSB | 陀螺仪量程 | °/s/LSB |
| --- | --- | --- | --- |
| ±2g | 0.000061 | ±125 dps | 0.004375 |
| ±4g | 0.000122 | ±250 dps | 0.008750 |
| ±8g | 0.000244 | ±500 dps | 0.017500 |
| ±16g | 0.000488 | ±1000 dps | 0.035000 |
| — | — | ±2000 dps | 0.070000 |
| — | — | ±4000 dps | 0.140000 |

例如，采用 ±1000 dps 量程：

```
float gx_dps = gx * 0.035f;
float gy_dps = gy * 0.035f;
float gz_dps = gz * 0.035f;
```

这些系数是手册的典型灵敏度值。实际应用中还需要考虑零偏、温漂、轴向安装误差和标定。

### **5.6 温度数据读取**

LSM6DSR 还内置温度传感器。

| 寄存器 | 地址 |
| --- | --- |
| OUT_TEMP_L | `0x20` |
| OUT_TEMP_H | `0x21` |

根据第 30 页 Table 5，温度灵敏度为 `256 LSB/°C`，25°C 时典型输出为 0 LSB。

因此：

$$
T(°C)=25+\frac{RawTemp}{256}
$$

例如：

```
int16_t temp_raw = ...;

float temp_c = 25.0f +
               temp_raw / 256.0f;
```

手册给出的温度偏移范围较大，因此该读数更适合作为芯片内部温度变化或陀螺仪温漂补偿的参考，而不是直接当作高精度环境温度。

## **六、INT1 / INT2 中断配置**

手册第 72～73 页，Table 44～47

你的原理图已经将 `LSM_INT1` 和 `LSM_INT2` 引出，因此可以采用硬件中断减少 MCU 的轮询次数。

### **6.1 INT1_CTRL（0x0D）**

| 位 | 掩码 | 功能 |
| --- | --- | --- |
| bit7 | `0x80` | DEN 数据就绪标志 |
| bit6 | `0x40` | Batch Counter 阈值 |
| bit5 | `0x20` | FIFO Full |
| bit4 | `0x10` | FIFO Overrun |
| bit3 | `0x08` | FIFO Watermark |
| bit2 | `0x04` | Boot 状态 |
| bit1 | `0x02` | 陀螺仪 DRDY |
| bit0 | `0x01` | 加速度计 DRDY |

例如：

```
// 仅加速度计 DRDY 输出到 INT1
INT1_CTRL = 0x01;

// 仅陀螺仪 DRDY 输出到 INT1
INT1_CTRL = 0x02;

// 两种 DRDY 都输出到 INT1
INT1_CTRL = 0x03;
```

注意，多个中断源路由至同一引脚时，其输出是逻辑 OR。如果同时打开 `0x03`，一次中断并不保证两种采样都同时就绪，需要配合读取 `STATUS_REG` 判断。

### **6.2 INT2_CTRL（0x0E）**

INT2 可以配置温度、加速度、陀螺仪及 FIFO 相关中断源。

例如：

```
// 加速度 DRDY 输出到 INT2
INT2_CTRL = 0x01;
```

**当前阶段的建议：** 初期使用 `STATUS_REG` 轮询进行调试；功能稳定后，再将采样改为 INT1 数据就绪中断驱动。

还有一个与你的原理图直接相关的注意事项：手册第 44 页说明，LSM6DSR 上电时会根据 INT1 引脚状态影响 I²C/I3C 接口共存选择。INT1 上电悬空（内部下拉）时两种接口均可用；如果上电时 INT1 被连接到 VDDIO，则进入仅 I3C 可用的情况。因此，**不要在 nRF52832 侧为 INT1 添加强制的上拉高电平或将其默认配置为高电平输出**，以免影响 I²C 通信。

## **七、可直接用于开发的 Zephyr 驱动示例**

下面给出一个最小的六轴驱动示例。

与技术手册直接规定的寄存器定义不同，**以下初始化参数是结合你的项目选取的工程示例**，并非 ST 唯一要求的配置。

建议使用：

| 配置项 | 初始测试值 |
| --- | --- |
| I²C 地址 | `0x6A` |
| I²C 频率 | 400kHz |
| 加速度 ODR | 104Hz |
| 加速度量程 | ±4g |
| 陀螺仪 ODR | 104Hz |
| 陀螺仪量程 | ±1000dps |
| BDU | 开启 |
| IF_INC | 开启 |
| FIFO | 暂时关闭 |
| 中断 | 暂时关闭，轮询采样 |

### **7.1 驱动头文件中的寄存器宏**

```

#ifndef LSM6DSR_H
#define LSM6DSR_H

#include <stdint.h>

#define LSM6DSR_I2C_ADDR       0x6A
#define LSM6DSR_CHIP_ID        0x6B

/* Basic registers */
#define LSM6DSR_WHO_AM_I       0x0F
#define LSM6DSR_CTRL1_XL       0x10
#define LSM6DSR_CTRL2_G        0x11
#define LSM6DSR_CTRL3_C        0x12
#define LSM6DSR_CTRL4_C        0x13
#define LSM6DSR_CTRL6_C        0x15
#define LSM6DSR_CTRL7_G        0x16

/* Interrupt and status */
#define LSM6DSR_INT1_CTRL      0x0D
#define LSM6DSR_INT2_CTRL      0x0E
#define LSM6DSR_STATUS_REG     0x1E

/* Sensor output */
#define LSM6DSR_OUT_TEMP_L     0x20
#define LSM6DSR_OUTX_L_G      0x22
#define LSM6DSR_OUTX_L_A      0x28

/* CTRL3_C bits */
#define LSM6DSR_SW_RESET       0x01
#define LSM6DSR_IF_INC         0x04
#define LSM6DSR_BDU            0x40

/* STATUS_REG bits */
#define LSM6DSR_XLDA           0x01
#define LSM6DSR_GDA            0x02
#define LSM6DSR_TDA            0x04

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
} lsm6dsr_raw_data_t;

typedef struct {
    float ax_g, ay_g, az_g;
    float gx_dps, gy_dps, gz_dps;
} lsm6dsr_data_t;

#endif

```

### **7.2 设备树配置示例**

假设你的 LSM6DSR 连接在 Zephyr 设备树中的 `i2c0` 控制器上：

```

&i2c0 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;

    lsm6dsr: imu@6a {
        compatible = "st,lsm6dsl";
        reg = <0x6a>;
        status = "okay";
    };
};

```

这里的 `compatible = "st,lsm6dsl"` **只是为说明设备树结构而给出的近似示例，不应直接当作 LSM6DSR 的正式绑定使用**。实际 Zephyr 项目应先检查当前 SDK 是否提供 `st,lsm6dsr` 绑定及对应驱动。

如果准备自己实现寄存器级驱动，可以定义自有的 `compatible`（例如 `custom,lsm6dsr`），提供相应 YAML binding，并通过 `I2C_DT_SPEC_GET()` 获取总线地址；此时还应避免系统内置 Sensor 驱动与自编驱动同时控制芯片。

后面的代码假设设备树节点 `lsm6dsr` 已正确建立且不会与内置驱动冲突。

### **7.3 初始化与采样代码**

```

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include "lsm6dsr.h"

#define IMU_NODE DT_NODELABEL(lsm6dsr)

static const struct i2c_dt_spec imu =
    I2C_DT_SPEC_GET(IMU_NODE);

static int lsm6dsr_write_reg(uint8_t reg, uint8_t value)
{
    return i2c_reg_write_byte_dt(&imu, reg, value);
}

static int lsm6dsr_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_reg_read_byte_dt(&imu, reg, value);
}

static int lsm6dsr_read_regs(
    uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_burst_read_dt(&imu, reg, buf, len);
}

/* Initialization */
static int lsm6dsr_init(void)
{
    uint8_t id = 0;
    uint8_t ctrl = 0;
    int ret;

    if (!i2c_is_ready_dt(&imu)) {
        return -ENODEV;
    }

    ret = lsm6dsr_read_reg(LSM6DSR_WHO_AM_I, &id);
    if (ret < 0) {
        return ret;
    }

    if (id != LSM6DSR_CHIP_ID) {
        printk("Unexpected chip ID: 0x%02X\n", id);
        return -ENODEV;
    }

    printk("LSM6DSR detected: 0x%02X\n", id);

    /* Software reset */
    ret = lsm6dsr_write_reg(
        LSM6DSR_CTRL3_C, LSM6DSR_SW_RESET);
    if (ret < 0) {
        return ret;
    }

    /* Wait for reset bit to clear */
    bool reset_done = false;

    for (int i = 0; i < 100; i++) {
        ret = lsm6dsr_read_reg(LSM6DSR_CTRL3_C, &ctrl);
        if (ret < 0) {
            return ret;
        }

        if ((ctrl & LSM6DSR_SW_RESET) == 0) {
            reset_done = true;
            break;
        }

        k_msleep(1);
    }

    if (!reset_done) {
        return -ETIMEDOUT;
    }

    /* BDU = 1, IF_INC = 1 */
    ret = lsm6dsr_write_reg(
        LSM6DSR_CTRL3_C,
        LSM6DSR_BDU | LSM6DSR_IF_INC);
    if (ret < 0) {
        return ret;
    }

    /* Accelerometer: 104 Hz, +/-4g */
    ret = lsm6dsr_write_reg(LSM6DSR_CTRL1_XL, 0x48);
    if (ret < 0) {
        return ret;
    }

    /* Gyroscope: 104 Hz, +/-1000 dps */
    ret = lsm6dsr_write_reg(LSM6DSR_CTRL2_G, 0x48);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static int16_t unpack_s16(const uint8_t *buf)
{
    uint16_t u = (uint16_t)buf[0] |
                 ((uint16_t)buf[1] << 8);

    /* Explicit signed conversion for 16-bit two's complement */
    int32_t s = (u & 0x8000u) ?
                ((int32_t)u - 65536) : (int32_t)u;

    return (int16_t)s;
}

/* Return 0 for a new sample, -EAGAIN if not ready */
static int lsm6dsr_read_raw(lsm6dsr_raw_data_t *out)
{
    uint8_t status;
    uint8_t buf[12];
    int ret;

    if (out == NULL) {
        return -EINVAL;
    }

    ret = lsm6dsr_read_reg(LSM6DSR_STATUS_REG, &status);
    if (ret < 0) {
        return ret;
    }

    if ((status & (LSM6DSR_XLDA | LSM6DSR_GDA)) !=
                  (LSM6DSR_XLDA | LSM6DSR_GDA)) {
        return -EAGAIN;
    }

    ret = lsm6dsr_read_regs(
        LSM6DSR_OUTX_L_G, buf, sizeof(buf));

    if (ret < 0) {
        return ret;
    }

    out->gx = unpack_s16(&buf[0]);
    out->gy = unpack_s16(&buf[2]);
    out->gz = unpack_s16(&buf[4]);

    out->ax = unpack_s16(&buf[6]);
    out->ay = unpack_s16(&buf[8]);
    out->az = unpack_s16(&buf[10]);

    return 0;
}

static void lsm6dsr_convert(
    const lsm6dsr_raw_data_t *raw,
    lsm6dsr_data_t *data)
{
    /* +/-4g */
    const float accel_scale = 0.000122f;

    /* +/-1000 dps */
    const float gyro_scale = 0.035f;

    data->ax_g = raw->ax * accel_scale;
    data->ay_g = raw->ay * accel_scale;
    data->az_g = raw->az * accel_scale;

    data->gx_dps = raw->gx * gyro_scale;
    data->gy_dps = raw->gy * gyro_scale;
    data->gz_dps = raw->gz * gyro_scale;
}

int main(void)
{
    int ret = lsm6dsr_init();

    if (ret < 0) {
        printk("LSM6DSR init failed: %d\n", ret);
        return 0;
    }

    /* Allow sensor output to settle */
    k_msleep(100);

    while (1) {
        lsm6dsr_raw_data_t raw;
        lsm6dsr_data_t data;

        ret = lsm6dsr_read_raw(&raw);

        if (ret == 0) {
            lsm6dsr_convert(&raw, &data);

            /* Integer values simplify printk output */
            printk("AX=%d AY=%d AZ=%d "
                   "GX=%d GY=%d GZ=%d\n",
                   raw.ax, raw.ay, raw.az,
                   raw.gx, raw.gy, raw.gz);
        } else if (ret != -EAGAIN) {
            printk("Read error: %d\n", ret);
        }

        k_msleep(10);
    }

    return 0;
}

```

这份代码的目标是完成最基础的寄存器通信验证，并非完整的产品级驱动。实际项目还应增加初始化配置回读、复位异常恢复、校准参数处理，以及数据丢失和采样时间记录。

由于这里直接使用 `float` 转换而暂时打印原始整数，能够避免初期调试阶段因 `printk()` 浮点格式支持配置导致输出问题。

## **八、FIFO 读取机制：后续优化方向**

手册第 53～56 页，68～70 页，92 页，108～109 页

LSM6DSR 内置 3KB FIFO，通过压缩功能可以实现最高约 9KB 的等效数据存储容量。

FIFO 与前面直接读取输出寄存器的区别是：

| 对比 | 普通寄存器读取 | FIFO 读取 |
| --- | --- | --- |
| 读取内容 | 最新数据 | 缓存的一组历史数据 |
| MCU 参与 | 每次采样都需要读取 | 可以积累数据后批量读取 |
| 数据量 | 通常每次 6/12 字节 | 一次读取多组数据 |
| 时间序列保留 | 有限 | 更适合连续样本记录 |
| 实现复杂度 | 低 | 中等或较高 |
| 当前阶段 | 推荐先实现 | 第二阶段实现 |

### **8.1 FIFO 相关寄存器**

| 寄存器 | 地址 | 功能 |
| --- | --- | --- |
| FIFO_CTRL1 | `0x07` | Watermark 低8位 |
| FIFO_CTRL2 | `0x08` | Watermark 高位及压缩设置 |
| FIFO_CTRL3 | `0x09` | 加速度与陀螺仪 BDR |
| FIFO_CTRL4 | `0x0A` | FIFO 工作模式 |
| FIFO_STATUS1 | `0x3A` | 未读记录数低8位 |
| FIFO_STATUS2 | `0x3B` | 未读记录数高2位、满/溢出/水位标志 |
| FIFO_DATA_OUT_TAG | `0x78` | 数据标签 |
| FIFO_DATA_OUT_X_L | `0x79` | 数据起始地址 |

### **8.2 FIFO 数据格式**

手册第 56 页指出，每个 FIFO word 是 7 字节：

```
Byte 0 : TAG
Byte 1 : X_L
Byte 2 : X_H
Byte 3 : Y_L
Byte 4 : Y_H
Byte 5 : Z_L
Byte 6 : Z_H
```

其中 `TAG` 的高5位用于识别当前数据来源：

| TAG_SENSOR | 数据来源 |
| --- | --- |
| `0x01` | 陀螺仪普通未压缩数据 |
| `0x02` | 加速度计普通未压缩数据 |
| `0x03` | 温度 |
| `0x04` | 时间戳 |
| `0x05` | 配置变化 |

例如：

```
uint8_t fifo[7];

i2c_burst_read_dt(
    &lsm6dsr,
    0x78,
    fifo,
    sizeof(fifo)
);

uint8_t tag = fifo[0] >> 3;

if (tag == 0x01) {
    // Gyroscope
} else if (tag == 0x02) {
    // Accelerometer
}
```

这里假设已经正确配置并填充 FIFO，且未启用压缩功能。若启用压缩，就不能简单地把所有记录都解释成普通的三轴 16 位原始数据。

### **8.3 FIFO Continuous 模式**

手册规定 `FIFO_CTRL4` 的 `FIFO_MODE[2:0]` 有多种取值，其中：

```
000 : Bypass（关闭 FIFO）
001 : FIFO（写满后停止）
110 : Continuous（满后覆盖旧数据）
```

例如，简单的非压缩测试配置可以采用：

```
// Accel BDR = 104 Hz
// Gyro  BDR = 104 Hz
FIFO_CTRL3 = 0x44;

// Continuous mode
FIFO_CTRL4 = 0x06;
```

建议先使用不带压缩的 Continuous 模式，验证 FIFO 状态、TAG 解析以及未读数据计数，再增加 Watermark 中断和时间戳。

这对于后续功率计非常有价值：MCU 不必为了每一个 IMU 数据点都执行 I²C 通信，可以在适当的时间窗口批量取回传感器数据。

## **九、建议的驱动文件结构**

你之前已经开始使用 Zephyr 的 `.overlay`、`prj.conf` 和 `main.c`，我建议将 LSM6DSR 驱动独立出来，而不是将全部代码放进 `main.c`。

```
CadenceSensor/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── custom_board.overlay
├── dts/
│   └── bindings/
│       └── sensor/
│           └── custom,lsm6dsr.yaml
├── include/
│   └── lsm6dsr.h
└── src/
    ├── main.c
    └── drivers/
        └── lsm6dsr.c
```

上面的自定义 binding 路径是自编寄存器级驱动的一种组织方式，具体还需要在构建系统中正确设置 DTS 根路径；若使用 SDK 已有的正式绑定，则不需要自行创建。

建议将驱动逐步拆分为以下 API：

| API | 功能 |
| --- | --- |
| `lsm6dsr_init()` | 芯片检测与初始化 |
| `lsm6dsr_read_reg()` | 单寄存器读取 |
| `lsm6dsr_write_reg()` | 单寄存器写入 |
| `lsm6dsr_read_regs()` | 连续读取 |
| `lsm6dsr_set_accel()` | 加速度计 ODR、量程配置 |
| `lsm6dsr_set_gyro()` | 陀螺仪 ODR、量程配置 |
| `lsm6dsr_read_raw()` | 读取六轴原始数据 |
| `lsm6dsr_convert()` | 转换实际物理量 |
| `lsm6dsr_set_interrupt()` | 配置 INT1/INT2 |
| `lsm6dsr_set_power_mode()` | 运行和低功耗模式切换 |
| `lsm6dsr_fifo_read()` | FIFO 批量数据读取 |

## **十、建议的硬件验证顺序**

> [Chart or card]
>
> LSM6DSR 驱动验证清单 已完成 0 / 9 读取 WHO_AM_I（0x0F），确认返回 0x6B 写入 CTRL3_C（0x12），确认 BDU 与 IF_INC 配置 CTRL1_XL 和 CTRL2_G 读取 STATUS_REG，确认 XLDA/GDA 周期性出现 连续读取 12 字节六轴数据 静止状态检查加速度模长接近 1g 旋转 PCB，观察角速度数据变化 配置 INT1 数据就绪中断 增加 FIFO 与低功耗状态切换

在检查静止状态数据时，需要注意：实际测量的 X、Y、Z 三个分量取决于 PCB 放置方向，并不是每个轴都应该接近 1g。正常情况下，加速度向量模长近似为：

$$
\sqrt{a_x^2+a_y^2+a_z^2}\approx1g
$$

而静止时三轴角速度应在零附近波动，并存在一定零偏和噪声。

## **十一、针对踏频功率计的下一阶段配置**

完成基础驱动之后，我建议按三种运行状态设计传感器控制策略：

| 状态 | 加速度计 | 陀螺仪 | MCU 处理方式 |
| --- | --- | --- | --- |
| 静止待机 | 12.5/26Hz，低功耗 | 关闭 | 运动中断唤醒 |
| 检测到运动 | 52/104Hz | 按需启动 | 判断是否持续踩踏 |
| 骑行测量 | 104/208Hz | 104/208Hz | 连续采样，结合 ADS1220 计算功率 |

这是建议的系统策略，不是手册预先定义的三种应用状态。实际 ODR、量程和唤醒阈值需要结合骑行数据测试。

**最后总结：** 当前开发阶段最重要的六个寄存器组是：

- `WHO_AM_I (0x0F)`：识别芯片。
- `CTRL1_XL (0x10)`：启动并配置加速度计。
- `CTRL2_G (0x11)`：启动并配置陀螺仪。
- `CTRL3_C (0x12)`：配置 BDU、IF_INC 和软件复位。
- `STATUS_REG (0x1E)`：判断数据就绪。
- `OUTX_L_G～OUTZ_H_A (0x22～0x2D)`：获取全部六轴数据。

只要完成这部分，你就已经具备一个可用于踏频算法开发的 LSM6DSRTR 基础驱动。之后再加入 FIFO、时间戳、运动中断和校准功能，就可以逐步进入正式的低功耗功率计固件开发。

