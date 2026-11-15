"""Launch the unmodified game, then install neighbor guards after startup checks."""
from pathlib import Path
from datetime import datetime
import ctypes as c
from ctypes import wintypes as w
import subprocess
import struct
import time
import json
import sys
from patch_zoc_null import SITES, CAVE, guard


def main():
    game = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]/'build/crash-repair'/datetime.now().strftime('live-zoc-%Y%m%d-%H%M%S')
    root.mkdir(parents=True)
    (root.parent/'active-live.txt').write_text(str(root),encoding='utf-8')
    result = {'installed':False}
    try:
        started = time.time()
        if len(sys.argv)>2:
            # Explicit attach is used only for verification of an already-started game.
            class Existing:
                pid=int(sys.argv[2])
                def poll(self):
                    return None
            process=Existing()
            started=0
        else:
            process = subprocess.Popen([str(game/'san11pk_world.exe')],cwd=game)
        result['pid'] = process.pid
        (root/'status.json').write_text(json.dumps(result))
        log = game/'worldmod_san11pk_world.log'
        for _ in range(600):
            assert process.poll() is None,'Game exited before title screen'
            try:
                ready = log.stat().st_mtime >= started and 'loaded stream kind 0x16:' in log.read_text(errors='replace')
            except OSError:
                ready = False
            if ready:
                break
            time.sleep(.2)
        else:
            raise RuntimeError('Title screen not confirmed; no patch installed')
        k = c.WinDLL('kernel32',use_last_error=True)
        nt = c.WinDLL('ntdll')
        k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
        for fn in ['ReadProcessMemory','WriteProcessMemory']:
            getattr(k,fn).argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.c_void_p]
        k.VirtualProtectEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD,c.POINTER(w.DWORD)]
        k.FlushInstructionCache.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t]
        k.CloseHandle.argtypes=[w.HANDLE]
        nt.NtSuspendProcess.argtypes=[w.HANDLE];nt.NtResumeProcess.argtypes=[w.HANDLE]
        handle=k.OpenProcess(0x1fffff,False,process.pid)
        assert handle,c.get_last_error()
        def read(address,size):
            data=c.create_string_buffer(size)
            assert k.ReadProcessMemory(handle,address,data,size,None)
            return data.raw
        def write(address,data):
            protection=w.DWORD()
            assert k.VirtualProtectEx(handle,address,len(data),0x40,c.byref(protection))
            try:
                assert k.WriteProcessMemory(handle,address,data,len(data),None)
                assert k.FlushInstructionCache(handle,address,len(data))
            finally:
                unused=w.DWORD()
                assert k.VirtualProtectEx(handle,address,len(data),protection.value,c.byref(unused))
        changes=[]
        try:
            assert nt.NtSuspendProcess(handle)==0
            try:
                installed = []
                for i,(site,old,_) in enumerate(SITES):
                    live=read(site,len(old))
                    match=False
                    if live[:1]==b'\xe9' and live[5:]==b'\x90'*(len(old)-5):
                        target=(site+5+struct.unpack('<i',live[1:5])[0])&0xffffffff
                        expected=guard(i,target)
                        match=read(target,len(expected))==expected
                    installed.append(match)
                if all(installed):
                    result.update(installed=True,startup_confirmed=True,
                                  both_live_guards_verified=True,already_installed=True)
                    (root/'status.json').write_text(json.dumps(result,indent=2))
                    print(json.dumps(result))
                    return 0
                for i,(site,old,_) in enumerate(SITES):
                    assert read(site,len(old))==old,'Unknown native code'
                    assert read(CAVE+i*0x40,0x40)==b'\xcc'*0x40,'Code padding occupied'
                for i,(site,old,_) in enumerate(SITES):
                    code=guard(i);changes.append((CAVE+i*0x40,read(CAVE+i*0x40,len(code))))
                    write(CAVE+i*0x40,code)
                    jump=b'\xe9'+struct.pack('<i',CAVE+i*0x40-site-5)+b'\x90'*(len(old)-5)
                    changes.append((site,old));write(site,jump)
                    assert read(site,len(jump))==jump and read(CAVE+i*0x40,len(code))==code
            except BaseException:
                for address,data in reversed(changes):write(address,data)
                raise
            finally:
                assert nt.NtResumeProcess(handle)==0
        finally:k.CloseHandle(handle)
        result.update(installed=True,startup_confirmed=True,both_live_guards_verified=True)
        (root/'original-memory.json').write_text(json.dumps([{'address':a,'original':d.hex()} for a,d in changes],indent=2))
    except BaseException as error:
        result['error']=repr(error)
    (root/'status.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result))
    return 0 if result['installed'] else 1


if __name__=='__main__':sys.exit(main())
