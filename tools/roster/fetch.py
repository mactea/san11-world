"""下载名单用的公开数据 → data/persons/_cache/（已有的跳过）

  laouenan.csv.gz   全球知名人物交叉核实数据库（HuggingFace jstet/laouenan-notable-people）
  cbdb_*.sqlite3    CBDB（HuggingFace cbdb/cbdb-sqlite）
  texts/*.txt       史书全文（GitHub garychowcmu/daizhigev20 殆知阁）
国内网络用 hf-mirror.com；HF_ENDPOINT 环境变量可改。"""
import os
import urllib.parse
import urllib.request
import zipfile
from common import CACHE

HF = os.environ.get("HF_ENDPOINT", "https://hf-mirror.com")
FILES = {
    "laouenan.csv.gz": f"{HF}/datasets/jstet/laouenan-notable-people/resolve/main/cross-verified-database.csv.gz",
    "cbdb.zip": f"{HF}/datasets/cbdb/cbdb-sqlite/resolve/main/history/cbdb_202610/cbdb_20261017.zip",
}
DZG = "https://raw.githubusercontent.com/garychowcmu/daizhigev20/master/"
TEXTS = ["史藏/正史/三国志.txt", "史藏/正史/后汉书.txt", "史藏/正史/晋书.txt", "史藏/正史/宋书.txt", "史藏/正史/魏书.txt",
         "史藏/载记/十六国春秋.txt", "史藏/地理/华阳国志.txt", "史藏/编年/资治通鉴.txt", "史藏/编年/后汉纪.txt",
         "史藏/传记/高僧传.txt", "子藏/笔记/世说新语.txt"]


def get(url, path):
    if path.exists() and path.stat().st_size > 0:
        return
    print("下载", url)
    path.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "san11world-roster"})
    with urllib.request.urlopen(req, timeout=600) as r, open(str(path) + ".part", "wb") as f:
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            f.write(b)
    os.replace(str(path) + ".part", path)


for name, url in FILES.items():
    get(url, CACHE / name)
if not list(CACHE.glob("cbdb_*.sqlite3")):
    zipfile.ZipFile(CACHE / "cbdb.zip").extractall(CACHE)
for t in TEXTS:
    get(DZG + urllib.parse.quote(t), CACHE / "texts" / t.split("/")[-1])
print("完成")
