"""Regression checks for source-identity errors found in the actual roster."""
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out/person_chronology"


class MaterializedChronologyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reviews = {r["id"]: r for r in (
            json.loads(line) for line in (OUT / "reviewed_roster.jsonl").read_text(encoding="utf-8").splitlines())}
        cls.patches = json.loads((OUT / "strict_date_patch_plan.json").read_text(encoding="utf-8"))

    def test_all_officers_are_present_once(self):
        self.assertEqual(set(self.reviews), set(range(850, 14215)))

    def test_retired_source_slots_are_audited_but_not_exported_as_people(self):
        retired = {pid for pid, row in self.reviews.items()
                   if row["keys"] and all(key.startswith("retired:") for key in row["keys"])}
        canonical = {json.loads(line)["id"] for line in
                     (OUT / "canonical_roster_reviewed.jsonl").read_text(encoding="utf-8").splitlines()}
        patch_ids = {row["id"] for row in self.patches}
        patch_ids.update(row["id"] for row in json.loads(
            (OUT / "estimated_game_patch_plan.json").read_text(encoding="utf-8")))
        self.assertFalse(retired & canonical)
        self.assertFalse(retired & patch_ids)
        for pid in retired:
            self.assertEqual(self.reviews[pid]["review_status"], "retired_source_slot")

    def test_retracted_other_person_deaths_cannot_return_to_patch_plan(self):
        borrowed = {899: 208, 1375: 349, 1857: 255, 2466: 214,
                    2968: 232, 2997: 258, 3661: 270}
        for patch in self.patches:
            if patch["id"] in borrowed:
                self.assertNotEqual(patch["changes"].get("death"), borrowed[patch["id"]])

    def test_emperor_and_relative_conflations_are_held(self):
        for pid in (10875, 12443, 12497, 12523, 12539, 13205, 13216):
            self.assertEqual(self.reviews[pid]["review_status"], "identity_conflict")
            self.assertIsNone(self.reviews[pid]["historical_birth"])
            self.assertIsNone(self.reviews[pid]["historical_death"])
            self.assertNotIn(pid, {patch["id"] for patch in self.patches})

    def test_birth_correction_does_not_invent_a_new_appearance(self):
        for patch in self.patches:
            if "appear" in patch["changes"]:
                self.assertEqual(patch["changes"]["appear"], self.reviews[patch["id"]]["proposed_appear"])

    def test_martyr_commemorations_are_not_living_activity_or_life_dates(self):
        for pid in (11553, 11635, 11657, 11659, 11865, 12857, 12858, 12859):
            row = self.reviews[pid]
            self.assertIsNone(row["historical_birth"])
            self.assertIsNone(row["historical_death"])
            self.assertIsNone(row["proposed_appear"])
            self.assertNotIn(354, row.get("attested_activity_years", []))

    def test_martyrs_and_same_named_civil_officials_remain_on_identity_hold(self):
        for pid in (11860, 11870, 11875, 12856):
            self.assertEqual(self.reviews[pid]["review_status"], "identity_conflict")
            self.assertNotIn(pid, {patch["id"] for patch in self.patches})

    def test_disputed_law_subscriptions_do_not_become_exact_activity(self):
        for pid in (9895, 10369, 11884, 11895, 11904):
            row = self.reviews[pid]
            self.assertFalse(row.get("attested_activity_years"))
            self.assertIsNone(row["historical_death"])

    def test_consular_dating_is_not_a_law_address_or_death(self):
        row = self.reviews[11944]
        self.assertEqual(row["attested_activity_years"], [328])
        self.assertIsNone(row["historical_death"])
        self.assertIsNone(row["proposed_appear"])

    def test_egyptian_martyr_recension_disagreement_stays_a_range(self):
        for pid in (13751, 13753):
            row = self.reviews[pid]
            self.assertIsNone(row["historical_birth"])
            self.assertEqual(row["historical_death"]["year_range"], [309, 311])
            self.assertNotIn("year", row["historical_death"])
            self.assertGreaterEqual(len(row["evidence"]), 3)

    def test_palace_servants_do_not_inherit_bishops_life_dates(self):
        for pid in (13192, 13197):
            row = self.reviews[pid]
            self.assertEqual(row["review_status"], "identity_conflict")
            self.assertIsNone(row["historical_death"])

    def test_released_confessor_and_missing_bishop_are_not_recorded_dead(self):
        for pid in (10208, 10269):
            self.assertIsNone(self.reviews[pid]["historical_death"])
        self.assertEqual(self.reviews[10208]["attested_age_at_confession"]["precision"], "circa")
        self.assertIsNone(self.reviews[10208]["historical_birth"])

    def test_lyon_age_repairs_are_game_estimates_not_exact_birth_dates(self):
        estimates = {row["id"]: row for row in json.loads(
            (OUT / "estimated_game_patch_plan.json").read_text(encoding="utf-8"))}
        for pid in (13931, 13932):
            self.assertIn(pid, estimates)
            self.assertNotIn("birth", next((p["changes"] for p in self.patches if p["id"] == pid), {}))
        self.assertEqual(self.reviews[13931]["historical_birth"]["relation"], "on_or_before")
        self.assertEqual(self.reviews[13932]["historical_birth"]["relation"], "circa")

    def test_reported_montanist_deaths_are_not_certified_absolute_dates(self):
        for pid in (9557, 9567, 9568):
            self.assertIsNone(self.reviews[pid]["historical_death"])
        self.assertEqual(self.reviews[9587]["review_status"], "identity_conflict")

    def test_alexandrian_martyrs_do_not_inherit_other_named_figures_dates(self):
        for pid in (10123, 10135, 10172, 10175, 10181, 10201):
            row = self.reviews[pid]
            self.assertEqual(row["review_status"], "identity_conflict")
            self.assertIsNone(row["historical_birth"])
            self.assertIsNone(row["historical_death"])

    def test_yufuluo_sources_merge_to_reviewed_expanded_identity(self):
        row = self.reviews[10037]
        self.assertEqual(row["duplicate_of"], 1114)
        self.assertTrue(row["source_identities_all_adjudicated"])
        self.assertEqual(row["historical_death"]["year"], 195)
        self.assertEqual(self.reviews[1114]["historical_death"]["year"], 195)

    def test_infant_consul_gets_estimated_game_years_without_inventing_birth(self):
        row = self.reviews[9865]
        self.assertIsNone(row["historical_birth"])
        self.assertIsNone(row["historical_death"])
        self.assertEqual(row["estimated_game_years"], {"birth": 363, "appear": 364})
        self.assertIsNone(row["proposed_appear"])

    def test_full_coverage_is_not_reported_as_finished_research(self):
        summary = json.loads((OUT / "merged_review_summary.json").read_text(encoding="utf-8"))
        if summary["review_status"].get("needs_further_research", 0):
            self.assertFalse(summary["historical_verification_complete"])

    def test_pending_count_includes_secondary_fields_and_identity_holds(self):
        summary = json.loads((OUT / "merged_review_summary.json").read_text(encoding="utf-8"))
        statuses = summary["review_status"]
        expected = sum(statuses.get(k, 0) for k in (
            "needs_further_research", "secondary_source_checked", "secondary_fields_extracted", "identity_conflict"))
        queue = (OUT / "pending_primary_review_queue.jsonl").read_text(encoding="utf-8").splitlines()
        self.assertEqual(summary["pending_primary_or_identity_review_records"], expected)
        self.assertEqual(len(queue), expected)

    def test_reviewed_game_years_allow_appearance_during_life(self):
        path = OUT / "canonical_roster_reviewed.jsonl"
        for line in path.read_text(encoding="utf-8").splitlines():
            person = json.loads(line)
            self.assertLess(person["birth"], person["appear"], person["id"])
            self.assertLessEqual(person["appear"], person["death"], person["id"])

    def test_relationships_follow_duplicate_redirects(self):
        removed = {int(pid) for pid in json.loads((OUT / "duplicate_redirects.json").read_text(encoding="utf-8"))}
        for line in (OUT / "canonical_roster_reviewed.jsonl").read_text(encoding="utf-8").splitlines():
            person = json.loads(line)
            links = [person[field] for field in ("father", "mother", "spouse", "sworn", "blood") if field in person]
            links += person.get("liked", []) + person.get("disliked", [])
            self.assertFalse(removed.intersection(links), person["id"])


if __name__ == "__main__":
    unittest.main()
