"""Merge disjoint historical reviews and materialize a conservative date plan.

Only individually corroborated primary dates enter the strict patch plan.
Other dates, estimates, disputes and identity holds remain visible in the ledger.
"""
import collections
import copy
import hashlib
import json
from pathlib import Path

from audit_person_chronology import is_named_stock_officer


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "san11world-persons/data/persons/roster.json"
SHARDS = ((850, 3999), (4000, 6999), (7000, 9492), (9493, 14214))


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def review_progress_counts(reviews):
    """Count conservative follow-up separately from the legacy status queue."""
    active = [row for row in reviews if row["review_status"] not in {
        "duplicate", "retired_source_slot", "excluded_out_of_period"}]
    pending_statuses = {"needs_further_research", "secondary_source_checked",
                        "secondary_fields_extracted", "identity_conflict"}
    return {
        "conservative_review_queue_records": sum(
            row["review_status"] in pending_statuses
            or row.get("source_identities_all_adjudicated") is not True for row in active),
        "identity_scope_unadjudicated_records": sum(
            row.get("source_identities_all_adjudicated") is False for row in active),
        "identity_scope_flag_missing_records": sum(
            row.get("source_identities_all_adjudicated") is None for row in active),
    }


def explicit_year(value):
    if not isinstance(value, dict) or not isinstance(value.get("year"), int):
        return None
    if value.get("relation", "at") not in {"at", "exact", "not_specified"}:
        return None
    if any(key in value for key in ("alternatives", "unassigned_source_statements", "year_alternatives", "year_range")):
        return None
    precision = value.get("precision", "")
    if any(word in precision for word in ("circa", "approx", "bound", "disput", "range")):
        return None
    return value["year"]


def period_exclusion(person, exclusions):
    keys = set(person["keys"])
    matches = [row for row in exclusions if keys.intersection(row["keys"])]
    if not matches:
        return None
    if len(matches) != 1 or not keys.issubset(matches[0]["keys"]):
        raise ValueError(f"Cross-era identity merged with other sources: {person['id']}")
    return matches[0]


def identity_date_patch_allowed(review, audit):
    if review.get("review_status") == "identity_conflict":
        return False
    flags = {"multiple_source_qids_identity_review_required",
             "multiple_original_names_identity_review_required",
             "manually_confirmed_identity_conflict"}
    unresolved = (len(review.get("keys", [])) > 1
                  or flags.intersection(audit.get("issues", []))
                  or review.get("identity_status") == "identity_not_fully_verified")
    return not unresolved or review.get("source_identities_all_adjudicated") is True


