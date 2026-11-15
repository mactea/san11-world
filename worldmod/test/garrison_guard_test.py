"""Run the original final sortie guard against new reserve values (no game writes)."""
import struct
import sys
import unittest
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP


class GuardTest(unittest.TestCase):
    def test_final_sortie_preserves_reserve(self):
        pe = pefile.PE(sys.argv[1])
        code = pe.get_data(0x5EFEDF - pe.OPTIONAL_HEADER.ImageBase, 18)
        self.assertEqual(code.hex(), '8b5424688b4c24282bc23bc10f8ca6010000')
        for reserve in [3000, 5000, 10000, 15000, 18000, 30000, 200000]:
            for delta in [-1, 0, 1]:
                cpu = Uc(UC_ARCH_X86, UC_MODE_32)
                cpu.mem_map(0x5EF000, 0x2000)
                cpu.mem_map(0x20000000, 0x1000)
                cpu.mem_write(0x5EFEDF, code)
                cpu.reg_write(UC_X86_REG_ESP, 0x20000000)
                cpu.reg_write(UC_X86_REG_EAX, reserve + 8000)
                cpu.mem_write(0x20000068, struct.pack('<I', 8000 + delta))
                cpu.mem_write(0x20000028, struct.pack('<I', reserve))
                # Execute the two loads, subtraction, comparison, conditional jump.
                cpu.emu_start(0x5EFEDF, 0x5EFEF1, count=5)
                from unicorn.x86_const import UC_X86_REG_EIP
                self.assertEqual(cpu.reg_read(UC_X86_REG_EIP), 0x5F0097 if delta > 0 else 0x5EFEF1)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]])
