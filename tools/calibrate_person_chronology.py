"""Build dated primary-source evidence and a conservative appearance proposal.

The annual context of a mention alone is not an activity date. Only literal,
direct subject/event matches outside retrospective and quoted passages qualify.
Proposals remain separate from installed game data and unresolved identities.
"""
import argparse
import collections
import hashlib
import html
import json
import re
from pathlib import Path

import opencc


ROOT = Path(__file__).resolve().parents[1]
NUMERALS = dict(zip("零〇一二三四五六七八九", [0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]))
HEADER = re.compile(r"(?m)^[　 ]*[^\n，。：“]{2,20}?[元一二三四五六七八九十]+年(?:（[^\n]*）)?$")
RETROSPECTIVE = re.compile(r"先是|初，|初、|初。|昔|尝|嘗|追述|追赠|追贈|追封|故[\u4e00-\u9fff]{1,8}(?:为|為|曰)")
DEATH = r"(?:卒|薨|崩|病死|自杀|自殺|自缢|自縊|伏诛|伏誅|被杀|被殺|为[^。；\n“”]{1,12}所(?:杀|诛|斩))"
ACTIVITY = r"(?:为|為|任|拜|迁|遷|率|领|領|讨|討|攻|屯|遣|上书|上書|降|起兵|反叛|进军|進軍)"
MONTHS = {month.casefold(): number for number, month in enumerate(
    ("January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"), 1)}


def wikipedia_date(raw):
    """Read an explicit date field, retaining alternatives, bounds and traditions."""
    original = raw
    raw = html.unescape(raw)
    raw = re.sub(r"<ref\b[^>]*>.*?</ref>|<ref\b[^>]*/>", "", raw, flags=re.S)
    raw = re.sub(r"\{\{(?:sfn|sfnp|harvnb|citation|cite\s+\w+)\b.*?\}\}", "", raw, flags=re.I | re.S)
    raw = re.sub(r"\[\[(?:[^\]|]+\|)?([^\]]+)\]\]", r"\1", raw)
    tradition = bool(re.search(r"supposed|tradition|legend|disputed", raw, re.I))
    raw = re.sub(r"\([^)]*\baged?\b[^)]*\)", "", raw, flags=re.I)
    relation = ("after" if re.search(r"\bafter\b", raw, re.I) else
                "before" if re.search(r"\bbefore\b", raw, re.I) else
                "circa" if re.search(r"circa|\bc\.?\s*(?:AD\s*)?\d|\bca\.|around|about|approx|~", raw, re.I) else "at")
    value = {"raw": original, "relation": relation, "tradition_or_dispute": tradition,
             "calendar": "as_reported_in_source_not_converted", "precision": "unparsed"}
    template = re.search(r"\{\{\s*((?:birth|death)[ _](?:date|year)(?:[ _]and[ _]age)?|[bd](?:da|ya))\s*\|([^{}]+)\}\}", raw, re.I)
    if template:
        numbers = [int(part.strip()) for part in template.group(2).split("|") if re.fullmatch(r"\s*\d+\s*", part)]
        if numbers:
            value.update(year=numbers[0], precision="year")
            if "year" not in template.group(1).casefold() and template.group(1).casefold() not in {"bya", "dya"}:
                if len(numbers) >= 3 and 1 <= numbers[1] <= 12 and 1 <= numbers[2] <= 31:
                    value.update(month=numbers[1], day=numbers[2], precision="day")
            return value
    raw = re.sub(r"\{\{(?:circa|circa2|c\.)\s*\|?([^{}]*)\}\}", r"\1", raw, flags=re.I)
    if re.search(r"\bcentury\b", raw, re.I):
        centuries = [int(number) for number in re.findall(r"(\d)(?:st|nd|rd|th)", raw)]
        if centuries:
            value.update(year_range=[(min(centuries)-1)*100+1, max(centuries)*100], precision="century")
        return value
    # Remaining unknown templates can contain publication years; do not parse them.
    raw = re.sub(r"\{\{.*?\}\}", "", raw, flags=re.S)
    month_names = "|".join(MONTHS)
    year_text = re.sub(r"\b\d{1,2}\s+(" + month_names + r")\b", r"\1", raw, flags=re.I)
    year_text = re.sub(r"\b(" + month_names + r")\s+\d{1,2}\b", r"\1", year_text, flags=re.I)
    years = sorted({int(year) for year in re.findall(r"(?<!\d)(\d{2,4})(?!\d)", year_text)})
    if re.search(r"\bBCE?\b", raw):
        value["era"] = "BCE"
        value["precision"] = "requires_calendar_review"
        return value
    if len(years) > 1:
        value.update(year_alternatives=years, precision="alternatives_or_range")
    elif years:
        year = years[0]
        value.update(year=year, precision="year")
        date = re.search(r"\b(\d{1,2})\s+(" + "|".join(MONTHS) + r")\b", raw, re.I)
        if date:
            value.update(day=int(date.group(1)), month=MONTHS[date.group(2).casefold()], precision="day")
        else:
            month = re.search(r"\b(" + "|".join(MONTHS) + r")\b", raw, re.I)
            if month:
                value.update(month=MONTHS[month.group(1).casefold()], precision="month")
    return value


