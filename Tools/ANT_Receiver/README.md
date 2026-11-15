# BLE / ANT+ 多设备监测

双击 `start.cmd`，首次启动会创建本地 Python 虚拟环境并安装依赖。需要 Windows 11、Python 3.10+ 和 Tk。蓝牙使用 Windows BLE 驱动；ANT+ 必须使用支持 ANT 连续扫描的 ANT USB 接收器，Windows 驱动需兼容 libusb（如 WinUSB），并且不能被其他程序占用。普通蓝牙 USB 适配器不能接收 ANT+。

BLE：扫描，选择设备并连接；可逐个连接多个设备。支持标准 Heart Rate Measurement、CSC Measurement、Cycling Power Measurement 通知。连接数量取决于蓝牙适配器和设备。设备未在广播中声明服务也可选择连接后发现服务。

ANT+：插入 ANT USB 接收器后启动 ANT+ 接收；连续扫描自动列出多个设备，不需要与每个广播设备建立 BLE 式连接。支持心率类型 120、踏频类型 122、速度踏频类型 121 和功率计类型 11 的标准功率页 0x10；其他页仅显示原始数据。高流量时扫描模式可能丢包。

表格分别显示协议、设备、心率、踏频、功率和更新时间。原始数据显示 BLE GATT 通知或 ANT+ 8 字节数据页。踏频需要两个不同的曲柄事件计算；当前实现保留上次有效踏频，静止或断链后该值可能过期，请参考最后更新时间。暂未实现轮速计算、ANT+ 扭矩页或设备自动重连。队列满时可能丢失数据，不适用于无损记录或医疗用途。

若 ANT 启动失败，检查是否插入 ANT 接收器、驱动绑定、libusb、权限和设备占用。当前本机仅检测到 BARROT 蓝牙适配器；ANT+ 硬件收包尚未验证。

依赖和接口：[openant](https://github.com/Tigge/openant)、[Bleak](https://bleak.readthedocs.io/)。
