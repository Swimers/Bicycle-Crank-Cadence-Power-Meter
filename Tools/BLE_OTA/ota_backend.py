"""GUI operations using the same SMP uploader as ota.py. No GUI dependencies."""
import asyncio
import hashlib
import struct
import time
from dataclasses import dataclass
from pathlib import Path

from bleak import BleakClient, BleakScanner

from ota import NUS_RX, NUS_TX, SMP, SmpClient, printable

NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
SMP_SERVICE = "8d53dc1d-1db7-4cd3-868b-8a527460aa84"
SLOT_SIZE = 0x3C000  # Current board's 240 KiB business slot, not Loader slot1.


@dataclass(frozen=True)
class Firmware:
    path: Path
    data: bytes
    version: str
    sha256: str


def load_firmware(path):
    """Check MCUboot structure and signature TLV presence, not cryptographic trust."""
    path = Path(path)
    if path.suffix.lower() != ".bin":
        raise ValueError("请选择 zephyr.signed.bin；不支持 HEX、ZIP 或整机镜像。")
    with path.open("rb") as stream:
        data = stream.read(SLOT_SIZE + 1)
    if len(data) > SLOT_SIZE:
        raise ValueError("镜像超过当前业务槽 240 KiB，拒绝上传。")
    if len(data) < 32 or struct.unpack_from("<I", data)[0] != 0x96F3B83D:
        raise ValueError("不是 MCUboot 镜像：请使用带签名的业务 BIN。")
    header, protected = struct.unpack_from("<HH", data, 8)
    size = struct.unpack_from("<I", data, 12)[0]
    if header < 32 or not size or header + size + protected + 4 > len(data):
        raise ValueError("镜像头长度、业务数据长度或 TLV 区域不完整。")
    if protected:
        magic, length = struct.unpack_from("<HH", data, header + size)
        if magic != 0x6908 or length != protected:
            raise ValueError("受保护 TLV 区域不正确。")
    pos = header + size + protected
    magic, length = struct.unpack_from("<HH", data, pos)
    end = pos + length
    if magic != 0x6907 or length < 4 or end > len(data):
        raise ValueError("签名 TLV 区域缺失或损坏。")
    pos += 4
    kinds = set()
    while pos < end:
        if pos + 4 > end:
            raise ValueError("TLV 条目头被截断。")
        kind, _, count = struct.unpack_from("<BBH", data, pos)
        pos += 4
        if not count or pos + count > end:
            raise ValueError("TLV 条目长度不正确。")
        kinds.add(kind)
        pos += count
    if 0x10 not in kinds or not kinds.intersection({0x20, 0x22, 0x23, 0x24}):
        raise ValueError("未找到 SHA-256 和签名 TLV；不能上传裸 BIN。")
    major, minor, revision, build = struct.unpack_from("<BBHI", data, 20)
    return Firmware(path.resolve(), data, f"{major}.{minor}.{revision}+{build}",
                    hashlib.sha256(data).hexdigest())


