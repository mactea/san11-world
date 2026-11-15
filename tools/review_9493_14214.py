"""Review the overseas/late-added shard against recovered source statements.

Secondary reference checks are kept distinct from primary historical evidence.
No installed game file or source roster is modified.
"""
import collections
import copy
import hashlib
import json
import re
import unicodedata
from pathlib import Path

from calibrate_person_chronology import cbdb_date


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "san11world-persons/data/persons"
OUT = ROOT / "out/person_chronology"


def jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def select_statement(claims):
    """Retain conflicting source statements rather than choosing a midpoint."""
    values = [claim for claim in claims if claim["value"]["precision"] != "unparsed"]
    if not values:
        return None, False
    years = {value["value"].get("year") for value in values}
    relations = {value["value"]["relation"] for value in values}
    signatures = {json.dumps({key: claim["value"].get(key) for key in (
        "year", "year_range", "year_alternatives", "relation", "month", "day")}, sort_keys=True) for claim in values}
    conflict = len(signatures) > 1 and (len(years) != 1 or None in years or len(relations) > 1)
    month_days = {(claim["value"].get("month"), claim["value"].get("day")) for claim in values
                  if claim["value"].get("month")}
    conflict = conflict or len(month_days) > 1
    if conflict:
        return {"alternatives": values, "verification_level": "conflicting_secondary_statements"}, True
    chosen = max(values, key=lambda claim: {"day": 4, "month": 3, "year": 2}.get(claim["value"]["precision"], 1))
    result = copy.deepcopy(chosen["value"])
    result["sources"] = [{key: claim[key] for key in ("title", "url", "field")} for claim in values]
    result["verification_level"] = "original_secondary_statement_checked"
    return result, False


def normalize(text):
    return "".join(char for char in unicodedata.normalize("NFKD", text).casefold()
                   if not unicodedata.combining(char))