def wikipedia_lead_life(lead):
    """Extract only the subject's parenthetical lifespan, not dates in prose."""
    lead = re.sub(r"<ref\b[^>]*>.*?</ref>|<ref\b[^>]*/>", "", lead, flags=re.S)
    match = re.search(r"'''[^'\n]{2,160}'''\s*\((.{1,800}?)\)", lead, re.S)
    if not match:
        return {}
    expression = html.unescape(match.group(1))
    expression = re.sub(r"\{\{(?:IPAc-en|IPA|langx?|lang-[^|]+)\|[^{}]*\}\}", "", expression, flags=re.I)
    expression = expression.strip(" ;,")
    if re.search(r"\{\{\s*fl(?:oruit)?(?:[|}])|\bfl\.|\bfloruit\b|\bactive\b|\bruled\b|\breigned\b|\br\.", expression, re.I):
        return {}
    if re.match(r"(?:died\b|d\.)", expression, re.I):
        return {"death": wikipedia_date(expression)}
    if re.match(r"born\b", expression, re.I):
        return {"birth": wikipedia_date(expression)}
    circa_pair = re.search(r"\{\{circa\s*\|\s*(?:AD\s*)?(\d{2,4})\s*\|\s*(\d{2,4})\s*\}\}", expression, re.I)
    if circa_pair:
        return {"birth": wikipedia_date("circa " + circa_pair.group(1)),
                "death": wikipedia_date("circa " + circa_pair.group(2))}
    pair = re.split(r"[–—]|\s+-\s+", expression, maxsplit=1)
    if len(pair) == 2:
        dates = {field: wikipedia_date(raw.strip()) for field, raw in zip(("birth", "death"), pair)}
        return {field: date for field, date in dates.items() if date.get("year", 0) >= 50}
    return {}


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def annals_sections(text):
    start = text.rindex("\n", 0, text.index("孝桓皇帝下建宁元年"))
    end = text.index("太宗简文皇帝咸安元年")
    segment = text[start:end]
    headers = list(HEADER.finditer(segment))
    if len(headers) != 203:
        raise ValueError("The 168-370 annals do not contain exactly 203 annual headings")
    sections = []
    for index, match in enumerate(headers):
        year = 168 + index
        explicit = re.search(r"公元([零〇一二三四五六七八九]{3})年", match.group())
        if explicit:
            value = int("".join(str(NUMERALS[char]) for char in explicit.group(1)))
            if value != year:
                raise ValueError(f"Non-consecutive annual headings: {match.group()}")
        stop = headers[index+1].start() if index+1 < len(headers) else len(segment)
        sections.append({"year": year, "heading": match.group().strip(),
                         "start": start + match.start(), "end": start + stop})
    return sections


