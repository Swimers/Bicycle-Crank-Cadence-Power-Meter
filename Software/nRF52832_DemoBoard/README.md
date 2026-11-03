# nRF52832_DemoBoard

本工程仅适用于 2026-11-02 的 3.3V nRF52832 排针开发板，构建目标为 `nrf52832_proto/nrf52832`，适配 NCS 3.4.1。

已配置外部低频晶振、TWIM0、SPIM1、按键/中断输入、NFC 转 GPIO、硬件 RESET、片内 DC/DC 和 RTT。板载 ADS1220 使用 P0.06～P0.10，SC7A20H 使用 P0.13～P0.15。

完整引脚表、配置依据、构建/烧录步骤及验证范围见 [硬件配置说明](HARDWARE_CONFIGURATION.md)。在仓库根目录运行 `./Software/nRF52832_DemoBoard/tools/build.ps1`；VS Code 中新建 `nrf52832_proto/nrf52832` 构建配置。

当前完成板级初始化，传感器寄存器设置、采样、算法与 BLE 服务待实现。

## 项目结构

```text
nRF52832_DemoBoard/
├── CMakeLists.txt             # 单固件应用
├── prj.conf                   # 驱动、时钟与日志
├── src/main.c                 # 板级初始化
├── boards/                    # 当前开发板定义
├── dts/bindings/              # 板载传感器绑定
├── tools/build.ps1            # 单应用构建入口
├── HARDWARE_CONFIGURATION.md  # 引脚及使用说明
└── build/                     # 自动生成，Git 忽略
```

构建使用 `--no-sysbuild`，Applications 只需显示“应用 → build”两层。已有构建会由插件自动识别；重命名后若仍显示旧条目，刷新 Applications。新建配置时关闭 Build with sysbuild。

工程文件统一由仓库根目录的 Git 管理，应用目录内没有独立 Git 仓库。构建产物位于 `build/zephyr/`。
