"""CadenceSensor GATT entry and MCUboot firmware-loader SMP client.

Always specify the target address; uploads overwrite the only business image.
"""
import argparse
import asyncio
import hashlib
import json
import struct
from pathlib import Path

import cbor2
from bleak import BleakClient, BleakScanner

SMP = "da2e7828-fbce-4e01-ae9e-261174997c48"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
DFU_CONTROL = "85b8f1a1-c8ed-4be7-a48f-47d72e308066"


async def request_gatt_dfu(client):
    control = client.services.get_characteristic(DFU_CONTROL)
    if control is None:
        return False
    await client.write_gatt_char(control, b"OTA ENTER", response=True)
    return True


class SmpClient:
    def __init__(self, client):
        self.client = client
        self.sequence = 0
        self.received = bytearray()
        self.frames = asyncio.Queue()

    async def start(self):
        self.char = self.client.services.get_characteristic(SMP)
        if self.char is None:
            raise RuntimeError("SMP characteristic missing: target is not in Loader mode")
        await self.client.start_notify(self.char, self.notification)

    def notification(self, _, data):
        self.received.extend(data)
        while len(self.received) >= 8:
            size = 8 + int.from_bytes(self.received[2:4], "big")
            if len(self.received) < size:
                return
            self.frames.put_nowait(bytes(self.received[:size]))
            del self.received[:size]

    async def request(self, operation, group, command, data):
        seq = self.sequence
        self.sequence = (seq + 1) % 256
        body = cbor2.dumps(data)
        packet = struct.pack(">BBHHBB", operation, 0, len(body), group, seq, command) + body
        # SMP reassembly permits an SMP request to span ATT writes.
        chunk = min(244, self.char.max_write_without_response_size)
        for offset in range(0, len(packet), chunk):
            await self.client.write_gatt_char(self.char, packet[offset:offset + chunk],
                                              response=False)
            await asyncio.sleep(0.002)
        while True:
            frame = await asyncio.wait_for(self.frames.get(), timeout=30)
            op, _, _, response_group, response_seq, response_id = struct.unpack(">BBHHBB", frame[:8])
            if (response_group, response_seq, response_id) != (group, seq, command):
                continue
            if op & 7 != operation + 1:
                raise RuntimeError(f"Unexpected SMP response operation {op}")
            result = cbor2.loads(frame[8:])
            if result.get("rc", 0) != 0 or result.get("err", {}).get("rc", 0) != 0:
                raise RuntimeError(f"SMP rejected request: {result}")
            return result

    async def upload(self, image, stop_after=0, progress=None):
        if len(image) < 32 or int.from_bytes(image[:4], "little") != 0x96F3B83D:
            raise ValueError("Expected MCUboot signed BIN with image header")
        digest = hashlib.sha256(image).digest()
        offset = 0
        next_report = 0
        while offset < len(image):
            payload = {"off": offset, "data": image[offset:offset + 384]}
            if offset == 0:
                payload.update({"len": len(image), "sha": digest})
            result = await self.request(2, 1, 1, payload)
            new_offset = result.get("off")
            if not isinstance(new_offset, int) or not offset < new_offset <= len(image):
                raise RuntimeError(f"Invalid upload progress: {result}")
            offset = new_offset
            if progress is not None:
                progress(offset, len(image))
            if offset >= next_report or offset == len(image):
                print(f"UPLOAD {offset}/{len(image)}", flush=True)
                next_report = offset + 8192
            if stop_after and offset >= stop_after:
                print(f"UPLOAD deliberately interrupted at {offset} bytes", flush=True)
                return False
        print(f"UPLOAD complete file_sha256={digest.hex()}", flush=True)
        return True


def printable(value):
    return json.dumps(value, default=lambda x: x.hex() if isinstance(x, bytes) else str(x))


async def run(args):
    if args.command == "scan":
        for device, adv in (await BleakScanner.discover(timeout=6, return_adv=True)).values():
            if (adv.local_name or "").startswith("Cadence"):
                print(device.address, adv.local_name, adv.service_uuids)
        return
    device = await BleakScanner.find_device_by_address(args.address, timeout=15)
    if device is None:
        raise RuntimeError(f"Target {args.address} not advertising")
    async with BleakClient(device, pair=False, winrt={"use_cached_services": False}) as client:
        if args.command == "enter":
            if client.services.get_characteristic(SMP):
                print("Target is already in Loader mode")
                return
            if await request_gatt_dfu(client):
                print("DFU request acknowledged. Run status after the device restarts.")
                return
        if args.command in ("nus", "enter"):
            if client.services.get_characteristic(NUS_RX) is None:
                raise RuntimeError("Target has no DFU entry characteristic or NUS. Hold SW5 and reset.")
            await client.start_notify(NUS_TX, lambda _, data: print(
                data.decode("utf-8", errors="replace"), end="", flush=True))
            await asyncio.sleep(1)
            command = "OTA ENTER\n" if args.command == "enter" else args.text + "\n"
            for offset in range(0, len(command.encode()), 20):
                await client.write_gatt_char(NUS_RX, command.encode()[offset:offset + 20],
                                              response=False)
                await asyncio.sleep(0.02)
            await asyncio.sleep(args.seconds)
            return
        smp = SmpClient(client)
        await smp.start()
        if args.command == "status":
            print("BOOTLOADER", printable(await smp.request(0, 0, 8, {"query": "mode"})))
            print("IMAGES", printable(await smp.request(0, 1, 0, {})))
        elif args.command == "reset":
            print("RESET", printable(await smp.request(2, 0, 5, {})))
            await asyncio.sleep(1)
        elif args.command == "check-erase-guard":
            try:
                await smp.request(2, 1, 5, {"slot": 1})
            except RuntimeError as error:
                if "SMP rejected request" not in str(error):
                    raise
                print("ERASE guard rejected slot1:", error)
                print("BOOTLOADER", printable(await smp.request(0, 0, 8, {"query": "mode"})))
            else:
                raise RuntimeError("Unsafe: slot1 erase was accepted")
        elif args.command == "check-oversize":
            image = Path(args.image).read_bytes()
            try:
                await smp.request(2, 1, 1, {"off": 0, "len": 0x3C001,
                                           "data": image[:32],
                                           "sha": hashlib.sha256(image).digest()})
            except RuntimeError as error:
                if "SMP rejected request" not in str(error):
                    raise
                print("SIZE guard rejected oversized image:", error)
            else:
                raise RuntimeError("Unsafe: oversized upload was accepted")
        elif args.command == "upload":
            complete = await smp.upload(Path(args.image).read_bytes(), args.stop_after)
            if complete and args.reset:
                print("RESET", printable(await smp.request(2, 0, 5, {})))
                await asyncio.sleep(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("scan", "nus", "enter", "status", "upload", "reset", "check-erase-guard", "check-oversize"))
    parser.add_argument("--address")
    parser.add_argument("--text", default="VERSION")
    parser.add_argument("--seconds", type=float, default=3)
    parser.add_argument("--image")
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--stop-after", type=int, default=0,
                        help="Testing only: leave the only business image partially written")
    args = parser.parse_args()
    if args.command != "scan" and not args.address:
        parser.error("--address is required")
    if args.command in ("upload", "check-oversize") and not args.image:
        parser.error("--image is required")
    asyncio.run(run(args))