def constrain_game_estimates(person, review):
    """Repair only disproven game estimates; never turn them into life dates."""
    years = {field: person[field] for field in ("birth", "appear", "death")}
    reasons = {}
    for field, value in review.get("estimated_game_years", {}).items():
        if field in years and isinstance(value, int) and explicit_year(review.get("historical_" + field)) is None:
            years[field] = value
            reasons[field] = "individually_reviewed_game_estimate; see source uncertainty"
    activity = [year for year in review.get("attested_activity_years", []) if isinstance(year, int)]
    if isinstance(review.get("proven_activity_year"), int):
        activity.append(review["proven_activity_year"])
    for field in ("verified_activity", "attested_activity_events"):
        for event in review.get(field, []):
            if (year := explicit_year(event)) is not None:
                activity.append(year)
    last_activity = review.get("last_attested_activity")
    if isinstance(last_activity, int):
        activity.append(last_activity)
    elif (year := explicit_year(last_activity)) is not None:
        activity.append(year)
    interval = review.get("attested_activity_interval")
    upper = interval[1] if isinstance(interval, list) and len(interval) == 2 else None
    appearance_bound = review.get("appearance_no_later_than")
    if isinstance(appearance_bound, int):
        upper = appearance_bound if upper is None else min(upper, appearance_bound)
    appearance_bound = review.get("appearance_upper_bound")
    if isinstance(appearance_bound, dict) and appearance_bound.get("relation", "at_or_before") in {
            "at", "not_specified", "at_or_before", "on_or_before", "before"}:
        bound_year = explicit_year(dict(appearance_bound, relation="at"))
        if bound_year is not None:
            bound_year -= appearance_bound.get("relation") == "before"
            upper = bound_year if upper is None else min(upper, bound_year)
    if activity:
        upper = min(activity) if upper is None else min(upper, min(activity))
    if upper is not None and years["appear"] > upper:
        years["appear"] = upper
        reasons["appear"] = "game_estimate_at_attested_activity_upper_bound; not proven first career"
    if upper is not None and years["birth"] >= years["appear"] and explicit_year(review.get("historical_birth")) is None:
        years["birth"] = years["appear"] - 16
        reasons["birth"] = "game_default_16_year_entry_age_to_fit_attestation; historical_birth_unknown"
    alive_floor = max(activity) if activity else None
    if isinstance(interval, list) and len(interval) == 2 and isinstance(interval[0], int):
        alive_floor = interval[0] if alive_floor is None else max(alive_floor, interval[0])
    if alive_floor is not None and years["death"] < alive_floor and explicit_year(review.get("historical_death")) is None:
        years["death"] = alive_floor
        reasons["death"] = "minimum_game_lifespan_to_include_proven_activity; historical_death_unknown"
    if years["appear"] > years["death"] and explicit_year(review.get("historical_death")) is not None:
        years["appear"] = years["death"]
        reasons["appear"] = "latest_possible_game_entry_at_proven_death_year; first_career_unknown"
        if years["birth"] >= years["appear"] and explicit_year(review.get("historical_birth")) is None:
            years["birth"] = years["appear"] - 16
            reasons["birth"] = "game_default_16_year_entry_age_to_fit_death; historical_birth_unknown"
    if years["appear"] <= years["birth"] and explicit_year(review.get("historical_birth")) is not None:
        years["appear"] = min(years["birth"] + 16, years["death"])
        if upper is not None:
            years["appear"] = min(years["appear"], upper)
        reasons["appear"] = "game_default_birth_plus_16_limited_by_attestation_and_death; not historical_first_office"
    changes = {field: years[field] for field in years if years[field] != person[field]}
    return changes, {field: reasons[field] for field in changes}, years


