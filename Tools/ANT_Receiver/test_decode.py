import struct
import unittest
from decode import Decoder


class DecodeTests(unittest.TestCase):
    def test_heart_rate_formats(self):
        d = Decoder()
        self.assertEqual(d.ble('x', 'hr', b'\x00\x48'), {'hr': 72})
        self.assertEqual(d.ble('x', 'hr', b'\x01\x2c\x01'), {'hr': 300})
        self.assertEqual(d.ant('a', 120, b'\x00\x00\x00\x00\x00\x00\x00\x48'), {'hr': 72})

    def test_rollover_and_independent_devices(self):
        d = Decoder()
        self.assertEqual(d.ble('a', 'csc', b'\x02' + struct.pack('<HH', 65535, 65000)), {})
        rate = d.ble('a', 'csc', b'\x02' + struct.pack('<HH', 0, 488))['cadence']
        self.assertAlmostEqual(rate, 60)
        self.assertEqual(d.ble('b', 'csc', b'\x02' + struct.pack('<HH', 0, 488)), {})

    def test_power_flags_and_ant_page(self):
        d = Decoder()
        self.assertEqual(d.ble('a', 'power', struct.pack('<Hh', 0, -20)), {'power': -20})
        self.assertEqual(d.ant('a', 11, bytes([0x10, 1, 255, 88, 0, 0, 0xfa, 0])), {'power': 250, 'cadence': 88})
        self.assertEqual(d.ant('a', 11, bytes([0x50, 0, 0, 0, 0, 0, 0, 0])), {})


if __name__ == '__main__':
    unittest.main()