class OtaBackend:
    def __init__(self, emit):
        self.emit = emit

    async def scan(self):
        found = await BleakScanner.discover(timeout=6, return_adv=True)
        devices = []
        for device, adv in found.values():
            uuids = {u.lower() for u in adv.service_uuids}
            name = adv.local_name or device.name or "未命名"
            if name.startswith("Cadence") or uuids & {SMP_SERVICE, NUS_SERVICE}:
                mode = "Loader / SMP" if SMP_SERVICE in uuids else "业务 / NUS" if NUS_SERVICE in uuids else "待查询"
                devices.append((name, device.address, adv.rssi, mode))
        return sorted(devices, key=lambda d: ("Loader" not in d[3], -d[2]))

    async def connect(self, address, timeout=12):
        device = await BleakScanner.find_device_by_address(address, timeout=timeout)
        if device is None:
            raise RuntimeError(f"设备 {address} 未广播；请检查电源及其他客户端占用。")
        client = BleakClient(device, pair=False, timeout=12,
                             winrt={"use_cached_services": False})
        try:
            await client.connect()
            return client
        except BaseException:
            await self.disconnect(client)
            raise

    async def disconnect(self, client):
        try:
            if client.is_connected:
                await asyncio.wait_for(client.disconnect(), 5)
        except Exception as error:
            self.emit("log", f"断开连接：{error}")

    async def nus(self, client, command, seconds=1):
        if client.services.get_characteristic(NUS_RX) is None:
            raise RuntimeError("设备没有 NUS 服务，可能已经进入 Loader。")
        received = bytearray()

        def notification(_, data):
            # Retain a bounded response even when sensor logs are streaming.
            received.extend(data)
            if len(received) > 8192:
                del received[:-8192]

        await client.start_notify(NUS_TX, notification)
        await asyncio.sleep(0.2)
        data = (command + "\n").encode()
        for offset in range(0, len(data), 20):
            await client.write_gatt_char(NUS_RX, data[offset:offset + 20], response=False)
            await asyncio.sleep(0.02)
        await asyncio.sleep(seconds)
        text = received.decode("utf-8", errors="replace")
        self.emit("log", text.strip() or f"已发送 {command}，未收到文本响应。")
        return text

    async def inspect(self, address):
        client = await self.connect(address)
        try:
            if client.services.get_characteristic(SMP):
                smp = SmpClient(client)
                await smp.start()
                self.emit("log", "BOOTLOADER " + printable(await smp.request(0, 0, 8, {"query": "mode"})))
                self.emit("log", "IMAGES " + printable(await smp.request(0, 1, 0, {})))
                return "Loader / SMP"
            await self.nus(client, "VERSION")
            return "业务 / NUS"
        finally:
            await self.disconnect(client)

    async def enter(self, address):
        client = await self.connect(address)
        try:
            if client.services.get_characteristic(SMP):
                self.emit("log", "设备已在 Loader 模式。")
            else:
                await self.nus(client, "OTA ENTER")
        finally:
            await self.disconnect(client)

    async def wait_loader(self, address):
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            client = None
            try:
                client = await self.connect(address, timeout=3)
                if client.services.get_characteristic(SMP):
                    return client
            except Exception as error:
                self.emit("log", f"等待同一地址 Loader：{error}")
            if client is not None:
                await self.disconnect(client)
            await asyncio.sleep(1)
        raise TimeoutError("45 秒内未找到同一地址的 Loader；请按住 SW5 并复位后重试。")

    async def reboot(self, address):
        client = await self.connect(address)
        try:
            smp = SmpClient(client)
            await smp.start()
            await smp.request(2, 0, 5, {})
            self.emit("log", "已发送 Loader 复位命令。")
        finally:
            await self.disconnect(client)

    async def upgrade(self, address, firmware, auto_enter=True, reset=True):
        client = await self.connect(address)
        try:
            if client.services.get_characteristic(SMP) is None:
                if not auto_enter:
                    raise RuntimeError("当前为业务模式，请先进入 Loader 或勾选自动进入。")
                self.emit("phase", "进入 Loader")
                await self.nus(client, "OTA ENTER")
                await self.disconnect(client)
                client = None
                await asyncio.sleep(1)
                self.emit("phase", "等待 Loader")
                client = await self.wait_loader(address)
            smp = SmpClient(client)
            await smp.start()
            self.emit("phase", "正在上传")
            start = time.monotonic()

            def progress(done, total):
                elapsed = max(time.monotonic() - start, 0.001)
                self.emit("progress", (done, total, done / elapsed))

            if not await smp.upload(firmware.data, progress=progress):
                raise RuntimeError("上传未完成。")
            if reset:
                self.emit("phase", "请求复位")
                await smp.request(2, 0, 5, {})
        finally:
            if client is not None:
                await self.disconnect(client)
        # Transport success must not be presented as signature/boot success.
        if not reset:
            return "上传完成，未复位；尚未验证业务启动。"
        self.emit("phase", "核对业务启动")
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            client = None
            try:
                client = await self.connect(address, timeout=3)
                if client.services.get_characteristic(NUS_RX):
                    text = await self.nus(client, "VERSION")
                    if "CadenceSensor firmware " in text:
                        return "上传完成，设备已返回业务模式；请核对日志中的实际版本。"
            except Exception as error:
                self.emit("log", f"等待业务启动：{error}")
            finally:
                if client is not None:
                    await self.disconnect(client)
            await asyncio.sleep(1)
        return "传输完成并已请求复位，但未确认业务启动；请重新查询设备，必要时进入 Loader 重传。"