def main():
    source_bytes = SOURCE.read_bytes()
    roster = json.loads(source_bytes)
    by_id = {row["id"]: row for row in roster}
    retired = {pid: {"id": pid, "keys": person["keys"],
                     "source_merged_into": person.get("merged_into"),
                     "basis": "Source roster explicitly marks a retired slot; this does not adjudicate the target's historical identity."}
               for pid, person in by_id.items() if person.get("keys")
               and all(key.startswith("retired:") for key in person["keys"])}
    exclusions = json.loads((ROOT / "data/persons/chronology_exclusions.json").read_text(encoding="utf-8"))["exclusions"]
    excluded = {pid: decision for pid, person in by_id.items()
                if (decision := period_exclusion(person, exclusions)) is not None}
    audits = {row["id"]: row for row in read_jsonl(ROOT / "out/person_chronology/ledger.jsonl")}
    reviews, shard_hashes = {}, {}
    for lo, hi in SHARDS:
        path = ROOT / f"data/persons/reviews/review_{lo:04d}_{hi}.jsonl"
        data = path.read_bytes()
        shard_hashes[str(path.resolve())] = hashlib.sha256(data).hexdigest()
        rows = [json.loads(line) for line in data.decode("utf-8").splitlines() if line.strip()]
        if {row["id"] for row in rows} != set(range(lo, hi+1)) or len(rows) != hi-lo+1:
            raise ValueError(f"Missing or repeated IDs in {path}")
        for row in rows:
            if set(row["keys"]) != set(by_id[row["id"]]["keys"]):
                raise ValueError(f"Review belongs to a different roster revision: {row['id']}")
            reviews[row["id"]] = row
    if set(reviews) != set(by_id):
        raise ValueError("Review coverage differs from the source roster")
    continuation = ROOT / "data/persons/reviews/china_gap_continuation.json"
    if continuation.exists():
        data = continuation.read_bytes()
        shard_hashes[str(continuation.resolve())] = hashlib.sha256(data).hexdigest()
        decisions = json.loads(data.decode("utf-8-sig"))["decisions"]
        if len({row["id"] for row in decisions}) != len(decisions):
            raise ValueError("Repeated continuation IDs")
        for decision in decisions:
            pid = decision["id"]
            if not 850 <= pid <= 9492 or decision["keys"] != by_id[pid]["keys"]:
                raise ValueError(f"Continuation belongs to a different scope or roster: {pid}")
            review = reviews[pid]
            evidence = review.get("evidence", []) + decision.get("evidence", [])
            unresolved = sorted(set(review.get("unresolved", []) + decision.get("unresolved", [])))
            review.update({key: value for key, value in decision.items()
                           if key not in {"id", "keys", "evidence", "unresolved"}})
            review["evidence"] = list({json.dumps(item, sort_keys=True, ensure_ascii=False): item
                                       for item in evidence}.values())
            review["unresolved"] = unresolved
    for review in reviews.values():
        late_keys = [key for key in review["keys"] if key.startswith("sl:")]
        if late_keys:
            review.setdefault("source_limitations", []).append({
                "source_keys": late_keys,
                "source_layer": "Ming_reconstruction_not_direct_Cui_Hong_transmission",
                "path": "san11world-persons/data/persons/_cache/texts/十六国春秋.txt",
                "offset": 0,
                "excerpt": "十六国春秋一百卷，旧本题魏崔鸿撰\n\n实则明嘉兴屠乔孙项琳之伪本也。",
                "policy": "Retain the late reconstruction's statements; independently review dated evidence before treating a date as corroborated by an early historical source.",
            })
    for pid, decision in excluded.items():
        reviews[pid].update(review_status="excluded_out_of_period",
                            identity_status="source_identifies_cross_era_guest",
                            period_exclusion=decision)
    for pid in retired:
        reviews[pid].update(review_status="retired_source_slot", source_slot_retirement=retired[pid])
    duplicate_holds = [{"id": pid, "candidate_duplicate_of": row["duplicate_of"],
                        "reason": "whole_source_identity_not_yet_adjudicated"}
                       for pid, row in reviews.items() if row.get("duplicate_of") is not None
                       and (row["review_status"] != "duplicate" or not identity_date_patch_allowed(row, audits[pid]))]
    held_duplicates = {row["id"] for row in duplicate_holds}
    redirects = {pid: row["duplicate_of"] for pid, row in reviews.items()
                 if row.get("duplicate_of") is not None and pid not in held_duplicates}
    for old in list(redirects):
        new, seen = redirects[old], {old}
        while new in redirects:
            if new in seen:
                raise ValueError(f"Duplicate redirect cycle involving {old}")
            seen.add(new)
            new = redirects[new]
        if not isinstance(new, int) or not (is_named_stock_officer(new) or new in by_id):
            raise ValueError(f"Invalid duplicate target: {old} -> {new}")
        redirects[old] = new
        if new in excluded or new in retired:
            raise ValueError(f"Duplicate redirects to excluded officer: {old} -> {new}")
    patches, estimated_patches, blocked, canonical = [], [], [], []
    for pid in sorted(reviews):
        review = reviews[pid]
        person = copy.deepcopy(by_id[pid])
        if pid in redirects or pid in excluded or pid in retired:
            continue
        changes = {}
        if review["review_status"] == "primary_text_corroborated":
            for field in ("birth", "death"):
                year = explicit_year(review.get("historical_" + field))
                if year is not None and year != person[field]:
                    changes[field] = year
            appearance = review.get("proposed_appear")
            if isinstance(appearance, int) and appearance != person["appear"]:
                changes["appear"] = appearance
        years = {field: changes.get(field, person[field]) for field in ("birth", "appear", "death")}
        if changes and not identity_date_patch_allowed(review, audits[pid]):
            blocked.append({"id": pid, "keys": person["keys"], "proposed": years,
                            "reason": "all_merged_source_identities_must_be_adjudicated_before_date_patch"})
            changes = {}
        proposed_person = dict(person, **changes)
        estimate_changes, estimate_reasons = {}, {}
        if review["review_status"] in {"primary_text_corroborated", "source_checked_dates_unknown"} and identity_date_patch_allowed(review, audits[pid]):
            estimate_changes, estimate_reasons, years = constrain_game_estimates(proposed_person, review)
        if (changes or estimate_changes) and not years["birth"] < years["appear"] <= years["death"]:
            blocked.append({"id": pid, "keys": person["keys"], "proposed": years,
                            "reason": "requires_joint_lifespan_review_before_game_export"})
            changes = {}
            estimate_changes = {}
        if changes:
            patches.append({"id": pid, "keys": person["keys"], "name": person["name_full"],
                            "before": {field: person[field] for field in changes}, "changes": changes,
                            "appearance_is_game_rule": "appear" in changes and not review.get("proposed_appear"),
                            "source_limitations": review.get("source_limitations", []),
                            "evidence": review["evidence"]})
            person.update(changes)
        if estimate_changes:
            estimated_patches.append({"id": pid, "keys": person["keys"], "name": person["name_full"],
                                      "before": {field: person[field] for field in estimate_changes},
                                      "changes": estimate_changes, "basis": estimate_reasons,
                                      "source_limitations": review.get("source_limitations", []),
                                      "historical_dates_changed": False, "evidence": review["evidence"]})
            person.update(estimate_changes)
        for field in ("father", "mother", "spouse", "sworn", "blood"):
            if field in person:
                person[field] = redirects.get(person[field], person[field])
                if person[field] in excluded or person[field] in retired:
                    person[field] = pid if field == "blood" else -1
        for field in ("liked", "disliked"):
            person[field] = list(dict.fromkeys(redirects.get(value, value) for value in person[field]
                                              if redirects.get(value, value) != pid
                                              and redirects.get(value, value) not in excluded
                                              and redirects.get(value, value) not in retired))
        person["chronology_review"] = review
        person["chronology_review"]["game_year_policy"] = "Unknown historical dates remain unknown; retain game estimates except contradictions with individually corroborated chronology, as authorized by the user."
        person["chronology_review"]["game_year_annotations"] = {
            field: {"value": person[field], "basis": (
                "individually_corroborated_year" if review["review_status"] == "primary_text_corroborated"
                and identity_date_patch_allowed(review, audits[pid])
                and explicit_year(review.get("historical_" + field)) == person[field]
                else "game_estimate; exact_historical_year_not_established_for_this_identity")}
            for field in ("birth", "death")}
        person["chronology_review"]["game_year_annotations"]["appear"] = {
            "value": person["appear"], "basis": estimate_reasons.get("appear", review.get("appearance_basis", "game_estimate"))}
        for field, reason in estimate_reasons.items():
            person["chronology_review"]["game_year_annotations"][field]["basis"] = reason
        review["original_game_year_basis"] = person.get("year_basis")
        labels = []
        for field, label in (("birth", "生年"), ("death", "卒年")):
            basis = review["game_year_annotations"][field]["basis"]
            value = review.get("historical_" + field) or {}
            if basis == "individually_corroborated_year":
                note = "原文年龄推算，非确切生日" if "infer" in value.get("precision", "") else "有原历史料纪年"
            else:
                note = "游戏估算，史实日期未确证"
            labels.append(label + "：" + note)
        labels.append("登场：史料活动上界" if person["appear"] == review.get("proposed_appear")
                      else "登场：游戏估算")
        person["year_basis"] = "；".join(labels)
        person["chronology_review"]["merged_alias_reviews"] = [reviews[old] for old, new in redirects.items() if new == pid]
        canonical.append(person)
    status = collections.Counter(row["review_status"] for row in reviews.values())
    out = ROOT / "out/person_chronology"
    summary = {
        "officers_covered": len(reviews), "review_status": dict(status),
        "pending_primary_or_identity_review_records": sum(status[k] for k in (
            "needs_further_research", "secondary_source_checked", "secondary_fields_extracted", "identity_conflict")),
        **review_progress_counts(reviews.values()),
        "historical_verification_complete": all(row.get("all_life_dates_independently_verified") is True for row in reviews.values()),
        "unknown_date_policy": "Historical dates unknown; preserve game estimates unless individually corroborated chronology disproves them. Such repairs remain game estimates, per user authorization.",
        "important": "Full ID coverage is not full historical verification. Reference checks, disputed dates and unknown dates retain their own status.",
        "duplicate_records_removed_from_canonical_pool": len(redirects), "canonical_expanded_records": len(canonical),
        "out_of_period_records_removed_from_canonical_pool": len(excluded),
        "retired_source_slots_removed_from_canonical_pool": len(retired),
        "duplicate_redirect_holds": len(duplicate_holds),
        "strict_date_patch_records": len(patches), "date_patch_holds": len(blocked),
        "estimated_game_patch_records": len(estimated_patches),
        "canonical_pool_is_game_import_ready": False,
        "canonical_pool_note": "Stable source IDs are intentionally retained; existing binary importer requires consecutive IDs and cannot consume this sparse pool directly.",
        "source_roster_sha256": hashlib.sha256(source_bytes).hexdigest(), "review_shard_sha256": shard_hashes,
        "game_files_changed": 0,
    }
    out.mkdir(parents=True, exist_ok=True)
    for name, value in (("merged_review_summary.json", summary), ("duplicate_redirects.json", redirects),
                        ("retired_source_slots.json", list(retired.values())),
                        ("period_exclusions.json", [{"id": pid, **decision} for pid, decision in excluded.items()]),
                        ("strict_date_patch_plan.json", patches), ("estimated_game_patch_plan.json", estimated_patches),
                        ("duplicate_redirect_holds.json", duplicate_holds),
                        ("date_patch_holds.json", blocked)):
        (out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    (out / "reviewed_roster.jsonl").write_text("".join(json.dumps(reviews[pid], ensure_ascii=False)+"\n"
                                                   for pid in sorted(reviews)), encoding="utf-8")
    (out / "canonical_roster_reviewed.jsonl").write_text("".join(json.dumps(row, ensure_ascii=False)+"\n"
                                                             for row in canonical), encoding="utf-8")
    pending = [row for row in reviews.values() if row["review_status"] in {
        "needs_further_research", "secondary_source_checked", "secondary_fields_extracted", "identity_conflict"}]
    priority = {"identity_conflict": 0, "needs_further_research": 1,
                "secondary_source_checked": 2, "secondary_fields_extracted": 3}
    pending.sort(key=lambda row: (priority[row["review_status"]], -len(row["keys"]), row["id"]))
    (out / "pending_primary_review_queue.jsonl").write_text("".join(
        json.dumps({"id": row["id"], "name": row["name"], "region": audits[row["id"]]["region"],
                    "status": row["review_status"], "keys": row["keys"], "unresolved": row["unresolved"]},
                   ensure_ascii=False)+"\n" for row in pending), encoding="utf-8")
    if SOURCE.read_bytes() != source_bytes:
        raise ValueError("The source roster changed during materialization")
    print(json.dumps({key: summary[key] for key in ("officers_covered", "review_status", "duplicate_records_removed_from_canonical_pool",
                                                   "strict_date_patch_records", "date_patch_holds")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
