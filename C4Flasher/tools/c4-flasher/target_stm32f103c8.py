# -*- coding: utf-8 -*-
"""STM32F103C8 target for pyOCD (64KB Flash / 20KB RAM)."""

from pyocd.coresight.coresight_target import CoreSightTarget
from pyocd.core.exceptions import FlashProgramFailure
from pyocd.core.memory_map import FlashRegion, MemoryMap, RamRegion
from pyocd.debug.svd.loader import SVDFile
from pyocd.flash.flash import Flash

from flash_algo_md import FLASH_ALGO

DBGMCU_CR = 0xE0042004
DBGMCU_VAL = 0x7E3FFF00

# Keil F10x 算法 Init() 的 clk 参数为系统主频(Hz)，0 会导致编程失败
INIT_CLOCK_HZ = 8_000_000


class Flash_STM32F103MD(Flash):
    def init(self, operation, address=None, clock=0, reset=False):
        super().init(operation, address, INIT_CLOCK_HZ, reset)

    def program_page(self, address, data):
        page_info = self.get_page_info(address)
        if page_info is None:
            raise FlashProgramFailure("invalid program page address", address=address)

        page_bytes = bytearray(data)
        if len(page_bytes) < page_info.size:
            page_bytes.extend([0xFF] * (page_info.size - len(page_bytes)))
        elif len(page_bytes) > page_info.size:
            page_bytes = page_bytes[: page_info.size]

        super().program_page(address, page_bytes)


class STM32F103C8(CoreSightTarget):
    VENDOR = "STMicroelectronics"
    PART_NUMBER = "STM32F103C8"

    MEMORY_MAP = MemoryMap(
        FlashRegion(
            start=0x08000000,
            length=0x10000,
            blocksize=0x400,
            page_size=0x400,
            is_boot_memory=True,
            algo=FLASH_ALGO,
            flash_class=Flash_STM32F103MD,
        ),
        RamRegion(start=0x20000000, length=0x5000),
    )

    def __init__(self, session):
        super().__init__(session, self.MEMORY_MAP)
        self._svd_location = SVDFile.from_builtin("STM32F103xx.svd")

    def post_connect_hook(self):
        self.write_memory(DBGMCU_CR, DBGMCU_VAL)
