"""Transport tests without a Bluetooth peripheral; run python -m unittest -v."""
import unittest
from unittest.mock import patch

from nus_client import NusClient, SERVICE, RX, TX


class Characteristic:
    def __init__(self, properties):
        self.properties = properties


class Service:
    def get_characteristic(self, uuid):
        return Characteristic(["write"]) if uuid == RX else Characteristic(["notify"])


class FakeClient:
    instances = []

    def __init__(self, device, **kwargs):
        self.instances.append(self)
        self.services = self
        self.is_connected = False
        self.writes = []
        self.kwargs = kwargs
        self.notify = None

    async def connect(self):
        self.is_connected = True

    def get_service(self, uuid):
        return Service() if uuid == SERVICE else None

    async def disconnect(self):
        self.is_connected = False

    async def start_notify(self, characteristic, callback):
        self.notify = callback

    async def write_gatt_char(self, characteristic, data, response):
        self.writes.append((bytes(data), response))


class TransportTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.events = []
        self.backend = NusClient(lambda kind, value: self.events.append((kind, value)))
        self.device = type("Device", (), {"address": "00:11:22:33:44:55"})()

    async def test_utf8_chunking_and_notifications(self):
        with patch("nus_client.BleakClient", FakeClient):
            await self.backend.connect(self.device)
            client = self.backend.client
            self.assertIsNotNone(client.notify)
            self.assertFalse(client.kwargs["pair"])
            data = ("踏频调试数据" * 8 + "\r\n").encode()
            await self.backend.send(data)
            self.assertEqual(b"".join(chunk for chunk, _ in client.writes), data)
            self.assertTrue(all(len(chunk) <= 20 and response for chunk, response in client.writes))
            client.notify(None, bytearray(b"MCU printk\n"))
            self.assertIn(("rx", b"MCU printk\n"), self.events)
            await self.backend.disconnect()
            self.assertIsNone(self.backend.client)
            self.assertFalse(client.is_connected)

    async def test_missing_service_cleans_connection(self):
        with patch("nus_client.BleakClient", FakeClient), patch.object(FakeClient, "get_service", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "NUS"):
                await self.backend.connect(self.device)
            self.assertIsNone(self.backend.client)
            self.assertFalse(FakeClient.instances[-1].is_connected)

    async def test_no_send_after_remote_disconnect(self):
        with patch("nus_client.BleakClient", FakeClient):
            await self.backend.connect(self.device)
            self.backend._disconnected(self.backend.client)
            with self.assertRaisesRegex(RuntimeError, "连接"):
                await self.backend.send(b"test")

    async def test_partial_send_error_propagates(self):
        with patch("nus_client.BleakClient", FakeClient):
            await self.backend.connect(self.device)
            with patch.object(self.backend.client, "write_gatt_char", side_effect=OSError("radio lost")):
                with self.assertRaises(OSError):
                    await self.backend.send(b"text")
            self.assertFalse(any(kind == "sent" for kind, _ in self.events))
            await self.backend.disconnect()


if __name__ == "__main__":
    unittest.main()
