"""Append Hero's Dawn to the installed playlist, preserving every existing archive byte."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import struct
import subprocess

root = Path(__file__).resolve().parent
game = Path(json.loads((root.parents[1] / "project.json").read_text(encoding="utf-8"))["game_dir"])
source = Path(r"C:\Users\garib\Downloads\横山菁儿+-+英雄的黎明.flac")
config = game / "worldmusic.ini"
assert b"track2=" not in config.read_bytes(), "Second track already configured; inspect before repeating"
archive = game / "Media/san11pkres.bin"
audio = game / "Media/custom_music/hero_dawn.ogg"
kovs_file = audio.with_suffix(".kovs")
assert not audio.exists() and not kovs_file.exists()
backup = game / "music-backups" / ("hero-dawn-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
backup.mkdir(parents=True)
for name in ("winmm.dll", "worldmusic.ini"):
    shutil.copy2(game / name, backup / name)
old_bytes = archive.read_bytes()
offset = len(old_bytes)
subprocess.run(["ffmpeg", "-v", "error", "-n", "-i", str(source), "-map", "0:a:0",
                "-c:a", "libvorbis", "-q:a", "9", "-ar", "44100", "-ac", "2", str(audio)], check=True)
subprocess.run(["ffmpeg", "-v", "error", "-i", str(audio), "-f", "null", "-"], check=True)
ogg = bytearray(audio.read_bytes())
assert ogg[:4] == b"OggS"
for index in range(256):
    ogg[index] ^= index
kovs = b"KOVS" + struct.pack("<I", len(ogg)) + bytes(24) + ogg
kovs_file.write_bytes(kovs)
manifest = {"source": str(source), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "previous_archive_size": offset, "previous_archive_sha256": hashlib.sha256(old_bytes).hexdigest(),
            "new_track_size": len(kovs), "new_track_sha256": hashlib.sha256(kovs).hexdigest(),
            "order": ["current season original", "langshu medley", "Hero's Dawn"],
            "status": "installing"}
manifest_file = backup / "manifest.json"
manifest_file.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
with archive.open("ab") as stream:
    stream.write(kovs)
with archive.open("rb") as stream:
    assert stream.read(offset) == old_bytes
    assert stream.read() == kovs
with config.open("ab") as stream:
    stream.write(f"track2=Media\\custom_music\\hero_dawn.kovs\narchive_offset2={offset}\n".encode("ascii"))
shutil.copy2(root / "build/winmm.dll", game / "winmm.dll")
manifest["status"] = "installed; runtime validation pending"
manifest_file.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
print("Installed second track:", kovs_file)
print("New offset:", offset, "size:", len(kovs))
print("Backup:", backup)
