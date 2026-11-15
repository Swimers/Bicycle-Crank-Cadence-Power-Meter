import asyncio
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import AsyncMock

import cbor2

from ota import SMP, NUS_RX, SmpClient
from ota_backend import OtaBackend, SLOT_SIZE, load_firmware


def signed_image():
    """Structurally signed fixture, deliberately not a trusted/valid signature."""
    header = bytearray(32)
    struct.pack_into("<I", header, 0, 0x96F3B83D)
    struct.pack_into("<HHII", header, 8, 32, 0, 16, 0)
    struct.pack_into("<BBHI", header, 20, 1, 2, 3, 4)
    tlv = struct.pack("<BBH", 0x10, 0, 32) + b"x" * 32
    tlv += struct.pack("<BBH", 0x22, 0, 64) + b"y" * 64
    return bytes(header) + b"z" * 16 + struct.pack("<HH", 0x6907, 4 + len(tlv)) + tlv


class ImageTests(unittest.TestCase):
    def load(self, data, name="app.signed.bin"):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / name
            path.write_bytes(data)
            return load_firmware(path)

    def test_version_and_immutable_bytes(self):
        fw = self.load(signed_image())
        self.assertEqual(fw.version, "1.2.3+4")
        self.assertEqual(fw.data, signed_image())
        self.assertEqual(len(fw.sha256), 64)

    def test_reject_invalid_images(self):
        for data, name in ((signed_image(), "app.hex"), (b"raw", "app.bin"),
                           (signed_image()[:-1], "app.bin"),
                           (b"x" * (SLOT_SIZE + 1), "app.bin"),
                           (signed_image()[:48], "app.bin")):
            with self.subTest(name=name, size=len(data)):
                with self.assertRaises(ValueError):
                    self.load(data, name)

    def test_reject_hash_only_tlv(self):
        image = bytearray(signed_image())
        image[88] = 0x11  # Replace the signature TLV type.
        with self.assertRaises(ValueError):
            self.load(image)


class FakeClient:
    def __init__(self, loader=True):
        self.services = self
        self.loader = loader
        self.is_connected = True
        self.max_write_without_response_size = 20
        self.notification = None
        self.packet = bytearray()
        self.writes = []

    def get_characteristic(self, uuid):
        return self if (uuid == SMP and self.loader) or (uuid == NUS_RX and not self.loader) else None

    async def start_notify(self, _, callback):
        self.notification = callback

    async def disconnect(self):
        self.is_connected = False

    async def write_gatt_char(self, _, data, response=False):
        self.writes.append(bytes(data))
        self.packet.extend(data)
        if len(self.packet) < 8:
            return
        op, _, size, group, seq, command = struct.unpack(">BBHHBB", self.packet[:8])
        if len(self.packet) < size + 8:
            return
        payload = cbor2.loads(self.packet[8:])
        self.packet.clear()
        result = {"off": payload["off"] + len(payload["data"])} if "data" in payload else {"rc": 0}
        body = cbor2.dumps(result)
        frame = struct.pack(">BBHHBB", op + 1, 0, len(body), group, seq, command) + body
        self.notification(None, frame[:5])
        self.notification(None, frame[5:])


class ProtocolTests(unittest.IsolatedAsyncioTestCase):
    async def test_fragmented_roundtrip_and_progress(self):
        client = FakeClient()
        smp = SmpClient(client)
        await smp.start()
        progress = []
        image = signed_image() + b"padding" * 70
        self.assertTrue(await smp.upload(image, progress=lambda n, total: progress.append((n, total))))
        self.assertEqual(progress[-1], (len(image), len(image)))
        self.assertTrue(all(len(w) <= 20 for w in client.writes))
        self.assertGreater(len(progress), 1)

    async def test_cancel_disconnects(self):
        backend = OtaBackend(lambda *_: None)
        client = FakeClient()
        backend.connect = AsyncMock(return_value=client)
        from unittest.mock import patch
        with patch("ota_backend.SmpClient") as smp_type:
            smp = smp_type.return_value
            smp.start = AsyncMock()
            began = asyncio.Event()

            async def block(*_, **__):
                began.set()
                await asyncio.Event().wait()
            smp.upload = block
            from types import SimpleNamespace
            task = asyncio.create_task(backend.upgrade("explicit-target", SimpleNamespace(data=b"fixture")))
            await began.wait()
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task
            self.assertFalse(client.is_connected)

    async def test_nus_not_uploaded_when_auto_entry_disabled(self):
        backend = OtaBackend(lambda *_: None)
        client = FakeClient(loader=False)
        backend.connect = AsyncMock(return_value=client)
        with self.assertRaisesRegex(RuntimeError, "业务模式"):
            await backend.upgrade("explicit-target", None, auto_enter=False)
        self.assertFalse(client.is_connected)

    async def test_auto_entry_preserves_target_and_no_reset_option(self):
        from types import SimpleNamespace
        from unittest.mock import patch
        backend = OtaBackend(lambda *_: None)
        app, loader = FakeClient(loader=False), FakeClient()
        backend.connect = AsyncMock(return_value=app)
        backend.nus = AsyncMock()
        backend.wait_loader = AsyncMock(return_value=loader)
        with patch("ota_backend.SmpClient") as smp_type, patch("ota_backend.asyncio.sleep", new_callable=AsyncMock):
            smp = smp_type.return_value
            smp.start = AsyncMock()
            smp.upload = AsyncMock(return_value=True)
            smp.request = AsyncMock()
            result = await backend.upgrade("explicit-target", SimpleNamespace(data=b"image"), reset=False)
            backend.nus.assert_awaited_once_with(app, "OTA ENTER")
            backend.wait_loader.assert_awaited_once_with("explicit-target")
            smp.request.assert_not_awaited()
            self.assertIn("未复位", result)
        self.assertFalse(app.is_connected)
        self.assertFalse(loader.is_connected)

    async def test_reset_and_verify_app_separate_from_upload(self):
        from types import SimpleNamespace
        from unittest.mock import patch
        events = []
        backend = OtaBackend(lambda *event: events.append(event))
        loader, app = FakeClient(), FakeClient(loader=False)
        backend.connect = AsyncMock(side_effect=[loader, app])
        backend.nus = AsyncMock(return_value="CadenceSensor firmware 1.0.2\n")
        with patch("ota_backend.SmpClient") as smp_type:
            smp = smp_type.return_value
            smp.start = AsyncMock()
            smp.upload = AsyncMock(return_value=True)
            smp.request = AsyncMock()
            result = await backend.upgrade("explicit-target", SimpleNamespace(data=b"image"))
            smp.request.assert_awaited_once_with(2, 0, 5, {})
            backend.nus.assert_awaited_once_with(app, "VERSION")
            self.assertIn("已返回业务模式", result)
            self.assertIn(("phase", "核对业务启动"), events)
        self.assertFalse(loader.is_connected)
        self.assertFalse(app.is_connected)


if __name__ == "__main__":
    unittest.main()
