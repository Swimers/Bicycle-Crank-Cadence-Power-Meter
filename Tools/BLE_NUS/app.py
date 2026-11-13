"""CadenceSensor BLE/NUS text terminal for Windows."""
import asyncio
import codecs
from datetime import datetime
import queue
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

from nus_client import NusClient, SERVICE


class Terminal:
    def __init__(self, root):
        self.root = root
        self.events = queue.SimpleQueue()
        self.received = queue.Queue(maxsize=1024)
        self.dropped = 0
        self.loop = asyncio.new_event_loop()
        self.backend = NusClient(self.emit)
        self.thread = threading.Thread(target=self.run_loop, daemon=True)
        self.thread.start()
        self.devices = []
        self.connected = False
        self.busy = False
        self.closing = False
        self.decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
        self.history = bytearray()
        self.rx_bytes = 0
        self.tx_bytes = 0
        self.build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(50, self.poll)

    def run_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()
        self.loop.close()

    def emit(self, kind, data):
        if kind == "rx":
            try:
                self.received.put_nowait(data)
            except queue.Full:
                self.dropped += len(data)
        else:
            self.events.put((kind, data))

    def build_ui(self):
        self.root.title("CadenceSensor · BLE/NUS 文本调试")
        self.root.geometry("980x720")
        self.root.minsize(760, 540)
        frame = ttk.Frame(self.root, padding=12)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="BLE / NUS 文本终端", font=("Microsoft YaHei UI", 16, "bold")).pack(anchor="w")
        ttk.Label(frame, text="扫描 → 选择设备 → 连接（自动订阅 Notify） · 无需系统配对或 COM 串口").pack(anchor="w", pady=(4, 12))
        bar = ttk.Frame(frame)
        bar.pack(fill="x")
        self.scan_button = ttk.Button(bar, text="扫描设备（6秒）", command=self.scan)
        self.scan_button.pack(side="left")
        self.connect_button = ttk.Button(bar, text="连接", command=self.connect)
        self.connect_button.pack(side="left", padx=8)
        self.disconnect_button = ttk.Button(bar, text="断开", command=lambda: self.submit(self.backend.disconnect()))
        self.disconnect_button.pack(side="left")
        self.status = tk.StringVar(value="未连接")
        ttk.Label(bar, textvariable=self.status).pack(side="right")
        self.table = ttk.Treeview(frame, columns=("name", "address", "rssi", "nus"), show="headings", height=5, selectmode="browse")
        for key, title, width in (("name", "设备名称", 300), ("address", "地址", 220), ("rssi", "RSSI / dBm", 100), ("nus", "广播 NUS", 100)):
            self.table.heading(key, text=title)
            self.table.column(key, width=width)
        self.table.pack(fill="x", pady=10)
        self.table.bind("<<TreeviewSelect>>", lambda _: self.update_buttons())
        self.table.bind("<Double-1>", lambda _: self.connect() if not self.busy and not self.connected else None)
        actions = ttk.Frame(frame)
        actions.pack(fill="x", pady=(0, 6))
        ttk.Label(actions, text="MCU 接收文本（UTF-8，保留原始换行）").pack(side="left")
        ttk.Button(actions, text="保存接收数据", command=self.save).pack(side="right")
        ttk.Button(actions, text="清空", command=self.clear).pack(side="right", padx=8)
        self.output = scrolledtext.ScrolledText(frame, height=12, wrap="word", font=("Consolas", 11), state="disabled")
        self.output.pack(fill="both", expand=True)
        send = ttk.Frame(frame)
        send.pack(fill="x", pady=10)
        self.text = tk.StringVar()
        self.entry = ttk.Entry(send, textvariable=self.text, font=("Consolas", 11))
        self.entry.pack(side="left", fill="x", expand=True)
        self.entry.bind("<Return>", lambda _: self.send())
        self.ending = tk.StringVar(value="LF")
        ttk.Combobox(send, textvariable=self.ending, values=("无换行", "LF", "CRLF"), state="readonly", width=8).pack(side="left", padx=8)
        self.send_button = ttk.Button(send, text="发送 / Enter", command=self.send)
        self.send_button.pack(side="left")
        self.counts = tk.StringVar(value="RX 0 B · TX 0 B")
        ttk.Label(frame, textvariable=self.counts).pack(anchor="w")
        self.log = scrolledtext.ScrolledText(frame, height=5, wrap="word", state="disabled", font=("Consolas", 9))
        self.log.pack(fill="x", pady=(6, 0))
        self.note("已就绪。请打开 MCU 电源，并关闭手机上占用连接的 App。")
        self.update_buttons()

    def update_buttons(self):
        self.scan_button.configure(state="normal" if not self.busy and not self.connected else "disabled")
        self.connect_button.configure(state="normal" if not self.busy and not self.connected and self.table.selection() else "disabled")
        self.disconnect_button.configure(state="normal" if self.connected and not self.busy else "disabled")
        self.send_button.configure(state="normal" if self.connected and not self.busy else "disabled")

    def submit(self, coro):
        self.busy = True
        self.update_buttons()
        future = asyncio.run_coroutine_threadsafe(coro, self.loop)

        def done(result):
            try:
                result.result()
            except Exception as exc:
                self.emit("error", f"{type(exc).__name__}: {exc}")
            finally:
                self.emit("idle", None)
        future.add_done_callback(done)

    def scan(self):
        self.status.set("正在扫描…")
        self.note("开始扫描附近 BLE 设备，NUS / CadenceSensor 优先显示。")

        async def scan_task():
            found = await self.backend.scan()
            self.emit("devices", found)
        self.submit(scan_task())

    def connect(self):
        if self.busy or self.connected or not self.table.selection():
            return
        device = self.devices[int(self.table.selection()[0])][0]
        self.status.set("正在连接并订阅…")
        self.note(f"连接 {device.address}")
        self.submit(self.backend.connect(device))

    def send(self):
        if not self.connected or self.busy or self.closing:
            return
        ending = {"无换行": "", "LF": "\n", "CRLF": "\r\n"}[self.ending.get()]
        data = (self.text.get() + ending).encode("utf-8")
        if not data:
            return
        if len(data) > 512:
            messagebox.showwarning("文本过长", "单次发送最多 512 个 UTF-8 字节，请拆分发送。")
            return
        self.submit(self.backend.send(data))

    def append(self, widget, text, lines):
        widget.configure(state="normal")
        widget.insert("end", text)
        # Bound the displayed history as well as the raw byte history.
        count = int(widget.index("end-1c").split(".")[0])
        if count > lines:
            widget.delete("1.0", f"{count - lines}.0")
        if len(widget.get("1.0", "end-1c")) > 200000:
            widget.delete("1.0", "end-100000c")
        widget.see("end")
        widget.configure(state="disabled")

    def note(self, text):
        self.append(self.log, f"[{datetime.now():%H:%M:%S}] {text}\n", 200)

    def poll(self):
        if self.closing:
            return
        for _ in range(200):
            try:
                kind, data = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == "devices":
                self.devices = data
                self.table.delete(*self.table.get_children())
                for i, (device, adv) in enumerate(data):
                    self.table.insert("", "end", iid=str(i), values=(adv.local_name or device.name or "(未命名)", device.address, adv.rssi, "是" if SERVICE in [u.lower() for u in adv.service_uuids] else "否"))
                if data:
                    self.table.selection_set("0")
                self.status.set(f"扫描完成：{len(data)} 个设备")
            elif kind == "session":
                self.decoder.reset()
            elif kind == "connected":
                self.connected = True
                self.status.set("已连接 · Notify 已订阅")
                self.note(f"{data}：NUS 已就绪，可以接收 printk 或发送文本。")
            elif kind == "disconnected":
                self.connected = False
                self.status.set("未连接")
                self.note(data)
            elif kind == "sent":
                self.tx_bytes += len(data)
                self.note(f"TX {len(data)} B: {data.decode('utf-8')!r}")
            elif kind == "error":
                self.note("错误：" + data)
                self.status.set("操作失败，请查看日志")
            elif kind == "idle":
                self.busy = False
            self.update_buttons()
        chunks = []
        for _ in range(200):
            try:
                chunks.append(self.received.get_nowait())
            except queue.Empty:
                break
        if chunks:
            raw = b"".join(chunks)
            self.rx_bytes += len(raw)
            self.history.extend(raw)
            if len(self.history) > 2 * 1024 * 1024:
                del self.history[:-2 * 1024 * 1024]
            self.append(self.output, self.decoder.decode(raw), 5000)
        self.counts.set(f"RX {self.rx_bytes} B · TX {self.tx_bytes} B · 接收队列丢弃 {self.dropped} B · 保存保留最近 2 MiB")
        self.root.after(50, self.poll)

    def clear(self):
        self.output.configure(state="normal")
        self.output.delete("1.0", "end")
        self.output.configure(state="disabled")
        self.history.clear()
        self.decoder.reset()
        self.rx_bytes = self.tx_bytes = self.dropped = 0

    def save(self):
        path = filedialog.asksaveasfilename(defaultextension=".log", filetypes=[("原始 UTF-8 数据", "*.log"), ("所有文件", "*.*")])
        if path:
            try:
                with open(path, "wb") as stream:
                    stream.write(self.history)
                self.note(f"已保存 {len(self.history)} B 到 {path}")
            except OSError as exc:
                messagebox.showerror("保存失败", str(exc))

    def close(self):
        if self.closing:
            return
        self.closing = True
        self.busy = True
        self.update_buttons()
        self.status.set("正在断开并退出…")

        async def shutdown():
            pending = [task for task in asyncio.all_tasks() if task is not asyncio.current_task()]
            for task in pending:
                task.cancel()
            await asyncio.gather(*pending, return_exceptions=True)
            try:
                await self.backend.disconnect()
            finally:
                self.loop.call_soon(self.loop.stop)
        asyncio.run_coroutine_threadsafe(shutdown(), self.loop)
        self.wait_closed(0)

    def wait_closed(self, count):
        if not self.thread.is_alive() or count >= 120:
            self.root.destroy()
        else:
            self.root.after(100, self.wait_closed, count + 1)


if __name__ == "__main__":
    root = tk.Tk()
    Terminal(root)
    root.mainloop()
