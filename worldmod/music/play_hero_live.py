"""Apply the verified map-range fix and queue Hero's Dawn on the game's music thread.

For the previous two-track production proxy only. Does not restart or save the game.
The one-shot timer bridge restores the original IAT entry before requesting playback.
"""
import ctypes as c
from pathlib import Path
import struct
import sys
import time
import pefile

pid = int(sys.argv[1])
k = c.WinDLL("kernel32", use_last_error=True)
k.OpenProcess.argtypes = [c.c_ulong, c.c_int, c.c_ulong]
k.OpenProcess.restype = c.c_void_p
for name in ("ReadProcessMemory", "WriteProcessMemory"):
    getattr(k, name).argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_size_t, c.c_void_p]
k.VirtualAllocEx.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.c_ulong, c.c_ulong]
k.VirtualAllocEx.restype = c.c_void_p
k.VirtualProtectEx.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.c_ulong, c.c_void_p]
k.FlushInstructionCache.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t]
k.CloseHandle.argtypes = [c.c_void_p]
h = k.OpenProcess(0x438, False, pid)
assert h, c.get_last_error()

def read(address, size):
    data = c.create_string_buffer(size)
    assert k.ReadProcessMemory(h, address, data, size, None), c.get_last_error()
    return data.raw

def write(address, data):
    data = bytes(data)
    assert k.WriteProcessMemory(h, address, data, len(data), None), c.get_last_error()
    k.FlushInstructionCache(h, address, len(data))

def protect(address, size, mode):
    before = c.c_ulong()
    assert k.VirtualProtectEx(h, address, size, mode, c.byref(before)), c.get_last_error()
    return before.value

def word(address):
    return struct.unpack("<I", read(address, 4))[0]

try:
    entry = read(0x4d04f0, 6)
    assert entry[0] == 0xe9 and entry[5] == 0x90, "Unexpected playback hook"
    hook = 0x4d04f5 + struct.unpack_from("<i", entry, 1)[0]
    body = read(hook, 0x50)
    assert body[10:16] == bytes.fromhex("8d46fd83f803"), "Unexpected proxy build"
    assert body[0x28] == 0xa3, "Unexpected music-thread field"
    owner_address = struct.unpack_from("<I", body, 0x29)[0]
    owner = word(owner_address)
    assert owner, "No native music thread recorded yet"
    game = Path(r"C:\Games\san11\Romance Of Three Kingdom 11 PK")
    pe = pefile.PE(str(game / "san11pk_world.exe"))
    iat = next(s.address for d in pe.DIRECTORY_ENTRY_IMPORT for s in d.imports if s.name == b"timeGetTime")
    timer = word(iat)
    assert hook - 0x10000 < timer < hook + 0x10000, "Unexpected timer proxy"
    allocation = k.VirtualAllocEx(h, None, 4096, 0x3000, 0x40)
    assert allocation, c.get_last_error()
    flag = allocation + 1024
    code = bytearray(b"\x9c\x60")  # Preserve caller flags and registers.
    code += bytes.fromhex("64a1240000003d") + struct.pack("<I", owner)
    branches = []
    code += b"\x0f\x85"; branches.append(len(code)); code += bytes(4)
    code += bytes.fromhex("f00fba2d") + struct.pack("<I", flag) + b"\x00"
    code += b"\x0f\x82"; branches.append(len(code)); code += bytes(4)
    code += b"\xb8" + struct.pack("<I", timer) + b"\xa3" + struct.pack("<I", iat)
    code += bytes.fromhex("68000080bf6a0068f40100006a006a1fb910981109")
    call_at = allocation + len(code)
    code += b"\xe8" + struct.pack("<i", 0x4d04f0 - call_at - 5)
    tail = len(code)
    code += b"\x61\x9d"
    jump_at = allocation + len(code)
    code += b"\xe9" + struct.pack("<i", timer - jump_at - 5)
    for location in branches:
        struct.pack_into("<i", code, location, tail - location - 4)
    write(allocation, code)
    old_hook_protection = protect(hook + 15, 1, 0x40)
    write(hook + 15, b"\x07")  # (id - 3) <= 7 includes map music 3..10.
    protect(hook + 15, 1, old_hook_protection)
    old_iat_protection = protect(iat, 4, 4)
    write(iat, struct.pack("<I", allocation))
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if word(flag) and word(iat) == timer and word(0x9119870) == 31:
            time.sleep(3)
            assert word(0x9119870) == 31, "Game interrupted Hero's Dawn"
            print("PASS: live native BGM 31 (Hero's Dawn), loop off:", word(0x9119878) == 0)
            break
        time.sleep(.2)
    else:
        if word(iat) == allocation:
            write(iat, struct.pack("<I", timer))
        raise RuntimeError("Music request was not confirmed; original timer restored")
    protect(iat, 4, old_iat_protection)
    # Leave the small bridge allocated: another timer caller may still be returning through it.
finally:
    k.CloseHandle(h)
