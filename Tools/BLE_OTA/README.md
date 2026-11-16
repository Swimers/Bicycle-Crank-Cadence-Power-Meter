# CadenceSensor BLE OTA 工具

`ota.py` 使用 Bleak + cbor2，通过 NUS 进入独立 Loader，再通过 SMP over BLE 覆盖业务镜像。所有设备操作必须显式指定 `--address`。

安装依赖：`python -m pip install -r requirements.txt`。本机已在 `Tools/BLE_NUS/.venv` 中验证。

常用命令：`scan`、`nus`（默认发送 VERSION）、`enter`、`status`、`upload --image <签名BIN> --reset`、`reset`。详见 `python ota.py --help`。

`--stop-after` 故意中断唯一业务镜像的上传，仅供恢复测试；不保证业务镜像仍可运行。`check-erase-guard` 和 `check-oversize --image <签名BIN>` 用于检查 Loader 的拒绝行为。

完整操作、分区及实机结果见 [实现与验证记录](../../Software/nRF52832_Demo/doc/BLE_OTA_Implementation_and_Validation.md)。

## 图形界面

双击 `start.cmd`，或在 PowerShell 执行 `./start.ps1`。需要 Python 3.10+（含 Tk）；
首次启动在本目录创建 `.venv` 并安装 requirements。也可使用已有环境：

```powershell
& ../BLE_NUS/.venv/Scripts/python.exe app.py
```

1. 扫描并选择明确的目标地址，也可手工输入。业务设备通常叫 CadenceSensor，Loader 叫 CadenceDFU。
2. 点击“查询版本 / 状态”确认目标；软件每次操作建立连接，完成后自动断开。
3. 选择 `Software/nRF52832_Demo/build/nRF52832_Demo/zephyr/zephyr.signed.bin`。显示镜像版本、大小、完整文件 SHA-256。
4. 保持“自动从业务模式进入 Loader”和“上传后复位并查询业务版本”勾选，点击开始并确认目标。
5. 观察进入 Loader、上传、复位、业务版本查询各阶段及字节数、速度和预计剩余时间。
   100% 仅表示传输完成；业务启动核对和签名验证是另外的步骤。最终签名由 MCUboot 验证。

设备已在 Loader 时可以直接上传；单独“进入 Loader”和“Loader 复位”用于分步调试。
升级过程只重新寻找同一地址，绝不按同名设备替换目标。关闭占用 BLE 连接的手机、NUS 终端后再操作。
日志可保存。取消或退出会请求取消任务并断开 BLE，不会宣称回滚成功；请重新扫描 Loader，完整重传。

界面只接受不超过 240 KiB 的 MCUboot 签名业务 BIN，拒绝 HEX、ZIP、裸 BIN 及不完整 TLV。
该检查不验证密码学签名、不保证密钥匹配或业务运行正确；请使用本项目构建输出。
单槽覆盖无回滚。若传输完成但仍在 Loader，请核查签名、镜像布局，再重传有效固件。
界面不提供破坏性拒绝测试或故意中断字节数选项；这些仍保留在 CLI 中供专门测试使用。

代码组织：`app.py` 为 Tk 界面，`ota_backend.py` 为异步升级流程，`ota.py` 保留 CLI 和公共 SMP 客户端。
上传复用同一份 `SmpClient.upload()`，新增可选进度回调，不改变原 CLI 参数。

验证命令（无真实 BLE 设备写入）：

```powershell
python -m unittest -v test_ota.py
```
