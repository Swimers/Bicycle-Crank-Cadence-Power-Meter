# CadenceSensor nRF52832 固件

项目只维护一个顶层构建配置 `build`，同时启用 ANT Bicycle Power、BLE NUS 调试、SC7A20H 采样及单业务槽 BLE OTA。内部 `nRF52832_Demo`、`mcuboot`、`ble_loader` 是三个启动链镜像，不是三套可选配置。

## 开发环境

| 组件 | 本机路径 |
| --- | --- |
| NCS 3.2.4 | `D:/ncs/v3.2.4` |
| 对应工具链 fd21892d0f | `C:/ncs/toolchains/fd21892d0f` |
| ANT add-on 2.1.1 模块 | `D:/ncs/ant-v2.1.1/ant` |

ANT 模块单独加载，其旁边原来用于安装 ANT 的 SDK 副本不参与当前构建。`prj.conf` 为唯一业务功能配置，`sysbuild.conf` 控制启动链；Loader 与 MCUboot 继续使用各自的镜像配置。

## 构建

在应用目录的 PowerShell 中运行：

```powershell
.\scripts\build.ps1 -Pristine  # 首次或更换 SDK 后完整构建
.\scripts\build.ps1            # 后续增量构建
```

安装位置不同可指定 `-SdkRoot`、`-ToolchainRoot`、`-AntModuleDir`。输出固定为本项目的 `build`，不再创建其他构建目录。

nRF Connect IDE 中使用 NCS **3.2.4**、对应工具链，选择 **cadence_sensor/nrf52832**，开启 **Sysbuild**，选择顶层 **build**。已生成的构建缓存保存 SDK 和工具链位置；不要为各子镜像另建配置。应用 CMake 自动加载 ANT 模块，无需手动追加 ANT 配置文件。

从已激活该 SDK/工具链的终端也可运行 `west build --sysbuild -b cadence_sensor/nrf52832 -d build . -- "-DBOARD_ROOT=$PWD" "-DDTS_ROOT=$PWD"`。sysbuild 会自动校验 ANT/BLE、Flash 分区及 OTA 保护配置，并合并完整 HEX，IDE 构建同样生效。

| 输出 | 用途 |
| --- | --- |
| `build/merged.hex` | 初次 SWD 烧录完整启动链 |
| `build/nRF52832_Demo/zephyr/zephyr.signed.bin` | BLE OTA 的签名业务固件 |
| `build/ota_artifacts.json` | 各镜像地址段和 SHA-256 |

业务 Flash 分区保持 `0x10000–0x4C000`（240 KiB），Loader 和参数地址保持不变。ANT 默认发送 `0x10`，250 W / 90 rpm；BLE NUS 文本、OTA 指令及传感器采样并行。当前使用 ANT 非商业评估授权。

硬件连接见 [板级说明](boards/others/cadence_sensor/README.md)，ANT 接口见 [ANT 说明](doc/ANT_App_Interface.md)，OTA 历史实机证据见 [验证记录](doc/BLE_OTA_Implementation_and_Validation.md)。
