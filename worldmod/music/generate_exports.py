"""Generate x86 forwarding thunks matching this Windows installation's WinMM ABI."""
from pathlib import Path
import pefile

root = Path(__file__).resolve().parent
symbols = pefile.PE(r"C:\Windows\SysWOW64\winmm.dll").DIRECTORY_ENTRY_EXPORT.symbols
lines = ["extern \"C\" void EnsureLoaded();", f"static const unsigned exportCount = {len(symbols)};",
         f"static FARPROC exports[{len(symbols)}];",
         "static const WORD ordinals[] = {" + ",".join(str(s.ordinal) for s in symbols) + "};"]
definitions = ["LIBRARY winmm", "EXPORTS"]
for i, symbol in enumerate(symbols):
    name = symbol.name.decode() if symbol.name else f"ordinal{symbol.ordinal}"
    target = f"Proxy{i}"
    if name == "timeGetTime":
        lines.append(f"static const unsigned timeIndex = {i};")
        target = "_MusicTimeGetTime@0"
    else:
        lines.append(f'extern "C" __declspec(naked) void Proxy{i}() {{ __asm {{ pushfd }} __asm {{ pushad }} '
                     f'__asm {{ call EnsureLoaded }} __asm {{ popad }} __asm {{ popfd }} '
                     f'__asm {{ jmp dword ptr [exports + {i * 4}] }} }}')
    suffix = " NONAME" if symbol.name is None else ""
    definitions.append(f"    {name}={target} @{symbol.ordinal}{suffix}")
(root / "exports.inc").write_text("\n".join(lines) + "\n")
(root / "winmm.def").write_text("\n".join(definitions) + "\n")
print(f"Generated {len(symbols)} exports")
