"""Find original direct branch/call entries into the interior of a skill hook."""
import argparse
import bisect
import hashlib
import json
import re
from pathlib import Path
import capstone
import pefile

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();raw=args.exe.read_bytes();pe=pefile.PE(data=raw)
    table=(ROOT/'worldmod/src/skills.cpp').read_text(encoding='utf-8').split('Hook g_hooks[] = {',1)[1].split('\n};',1)[0]
    hooks=[dict(at=int(a,16),size=int(n),name=name) for a,n,name in re.findall(r'\{(0x[0-9a-f]+),\s*"[^"\n]+",\s*(\d+),\s*(\w+)',table)]
    hooks.sort(key=lambda h:h['at']);starts=[h['at'] for h in hooks]
    interiors={at:h for h in hooks for at in range(h['at']+1,h['at']+h['size'])}
    def overwritten(at):
        n=bisect.bisect_right(starts,at)-1
        return n>=0 and at<hooks[n]['at']+hooks[n]['size']
    decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);decoder.skipdata=True
    incoming=[];previous=[]
    for section in pe.sections:
        if not section.Characteristics&0x20000000:continue
        for ins in decoder.disasm(section.get_data(),pe.OPTIONAL_HEADER.ImageBase+section.VirtualAddress):
            if not (ins.mnemonic.startswith('j') or ins.mnemonic in ('call','loop','loope','loopne')):continue
            if not re.fullmatch(r'0x[0-9a-f]+',ins.op_str):continue
            target=int(ins.op_str,16)
            edge=dict(source=hex(ins.address),target=hex(target),instruction=ins.mnemonic+' '+ins.op_str)
            if 0x5b0419<target<0x5b041f:previous.append(edge)
            if target in interiors and not overwritten(ins.address):
                h=interiors[target];incoming.append(dict(**edge,hook=hex(h['at']),hook_size=h['size'],hook_name=h['name']))
    result=dict(executable=str(args.exe),sha256=hashlib.sha256(raw).hexdigest(),hooks=len(hooks),
        potential_interior_entries=incoming,previous_vine_hook=dict(at='0x5b0419',size=6,incoming=previous),
        limitation='Original executable direct branches/calls only. Sources inside hooks are removed by installation. Does not prove indirect dispatch, generated caves, world patch replacements or live behavior safe.')
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(dict(hooks=len(hooks),potential_interior_entries=len(incoming),previous_vine_interior_entries=len(previous))))


if __name__=='__main__':main()
