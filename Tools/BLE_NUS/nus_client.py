"""BLE transport. All methods and callbacks run on one asyncio thread."""
import asyncio
from bleak import BleakClient, BleakScanner

SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # PC -> MCU
TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # MCU -> PC


class NusClient:
    def __init__(self, emit):
        self.emit = emit
        self.client = None
        self.rx = None

    async def scan(self, seconds=6):
        # Preserve BLEDevice objects so connecting never triggers an implicit scan.
        async with BleakScanner() as scanner:
            await asyncio.sleep(seconds)
        found = list(scanner.discovered_devices_and_advertisement_data.values())
        found.sort(key=lambda item: (
            SERVICE not in [u.lower() for u in item[1].service_uuids],
            "cadencesensor" not in (item[1].local_name or item[0].name or "").lower(),
            -item[1].rssi,
        ))
        return found

    def _disconnected(self, client):
        if self.client is client:
            self.client = None
            self.rx = None
            self.emit("disconnected", "设备连接已断开")

    async def connect(self, device):
        await self.disconnect()
        client = BleakClient(device, disconnected_callback=self._disconnected,
                             pair=False, winrt={"use_cached_services": False})
        self.client = client
        try:
            await asyncio.wait_for(client.connect(), timeout=25)
            service = client.services.get_service(SERVICE)
            if service is None:
                raise RuntimeError("设备未提供 NUS 服务，请烧录包含 BLE 控制台的 MCU 固件。")
            self.rx = service.get_characteristic(RX)
            tx = service.get_characteristic(TX)
            if self.rx is None or tx is None or "notify" not in tx.properties:
                raise RuntimeError("NUS RX/TX 特征不完整，或 TX 不支持 Notify。")
            if not {"write", "write-without-response"}.intersection(self.rx.properties):
                raise RuntimeError("NUS RX 特征不可写。")
            self.emit("session", device.address)
            await client.start_notify(tx, lambda _, data: self.emit("rx", bytes(data)))
            if not client.is_connected:
                raise RuntimeError("订阅通知时设备已断开。")
            self.emit("connected", device.address)
        except BaseException:
            await self.disconnect()
            raise

    async def send(self, data):
        client, rx = self.client, self.rx
        if client is None or not client.is_connected or rx is None:
            raise RuntimeError("请先连接设备。")
        # Conservative 20-byte chunks work with the MCU's default ATT MTU 23.
        # Prefer write requests for flow control; fallback for other NUS servers.
        response = "write" in rx.properties
        for offset in range(0, len(data), 20):
            await client.write_gatt_char(rx, data[offset:offset + 20], response=response)
            if not response:
                await asyncio.sleep(0.02)
        self.emit("sent", data)

    async def disconnect(self):
        client = self.client
        self.client = None
        self.rx = None
        try:
            if client is not None:
                await asyncio.wait_for(client.disconnect(), timeout=8)
        finally:
            if client is not None:
                self.emit("disconnected", "连接已关闭")
