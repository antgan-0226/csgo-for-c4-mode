# -*- coding: utf-8 -*-
"""pyOCD 烧录后端（内置，无需 STM32CubeProgrammer）。"""

import logging
import os
import sys
import time

os.environ.setdefault("PYOCD_DISABLE_CMSIS_PACK", "1")
if getattr(sys, "frozen", False):
    os.environ["PYOCD_DISABLE_CMSIS_PACK"] = "1"

FLASH_WRPR = 0x40022008


class _GuiLogHandler(logging.Handler):
    def __init__(self, emit_fn):
        super().__init__()
        self._emit_fn = emit_fn
        self.setFormatter(logging.Formatter("%(message)s"))

    def emit(self, record):
        try:
            self._emit_fn(self.format(record))
        except Exception:
            self.handleError(record)


def _attach_pyocd_logging(log_fn):
    handler = _GuiLogHandler(log_fn)
    root = logging.getLogger("pyocd")
    root.addHandler(handler)
    root.setLevel(logging.INFO)
    return handler


def _detach_pyocd_logging(handler):
    logging.getLogger("pyocd").removeHandler(handler)


def _ensure_targets_registered():
    from pyocd.target import TARGET

    import target_stm32f103c8

    TARGET.setdefault("stm32f103c8", target_stm32f103c8.STM32F103C8)


def list_probes():
    _ensure_targets_registered()
    from pyocd.core.helpers import ConnectHelper

    probes = ConnectHelper.get_all_connected_probes()
    return [{"description": p.description, "unique_id": p.unique_id} for p in probes]


def format_probe_list(probes):
    if not probes:
        return "未检测到调试器（请插入 ST-Link 并确认驱动）"
    lines = [f"检测到 {len(probes)} 个调试器:"]
    for i, probe in enumerate(probes, 1):
        lines.append(f"  {i}. {probe['description']}  [{probe['unique_id']}]")
    return "\n".join(lines)


def _noop_progress(_percent):
    pass


def _format_flash_error(exc):
    text = str(exc)
    if "result code 0x1" in text or "result code 1" in text:
        text += (
            "\n\n可能原因: Flash 写保护(WRP)、供电不足、或芯片在运行中干扰烧录。"
            "\n建议: 先用 ST-Link Utility / CubeProgrammer 做一次 Full Chip Erase。"
        )
    return text


def flash_firmware(bin_path, base_address, target, frequency_hz, log_fn):
    _ensure_targets_registered()
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.exceptions import FlashProgramFailure
    from pyocd.flash.file_programmer import FileProgrammer

    handler = _attach_pyocd_logging(log_fn)
    try:
        session = ConnectHelper.session_with_chosen_probe(
            options={
                "target_override": target,
                "frequency": int(frequency_hz),
                "hide_programming_progress": True,
                "connect_mode": "halt",
            }
        )
        if session is None:
            return False, "未找到 ST-Link 调试器"

        with session:
            target_obj = session.board.target
            log_fn(f"芯片目标: {target}")
            log_fn(f"调试器: {session.probe.description}")
            if hasattr(target_obj, "part_number"):
                log_fn(f"Part: {target_obj.part_number}")

            log_fn(f"烧录: {bin_path}")
            log_fn(f"地址: 0x{base_address:08X}")

            target_obj.reset_and_halt()
            time.sleep(0.05)
            try:
                wrpr = target_obj.read32(FLASH_WRPR)
                log_fn(f"FLASH_WRPR = 0x{wrpr:08X}")
            except Exception:
                pass

            FileProgrammer(
                session,
                progress=_noop_progress,
                smart_flash=False,
                chip_erase="chip",
                keep_unwritten=False,
            ).program(
                bin_path,
                file_format="bin",
                base_address=base_address,
            )
            target_obj.reset()
        return True, "烧录成功，设备已复位"
    except FlashProgramFailure as exc:
        return False, _format_flash_error(exc)
    except Exception as exc:
        return False, _format_flash_error(exc)
    finally:
        _detach_pyocd_logging(handler)
