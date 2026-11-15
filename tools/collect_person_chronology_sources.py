"""Recover dated evidence from the original local CBDB and Wikipedia archives.

Preserves original date expressions and calendar precision. Extracting a source
does not certify its historical accuracy or resolve a mixed-person identity.
"""
import argparse
import bz2
import collections
import gzip
import hashlib
import html
import json
import re
import sqlite3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATE_FIELDS = re.compile(
    r"(?mi)^\|\s*(birth_date|death_date|birth_year|death_year|date_of_birth|date_of_death|"
    r"reign|tenure|years_active|floruit)\s*=\s*(.*?)(?=^\|\s*[\w ]+\s*=|^\}\}|\Z)", re.S)


def readonly_sqlite(path):
    return sqlite3.connect(f"file:{path.resolve().as_posix()}?mode=ro", uri=True)


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT.parent / "san11world-persons")
    parser.add_argument("--out", type=Path, default=ROOT / "out/person_chronology")
    args = parser.parse_args()
    source = args.source_root / "data/persons"
    roster = load_json(source / "roster.json")
    candidates = {}
    for path in sorted((source / "build").glob("cands_*.jsonl")):
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                row = json.loads(line)
                candidates[row["key"]] = row
    wiki_meta = {row["qid"]: row for row in load_json(source / "src/wiki.json") if row.get("qid")}
    global_meta = {row["wikidata_code"]: row for row in load_json(source / "src/laouenan.json")}
    titles_by_id = {}
    for person in roster:
        names = {person.get("name_orig")}
        for key in person["keys"]:
            record = candidates.get(key, {})
            names.add(record.get("name_orig"))
            qid = record.get("qid")
            if qid in wiki_meta:
                names.add(wiki_meta[qid]["title"])
            if qid in global_meta:
                names.add(global_meta[qid]["name"].replace("_", " "))
        titles_by_id[person["id"]] = sorted(name for name in names if name)
    titles = {name for names in titles_by_id.values() for name in names}
    wiki_dir = source / "_cache/wiki"
    wiki_db = readonly_sqlite(wiki_dir / "titles.sqlite")
    pending, pages = set(titles), {}
    dump = wiki_dir / "enwiki-latest-pages-articles-multistream.xml.bz2"
    args.out.mkdir(parents=True, exist_ok=True)
    cache_path = args.out / "wikipedia_original_pages.json"
    archive_id = [dump.stat().st_size, dump.stat().st_mtime_ns]
    if cache_path.exists():
        cache = load_json(cache_path)
        if cache.get("archive_id") == archive_id:
            pages = cache["pages"]
            pending -= pages.keys()
    streams_read = 0
    with dump.open("rb") as handle:
        for depth in range(5):
            streams = collections.defaultdict(set)
            for title in sorted(pending):
                row = wiki_db.execute("select stream from t where title=?", (title,)).fetchone()
                if row:
                    streams[row[0]].add(title)
            pending = set()
            for stream_id, wanted in sorted(streams.items()):
                lo, hi = wiki_db.execute("select lo,hi from s where i=?", (stream_id,)).fetchone()
                handle.seek(lo)
                text = bz2.decompress(handle.read(hi-lo)).decode("utf-8")
                for page in re.findall(r"<page>(.*?)</page>", text, re.S):
                    title_match = re.search(r"<title>(.*?)</title>", page, re.S)
                    if not title_match:
                        continue
                    title = html.unescape(title_match.group(1))
                    if title not in wanted:
                        continue
                    redirect = re.search(r'<redirect title="([^"]+)"', page)
                    if redirect:
                        destination = html.unescape(redirect.group(1))
                        pages[title] = {"title": title, "redirect": destination}
                        if destination not in pages:
                            pending.add(destination)
                        continue
                    match = re.search(r"<text\b[^>]*>(.*?)</text>", page, re.S)
                    content = html.unescape(match.group(1)) if match else ""
                    revision = re.search(r"<revision>\s*<id>(\d+)</id>", page)
                    page_id = re.search(r"<title>.*?</title>\s*<ns>\d+</ns>\s*<id>(\d+)</id>", page, re.S)
                    fields = {key.casefold(): value.strip() for key, value in DATE_FIELDS.findall(content)}
                    pages[title] = {
                        "title": title, "revision_id": int(revision.group(1)) if revision else None,
                        "page_id": int(page_id.group(1)) if page_id else None,
                        "content_sha256": hashlib.sha256(content.encode()).hexdigest(),
                        "date_fields": fields,
                        "date_categories": re.findall(r"\[\[Category:([^\]]*(?:births|deaths)[^\]]*)\]\]", content),
                        "lead_wikitext": content[:9000],
                        "url": "https://en.wikipedia.org/wiki/" + title.replace(" ", "_"),
                    }
                streams_read += 1
                if streams_read % 400 == 0:
                    print(f"Wikipedia streams read: {streams_read}; pages recovered: {len(pages)}", flush=True)
            if not pending:
                break
    cache_path.write_text(json.dumps({"archive_id": archive_id, "pages": pages}, ensure_ascii=False), encoding="utf-8")
    wanted_ids = {page["page_id"] for page in pages.values() if page.get("page_id")}
    page_qids = {}
    props_path = wiki_dir / "enwiki-latest-page_props.sql.gz"
    with gzip.open(props_path, "rt", encoding="utf-8", errors="replace") as props:
        for line in props:
            if "wikibase_item" not in line:
                continue
            for match in re.finditer(r"\((\d+),'wikibase_item','(Q\d+)'", line):
                pid = int(match.group(1))
                if pid in wanted_ids:
                    page_qids[pid] = match.group(2)
    for page in pages.values():
        if page.get("page_id"):
            page["wikidata_qid"] = page_qids.get(page["page_id"])
    cbdb_path = source / "_cache/cbdb_20261017.sqlite3"
    cbdb_db = readonly_sqlite(cbdb_path)
    cbdb_db.row_factory = sqlite3.Row
    cbdb_ids = {int(key.split(":")[1]) for person in roster for key in person["keys"] if key.startswith("cbdb:")}
    cbdb = {}
    for pid in sorted(cbdb_ids):
        row = cbdb_db.execute("select * from BIOG_MAIN where c_personid=?", (pid,)).fetchone()
        if not row:
            raise ValueError(f"CBDB person missing: {pid}")
        cbdb[pid] = {key: row[key] for key in row.keys() if key in {
            "c_personid", "c_name_chn", "c_index_year", "c_index_year_type_code",
            "c_birthyear", "c_by_nh_code", "c_by_nh_year", "c_by_range", "c_by_month", "c_by_day",
            "c_by_day_gz", "c_by_intercalary", "c_deathyear", "c_dy_nh_code", "c_dy_nh_year",
            "c_dy_range", "c_dy_month", "c_dy_day", "c_dy_day_gz", "c_dy_intercalary",
            "c_death_age", "c_death_age_range", "c_fl_earliest_year", "c_fl_latest_year", "c_notes"}}
        offices = cbdb_db.execute(
            "select o.c_office_chn,p.c_firstyear from POSTED_TO_OFFICE_DATA p "
            "left join OFFICE_CODES o on p.c_office_id=o.c_office_id where p.c_personid=?", (pid,)).fetchall()
        cbdb[pid]["offices"] = [dict(row) for row in offices]
    evidence = []
    for person in roster:
        wiki_pages = []
        seen = set()
        expected_qids = {candidates[key]["qid"] for key in person["keys"]
                         if key in candidates and candidates[key].get("qid")}
        for title in titles_by_id[person["id"]]:
            page = pages.get(title)
            visited = set()
            while page and page.get("redirect") and page["title"] not in visited:
                visited.add(page["title"])
                page = pages.get(page["redirect"])
            if page and not page.get("redirect") and page["title"] not in seen:
                page = dict(page)
                page["identity_check"] = ("qid_matches_source_record" if page.get("wikidata_qid") in expected_qids
                                           else "qid_mismatch" if page.get("wikidata_qid") and expected_qids
                                           else "name_only_identity_unconfirmed")
                page["expected_qids"] = sorted(expected_qids)
                wiki_pages.append(page)
                seen.add(page["title"])
        db_rows = [cbdb[int(key.split(":")[1])] for key in person["keys"] if key.startswith("cbdb:")]
        evidence.append({"id": person["id"], "keys": person["keys"], "wikipedia_originals": wiki_pages,
                         "cbdb_originals": db_rows,
                         "status": "original_sources_extracted_not_historically_adjudicated"})
    if len(evidence) != len(roster):
        raise ValueError("Incomplete evidence export")
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "original_source_evidence.jsonl").write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in evidence), encoding="utf-8")
    summary = {"officers": len(evidence), "wikipedia_original_pages": len(pages), "streams_read": streams_read,
               "officers_with_wikipedia_originals": sum(bool(row["wikipedia_originals"]) for row in evidence),
               "cbdb_original_records": len(cbdb), "cbdb_database": str(cbdb_path.resolve()),
               "wikipedia_pages_with_dump_qid": len(page_qids),
               "cbdb_index_year_is_not_activity_year": True}
    (args.out / "source_collection_summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
