"""Read-only x86 call-site inventory; nearby constants are leads, not proven skill arguments."""
import argparse
import hashlib
import json
from pathlib import Path

import capstone
import pefile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--extended', action='store_true', help='Also inventory tail dispatch and other memory-e8 opcodes')
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    pe = pefile.PE(data=raw)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.detail = True
    decoder.skipdata = True
    sites, previous, pending = [], [], []
    targets = {0x4890f0: 'person_has_skill', 0x47a8a0: 'object_has_skill'}
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        previous.clear()
        pending.clear()
        for ins in decoder.disasm(section.get_data(), pe.OPTIONAL_HEADER.ImageBase+section.VirtualAddress):
            if ins.id == 0:
                previous.clear()
                pending.clear()
                continue
            description = f'{ins.address:08x} {ins.mnemonic} {ins.op_str}'
            for site in pending:
                site['following_instructions'].append(description)
            pending = [site for site in pending if len(site['following_instructions']) < 8]
            kind = None
            if ins.mnemonic == 'call' and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                kind = targets.get(ins.operands[0].imm)
            if ins.mnemonic == 'call' and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_MEM:
                if ins.operands[0].mem.disp in (0x58, 0x5c):
                    kind = 'potential_virtual_skill_query'
            if ins.mnemonic in ('cmp', 'mov') and any(
                op.type == capstone.x86.X86_OP_MEM and op.mem.disp == 0xe8 for op in ins.operands
            ):
                kind = 'possible_inline_skill_access'
            if args.extended and not kind:
                if ins.mnemonic == 'jmp' and ins.operands:
                    operand=ins.operands[0]
                    if operand.type == capstone.x86.X86_OP_IMM and operand.imm in targets:
                        kind = 'tail_'+targets[operand.imm]
                    if operand.type == capstone.x86.X86_OP_MEM and operand.mem.disp in (0x58,0x5c):
                        kind = 'potential_tail_virtual_skill_query'
                if any(op.type == capstone.x86.X86_OP_MEM and op.mem.disp == 0xe8 for op in ins.operands):
                    kind = 'possible_other_inline_skill_access'
            if kind:
                operands = []
                for number, operand in enumerate(ins.operands):
                    if operand.type != capstone.x86.X86_OP_MEM:
                        continue
                    operands.append(dict(index=number, base=ins.reg_name(operand.mem.base),
                        index_register=ins.reg_name(operand.mem.index), scale=operand.mem.scale,
                        displacement=operand.mem.disp, size=operand.size,
                        read=bool(operand.access & capstone.CS_AC_READ), write=bool(operand.access & capstone.CS_AC_WRITE)))
                site = dict(address=hex(ins.address), kind=kind, instruction=ins.mnemonic+' '+ins.op_str,
                            bytes=ins.bytes.hex(), preceding_instructions=previous[-8:], following_instructions=[],
                            memory_operands=operands)
                sites.append(site)
                pending.append(site)
            previous.append(description)
            if len(previous) > 8:
                previous.pop(0)
    result = dict(executable=str(args.exe), sha256=hashlib.sha256(raw).hexdigest(), sites=sites,extended=args.extended,
                  limitation='Linear executable-section disassembly inventories direct target calls, memory58/5c calls and memorye8 mov/cmp; extended mode adds direct/slot tail jumps and other memorye8 instructions. Register-indirect targets, computed aliases, instructions decoded differently from linear sweep and downstream committed effects require individual verification; this inventory does not prove all-skill coverage.')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(sites=len(sites), by_kind={kind: sum(s['kind']==kind for s in sites)
                                                for kind in sorted({s['kind'] for s in sites})})))


if __name__ == '__main__':
    main()
