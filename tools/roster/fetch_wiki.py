"""下载英文维基百科转储到 data/persons/_cache/wiki/（分段并行、可断点续传）

    python fetch_wiki.py            # 全文（多流版，约 26GB）、索引、跨语言链接、页面属性（Wikidata 编号）"""
import concurrent.futures as cf
import json
import os
import sys
import urllib.request
from common import CACHE

BASE = "https://dumps.wikimedia.org/enwiki/latest/"
FILES = ["enwiki-latest-pages-articles-multistream-index.txt.bz2", "enwiki-latest-langlinks.sql.gz",
         "enwiki-latest-page_props.sql.gz", "enwiki-latest-pages-articles-multistream.xml.bz2"]
PART = 64 * 1024 * 1024
WORKERS = 3                                       # dumps.wikimedia.org 每个 IP 只允许少数几个连接
OUT = CACHE / "wiki"
UA = {"User-Agent": "san11world-roster/1.0 (personal research; python urllib)"}   # 没有 UA 会 403


def size_of(url):
    req = urllib.request.Request(url, method="HEAD", headers=UA)
    with urllib.request.urlopen(req, timeout=60) as r:
        return int(r.headers["Content-Length"])


def get_part(url, path, lo, hi, done_path):
    for attempt in range(8):
        try:
            req = urllib.request.Request(url, headers={"Range": f"bytes={lo}-{hi}", **UA})
            with urllib.request.urlopen(req, timeout=120) as r, open(path, "r+b") as f:
                f.seek(lo)
                pos = lo
                while chunk := r.read(1 << 20):
                    f.write(chunk)
                    pos += len(chunk)
            if pos == hi + 1:
                with open(done_path, "a") as d:
                    d.write(f"{lo}\n")
                return lo
        except Exception as e:                     # 断线重试
            print("retry", lo, attempt, e, flush=True)
    raise RuntimeError(f"part {lo} failed")


def fetch(name):
    url = BASE + name
    path = OUT / name
    done_path = OUT / (name + ".parts")
    total = size_of(url)
    if path.exists() and path.stat().st_size == total and not done_path.exists():
        print(name, "已存在", flush=True)
        return
    if not path.exists() or path.stat().st_size != total:
        with open(path, "wb") as f:
            f.truncate(total)
    done = set(int(x) for x in open(done_path).read().split()) if done_path.exists() else set()
    todo = [(lo, min(lo + PART, total) - 1) for lo in range(0, total, PART) if lo not in done]
    print(name, total // 2 ** 20, "MB，剩", len(todo), "段", flush=True)
    with cf.ThreadPoolExecutor(WORKERS) as ex:
        for i, _ in enumerate(cf.as_completed([ex.submit(get_part, url, path, lo, hi, done_path) for lo, hi in todo])):
            if i % 20 == 0:
                print(name, f"{i + 1}/{len(todo)}", flush=True)
    os.remove(done_path)
    print(name, "完成", flush=True)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for n in (sys.argv[1:] or FILES):
        fetch(n)
