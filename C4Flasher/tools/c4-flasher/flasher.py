# -*- coding: utf-8 -*-
"""C4 STM32F103 固件烧录工具 - 内置 pyOCD，选择 bin 版本后 ST-Link 一键烧录。"""

import os

os.environ.setdefault("PYOCD_DISABLE_CMSIS_PACK", "1")

import json
import sys
import threading
import tkinter as tk
from tkinter import messagebox, ttk

import pyocd_flasher


def app_root():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def config_path():
    if getattr(sys, "frozen", False):
        beside_exe = os.path.join(os.path.dirname(sys.executable), "config.json")
        if os.path.isfile(beside_exe):
            return beside_exe
        bundled = os.path.join(getattr(sys, "_MEIPASS", ""), "config.json")
        if os.path.isfile(bundled):
            return bundled
        return beside_exe
    return os.path.join(os.path.dirname(__file__), "config.json")


ROOT = app_root()
FIRMWARE_DIR = os.path.join(ROOT, "firmware")
VERSIONS_JSON = os.path.join(FIRMWARE_DIR, "versions.json")
CONFIG_JSON = config_path()
DEFAULT_FLASH_ADDR = "0x08000000"
DEFAULT_TARGET = "stm32f103c8"
DEFAULT_FREQUENCY_HZ = 4000000


def load_json(path, default):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return default


def load_config():
    cfg = load_json(CONFIG_JSON, {})
    return {
        "target": cfg.get("target", DEFAULT_TARGET),
        "frequency_hz": int(cfg.get("frequency_hz", DEFAULT_FREQUENCY_HZ)),
    }


def parse_flash_address(addr_text):
    text = (addr_text or DEFAULT_FLASH_ADDR).strip().lower()
    if text.startswith("0x"):
        return int(text, 16)
    return int(text, 16 if all(c in "0123456789abcdef" for c in text) else 10)


def load_versions():
    data = load_json(VERSIONS_JSON, {"versions": [], "flash_address": DEFAULT_FLASH_ADDR})
    versions = []
    for item in data.get("versions", []):
        bin_path = os.path.join(FIRMWARE_DIR, item.get("file", ""))
        versions.append(
            {
                "id": item.get("id", ""),
                "name": item.get("name", item.get("file", "")),
                "file": item.get("file", ""),
                "note": item.get("note", ""),
                "path": bin_path,
                "exists": os.path.isfile(bin_path),
            }
        )
    return versions, data.get("flash_address", DEFAULT_FLASH_ADDR)


class FlasherApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("C4 固件烧录工具")
        self.geometry("560x420")
        self.resizable(False, False)

        self.cfg = load_config()
        self.versions, self.flash_addr = load_versions()
        self.selected_index = tk.IntVar(value=0)

        self._build_ui()
        self._refresh_version_list()
        self._log("程序目录: " + ROOT)
        self._log("固件目录: " + FIRMWARE_DIR)
        self._log("烧录引擎: pyOCD（已内置，无需 STM32CubeProgrammer）")
        self._log(f"目标芯片: {self.cfg['target']} @ {self.flash_addr}")

    def _build_ui(self):
        pad = {"padx": 12, "pady": 6}

        frm = ttk.Frame(self)
        frm.pack(fill=tk.BOTH, expand=True, **pad)

        ttk.Label(frm, text="选择固件版本", font=("Microsoft YaHei UI", 10, "bold")).pack(anchor=tk.W)
        self.version_combo = ttk.Combobox(frm, state="readonly", width=62)
        self.version_combo.pack(fill=tk.X, pady=(4, 0))
        self.version_combo.bind("<<ComboboxSelected>>", self._on_version_changed)

        self.bin_label = ttk.Label(frm, text="", foreground="#555")
        self.bin_label.pack(anchor=tk.W, pady=(4, 0))

        btn_row = ttk.Frame(frm)
        btn_row.pack(fill=tk.X, pady=(12, 0))
        ttk.Button(btn_row, text="检测 ST-Link", command=self._detect_stlink).pack(side=tk.LEFT)
        ttk.Button(btn_row, text="刷新固件列表", command=self._refresh_version_list).pack(side=tk.LEFT, padx=(8, 0))
        self.flash_btn = ttk.Button(btn_row, text="开始烧录", command=self._start_flash)
        self.flash_btn.pack(side=tk.RIGHT)

        ttk.Label(frm, text="日志", font=("Microsoft YaHei UI", 10, "bold")).pack(anchor=tk.W, pady=(12, 0))
        self.log = tk.Text(frm, height=14, wrap=tk.WORD, font=("Consolas", 9))
        self.log.pack(fill=tk.BOTH, expand=True, pady=(4, 0))
        self.log.configure(state=tk.DISABLED)

        self.status = ttk.Label(frm, text="请插入 ST-Link 并选择固件", foreground="#333")
        self.status.pack(anchor=tk.W, pady=(6, 0))

    def _log(self, msg):
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, msg + "\n")
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    def _refresh_version_list(self):
        self.versions, self.flash_addr = load_versions()
        labels = []
        for v in self.versions:
            flag = "OK" if v["exists"] else "缺失"
            labels.append(f"[{flag}] {v['name']}")
        if not labels:
            labels = ["(无固件，请先 Keil 编译或放入 bin)"]
        self.version_combo["values"] = labels
        if labels:
            self.version_combo.current(0)
            self.selected_index.set(0)
        self._on_version_changed()

    def _current_version(self):
        idx = self.version_combo.current()
        if idx < 0 or idx >= len(self.versions):
            return None
        return self.versions[idx]

    def _on_version_changed(self, _event=None):
        v = self._current_version()
        if not v:
            self.bin_label.configure(text="")
            return
        note = v.get("note", "")
        extra = f"  |  {note}" if note else ""
        self.bin_label.configure(text=f"{v['path']}{extra}")

    def _detect_stlink(self):
        self.status.configure(text="正在检测 ST-Link...")
        threading.Thread(target=self._detect_worker, daemon=True).start()

    def _detect_worker(self):
        try:
            probes = pyocd_flasher.list_probes()
            text = pyocd_flasher.format_probe_list(probes)
        except Exception as exc:
            text = f"检测失败: {exc}"
            probes = []

        self.after(0, lambda: self._log(text))
        if probes:
            self.after(0, lambda: self.status.configure(text=f"已检测到 {len(probes)} 个 ST-Link"))
        else:
            self.after(0, lambda: self.status.configure(text="未检测到 ST-Link，请检查连接与驱动"))

    def _start_flash(self):
        v = self._current_version()
        if not v:
            messagebox.showwarning("提示", "没有可烧录的固件版本")
            return
        if not v["exists"]:
            messagebox.showerror(
                "错误",
                f"固件文件不存在:\n{v['path']}\n\n请先用 Keil 编译，或把 bin 放入 firmware 目录",
            )
            return

        if not messagebox.askyesno(
            "确认烧录",
            f"设备: {self.cfg['target']}\n固件: {v['name']}\n地址: {self.flash_addr}\n\n确认开始烧录?",
        ):
            return

        self.flash_btn.configure(state=tk.DISABLED)
        self.status.configure(text="烧录中，请勿拔线...")
        threading.Thread(target=self._flash_worker, args=(v,), daemon=True).start()

    def _flash_worker(self, version):
        bin_path = version["path"]
        base_address = parse_flash_address(self.flash_addr)
        logs = []

        def log_fn(msg):
            logs.append(msg)
            self.after(0, lambda m=msg: self._log(m))

        ok, summary = pyocd_flasher.flash_firmware(
            bin_path,
            base_address,
            self.cfg["target"],
            self.cfg["frequency_hz"],
            log_fn,
        )
        if summary and summary not in logs:
            self.after(0, lambda: self._log(summary))

        def done():
            self.flash_btn.configure(state=tk.NORMAL)
            if ok:
                self.status.configure(text="烧录成功")
                messagebox.showinfo("完成", summary)
            else:
                self.status.configure(text="烧录失败")
                messagebox.showerror("失败", summary)

        self.after(0, done)


def main():
    app = FlasherApp()
    app.mainloop()


if __name__ == "__main__":
    main()
