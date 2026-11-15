"""Read the stock shader assembly from the local EXE; never execute the game.

0x41d9c0 creates these shaders in base/outline/lighting order. Hash checks
reject another game revision instead of silently testing the wrong reference.
Extracted game assets remain under the ignored build directory.
"""
from pathlib import Path
import hashlib
import sys

root = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "build/render-game/san11pk_render.exe"
data = exe.read_bytes()
fixtures = [
    (0x37AA90, "3ea339a96c0c8563017540663b84b9494a5cb489e7fadbddaf7c4cd2d15bb064"),
    (0x37ABA8, "47202c808d63cf7b2e4f43b9ff3a3121a00fc3f152df07e6f0ca9f2dca64953a"),
    (0x37AD28, "117b109996e167beecdada2b8a0c85c466e5aeac0ba84bb53e3679ef9579393e"),
    # Shared unlit shader: initializer 0x402870, water slot 0x8f78e8.
    (0x34E7C0, "e5d0438981d382f7f9eef8254ba2d25712d9122bcace9d5b01fbfb6ba9177cb1"),
    (0x37AE68, "476806c4d5fb2af7d4999df567148ab54eb3fabc7e01236df72aee71eaf58942"),
    (0x37B008, "7548fdd3e247eaf15ee485901c031ee6c0cd4c8c038b0452f34e82e1378a1404"),
    (0x393728, "ce6c00d7eb23048aa35b6266de8e5765ccd306b4bf7d2bfb06ff1007dbf76a84"),
    (0x393F70, "b3d3e7ac041e837a5684fddb49bd117ec958eefae2db89e982215bf5ddb7d038"),
    (0x393928, "53a16290ba35d179f2bad76ebccace504fafa37c57c7aa08cc16837d5afaeeb8"),
    (0x394150, "cf05036bd84f44662ae5158d26e45e5e8e66ff54c1109716447c53184695eb67"),
    (0x393BD8, "47fcba73b39ee3a800b4941e2ef0b35ca3465f55ae4800f0cc2b0c9045ec1e3c"),
    (0x3943E0, "f57901c8872a397b25fef8a758bf231ba29f6af894757dfd598d16e56a48697b"),
    (0x3934D0, "3024b90eed84157773f0e4f1870a40780f37fb0957e3f7daea5ff34710f7e996"),
    (0x369DC8, "0358b9cd1ddd7ae0f31da7a1c0f5d1015f16e29637c24d7bc62afb7b62f4d292"),
]
sources = []
for offset, expected in fixtures:
    source = data[offset:data.index(b"\0", offset)]
    digest = hashlib.sha256(source).hexdigest()
    if digest != expected:
        raise SystemExit(f"Unsupported shader at {offset:#x}: {digest}")
    sources.append('R"stock(' + source.decode("ascii") + ')stock"')
    print(f"PASS stock terrain shader offset={offset:#x} sha256={digest}")
output = root / "build/terrain_reference.h"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("// Generated from local game; do not commit.\n"
                  "const char* const terrainReference[] = {\n" + ",\n".join(sources) + "\n};\n", encoding="ascii")
