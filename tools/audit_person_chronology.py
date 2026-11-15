"""Trace every expanded officer's chronology without treating estimates as facts.

Reads the persons worktree; writes a per-person review ledger and review queue.
Does not modify the source roster, installed scenarios, or saves.
"""
import argparse
import collections
import hashlib
import json
import re
import unicodedata
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def is_named_stock_officer(koei_id):
    # Original game slots: ordinary officers, named NPCs, and ancient officers.
    # Remaining slots are generic roles or unused placeholders.
    return (0 <= koei_id < 670 or 699 <= koei_id <= 711
            or koei_id in {729, 730} or 800 <= koei_id <= 831)


def stock_name_aliases(person):
    # Individually identified variants only; matches remain review candidates.
    aliases = {306: ("秦密",), 821: ("李績", "李世績", "李世勣", "徐世績", "徐世勣")}
    return (person["sei"] + person["mei"],) + aliases.get(person["koei_id"], ())


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT.parent / "san11world-persons")
    parser.add_argument("--out", type=Path, default=ROOT / "out/person_chronology")
    args = parser.parse_args()
    source = args.source_root / "data/persons"
    roster_path = source / "roster.json"
    merged_path = source / "build/merged.jsonl"
    koei_path = source / "src/koei.json"
    decisions_path = ROOT / "data/persons/chronology_decisions.json"
    watched = [roster_path, merged_path, koei_path]
    before = {str(path.resolve()): fingerprint(path) for path in watched}
    roster = read_json(roster_path)
    merged = {key: row for row in read_jsonl(merged_path) for key in row["keys"]}
    koei = read_json(koei_path)
    decisions = read_json(decisions_path)
    candidates = {}
    for path in sorted((source / "build").glob("cands_*.jsonl")):
        for row in read_jsonl(path):
            key = row["key"]
            if key in candidates and candidates[key] != row:
                raise ValueError(f"Conflicting source records: {key}")
            candidates[key] = row
    wiki = {row["qid"]: row for row in read_json(source / "src/wiki.json") if row.get("qid")}
    cbdb = {str(row["cbdb_id"]): row for row in read_json(source / "src/cbdb.json")}
    import opencc
    converter = opencc.OpenCC("t2s")

    def normalize(name):
        value = converter.convert(unicodedata.normalize("NFKC", name or "")).casefold()
        return re.sub(r"[\s·・，,。.]", "", value)

    groups = {field: collections.defaultdict(list) for field in ("qid", "name_orig", "name_full")}
    original_names = collections.defaultdict(list)
    for person in koei:
        if is_named_stock_officer(person["koei_id"]):
            for name in stock_name_aliases(person):
                original_names[normalize(name)].append(person)
    for person in roster:
        for field, index in groups.items():
            if person.get(field):
                index[normalize(person[field])].append(person["id"])
    duplicate_groups = {field: {key: ids for key, ids in index.items() if len(ids) > 1}
                        for field, index in groups.items()}
    decision_index = {}
    for decision in decisions["decisions"]:
        for key in decision["keys"]:
            if key in decision_index:
                raise ValueError(f"Multiple manual decisions for {key}")
            decision_index[key] = decision
    ledger, issue_counts, queue = [], collections.Counter(), []
    region_counts = collections.Counter()
    for person in roster:
        matches = {id(merged[key]): merged[key] for key in person["keys"] if key in merged}
        if len(matches) != 1:
            raise ValueError(f"No unique merged source for officer {person['id']}")
        upstream = next(iter(matches.values()))
        records = [candidates[key] for key in person["keys"] if key in candidates]
        issues = []
        claims = {field: [] for field in ("birth", "death", "appear", "years")}
        for record in records:
            family = record["key"].split(":")[0]
            kind = ("game_setting" if family in {"zhsan", "qxz", "tprsn"}
                    else "secondary_database" if family in {"gl", "wk", "cbdb", "syr"}
                    else "historical_text_extraction_not_independently_verified")
            for field in claims:
                value = record.get(field)
                if value is not None and value != [None, None]:
                    claims[field].append({"value": value, "key": record["key"],
                                          "source_kind": kind, "citation": record.get("src")})
        for field in ("birth", "death"):
            if upstream.get(field) is None:
                issues.append(f"{field}_filled_without_source_date")
            if len({claim["value"] for claim in claims[field]}) > 1:
                issues.append(f"conflicting_{field}_claims")
        source_qids = sorted({row["qid"] for row in records if row.get("qid")})
        original_names_in_sources = sorted({row["name_orig"] for row in records if row.get("name_orig")})
        if len(source_qids) > 1:
            issues.append("multiple_source_qids_identity_review_required")
        if len({normalize(name) for name in original_names_in_sources}) > 1:
            issues.append("multiple_original_names_identity_review_required")
        activity = [year for year in upstream.get("years", []) if isinstance(year, (int, float))]
        if activity and (min(activity) < person["birth"] or max(activity) > person["death"]):
            issues.append("source_activity_outside_game_lifespan")
        if person["year_basis"] in {"政权年代", "默认"}:
            issues.append("chronology_from_polity_or_default")
        if not (person["birth"] < person["appear"] <= person["death"]):
            issues.append("invalid_game_date_order")
        for field in ("qid", "name_orig", "name_full"):
            if normalize(person.get(field)) in duplicate_groups[field]:
                issues.append(f"duplicate_{field}_candidate")
        original_matches = {}
        names = {normalize(person["name_full"]), normalize(person["sei"] + person["mei"])}
        for name in names:
            for original in original_names[name]:
                if max(person["birth"], original["birth"]) <= min(person["death"], original["death"]):
                    original_matches[original["koei_id"]] = {
                        "id": original["koei_id"], "name": original["sei"] + original["mei"],
                        "birth": original["birth"], "death": original["death"]}
        if original_matches:
            issues.append("possible_duplicate_of_stock_officer")
        references = []
        for key in person["keys"]:
            parts = key.split(":")
            if len(parts) >= 4:
                chunk = source / "_cache/chunks" / (parts[1] + ".txt")
                if chunk.exists():
                    references.append({"key": key, "path": str(chunk.resolve()),
                                       "note": "Source chunk located; has not been individually adjudicated."})
        raw_cbdb = [cbdb[key.split(":")[1]] for key in person["keys"]
                    if key.startswith("cbdb:") and key.split(":")[1] in cbdb]
        cached_wiki = wiki.get(person.get("qid"))
        decision_matches = {id(decision_index[key]): decision_index[key]
                            for key in person["keys"] if key in decision_index}
        if len(decision_matches) > 1:
            raise ValueError(f"Conflicting decisions for officer {person['id']}")
        decision = next(iter(decision_matches.values()), None)
        if decision and decision["action"] == "identity_conflict":
            issues.append("manually_confirmed_identity_conflict")
        entry = {
            "id": person["id"], "name_full": person["name_full"], "name_orig": person.get("name_orig"),
            "region": person["region"], "qid": person.get("qid"), "keys": person["keys"],
            "current_game": {field: person[field] for field in ("birth", "appear", "death", "year_basis")},
            "upstream_claims": claims,
            "source_qids": source_qids, "original_names_in_sources": original_names_in_sources,
            "historical_birth": decision.get("historical_birth") if decision else None,
            "historical_death": decision.get("historical_death") if decision else None,
            "proposed_appear": decision.get("proposed_appear") if decision else None,
            "manual_decision": decision,
            "date_review_status": "partially_reviewed" if decision else "pending_primary_source_review",
            "source_chunks": references,
            "cached_wikipedia": ({key: cached_wiki.get(key) for key in ("title", "b", "d", "info", "lead")}
                                 if cached_wiki else None),
            "cbdb_identity_evidence": [{key: row.get(key) for key in ("cbdb_id", "name", "kin", "sources")}
                                       for row in raw_cbdb],
            "stock_matches": list(original_matches.values()), "issues": issues,
        }
        ledger.append(entry)
        region_counts[person["region"]] += 1
        issue_counts.update(issues)
        queue.append({"id": person["id"], "name": person["name_full"], "region": person["region"],
                      "issues": issues, "priority": 0 if decision else 1 if any(
                          issue in issues for issue in ("source_activity_outside_game_lifespan",
                          "duplicate_qid_candidate", "possible_duplicate_of_stock_officer")) else 2 if issues else 3})
    queue.sort(key=lambda item: (item["priority"], item["id"]))
    ids = {entry["id"] for entry in ledger}
    if len(ids) != len(roster) or len(ledger) != len(roster):
        raise ValueError("Incomplete or duplicated ledger")
    for path in watched:
        if fingerprint(path) != before[str(path.resolve())]:
            raise ValueError(f"Source changed during audit: {path}")
    summary = {
        "scope": "All expanded officers in the current source roster; no game deployment performed.",
        "historical_verification_complete": False,
        "important": "Source claims and located source chunks are not independent historical verification. Null means unverified, not proof that a date cannot be known.",
        "officers_scanned": len(ledger), "regions": dict(region_counts.most_common()),
        "issue_counts": dict(issue_counts), "records_with_flags": sum(bool(entry["issues"]) for entry in ledger),
        "records_with_manual_decisions": sum(entry["manual_decision"] is not None for entry in ledger),
        "duplicate_groups": duplicate_groups,
        "game_files_changed": 0, "source_fingerprints": before,
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "ledger.jsonl").write_text("".join(json.dumps(entry, ensure_ascii=False) + "\n"
                                                for entry in ledger), encoding="utf-8")
    corrections = [{"id": entry["id"], "keys": entry["keys"], **entry["manual_decision"]}
                   for entry in ledger if entry["manual_decision"]]
    for name, value in (("summary.json", summary), ("review_queue.json", queue),
                        ("confirmed_corrections.json", corrections)):
        (args.out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: summary[key] for key in ("officers_scanned", "records_with_flags",
                                                  "records_with_manual_decisions", "issue_counts")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
