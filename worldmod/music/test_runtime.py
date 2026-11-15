"""Integration check against a MUSIC_TEST build in an otherwise idle game menu.

Usage: python test_runtime.py <pid> <loaded proxy base address, decimal or 0xhex>
Requests are consumed on the game's own playback thread. No save is modified.
"""
import ctypes
from pathlib import Path
import sys
import time
import pefile

pid, base = (int(arg, 0) for arg in sys.argv[1:])
kernel = ctypes.windll.kernel32
kernel.OpenProcess.restype = ctypes.c_void_p
for name in ("ReadProcessMemory", "WriteProcessMemory"):
    getattr(kernel, name).argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_size_t, ctypes.c_void_p]
kernel.CloseHandle.argtypes = [ctypes.c_void_p]
handle = kernel.OpenProcess(0x38, False, pid)
assert handle
pe = pefile.PE(str(Path(__file__).parent / "build/winmm.dll"))
request = base + next(s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name == b"MusicTestRequest")

def read(address):
    value = ctypes.c_long()
    assert kernel.ReadProcessMemory(handle, address, ctypes.byref(value), 4, None)
    return value.value

def send(value, expected):
    data = ctypes.c_long(value)
    assert kernel.WriteProcessMemory(handle, request, ctypes.byref(data), 4, None)
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if read(request) == -999 and read(0x09119870) == expected:
            break
        time.sleep(.1)
    else:
        raise AssertionError((value, expected, read(0x09119870)))
    time.sleep(2)
    assert read(0x09119870) == expected, (value, expected, read(0x09119870))
    print(f"request {value}: current BGM {expected}")

try:
    send(3, 30)       # Integration builds start the medley immediately.
    assert read(0x09119878) == 0  # Native loop flag is off.
    send(6, 30)       # Changing season keeps the medley until it ends.
    send(-998, 31)    # Medley completion advances to Hero's Dawn.
    send(5, 31)       # Season changes do not interrupt Hero's Dawn either.
    assert read(0x09119878) == 0
    send(10, 31)      # Wartime map requests must not cancel Hero's Dawn.
    send(-998, 10)    # Hero's Dawn completion returns to the latest original map cue.
    send(-998, 30)    # Original completion advances to the medley.
    send(16, 16)     # Native event cue cancels the map playlist.
    send(-998, -1)   # Cancelling the battle cue cannot resurrect the playlist.
    send(1, 1)       # Restore normal menu music.
    print("PASS: both custom tracks, three-track rotation, seasons, battle cancellation, menu restore")
finally:
    kernel.CloseHandle(handle)