def direct_events(text, name, year, offset=0):
    if len(name) < 2:
        return []
    pattern = re.compile(re.escape(name) + r" *(?P<verb>" + DEATH + "|" + ACTIVITY + ")")
    events = []
    for match in pattern.finditer(text):
        left = max(text.rfind("\n", 0, match.start()), text.rfind("。", 0, match.start())) + 1
        right = text.find("。", match.end())
        right = right + 1 if right >= 0 else min(len(text), match.end()+140)
        context = text[left:right]
        prefix = text[:match.start()]
        paragraph_start = text.rfind("\n", 0, match.start()) + 1
        if RETROSPECTIVE.search(text[paragraph_start:right]) or prefix.count("“") != prefix.count("”"):
            continue
        if prefix.count("「") != prefix.count("」"):
            continue
        # A name occurring inside a longer personal/place name is not a subject.
        if match.start() and text[match.start()-1] in "之字名号號":
            continue
        verb = match.group("verb")
        events.append({"year": year, "kind": "death" if re.fullmatch(DEATH, verb) else "activity",
                       "matched_name": name, "verb": verb, "offset": offset+match.start(),
                       "quote": text[max(0, match.start()-70):min(len(text), right)]})
    return events


def cbdb_date(row, field):
    prefix = "by" if field == "birth" else "dy"
    year = row.get("c_birthyear" if field == "birth" else "c_deathyear")
    if not year:
        return None
    return {"year": year, "relation": {-1: "before", 0: "at", 1: "after", 2: "circa"}.get(
                row.get(f"c_{prefix}_range"), "not_specified"),
            "lunar_month": row.get(f"c_{prefix}_month") or None,
            "lunar_day": row.get(f"c_{prefix}_day") or None,
            "sexagenary_day_code": row.get(f"c_{prefix}_day_gz") or None,
            "intercalary_month": row.get(f"c_{prefix}_intercalary"),
            "era_code": row.get(f"c_{prefix}_nh_code"), "era_year": row.get(f"c_{prefix}_nh_year"),
            "source": "CBDB BIOG_MAIN", "source_id": row["c_personid"],
            "verification": "checked_against_local_database_not_primary_text"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT.parent / "san11world-persons")
    parser.add_argument("--out", type=Path, default=ROOT / "out/person_chronology")
    args = parser.parse_args()
    source = args.source_root / "data/persons"
    ledger = read_jsonl(args.out / "ledger.jsonl")
    originals = {row["id"]: row for row in read_jsonl(args.out / "original_source_evidence.jsonl")}
    path = source / "_cache/texts/资治通鉴.txt"
    original_text = path.read_text(encoding="utf-8")
    converter = opencc.OpenCC("t2s")
    text = converter.convert(original_text)
    sections = annals_sections(text)
    candidates = {}
    for candidate_path in (source / "build").glob("cands_*.jsonl"):
        for row in read_jsonl(candidate_path):
            candidates[row["key"]] = row
    results, chunk_locations = [], {}
    for person in ledger:
        events = []
        tj_records = [candidates[key] for key in person["keys"] if key.startswith("tj:") and key in candidates]
        for row in tj_records:
            name = converter.convert(row["name"])
            # Search only the cited source chunk's text, keeping the annal date.
            chunk_path = source / "_cache/chunks" / (row["key"].split(":")[1] + ".txt")
            if chunk_path not in chunk_locations:
                chunk = converter.convert(chunk_path.read_text(encoding="utf-8"))
                body = chunk.split("\n", 1)[1].strip("\n")
                chunk_locations[chunk_path] = text.find(body), len(body)
            position, length = chunk_locations[chunk_path]
            if position < 0:
                continue
            limit = position + length
            for section in sections:
                lo, hi = max(position, section["start"]), min(limit, section["end"])
                if lo >= hi:
                    continue
                found = direct_events(text[lo:hi], name, section["year"], lo)
                for event in found:
                    event.update({"source_key": row["key"], "heading": section["heading"],
                                  "source_path": str(path.resolve()),
                                  "quote_normalization": "OpenCC t2s; offsets address the normalized full text",
                                  "verification": "literal_primary_text_event_match_requires_identity_adjudication"})
                events.extend(found)
        unique = {(event["year"], event["kind"], event["offset"]): event for event in events}
        events = sorted(unique.values(), key=lambda event: (event["year"], event["offset"]))
        manual = person["manual_decision"]
        mixed = any(issue in person["issues"] for issue in (
            "multiple_source_qids_identity_review_required", "multiple_original_names_identity_review_required",
            "manually_confirmed_identity_conflict"))
        current = person["current_game"]
        proposed = None
        appearance_upper_bound = None
        basis = "no_independently_adjudicated_appearance_date"
        if manual and manual["action"] in {"duplicate_of_expanded", "duplicate_of_stock"}:
            basis = "duplicate_redirect"
        elif mixed:
            basis = "identity_hold"
        elif manual and manual.get("proposed_appear") is not None:
            proposed, basis = manual["proposed_appear"], "manual_decision"
        else:
            activity = [event["year"] for event in events if event["kind"] == "activity"]
            if activity:
                appearance_upper_bound = min(activity)
                # The excerpt begins in 168. First mention is an upper bound,
                # not proof that someone had no earlier career.
                if current["appear"] > appearance_upper_bound:
                    proposed, basis = appearance_upper_bound, "late_appearance_candidate_from_primary_event"
                else:
                    basis = "existing_appearance_not_invalidated_by_annals_bound"
        warnings = []
        if proposed is not None and not current["birth"] < proposed <= current["death"]:
            warnings.append("proposed_appearance_conflicts_with_existing_lifespan")
        db_rows = originals[person["id"]]["cbdb_originals"]
        wiki_dates = []
        for page in originals[person["id"]]["wikipedia_originals"]:
            if page.get("identity_check") != "qid_matches_source_record":
                continue
            existing_fields = set()
            for field, raw in page["date_fields"].items():
                if field in {"birth_date", "death_date", "birth_year", "death_year", "date_of_birth", "date_of_death"}:
                    if raw:
                        existing_fields.add("birth" if "birth" in field else "death")
                    wiki_dates.append({"field": field, "title": page["title"], "url": page["url"],
                                       "value": wikipedia_date(raw),
                                       "verification": "original_secondary_source_field_read_not_primary_text"})
            for field, value in wikipedia_lead_life(page["lead_wikitext"]).items():
                if field not in existing_fields:
                    wiki_dates.append({"field": field + "_lead", "title": page["title"], "url": page["url"],
                                       "value": value,
                                       "verification": "original_secondary_source_lifespan_read_not_primary_text"})
        results.append({"id": person["id"], "keys": person["keys"], "name": person["name_full"],
                        "current_game": current, "primary_events": events,
                        "wikipedia_date_claims": wiki_dates,
                        "rejected_wikipedia_titles": [page["title"] for page in originals[person["id"]]["wikipedia_originals"]
                                                      if page.get("identity_check") != "qid_matches_source_record"],
                        "cbdb_birth_claims": [date for row in db_rows if (date := cbdb_date(row, "birth"))],
                        "cbdb_death_claims": [date for row in db_rows if (date := cbdb_date(row, "death"))],
                        "cbdb_index_year_used_as_activity": False,
                        "appearance_no_later_than": appearance_upper_bound,
                        "proposed_appear": proposed, "proposal_basis": basis, "warnings": warnings,
                        "historical_verification_complete": False})
    if len(results) != len(ledger):
        raise ValueError("Incomplete calibration coverage")
    (args.out / "calibration_candidates.jsonl").write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in results), encoding="utf-8")
    summary = {"officers_processed": len(results), "event_proposals_are_not_final_adjudications": True,
               "proposal_basis": dict(collections.Counter(row["proposal_basis"] for row in results)),
               "primary_event_count": sum(len(row["primary_events"]) for row in results),
               "original_wikipedia_date_fields_read": sum(len(row["wikipedia_date_claims"]) for row in results),
               "annals_source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
               "game_files_changed": 0}
    (args.out / "calibration_summary.json").write_text(json.dumps(summary, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(summary))


if __name__ == "__main__":
    main()
