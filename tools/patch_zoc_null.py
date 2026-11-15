"""Guard missing neighbor objects in native movement ZOC checks (San11PK 1.1)."""
from keystone import Ks, KS_ARCH_X86, KS_MODE_32

CAVE = 0x6d3100
SKIP = 0x56e19f
SITES = [(0x56e151, bytes.fromhex('8b481485c97447'), 0x56e158),
         (0x56e17f, bytes.fromhex('83782401741a'), 0x56e185)]


def guard(index, address=None):
    _, _, resume = SITES[index]
    original = 'mov ecx, [eax+0x14]; test ecx, ecx' if index == 0 else 'cmp dword ptr [eax+0x24], 1'
    code = f'test eax, eax; jz {SKIP}; {original}; je {SKIP}; jmp {resume}'
    return bytes(Ks(KS_ARCH_X86, KS_MODE_32).asm(code, CAVE+index*0x40 if address is None else address)[0])


def cave_records():
    """Install through worldmod's startup loader, with its allocated cave pool."""
    records = []
    for i, (site, old, resume) in enumerate(SITES):
        offsets = (4, 15, 20) if i == 0 else (4, 14, 19)
        fixups = ' '.join(f'@{off}=rel:{target:x}' for off, target in
                          zip(offsets, (SKIP, SKIP, resume)))
        records.append(f'CAVE {site:08x} {len(old)} {old.hex()} {guard(i).hex()} '
                       f'{fixups} ; Native movement ZOC missing-neighbor guard')
    return records