def main():
    roster = {row["id"]: row for row in json.loads((SOURCE / "roster.json").read_text(encoding="utf-8"))}
    ledger = {row["id"]: row for row in jsonl(OUT / "ledger.jsonl")}
    calibration = {row["id"]: row for row in jsonl(OUT / "calibration_candidates.jsonl")}
    original = {row["id"]: row for row in jsonl(OUT / "original_source_evidence.jsonl")}
    candidates = {}
    for path in (SOURCE / "build").glob("cands_*.jsonl"):
        candidates.update({row["key"]: row for row in jsonl(path)})
    chunks = {}
    reviews = []
    for pid in range(9493, 14215):
        person, audit, dates, sources = roster[pid], ledger[pid], calibration[pid], original[pid]
        manual = audit["manual_decision"]
        evidence, unresolved = [], []
        article_matches = [page for page in sources["wikipedia_originals"]
                           if page.get("identity_check") == "qid_matches_source_record"]
        for page in article_matches:
            evidence.append({"kind": "original_secondary_reference", "url": page["url"],
                             "title": page["title"], "qid": page.get("wikidata_qid"),
                             "page_id": page.get("page_id"), "revision_id": page.get("revision_id"),
                             "content_sha256": page["content_sha256"],
                             "identity_check": page["identity_check"],
                             "date_fields": page["date_fields"]})
        # Locate cited original passages. A name match is not itself a date proof.
        for key in person["keys"]:
            parts = key.split(":")
            if len(parts) < 4:
                continue
            path = SOURCE / "_cache/chunks" / (parts[1] + ".txt")
            if not path.exists():
                continue
            if path not in chunks:
                text = path.read_text(encoding="utf-8")
                chunks[path] = text, normalize(text), hashlib.sha256(path.read_bytes()).hexdigest()
            text, normalized, digest = chunks[path]
            record = candidates.get(key, {})
            name = record.get("name_orig") or ":".join(parts[3:])
            name = re.sub(r"\s*\([^)]*\)", "", name).strip()
            variants = [name]
            if " " in name:
                variants.append(name.split()[-1])
            found = None
            for variant in variants:
                if len(variant) < 3:
                    continue
                position = normalized.find(normalize(variant))
                if position >= 0:
                    found = position
                    break
            if found is not None:
                evidence.append({"kind": "primary_text_name_attestation_only", "key": key,
                                 "citation": record.get("src"), "source_path": str(path.resolve()),
                                 "source_sha256": digest,
                                 "excerpt": normalized[max(0, found-100):found+400],
                                 "normalization": "Unicode NFKD, casefold, combining marks removed",
                                 "date_inference": "none; requires subject and chronology adjudication"})
        for row in sources["cbdb_originals"]:
            evidence.append({"kind": "original_cbdb_row", "source_id": row["c_personid"],
                             "name": row["c_name_chn"], "birth": cbdb_date(row, "birth"),
                             "death": cbdb_date(row, "death"), "index_year_used_as_activity": False})
        mixed = any(issue in audit["issues"] for issue in (
            "multiple_source_qids_identity_review_required", "multiple_original_names_identity_review_required",
            "manually_confirmed_identity_conflict"))
        duplicate_of = None
        birth, birth_conflict = select_statement([claim for claim in dates["wikipedia_date_claims"] if "birth" in claim["field"]])
        death, death_conflict = select_statement([claim for claim in dates["wikipedia_date_claims"] if "death" in claim["field"]])
        identity = "coherent_in_available_records"
        status = "secondary_fields_extracted" if article_matches else "needs_further_research"
        proposed, basis = None, "existing_game_estimate_not_historically_verified"
        if birth_conflict or death_conflict:
            unresolved.append("conflicting_secondary_date_statements")
            status = "needs_further_research"
        if birth is None:
            unresolved.append("birth_date_not_established_by_checked_sources")
        if death is None:
            unresolved.append("death_date_not_established_by_checked_sources")
        if manual:
            evidence.extend(manual.get("evidence", []))
            if manual["action"] in {"duplicate_of_expanded", "duplicate_of_stock"}:
                duplicate_of, status = manual["canonical_id"], "duplicate"
            elif manual["action"] == "identity_conflict":
                mixed = True
            if manual.get("historical_birth") is not None:
                birth = manual["historical_birth"]
            if manual.get("historical_death") is not None:
                death = manual["historical_death"]
            if manual.get("proposed_appear") is not None:
                proposed, basis = manual["proposed_appear"], manual.get("basis", "manual_decision")
        if mixed:
            confirmed_mix = bool(manual and manual["action"] == "identity_conflict")
            identity = "conflicting_source_identities_require_split" if confirmed_mix else "potential_identity_conflict_requires_review"
            status, proposed = ("identity_conflict" if confirmed_mix else "needs_further_research"), None
            unresolved.append("resolve_identity_before_changing_dates")
            # Keep alternative source statements attached, not a synthetic life.
            birth = {"unassigned_source_statements": birth} if birth else None
            death = {"unassigned_source_statements": death} if death else None
        if duplicate_of is None and not mixed and proposed is None and dates.get("proposed_appear") is not None:
            proposed = dates["proposed_appear"]
            basis = dates["proposal_basis"] + "; requires final primary event adjudication"
        if proposed is not None and death and death.get("year") and death.get("relation", "at") in {"at", "circa"}:
            if proposed > death["year"]:
                proposed = None
                unresolved.append("birth_plus_16_after_reported_death; child_or_identity_review_required")
        if not article_matches and any(item.get("kind") == "primary_text_name_attestation_only" for item in evidence):
            unresolved.append("primary_name_located_but_life_dates_not_independently_adjudicated")
        if not evidence:
            unresolved.append("original_subject_passage_not_located")
        reviews.append({"id": pid, "keys": person["keys"], "name": person["name_full"],
                        "historical_birth": birth, "historical_death": death,
                        "proposed_appear": proposed, "appearance_basis": basis,
                        "duplicate_of": duplicate_of, "identity_status": identity,
                        "evidence": evidence, "review_status": status, "unresolved": sorted(set(unresolved)),
                        "current_game": audit["current_game"],
                        "original_source_fields_recovered": bool(article_matches or sources["cbdb_originals"]),
                        "all_life_dates_independently_verified": False})
    adjudication_paths = [ROOT / "data/persons/reviews/overseas_primary_decisions.json"]
    adjudication_paths.extend(sorted((ROOT / "data/persons/reviews").glob("overseas_parallel_*.json")))
    adjudication_paths.extend(sorted((ROOT / "data/persons/reviews").glob("cross_shard_*.json")))
    for adjudications in adjudication_paths:
        if not adjudications.exists():
            continue
        by_id = {row["id"]: row for row in reviews}
        for decision in json.loads(adjudications.read_text(encoding="utf-8"))["decisions"]:
            review = by_id[decision["id"]]
            if decision["keys"] != review["keys"]:
                raise ValueError(f"Source identity changed: {decision['id']}")
            review["extracted_secondary_birth"] = review["historical_birth"]
            review["extracted_secondary_death"] = review["historical_death"]
            review.update({key: value for key, value in decision.items() if key not in {"id", "keys", "evidence"}})
            review["evidence"].extend(decision["evidence"])
            review["individual_primary_passage_checked"] = True
    path = ROOT / "data/persons/reviews/review_9493_14214.jsonl"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in reviews), encoding="utf-8")
    summary = {"records": len(reviews), "review_status": dict(collections.Counter(row["review_status"] for row in reviews)),
               "appearance_proposals": sum(row["proposed_appear"] is not None for row in reviews),
               "birth_statements": sum(row["historical_birth"] is not None for row in reviews),
               "death_statements": sum(row["historical_death"] is not None for row in reviews)}
    print(json.dumps(summary))


if __name__ == "__main__":
    main()
