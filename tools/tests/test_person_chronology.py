import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from calibrate_person_chronology import cbdb_date, direct_events, wikipedia_date, wikipedia_lead_life
from merge_person_chronology_reviews import constrain_game_estimates, explicit_year, identity_date_patch_allowed, period_exclusion, review_progress_counts
from audit_person_chronology import is_named_stock_officer, stock_name_aliases


class ChronologyEvidenceTests(unittest.TestCase):
    def test_conservative_progress_keeps_identity_holds_after_primary_reading(self):
        rows = [
            {"review_status": "primary_checked", "source_identities_all_adjudicated": False},
            {"review_status": "needs_further_research", "source_identities_all_adjudicated": True},
            {"review_status": "primary_checked"},
            {"review_status": "source_checked_dates_unknown", "source_identities_all_adjudicated": None},
            {"review_status": "source_checked_dates_unknown", "source_identities_all_adjudicated": True},
            {"review_status": "duplicate", "source_identities_all_adjudicated": False},
            {"review_status": "retired_source_slot"},
            {"review_status": "excluded_out_of_period", "source_identities_all_adjudicated": False},
            {"review_status": "identity_conflict", "source_identities_all_adjudicated": False},
        ]
        self.assertEqual(review_progress_counts(rows), {
            "conservative_review_queue_records": 5,
            "identity_scope_unadjudicated_records": 2,
            "identity_scope_flag_missing_records": 2,
        })

    def test_stock_dedup_includes_ancients_and_named_npcs_but_not_placeholders(self):
        for pid in (8, 56, 699, 704, 729, 821, 831):
            self.assertTrue(is_named_stock_officer(pid), pid)
        for pid in (-1, 670, 698, 714, 799, 832, 849, 850):
            self.assertFalse(is_named_stock_officer(pid), pid)

    def test_known_stock_name_variants_are_candidates_for_review(self):
        self.assertIn("秦密", stock_name_aliases({"koei_id": 306, "sei": "秦", "mei": "宓"}))
        self.assertIn("李績", stock_name_aliases({"koei_id": 821, "sei": "李", "mei": "勣"}))
        self.assertEqual(stock_name_aliases({"koei_id": 8, "sei": "衛", "mei": "瓘"}), ("衛瓘",))

    def test_tang_guest_is_excluded_by_source_identity_despite_game_dates(self):
        decision = {"keys": ["zhsan:8021"]}
        person = {"id": 2420, "keys": ["zhsan:8021"], "birth": 183, "death": 269}
        self.assertEqual(period_exclusion(person, [decision]), decision)

    def test_same_named_former_yan_officer_is_retained(self):
        person = {"id": 7731, "name_full": "李績", "keys": ["sl:sl_03:61:李績"]}
        self.assertIsNone(period_exclusion(person, [{"keys": ["zhsan:8021"]}]))

    def test_cross_era_identity_merge_requires_review(self):
        person = {"id": 2420, "keys": ["zhsan:8021", "other:person"]}
        with self.assertRaisesRegex(ValueError, "merged with other sources"):
            period_exclusion(person, [{"keys": ["zhsan:8021"]}])

    def test_living_document_corrects_game_death_without_inventing_historical_death(self):
        review = {"attested_activity_years": [351], "historical_death": None}
        changes, reasons, _ = constrain_game_estimates({"birth": 286, "appear": 303, "death": 323}, review)
        self.assertEqual(changes, {"death": 351})
        self.assertIn("historical_death_unknown", reasons["death"])
        self.assertIsNone(review["historical_death"])

    def test_verified_shard_activity_calibrates_late_game_appearance(self):
        review = {"verified_activity": [{"year": 188, "verification": "individually_read_primary_passage"}]}
        changes, _, _ = constrain_game_estimates({"birth": 150, "appear": 190, "death": 195}, review)
        self.assertEqual(changes, {"appear": 188})

    def test_last_living_attestation_extends_only_estimated_death(self):
        for value in (346, {"year": 346}):
            changes, _, _ = constrain_game_estimates(
                {"birth": 280, "appear": 296, "death": 330}, {"last_attested_activity": value})
            self.assertEqual(changes, {"death": 346})

    def test_unreviewed_candidate_activity_is_never_used_for_calibration(self):
        review = {"unverified_activity_candidates": [{"year": 220}],
                  "unadjudicated_activity_candidate": 219, "source_branch_activity": {"year": 218}}
        changes, _, _ = constrain_game_estimates({"birth": 230, "appear": 246, "death": 280}, review)
        self.assertEqual(changes, {})

    def test_disputed_shard_event_is_not_an_exact_activity_year(self):
        review = {"attested_activity_events": [{"year": 333, "year_alternatives": [332, 333]}],
                  "last_attested_activity": {"year": 333, "precision": "circa"}}
        changes, _, _ = constrain_game_estimates({"birth": 300, "appear": 340, "death": 350}, review)
        self.assertEqual(changes, {})

    def test_appearance_bound_calibrates_entry_without_claiming_living_at_bound(self):
        review = {"appearance_upper_bound": {"year": 290, "relation": "at_or_before"}}
        changes, _, _ = constrain_game_estimates({"birth": 292, "appear": 308, "death": 350}, review)
        self.assertEqual(changes, {"birth": 274, "appear": 290})
        changes, _, _ = constrain_game_estimates({"birth": 240, "appear": 256, "death": 270}, review)
        self.assertEqual(changes, {})

    def test_legacy_appearance_upper_bound_remains_a_game_estimate(self):
        changes, _, _ = constrain_game_estimates(
            {"birth": 299, "appear": 315, "death": 371}, {"appearance_no_later_than": 314})
        self.assertEqual(changes, {"appear": 314})

    def test_living_event_interval_extends_lifespan_to_earliest_possible_year(self):
        changes, _, _ = constrain_game_estimates(
            {"birth": 280, "appear": 296, "death": 370}, {"attested_activity_interval": [375, 383]})
        self.assertEqual(changes, {"death": 375})

    def test_disputed_appearance_bound_is_held(self):
        review = {"appearance_upper_bound": {"year": 290, "relation": "at_or_before", "year_range": [280, 290]}}
        changes, _, _ = constrain_game_estimates({"birth": 292, "appear": 308, "death": 350}, review)
        self.assertEqual(changes, {})

    def test_corrected_birth_can_repair_impossible_game_entry_as_estimate(self):
        review = {"historical_birth": {"year": 276}, "historical_death": {"year": 339}}
        changes, reasons, _ = constrain_game_estimates({"birth": 276, "appear": 263, "death": 339}, review)
        self.assertEqual(changes, {"appear": 292})
        self.assertIn("not historical_first_office", reasons["appear"])

    def test_child_ruler_is_not_forced_to_wait_until_sixteen(self):
        review = {"historical_birth": {"year": 343}, "historical_death": {"year": 361},
                  "proven_activity_year": 344}
        changes, _, _ = constrain_game_estimates({"birth": 343, "appear": 358, "death": 361}, review)
        self.assertEqual(changes, {"appear": 344})

    def test_dated_jin_biography_does_not_verify_merged_han_eunuch(self):
        review = {"id": 1277, "review_status": "primary_text_corroborated",
                  "identity_status": "identity_not_fully_verified",
                  "historical_death": {"year": 285}}
        self.assertFalse(identity_date_patch_allowed(review, {"issues": []}))

    def test_all_source_identity_review_required_before_using_foreign_dates(self):
        review = {"review_status": "primary_text_corroborated"}
        audit = {"issues": ["multiple_source_qids_identity_review_required"]}
        self.assertFalse(identity_date_patch_allowed(review, audit))
        review["source_identities_all_adjudicated"] = True
        self.assertTrue(identity_date_patch_allowed(review, audit))

    def test_same_chinese_name_in_multiple_keys_still_needs_identity_adjudication(self):
        review = {"id": 4455, "review_status": "primary_text_corroborated",
                  "identity_status": "biography_identity_adjudicated",
                  "keys": ["js:person:王導", "cbdb:25788", "cbdb:467992"]}
        self.assertFalse(identity_date_patch_allowed(review, {"issues": []}))

    def test_contradictory_primary_years_are_not_single_year_patches(self):
        self.assertIsNone(explicit_year({"year": 387, "year_alternatives": [386, 387]}))
        self.assertIsNone(explicit_year({"year": 232, "relation": "circa"}))
        self.assertIsNone(explicit_year({"year_range": [202, 211]}))

    def test_exact_primary_dates_accept_review_relation_alias(self):
        self.assertEqual(explicit_year({"year": 257, "relation": "exact"}), 257)
        self.assertIsNone(explicit_year({"year": 257, "relation": "exact",
                                         "year_alternatives": [257, 258]}))
        self.assertIsNone(explicit_year({"year": 257, "relation": "exact",
                                         "precision": "circa"}))

    def test_annal_records_a_real_appointment(self):
        events = direct_events("十二月，司徒许栩罢，以大鸿胪袁隗为司徒。", "袁隗", 172)
        self.assertEqual([(x["year"], x["kind"]) for x in events], [(172, "activity")])

    def test_quoted_proposal_is_not_an_appointment(self):
        text = "球说郃曰：“今曹节等放纵为害。今可表徙卫尉阳球为司隶校尉，以次收节等诛之。”"
        self.assertEqual(direct_events(text, "阳球", 179), [])

    def test_next_clause_is_not_the_dead_person_acting(self):
        self.assertEqual(direct_events("击斩梁龙，降者数万人，旬月尽定。", "梁龙", 181), [])

    def test_passive_killing_is_not_an_activity(self):
        events = direct_events("卫尉士孙瑞为傕所杀。", "士孙瑞", 195)
        self.assertEqual([x["kind"] for x in events], ["death"])

    def test_retrospective_events_do_not_inherit_annal_year(self):
        self.assertEqual(direct_events("先是，张奂为大司农。", "张奂", 180), [])

    def test_cbdb_index_year_is_not_birth(self):
        self.assertIsNone(cbdb_date({"c_personid": 135004, "c_index_year": 161}, "birth"))

    def test_cbdb_preserves_uncertainty_and_lunar_month(self):
        value = cbdb_date({"c_personid": 1, "c_birthyear": 200, "c_by_range": 2,
                           "c_by_month": 12, "c_by_intercalary": 1}, "birth")
        self.assertEqual(value["relation"], "circa")
        self.assertEqual(value["lunar_month"], 12)
        self.assertEqual(value["intercalary_month"], 1)
        self.assertNotIn("gregorian_date", value)

    def test_wikipedia_preserves_death_bound(self):
        value = wikipedia_date("after 205")
        self.assertEqual((value["year"], value["relation"]), (205, "after"))

    def test_calendar_day_is_not_an_alternative_year(self):
        value = wikipedia_date("25 July 306 (aged {{Circa|56}})")
        self.assertEqual((value["year"], value["month"], value["day"]), (306, 7, 25))

    def test_unknown_birth_does_not_become_publication_date(self):
        value = wikipedia_date('unknown<ref>{{cite book|date=1993|title=Biography}}</ref>')
        self.assertNotIn("year", value)

    def test_date_template_does_not_use_birth_year_as_month(self):
        value = wikipedia_date("{{death year and age|181|0117}}")
        self.assertEqual(value["year"], 181)
        self.assertNotIn("month", value)

    def test_disputed_dates_remain_alternatives(self):
        value = wikipedia_date("disputed: 209, {{Circa|251}} or 304")
        self.assertEqual(value["year_alternatives"], [209, 251, 304])
        self.assertTrue(value["tradition_or_dispute"])

    def test_century_does_not_become_exact_year(self):
        value = wikipedia_date("3rd or 4th century")
        self.assertEqual(value["year_range"], [201, 400])
        self.assertNotIn("year", value)

    def test_lead_preserves_abinnaeus_lower_bound(self):
        values = wikipedia_lead_life("'''Flavius Abinnaeus''' ({{circa|286}} – after 351) was an officer.")
        self.assertEqual(values["birth"]["relation"], "circa")
        self.assertEqual(values["death"]["relation"], "after")

    def test_activity_interval_is_not_lifespan(self):
        self.assertEqual(wikipedia_lead_life("'''Example''' (fl. 270–300) was a governor."), {})

    def test_circa_pair_does_not_lose_uncertainty(self):
        values = wikipedia_lead_life("'''Theon''' ({{Circa|AD 335|405}}) was a scholar.")
        self.assertEqual(values["birth"]["year"], 335)
        self.assertEqual(values["death"]["relation"], "circa")

    def test_death_interval_in_lead_is_not_a_lifespan(self):
        values = wikipedia_lead_life("'''Mocius''' ({{langx|el|Μώκιος}}; died 288–295) was a martyr.")
        self.assertNotIn("birth", values)
        self.assertEqual(values["death"]["year_alternatives"], [288, 295])

    def test_floruit_template_is_not_a_lifespan(self):
        self.assertEqual(wikipedia_lead_life("'''Lupicinus''' ({{fl}} 368–377) was an officer."), {})

    def test_reign_interval_is_not_a_lifespan(self):
        self.assertEqual(wikipedia_lead_life("'''King''' ({{Langx|si|තිස්ස}}, ruled 262–273) was a king."), {})

    def test_reference_hyphen_does_not_split_lifespan(self):
        values = wikipedia_lead_life("'''Titus''' (died c. 378<ref>Library - Bibliotheca (2006)</ref>) was a bishop.")
        self.assertNotIn("birth", values)
        self.assertEqual(values["death"]["year"], 378)


if __name__ == "__main__":
    unittest.main()
