"""Install the additive playlist; original archive entries remain intact."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
from datetime import datetime

source = Path(__file__).resolve().parent
project = json.loads((source.parents[1] / "project.json").read_text(encoding="utf-8"))
game = Path(project["game_dir"])
ogg = Path(r"C:\Users\garib\Downloads\霸王的大陆_网上重编试听\狼叔_游戏BGM.ogg")
expected = "67217a11a27eb39e65c8ce7c238d993b12c686a3658eaaf688080d75b6b17b3f"
data = bytearray(ogg.read_bytes())
assert hashlib.sha256(data).hexdigest() == expected, "Unexpected source audio"
assert data[:4] == b"OggS"
for i in range(256):
    data[i] ^= i
kovs = b"KOVS" + struct.pack("<I", len(data)) + bytes(24) + data
targets = [game / "winmm.dll", game / "worldmusic.ini", game / "Media/custom_music/langshu.kovs"]
assert not any(p.exists() for p in targets), "Add-on already exists; inspect before reinstalling"
backup = game / "music-backups" / ("additive-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
backup.mkdir(parents=True)
originals = [game / "d3d9.dll", game / "san11pk_world.exe"]
hashes = {str(p.relative_to(game)): hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
archive = game / "Media/san11pkres.bin"
archive_before = archive.read_bytes()
# Reuse an identical unreferenced tail from an earlier installation if present.
if archive_before.endswith(kovs):
    offset = len(archive_before) - len(kovs)
else:
    offset = len(archive_before)
    shutil.copy2(archive, backup / "san11pkres.bin")
    with archive.open("ab") as stream:
        stream.write(kovs)
assert archive.read_bytes()[:len(archive_before)] == archive_before
targets[2].parent.mkdir(parents=True, exist_ok=True)
targets[2].write_bytes(kovs)
targets[1].write_text("[music]\nenabled=1\ntrack=Media\\custom_music\\langshu.kovs\n"
                      f"archive_offset={offset}\n", encoding="ascii")
shutil.copy2(source / "build/winmm.dll", targets[0])
assert all(hashlib.sha256((game / p).read_bytes()).hexdigest() == digest for p, digest in hashes.items())
(backup / "manifest.json").write_text(json.dumps({"original_files_unchanged": hashes,
    "added": {str(p.relative_to(game)): hashlib.sha256(p.read_bytes()).hexdigest() for p in targets},
    "source_ogg_sha256": expected, "archive_offset": offset,
    "archive_prefix_preserved_sha256": hashlib.sha256(archive_before).hexdigest(),
    "mode": "seasonal-original / custom-medley alternating"}, indent=2), encoding="utf-8")
print("Installed:", *(str(p) for p in targets), "Manifest: " + str(backup), sep="\n")
