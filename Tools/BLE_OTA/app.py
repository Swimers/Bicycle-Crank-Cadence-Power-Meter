"""Native BLE OTA control panel. Run with Python 3.10+ and Tk."""
import asyncio
from datetime import datetime
import queue
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

from ota_backend import OtaBackend, load_firmware


class OtaWindow:
    def __init__(self, root):
        self.root = root
        self.events = queue.SimpleQueue()
        self.backend = OtaBackend(self.emit)
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self.run_loop, daemon=True)
        self.thread.start()
        self.busy = False
        self.closing = False
        self.future = None
        self.firmware = None
        self.address = tk.StringVar()
        self.phase = tk.StringVar(value="就绪 · 尚未连接")
        self.file = tk.StringVar(value="请选择 MCUboot 签名业务固件 (.bin)")
        self.info = tk.StringVar(value="版本 —     大小 —     SHA-256 —")
        self.progress_text = tk.StringVar(value="0% · 等待上传")
        self.auto_enter = tk.BooleanVar(value=True)
        self.reset = tk.BooleanVar(value=True)
        self.build_ui()
        self.address.trace_add("write", lambda *_: self.buttons())
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(80, self.poll)

    def run_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()
        self.loop.close()

    def emit(self, kind, data):
        self.events.put((kind, data))

    def build_ui(self):
        self.root.title("CadenceSensor · BLE OTA 固件更新")
        self.root.geometry("1040x800")
        self.root.minsize(860, 700)
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("TFrame", background="#f2f5f9")
        style.configure("TLabel", background="#f2f5f9", foreground="#223047", font=("Microsoft YaHei UI", 10))
        style.configure("TButton", font=("Microsoft YaHei UI", 10), padding=(12, 6))
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 21, "bold"))
        style.configure("Heading.TLabel", font=("Microsoft YaHei UI", 11, "bold"))
        style.configure("Accent.TButton", background="#2065d1", foreground="white")
        style.map("Accent.TButton", background=[("active", "#174ca4"), ("disabled", "#a8b8d0")])
        style.configure("Treeview", rowheight=29, font=("Microsoft YaHei UI", 10))
        style.configure("Treeview.Heading", font=("Microsoft YaHei UI", 10, "bold"))
        panel = ttk.Frame(self.root, padding=22)
        panel.pack(fill="both", expand=True)
        header = ttk.Frame(panel)
        header.pack(fill="x")
        ttk.Label(header, text="固件更新", style="Title.TLabel").pack(side="left")
        ttk.Label(header, text="CADENCESENSOR  /  BLE OTA", foreground="#536e96").pack(side="right", pady=10)
        ttk.Label(panel, textvariable=self.phase).pack(anchor="w", pady=(4, 16))
        ttk.Label(panel, text="01  选择目标设备", style="Heading.TLabel").pack(anchor="w")
        row = ttk.Frame(panel)
        row.pack(fill="x", pady=8)
        self.scan_button = ttk.Button(row, text="扫描设备 · 6秒", command=self.scan)
        self.scan_button.pack(side="left")
        ttk.Label(row, text="目标地址").pack(side="left", padx=(16, 8))
        self.address_entry = ttk.Entry(row, textvariable=self.address, width=25)
        self.address_entry.pack(side="left", fill="x", expand=True)
        self.query_button = ttk.Button(row, text="查询版本 / 状态", command=lambda: self.action("query"))
        self.query_button.pack(side="left", padx=(8, 0))
        self.table = ttk.Treeview(panel, columns=("name", "address", "rssi", "mode"), show="headings", height=4, selectmode="browse")
        for key, label, width in (("name", "设备名称", 250), ("address", "地址", 230), ("rssi", "信号 dBm", 95), ("mode", "广播模式", 180)):
            self.table.heading(key, text=label)
            self.table.column(key, width=width, minwidth=70)
        self.table.pack(fill="x")
        self.table.bind("<<TreeviewSelect>>", self.select_device)
        row = ttk.Frame(panel)
        row.pack(fill="x", pady=(8, 16))
        self.enter_button = ttk.Button(row, text="进入 Loader", command=lambda: self.action("enter"))
        self.enter_button.pack(side="left")
        self.reset_button = ttk.Button(row, text="Loader 复位", command=lambda: self.action("reset"))
        self.reset_button.pack(side="left", padx=8)
        ttk.Label(row, text="按操作连接，完成后自动断开；请关闭占用连接的手机或终端。", foreground="#607086").pack(side="right")
        ttk.Label(panel, text="02  选择业务固件", style="Heading.TLabel").pack(anchor="w")
        row = ttk.Frame(panel)
        row.pack(fill="x", pady=8)
        ttk.Label(row, textvariable=self.file, wraplength=760).pack(side="left", fill="x", expand=True)
        self.file_button = ttk.Button(row, text="选择 BIN…", command=self.choose_file)
        self.file_button.pack(side="right", padx=(8, 0))
        ttk.Label(panel, textvariable=self.info, font=("Consolas", 9), wraplength=980).pack(anchor="w")
        row = ttk.Frame(panel)
        row.pack(fill="x", pady=(12, 6))
        self.auto_check = ttk.Checkbutton(row, text="自动从业务模式进入 Loader", variable=self.auto_enter)
        self.auto_check.pack(side="left")
        self.reset_check = ttk.Checkbutton(row, text="上传后复位并确认业务启动", variable=self.reset)
        self.reset_check.pack(side="left", padx=20)
        ttk.Label(panel, text="单业务槽：上传会覆盖原固件，无回滚。中断后请进入 Loader，重新上传完整签名 BIN。", foreground="#9b4a13", wraplength=970).pack(anchor="w", pady=(2, 12))
        row = ttk.Frame(panel)
        row.pack(fill="x")
        self.start_button = ttk.Button(row, text="开始固件更新", style="Accent.TButton", command=self.upgrade)
        self.start_button.pack(side="left")
        self.cancel_button = ttk.Button(row, text="取消操作", command=self.cancel)
        self.cancel_button.pack(side="left", padx=8)
        ttk.Label(row, textvariable=self.progress_text).pack(side="right")
        self.progress = ttk.Progressbar(panel, maximum=100)
        self.progress.pack(fill="x", pady=(10, 15))
        row = ttk.Frame(panel)
        row.pack(fill="x")
        ttk.Label(row, text="03  操作记录", style="Heading.TLabel").pack(side="left")
        ttk.Button(row, text="保存日志", command=self.save_log).pack(side="right")
        self.log = scrolledtext.ScrolledText(panel, height=8, state="disabled", wrap="word", font=("Consolas", 10), bg="#162235", fg="#d8e4f3", insertbackground="white", relief="flat", padx=12, pady=10)
        self.log.pack(fill="both", expand=True, pady=(8, 0))
        self.note("就绪。选择设备及签名业务 BIN；只检查镜像格式，签名由设备上的 MCUboot 验证。")
        self.buttons()

    def buttons(self):
        ready = not self.busy and not self.closing
        target = ready and bool(self.address.get().strip())
        for widget in (self.scan_button, self.file_button, self.address_entry, self.auto_check, self.reset_check):
            widget.configure(state="normal" if ready else "disabled")
        for widget in (self.query_button, self.enter_button, self.reset_button):
            widget.configure(state="normal" if target else "disabled")
        self.start_button.configure(state="normal" if target and self.firmware else "disabled")
        self.cancel_button.configure(state="normal" if self.busy else "disabled")

    def select_device(self, _):
        if not self.busy and self.table.selection():
            self.address.set(self.table.item(self.table.selection()[0], "values")[1])

    def choose_file(self):
        path = filedialog.askopenfilename(title="选择签名业务 BIN", filetypes=[("MCUboot BIN", "*.bin")])
        if not path:
            return
        try:
            firmware = load_firmware(path)
        except (OSError, ValueError) as error:
            self.firmware = None
            self.file.set("镜像无效，请重新选择")
            self.info.set(str(error))
            messagebox.showerror("固件检查失败", str(error))
        else:
            self.firmware = firmware
            self.file.set(str(firmware.path))
            self.info.set(f"版本 {firmware.version}    大小 {len(firmware.data):,} B\nSHA-256 {firmware.sha256}")
        self.buttons()

    def submit(self, factory, label):
        if self.busy or self.closing:
            return
        self.busy = True
        self.phase.set(label)
        self.note(label)
        self.buttons()

        async def work():
            try:
                result = await factory()
                self.emit("result", result or "操作完成")
            except asyncio.CancelledError:
                self.emit("result", "已取消；若上传已开始，原业务镜像可能不完整，请重传。")
            except Exception as error:
                self.emit("error", f"{type(error).__name__}: {error}")
            finally:
                self.emit("idle", None)

        self.future = asyncio.run_coroutine_threadsafe(work(), self.loop)

    def scan(self):
        async def work():
            devices = await self.backend.scan()
            self.emit("devices", devices)
            return f"找到 {len(devices)} 个相关设备；请选择目标，或手工输入地址。"
        self.submit(work, "正在扫描设备…")

    def action(self, name):
        address = self.address.get().strip()
        if not address:
            return
        operation, label = {"query": (self.backend.inspect, "查询版本 / 状态"),
                            "enter": (self.backend.enter, "请求进入 Loader"),
                            "reset": (self.backend.reboot, "请求 Loader 复位")}[name]
        self.submit(lambda: operation(address), f"{label} · {address}")

    def upgrade(self):
        firmware = self.firmware
        address = self.address.get().strip()
        if self.busy or firmware is None or not address:
            return
        if not messagebox.askyesno("确认覆盖业务固件", f"目标：{address}\n固件：{firmware.path.name}\n版本：{firmware.version}\n大小：{len(firmware.data):,} B\n\n将覆盖唯一业务镜像，不支持回滚。确认开始？"):
            return
        auto_enter, reset = self.auto_enter.get(), self.reset.get()
        self.progress["value"] = 0
        self.progress_text.set("0% · 准备更新")
        self.note(f"固件 SHA-256 {firmware.sha256}")
        self.submit(lambda: self.backend.upgrade(address, firmware, auto_enter, reset), "准备更新")

    def cancel(self):
        if self.busy and messagebox.askyesno("取消操作", "如果正在上传，业务镜像可能已经被覆盖，需要重新上传完整固件。确定取消？"):
            self.phase.set("正在取消并断开…")
            self.future.cancel()

    def note(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", f"[{datetime.now():%H:%M:%S}] {text}\n")
        if int(self.log.index("end-1c").split(".")[0]) > 1500:
            self.log.delete("1.0", "end-1000l")
        self.log.see("end")
        self.log.configure(state="disabled")

    def poll(self):
        for _ in range(200):
            try:
                kind, data = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == "devices":
                self.table.delete(*self.table.get_children())
                for row in data:
                    self.table.insert("", "end", values=row)
            elif kind == "progress":
                done, total, speed = data
                self.progress["value"] = done / total * 100
                eta = (total - done) / max(speed, 1)
                self.progress_text.set(f"{done / total:.1%} · {done:,} / {total:,} B · {speed / 1024:.1f} KiB/s · 剩余 {eta:.0f}s")
            elif kind == "phase":
                self.phase.set(data)
                self.note(data)
            elif kind in ("result", "error"):
                self.phase.set(data if kind == "result" else "操作失败 · 请查看记录并重试")
                self.note(data)
            elif kind == "log":
                self.note(data)
            elif kind == "idle":
                self.busy = False
                self.future = None
            self.buttons()
        if self.closing and not self.busy:
            self.loop.call_soon_threadsafe(self.loop.stop)
            self.thread.join(timeout=2)
            self.root.destroy()
            return
        self.root.after(80, self.poll)

    def save_log(self):
        path = filedialog.asksaveasfilename(defaultextension=".log", filetypes=[("操作日志", "*.log")])
        if path:
            try:
                with open(path, "w", encoding="utf-8") as stream:
                    stream.write(self.log.get("1.0", "end-1c"))
            except OSError as error:
                messagebox.showerror("保存失败", str(error))

    def close(self):
        if self.busy:
            if not messagebox.askyesno("退出更新工具", "当前操作尚未结束。关闭会取消操作，上传中的业务镜像可能不完整。确定退出？"):
                return
            self.future.cancel()
        self.closing = True
        self.buttons()


if __name__ == "__main__":
    root = tk.Tk()
    OtaWindow(root)
    root.mainloop()
