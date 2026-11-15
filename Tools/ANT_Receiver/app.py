import asyncio
import queue
import struct
import threading
import tkinter as tk
from tkinter import ttk
from datetime import datetime
from bleak import BleakClient, BleakScanner
from decode import Decoder

CHARS = {'00002a37-0000-1000-8000-00805f9b34fb': 'hr',
         '00002a5b-0000-1000-8000-00805f9b34fb': 'csc',
         '00002a63-0000-1000-8000-00805f9b34fb': 'power'}


class App:
    def __init__(self, root):
        self.root = root
        self.events = queue.Queue(maxsize=5000)
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self.run_loop, daemon=True)
        self.thread.start()
        self.decoder = Decoder()
        self.devices, self.clients = {}, {}
        self.ant_node = None
        self.ant_thread = None
        self.stop_requested = threading.Event()
        self.closing = False
        root.title('BLE / ANT+ 多设备监测')
        root.geometry('1060x650')
        f = ttk.Frame(root, padding=12)
        f.pack(fill='both', expand=True)
        bar = ttk.Frame(f)
        bar.pack(fill='x')
        self.scan_btn = ttk.Button(bar, text='扫描 BLE（6秒）', command=self.scan)
        self.scan_btn.pack(side='left')
        ttk.Button(bar, text='连接所选 BLE', command=self.connect).pack(side='left', padx=6)
        ttk.Button(bar, text='断开所选 BLE', command=self.disconnect).pack(side='left')
        ttk.Button(bar, text='启动 ANT+ 接收', command=self.start_ant).pack(side='left', padx=6)
        ttk.Button(bar, text='停止 ANT+', command=self.stop_ant).pack(side='left')
        ttk.Label(f, text='BLE：选择设备后连接；ANT+：需要 ANT USB 接收器，启动后监听广播。').pack(anchor='w', pady=8)
        cols = ('protocol', 'device', 'name', 'state', 'hr', 'cadence', 'power', 'updated')
        self.table = ttk.Treeview(f, columns=cols, show='headings', selectmode='browse')
        for c, label, width in zip(cols, ('协议', '地址 / 设备号', '名称 / 类型', '状态', '心率 bpm', '踏频 rpm', '功率 W', '最后更新'), (70, 200, 220, 115, 90, 90, 80, 100)):
            self.table.heading(c, text=label)
            self.table.column(c, width=width)
        self.table.pack(fill='both', expand=True)
        ttk.Label(f, text='原始数据：BLE 为 GATT 特征值，ANT+ 为广播数据页').pack(anchor='w', pady=(8, 2))
        self.log = tk.Text(f, height=9, state='disabled', wrap='word')
        self.log.pack(fill='x')
        self.status = tk.StringVar(value='就绪')
        ttk.Label(f, textvariable=self.status).pack(anchor='w', pady=4)
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(50, self.poll)

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
                for service in client.services:
                    for char in service.characteristics:
                        kind = CHARS.get(char.uuid.lower())
                        if kind and 'notify' in char.properties:
                            await client.start_notify(char, lambda _, data, k=key, t=kind: self.emit('measurement', 'BLE', k, t, bytes(data)))
                            count += 1
                if not count:
                    raise RuntimeError('未发现心率、踏频或功率通知特征')
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
            self.emit('status', f'ANT+ 启动失败：{type(e).__name__}: {e}。请检查接收器和驱动。')
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

    def row(self, key, **changes):
        if not self.table.exists(key):
            self.table.insert('', 'end', iid=key, values=('', '', '', '', '', '', '', ''))
        for name, value in changes.items():
            self.table.set(key, name, value)

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
                elif kind == 'measurement':
                    protocol, key, typ, data = args
                    values = self.decoder.ble(key, typ, data) if protocol == 'BLE' else self.decoder.ant(key, typ, data)
                    self.row(key, updated=datetime.now().strftime('%H:%M:%S'), **{k: f'{v:.1f}' if isinstance(v, float) else str(v) for k, v in values.items()})
                    self.log.config(state='normal')
                    self.log.insert('end', f'{datetime.now():%H:%M:%S} {key} {typ} {data.hex(" ")}\n')
                    if int(self.log.index('end-1c').split('.')[0]) > 500:
                        self.log.delete('1.0', '101.0')
                    self.log.see('end')
                    self.log.config(state='disabled')
            except (ValueError, IndexError, struct.error) as e:
                self.status.set(f'数据解析失败：{e}')
        self.root.after(50, self.poll)

    def close(self):
        self.closing = True
        self.stop_ant()
        async def shutdown():
            tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            await asyncio.gather(*(c.disconnect() for c in list(self.clients.values())), return_exceptions=True)
            self.loop.call_soon(self.loop.stop)
        asyncio.run_coroutine_threadsafe(shutdown(), self.loop)
        def wait(count=0):
            if (not self.thread.is_alive() and (not self.ant_thread or not self.ant_thread.is_alive())) or count >= 100:
                self.root.destroy()
            else:
                self.root.after(100, wait, count + 1)
        wait()


if __name__ == '__main__':
    root = tk.Tk()
    App(root)
    root.mainloop()
