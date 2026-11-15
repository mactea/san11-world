"""Execute both injected in-game resolution enumeration branches."""
import struct
import sys
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI,
    UC_X86_REG_ESP, UC_X86_REG_EIP,
)
from keystone import Ks, KS_ARCH_X86, KS_MODE_32

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_windowed_resolution import SITES, STUB, MODE, stub


def execute(builder, mode_index, count, windowed):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    site, _, original, initialize, end = SITES[builder]
    stack, compare = 0x200800, 0x300000
    for page in {STUB & -4096, original & -4096, initialize & -4096,
                 end & -4096, stack & -4096, compare}:
        cpu.mem_map(page, 4096)
    cpu.mem_write(STUB, stub())
    for target in [original, initialize, end]:
        cpu.mem_write(target, b'\xcc')
    cpu.mem_write(stack+0x20, struct.pack('<I', windowed))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_EAX, count)
    cpu.reg_write(UC_X86_REG_ECX, mode_index)
    cpu.reg_write(UC_X86_REG_ESI, 123)
    # The hook replaces JAE, so its input flags come from the original CMP.
    cpu.mem_write(compare, bytes(Ks(KS_ARCH_X86, KS_MODE_32).asm(
        f'cmp ecx, eax; jmp {STUB+builder*0x60}', compare)[0]))

    def stop(uc, address, size, _):
        if address in [original, initialize, end]:
            uc.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, stop)
    cpu.emu_start(compare, 0xffffffff, count=30)
    assert cpu.reg_read(UC_X86_REG_ESP) == stack
    assert cpu.reg_read(UC_X86_REG_EAX) == count
    assert cpu.reg_read(UC_X86_REG_ECX) == mode_index
    if cpu.reg_read(UC_X86_REG_EIP) == initialize:
        assert cpu.reg_read(UC_X86_REG_ESI) == MODE
        assert struct.unpack('<4I', cpu.mem_read(MODE, 16)) == (3600, 2200, 0, 22)
    else:
        assert cpu.reg_read(UC_X86_REG_ESI) == 123
    return cpu.reg_read(UC_X86_REG_EIP)


class WindowedResolutionTest(unittest.TestCase):
    def test_hardware_modes_use_original_path(self):
        for builder in range(2):
            for windowed in range(2):
                for index in [0, 12, 24]:
                    self.assertEqual(execute(builder, index, 25, windowed), SITES[builder][2])

    def test_fullscreen_ends_at_original_count(self):
        for builder in range(2):
            self.assertEqual(execute(builder, 25, 25, 0), SITES[builder][4])

    def test_windowed_supplies_custom_mode_to_native_checks(self):
        for builder in range(2):
            self.assertEqual(execute(builder, 25, 25, 1), SITES[builder][3])

    def test_windowed_ends_after_one_custom_iteration(self):
        for builder in range(2):
            self.assertEqual(execute(builder, 26, 25, 1), SITES[builder][4])


if __name__ == '__main__':
    unittest.main()
