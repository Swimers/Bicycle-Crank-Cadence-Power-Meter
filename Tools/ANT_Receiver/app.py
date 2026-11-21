import asyncio
import json
import math
import queue
import struct
import threading
import time
import tkinter as tk
from collections import deque
from tkinter import ttk
from datetime import datetime
from bleak import BleakClient, BleakScanner
from decode import Decoder

CHARS = {'00002a37-0000-1000-8000-00805f9b34fb': 'hr',
         '00002a5b-0000-1000-8000-00805f9b34fb': 'csc',
         '00002a63-0000-1000-8000-00805f9b34fb': 'power'}
BATTERY_LEVEL_UUID = '00002a19-0000-1000-8000-00805f9b34fb'


class App:
    def __init__(self, root):
        self.root = root
        self.events = queue.Queue(maxsize=5000)
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self.run_loop, daemon=True)
        self.thread.start()
        self.decoder = Decoder()
        self.devices, self.clients = {}, {}
        self.cadence_history = {}
        self.ant_node = None
        self.ant_thread = None
        self.serial_thread = None
        self.serial_stop = threading.Event()
        self.stop_requested = threading.Event()
        self.closing = False
        root.title('BLE / ANT+ 多设备监测')
        root.geometry('1060x820')
        f = ttk.Frame(root, padding=12)
        f.pack(fill='both', expand=True)
        bar = ttk.Frame(f)
        bar.pack(fill='x')
        self.scan_btn = ttk.Button(bar, text='扫描 BLE（6秒）', command=self.scan)
        self.scan_btn.pack(side='left')
        ttk.Button(bar, text='连接所选 BLE', command=self.connect).pack(side='left', padx=6)
        ttk.Button(bar, text='断开所选 BLE', command=self.disconnect).pack(side='left')
        ttk.Button(bar, text='启动 ANT USB 棒', command=self.start_ant).pack(side='left', padx=6)
        ttk.Button(bar, text='停止 ANT USB 棒', command=self.stop_ant).pack(side='left')
        serial_bar = ttk.Frame(f)
        serial_bar.pack(fill='x', pady=(8, 0))
        ttk.Label(serial_bar, text='nRF52840 串口').pack(side='left')
        self.serial_port = ttk.Combobox(serial_bar, width=20)
        self.serial_port.pack(side='left', padx=6)
        ttk.Button(serial_bar, text='刷新串口', command=self.refresh_ports).pack(side='left')
        ttk.Button(serial_bar, text='连接串口', command=self.start_serial).pack(side='left', padx=6)
        ttk.Button(serial_bar, text='断开串口', command=self.stop_serial).pack(side='left')
        self.refresh_ports()
        ttk.Label(f, text='nRF52840 使用上方串口连接；ANT USB 棒按钮仅适用于独立 ANT USB 接收器。').pack(anchor='w', pady=8)
        cols = ('protocol', 'device', 'name', 'state', 'hr', 'cadence', 'power', 'battery', 'updated')
        self.table = ttk.Treeview(f, columns=cols, show='headings', selectmode='browse')
        for c, label, width in zip(cols, ('协议', '地址 / 设备号', '名称 / 类型', '状态', '心率 bpm', '踏频 rpm', '功率 W', '电量', '最后更新'), (70, 180, 180, 110, 85, 85, 75, 65, 90)):
            self.table.heading(c, text=label)
            self.table.column(c, width=width)
        self.table.pack(fill='both', expand=True)
        ttk.Label(f, text='所选设备踏频曲线（最近 60 秒，rpm）').pack(anchor='w', pady=(8, 2))
        self.chart = tk.Canvas(f, height=190, background='white', highlightthickness=1, highlightbackground='#cccccc')
        self.chart.pack(fill='x')
        self.table.bind('<<TreeviewSelect>>', lambda event: self.draw_cadence())
        self.chart.bind('<Configure>', lambda event: self.draw_cadence())
        ttk.Label(f, text='原始数据：BLE 为 GATT 特征值，ANT+ 为广播数据页').pack(anchor='w', pady=(8, 2))
        self.log = tk.Text(f, height=9, state='disabled', wrap='word')
        self.log.pack(fill='x')
        self.status = tk.StringVar(value='就绪')
        ttk.Label(f, textvariable=self.status).pack(anchor='w', pady=4)
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(50, self.poll)
        root.after(1000, self.refresh_chart)

    def run_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()
        self.loop.close()

    def emit(self, kind, *args):
        try:
            self.events.put_nowait((kind, args))
        except queue.Full:
            pass

    def submit(self, coro):
        future = asyncio.run_coroutine_threadsafe(coro, self.loop)
        def done(f):
            try:
                f.result()
            except Exception as e:
                self.emit('status', f'{type(e).__name__}: {e}')
        future.add_done_callback(done)

    def scan(self):
        self.scan_btn.config(state='disabled')
        async def work():
            try:
                self.emit('status', '正在扫描 BLE…')
                async with BleakScanner() as scanner:
                    await asyncio.sleep(6)
                for device, adv in scanner.discovered_devices_and_advertisement_data.values():
                    self.emit('ble_device', device, adv.local_name or device.name or '未命名')
                self.emit('status', 'BLE 扫描完成，请选择设备连接。')
            finally:
                self.emit('scan_done')
        self.submit(work())

    def selected(self):
        selected = self.table.selection()
        return selected[0] if selected else None

    def connect(self):
        key = self.selected()
        if not key or not key.startswith('BLE:') or key not in self.devices:
            self.status.set('请选择扫描到的 BLE 设备。')
            return
        async def work():
            if key in self.clients:
                return
            def disconnected(client):
                if self.clients.get(key) is client:
                    self.clients.pop(key, None)
                    self.emit('state', key, '已断开')
            client = BleakClient(self.devices[key], pair=False, disconnected_callback=disconnected)
            self.clients[key] = client
            try:
                self.emit('state', key, '连接中')
                await client.connect()
                count = 0
                battery_char = None
                for service in client.services:
                    for char in service.characteristics:
                        if char.uuid.lower() == BATTERY_LEVEL_UUID:
                            battery_char = char
                        kind = CHARS.get(char.uuid.lower())
                        if kind and 'notify' in char.properties:
                            await client.start_notify(char, lambda _, data, k=key, t=kind: self.emit('measurement', 'BLE', k, t, bytes(data)))
                            count += 1
                if not count:
                    raise RuntimeError('未发现心率、踏频或功率通知特征')
                if battery_char:
                    battery_notified = False
                    def battery_changed(_, data):
                        nonlocal battery_notified
                        battery_notified = True
                        self.emit('battery', key, bytes(data))
                    if 'notify' in battery_char.properties:
                        try:
                            await client.start_notify(battery_char, battery_changed)
                        except Exception as e:
                            self.emit('status', f'电量通知订阅失败：{e}')
                    if 'read' in battery_char.properties:
                        try:
                            data = await client.read_gatt_char(battery_char)
                            if not battery_notified:
                                self.emit('battery', key, bytes(data))
                        except Exception as e:
                            self.emit('status', f'电量读取失败：{e}')
                self.emit('state', key, f'已连接 ({count}项)')
            except BaseException:
                self.clients.pop(key, None)
                await client.disconnect()
                self.emit('state', key, '连接失败')
                raise
        self.submit(work())

    def disconnect(self):
        key = self.selected()
        async def work():
            client = self.clients.pop(key, None)
            if client:
                await client.disconnect()
                self.emit('state', key, '已断开')
        self.submit(work())

    def start_ant(self):
        if self.ant_thread and self.ant_thread.is_alive():
            self.status.set('ANT+ 已在运行。')
            return
        self.stop_requested.clear()
        self.ant_thread = threading.Thread(target=self.ant_run, daemon=True)
        self.ant_thread.start()

    def ant_run(self):
        node = None
        failed = False
        try:
            import functools
            import libusb_package
            import usb.core
            backend = libusb_package.get_libusb1_backend()
            if backend is None:
                raise RuntimeError('无法加载 libusb')
            if not getattr(usb.core.find, '_ant_backend_configured', False):
                finder = functools.partial(usb.core.find, backend=backend)
                finder._ant_backend_configured = True
                usb.core.find = finder
            from openant.easy.node import Node
            from openant.easy.channel import Channel
            from openant.devices import ANTPLUS_NETWORK_KEY
            node = Node()
            self.ant_node = node
            node.set_network_key(0, ANTPLUS_NETWORK_KEY)
            channel = node.new_channel(Channel.Type.BIDIRECTIONAL_RECEIVE)
            channel.set_id(0, 0, 0)
            channel.set_rf_freq(57)
            channel.enable_extended_messages(1)
            def receive(data):
                if len(data) >= 13 and data[8] & 0x80:
                    number = data[9] | data[10] << 8
                    typ, trans = data[11] & 0x7f, data[12]
                    key = f'ANT+:{number}:{typ}:{trans}'
                    self.emit('ant_device', key, number, typ, trans)
                    self.emit('measurement', 'ANT+', key, typ, bytes(data[:8]))
            channel.on_broadcast_data = receive
            channel.on_acknowledge_data = receive
            channel.open_rx_scan_mode()
            if not self.stop_requested.is_set():
                self.emit('status', 'ANT+ 连续接收中。')
                node.start()
        except Exception as e:
            failed = True
            self.emit('status', f'ANT USB 棒启动失败：{type(e).__name__}: {e}。nRF52840 请使用串口连接。')
        finally:
            self.ant_node = None
            if node:
                try:
                    node.stop()
                except Exception:
                    pass
            if not failed:
                self.emit('status', 'ANT+ 已停止。')

    def stop_ant(self):
        self.stop_requested.set()
        node = self.ant_node
        if node:
            threading.Thread(target=node.stop, daemon=True).start()

    def refresh_ports(self):
        try:
            from serial.tools import list_ports
            self.serial_port['values'] = [p.device for p in list_ports.comports()]
        except ImportError:
            self.serial_port['values'] = []

    def start_serial(self):
        if self.serial_thread and self.serial_thread.is_alive():
            self.status.set('串口已连接或正在连接。')
            return
        port = self.serial_port.get().strip()
        if not port:
            self.status.set('请选择或输入 nRF52840 串口。')
            return
        self.serial_stop.clear()
        self.serial_thread = threading.Thread(target=self.serial_run, args=(port,), daemon=True)
        self.serial_thread.start()

    def serial_run(self, port):
        try:
            import serial
            with serial.Serial(port, 115200, timeout=0.2, write_timeout=1) as conn:
                conn.write(b'STATUS\n')
                self.emit('status', f'{port} 已打开，等待 ANT 状态。')
                pending = b''
                discard = False
                while not self.serial_stop.is_set():
                    fragment = conn.readline(1024)
                    if not fragment:
                        continue
                    if discard:
                        discard = not fragment.endswith(b'\n')
                        continue
                    pending += fragment
                    if len(pending) > 1024:
                        discard = not fragment.endswith(b'\n')
                        pending = b''
                        continue
                    if not fragment.endswith(b'\n'):
                        continue
                    line, pending = pending, b''
                    try:
                        message = json.loads(line.decode('utf-8'))
                        if not isinstance(message, dict) or message.get('v') != 1:
                            continue
                        if message.get('type') == 'status':
                            error = message.get('ant_start_error')
                            self.emit('status', 'nRF52840 ANT+ 已启动，等待广播。' if error == 0 else f'nRF52840 ANT+ 启动失败：{error}')
                        elif message.get('type') == 'measurement':
                            number = int(message['device_id'])
                            typ = int(message['device_type'])
                            trans = int(message['transmission_type'])
                            raw = bytes.fromhex(message['raw'])
                            if not (0 <= number <= 65535 and 0 <= typ <= 127 and 0 <= trans <= 255 and len(raw) == 8):
                                continue
                            values = {}
                            for field, column in [('heart_rate_bpm', 'hr'), ('cadence_rpm', 'cadence'), ('power_w', 'power')]:
                                value = message.get(field)
                                if isinstance(value, (int, float)) and not isinstance(value, bool) and 0 <= value <= 65535:
                                    values[column] = value
                            key = f'CDC:{number}:{typ}:{trans}'
                            self.emit('serial_measurement', key, number, typ, trans, raw, values)
                    except (ValueError, TypeError, KeyError, UnicodeError):
                        continue
        except Exception as e:
            self.emit('status', f'串口连接失败：{type(e).__name__}: {e}')
        finally:
            self.emit('serial_closed')

    def stop_serial(self):
        self.serial_stop.set()

    def row(self, key, **changes):
        if not self.table.exists(key):
            self.table.insert('', 'end', iid=key, values=('',) * 9)
        for name, value in changes.items():
            self.table.set(key, name, value)

    def record_cadence(self, key, values):
        value = values.get('cadence')
        if not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
            return
        now = time.monotonic()
        points = self.cadence_history.setdefault(key, deque(maxlen=3600))
        points.append((now, value))
        while points and points[0][0] < now - 60:
            points.popleft()

    def refresh_chart(self):
        if self.closing:
            return
        self.draw_cadence()
        self.root.after(1000, self.refresh_chart)

    def draw_cadence(self):
        canvas = self.chart
        canvas.delete('all')
        width, height = canvas.winfo_width(), canvas.winfo_height()
        if width < 80 or height < 60:
            return
        left, right, top, bottom = 48, width - 15, 12, height - 28
        now = time.monotonic()
        key = self.selected()
        points = self.cadence_history.get(key, ())
        points = [(t, v) for t, v in points if now - 60 <= t <= now]
        ceiling = max(120, math.ceil(max((v for _, v in points), default=0) / 20) * 20)
        for i in range(4):
            y = bottom - (bottom - top) * i / 3
            canvas.create_line(left, y, right, y, fill='#e5e7eb')
            canvas.create_text(left - 6, y, text=f'{ceiling * i / 3:.0f}', anchor='e', fill='#555555')
        canvas.create_text(left, height - 8, text='-60 s', anchor='w', fill='#555555')
        canvas.create_text(right, height - 8, text='现在', anchor='e', fill='#555555')
        if not points:
            canvas.create_text((left + right) / 2, (top + bottom) / 2,
                               text='请选择设备，等待踏频数据', fill='#777777')
            return
        previous = None
        for stamp, value in points:
            x = right - (now - stamp) / 60 * (right - left)
            y = bottom - value / ceiling * (bottom - top)
            if previous is not None and stamp - previous[0] <= 5:
                canvas.create_line(previous[1], previous[2], x, y, fill='#0879c9', width=2)
            else:
                canvas.create_oval(x - 2, y - 2, x + 2, y + 2, fill='#0879c9', outline='')
            previous = (stamp, x, y)

    def poll(self):
        if self.closing:
            return
        for _ in range(300):
            try:
                kind, args = self.events.get_nowait()
            except queue.Empty:
                break
            try:
                if kind == 'status':
                    self.status.set(args[0])
                elif kind == 'scan_done':
                    self.scan_btn.config(state='normal')
                elif kind == 'ble_device':
                    device, name = args
                    key = 'BLE:' + device.address
                    self.devices[key] = device
                    state = self.table.set(key, 'state') if self.table.exists(key) else '已发现'
                    self.row(key, protocol='BLE', device=device.address, name=name, state=state)
                elif kind == 'ant_device':
                    key, number, typ, trans = args
                    self.row(key, protocol='ANT+', device=f'{number} / {trans}', name={11:'功率计', 120:'心率', 121:'速度/踏频', 122:'踏频'}.get(typ, f'类型 {typ}'), state='接收中')
                elif kind == 'state':
                    self.row(args[0], state=args[1])
                    if args[1] in ('已断开', '连接失败', '连接中'):
                        self.decoder.reset_device(args[0])
                        self.row(args[0], hr='', cadence='', power='', battery='')
                elif kind == 'battery':
                    key, data = args
                    if len(data) == 1 and data[0] <= 100:
                        self.row(key, battery=f'{data[0]}%')
                    else:
                        self.row(key, battery='--')
                elif kind == 'measurement':
                    protocol, key, typ, data = args
                    values = self.decoder.ble(key, typ, data) if protocol == 'BLE' else self.decoder.ant(key, typ, data)
                    self.record_cadence(key, values)
                    self.row(key, updated=datetime.now().strftime('%H:%M:%S'), **{k: f'{v:.1f}' if isinstance(v, float) else str(v) for k, v in values.items()})
                    self.log.config(state='normal')
                    self.log.insert('end', f'{datetime.now():%H:%M:%S} {key} {typ} {data.hex(" ")}\n')
                    if int(self.log.index('end-1c').split('.')[0]) > 500:
                        self.log.delete('1.0', '101.0')
                    self.log.see('end')
                    self.log.config(state='disabled')
                elif kind == 'serial_measurement':
                    key, number, typ, trans, data, values = args
                    self.record_cadence(key, values)
                    self.row(key, protocol='ANT+ 串口', device=f'{number} / {trans}', name={11:'功率计', 120:'心率', 121:'速度/踏频', 122:'踏频'}.get(typ, f'类型 {typ}'), state='接收中', updated=datetime.now().strftime('%H:%M:%S'), **{k: f'{v:.1f}' if isinstance(v, float) else str(v) for k, v in values.items()})
                    self.log.config(state='normal')
                    self.log.insert('end', f'{datetime.now():%H:%M:%S} {key} {typ} {data.hex(" ")}\n')
                    if int(self.log.index('end-1c').split('.')[0]) > 500:
                        self.log.delete('1.0', '101.0')
                    self.log.see('end')
                    self.log.config(state='disabled')
                elif kind == 'serial_closed':
                    for key in self.table.get_children():
                        if key.startswith('CDC:'):
                            self.row(key, state='已断开')
            except (ValueError, IndexError, struct.error) as e:
                self.status.set(f'数据解析失败：{e}')
        self.root.after(50, self.poll)

    def close(self):
        self.closing = True
        self.stop_ant()
        self.stop_serial()
        async def shutdown():
            tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            await asyncio.gather(*(c.disconnect() for c in list(self.clients.values())), return_exceptions=True)
            self.loop.call_soon(self.loop.stop)
        asyncio.run_coroutine_threadsafe(shutdown(), self.loop)
        def wait(count=0):
            if (not self.thread.is_alive() and (not self.ant_thread or not self.ant_thread.is_alive()) and (not self.serial_thread or not self.serial_thread.is_alive())) or count >= 100:
                self.root.destroy()
            else:
                self.root.after(100, wait, count + 1)
        wait()


if __name__ == '__main__':
    root = tk.Tk()
    App(root)
    root.mainloop()
