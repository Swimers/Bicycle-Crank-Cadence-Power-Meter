import struct


class Decoder:
    def __init__(self):
        self.previous = {}

    def crank(self, key, rev, stamp):
        old = self.previous.get(key)
        if old is None:
            self.previous[key] = (rev, stamp)
            return {}
        dr = (rev - old[0]) & 0xffff
        dt = (stamp - old[1]) & 0xffff
        if not dr:
            return {}
        self.previous[key] = (rev, stamp)
        return {'cadence': 60 * 1024 * dr / dt} if dt else {}

    def ble(self, key, kind, data):
        data = bytes(data)
        if kind == 'hr':
            return {'hr': struct.unpack_from('<H', data, 1)[0] if data[0] & 1 else data[1]}
        if kind == 'power':
            flags, power = struct.unpack_from('<Hh', data)
            out = {'power': power}
            offset = 4 + (1 if flags & 1 else 0) + (2 if flags & 4 else 0) + (6 if flags & 16 else 0)
            if flags & 32:
                rev, stamp = struct.unpack_from('<HH', data, offset)
                out.update(self.crank((key, kind), rev, stamp))
            return out
        if kind == 'csc' and data[0] & 2:
            rev, stamp = struct.unpack_from('<HH', data, 1 + (6 if data[0] & 1 else 0))
            return self.crank((key, kind), rev, stamp)
        return {}

    def ant(self, key, typ, data):
        d = bytes(data[:8])
        if len(d) != 8:
            raise ValueError('ANT 数据页长度错误')
        if typ == 120:
            return {'hr': d[7]} if (d[0] & 0x7f) <= 7 else {}
        if typ == 11 and d[0] == 0x10:
            out = {'power': int.from_bytes(d[6:8], 'little')}
            if d[3] != 255:
                out['cadence'] = d[3]
            return out
        if typ == 121 or (typ == 122 and (d[0] & 0x7f) <= 5):
            stamp, rev = struct.unpack_from('<HH', d, 0 if typ == 121 else 4)
            return self.crank((key, 'ant'), rev, stamp)
        return {}
