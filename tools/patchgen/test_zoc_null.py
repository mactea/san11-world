"""Reproduce the logged null dereference and compare normal native paths."""
import sys
import unittest
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UcError
from unicorn.x86_const import *
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from patch_zoc_null import CAVE, SKIP, SITES, guard, cave_records


def run(index, pointer, value, fixed, relocated=None):
    site, old, resume = SITES[index]
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    entry = CAVE+index*0x40 if relocated is None else relocated
    code = guard(index)
    if relocated is not None:
        parts = cave_records()[index].split(';')[0].split()
        assert int(parts[1],16) == site and bytes.fromhex(parts[3]) == old
        code = bytearray.fromhex(parts[4])
        for fixup in parts[5:]:
            off, target = fixup[1:].split('=rel:')
            off, target = int(off), int(target,16)
            code[off:off+4] = ((target-entry-off-4)&0xffffffff).to_bytes(4,'little')
        code = bytes(code)
    for page in {site & -4096,entry & -4096,0x200000}:
        cpu.mem_map(page,4096)
    for target in [resume,SKIP]:
        cpu.mem_write(target,b'\xcc')
    cpu.mem_write(site,old)
    cpu.mem_write(entry,code)
    cpu.mem_write(0x200014,value.to_bytes(4,'little'))
    cpu.mem_write(0x200024,value.to_bytes(4,'little'))
    regs = {UC_X86_REG_EAX:pointer,UC_X86_REG_EBX:0x17a009c0,
            UC_X86_REG_ECX:0x23280,UC_X86_REG_EDX:0x4d2,
            UC_X86_REG_ESI:5,UC_X86_REG_EDI:0x01bf04d1,
            UC_X86_REG_EBP:0,UC_X86_REG_ESP:0x200f00}
    for r,v in regs.items():cpu.reg_write(r,v)
    cpu.reg_write(UC_X86_REG_EFLAGS,0x202)
    def stop(uc,address,size,_):
        if address in [resume,SKIP]:uc.emu_stop()
    cpu.hook_add(UC_HOOK_CODE,stop)
    cpu.emu_start(entry if fixed else site,0xffffffff,count=20)
    for r,v in regs.items():
        if not (index==0 and pointer and r==UC_X86_REG_ECX):
            assert cpu.reg_read(r)==v
    return tuple(cpu.reg_read(r) for r in [UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EFLAGS])


class ZocNullTests(unittest.TestCase):
    def test_startup_loader_relocations(self):
        for address in [0x10000000,0x70000000]:
            for i in range(2):
                self.assertEqual(run(i,0,0,True,address)[0],SKIP)
                for value in [0,1,2,100,0xffffffff]:
                    self.assertEqual(run(i,0x200000,value,True,address),
                                     run(i,0x200000,value,False))

    def test_original_crashes_on_null(self):
        for i in range(2):
            with self.subTest(index=i),self.assertRaises(UcError):run(i,0,0,False)

    def test_logged_null_skips_neighbor(self):
        for i in range(2):
            self.assertEqual(run(i,0,0,True)[0],SKIP)

    def test_valid_objects_match_native_including_flags(self):
        for i in range(2):
            for value in [0,1,2,100,0xffffffff]:
                with self.subTest(index=i,value=value):
                    self.assertEqual(run(i,0x200000,value,True),run(i,0x200000,value,False))


if __name__ == '__main__':unittest.main()
