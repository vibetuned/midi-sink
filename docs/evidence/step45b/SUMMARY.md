# Evidence — Step 45b: the Android shell for Phase 6

Machine: the author's Linux box, the Galaxy Tab S8 Ultra (SM-X906B,
Android 16, Adreno 730, 2960×1848 panel, 1480×924 surface, sim_scale 0.75),
AGP 9.4.1 / Gradle 9.6 / NDK r27 / CMake 4.4.3. Decisions: `_work/DECISIONS_5.md`
#79 (the session, the sheet, the ledger, the gestures) and #80 (flagged:
the Adreno spark drift). The Play build on the Tab was uninstalled (by the
author's choice) so debug builds could go on; its settings went with it.

## Roadmap DONE (as 44 on the Galaxy Tab, plus the 16 KB check)

| Check | Result | Evidence |
|---|---|---|
| First line: the 1.1.0 JNI compiles and the app runs | **PASS** — the tree as the Mac left it built and ran unchanged (`sumi 1.1.0 ready`, no sokol validation errors with Anod's bloom, cells and export on GLES) | `selftest_first_build.txt` |
| On-device suites | **PASS** — hostmpe 1569, normalizer / mapper 19 004 checks | `tab/selftest.txt` |
| §4.6 field on GLES | **PASS** at the mobile tier (2.5e-2 / 1e-3): max 1.51e-2, mean 3.76e-4 — the numbers Part III #30 recorded for this Adreno before Phase 6, and the final build's dump is bit-identical to the first | `field_gate_gles3.txt`, `field_gate_gles3_final.txt` |
| Every new setting reachable on the tablet | **PASS** — the sheet's pages captured by intent in both media: Canvas (dip / clear, Prints), Medium & look (medium, Palette with built-ins, library and custom editor, Substrate per medium, Presets, Operators), layouts with "(playable)", input, the 1.1 routing modes with "Medium default", vortex profiles and torsion sweep, ripple, stylus wake, the CC map with 14–19 / 104–109 | `tab/settings_pages.jpg` |
| A desktop preset imports and renders the same | **PASS for the preset, PARTIAL for the render (#80).** `desktop-anod.json` pushed into `Presets/`, loaded from the sheet, saved back under its name: **byte-identical** (2040 bytes). Same look: the canonical field script under the preset prints the same on the Tab and the desktop (glass level 2.24 / 2.24, lit share 11.85 / 11.88 %, glow (134, 214, 153) / (135, 216, 153)). The six-strike print differs: 6 charges on the desktop, 3 on the Tab — traced to the Adreno's half-float filtering drifting over the spark episode's hundreds of sub-texel passes (#80, the author's call) | `desktop-anod.written-back-by-tab.json`, `compare/` |
| The byte path is untouched | **PASS** — 20 s storm in Play mode, 37 589 messages: ALL ASSERTS PASS | `tab/midi_asserts_storm.txt` |
| 16 KB pages | **PASS** — release APK: `zipalign -c -P 16` verification successful, both libraries `0x4000` | `page_size_16k.txt` |

## The rest of the handoff

| Item | Result | Evidence |
|---|---|---|
| One session, persisted through `presets/` | done (#79): native `sumi_preset_t`, JSON patches, `last_session.json`, `Presets/`, SAF import / export | code |
| 0.x migration, once | **PASS** — palette, viscosity, ink feed, bend ripple (+ bake), Rankine, ripple angle, layout, classic input and the ripple amount carried over; untouched pressure / slide rows = Medium default; the #50 stock map upgraded to 22 routes | `tab/prefs_0x.xml`, `tab/migrated_session.json`, `tab/migration.txt` |
| The print ledger | **PASS** after one fix (the export poll, #79): two dips with thumbnails; 4096×2557 export and 4096×2557 Anod over alpha (65.7 % transparent glass) in `Pictures/midi-sink` + the share sheet | `tab/prints_page.jpg`, `tab/ledger_export_4k.jpg`, `tab/ledger_export_4k_alpha_preview.png`, `tab/ledger_exports.txt` |
| Gestures through `sumi_gesture_*` | **PASS** in both media (tap, comb, pinch, twist, press push-then-pull, pen wake — driven natively, not by touch) | `tab/gestures_sumi_anod.jpg` |
| The play surface on the glass | **PASS** — white lattice over dark halos and a smoke strip in Anod; 1.0's marks on paper | `tab/play_surface_glass_and_paper.jpg` |
| S-Pen in Marble mode, ROLI / Airwave on the Tab | **YOURS** — hands on the instruments | — |

## Found on the way

* The handoff points at "the procedure in `android/RELEASING.md`" for the
  16 KB check; there was none — added (the zipalign / readelf commands).
* The session's controls are sent twice at start (the attach and the first
  CC-map apply) — harmless, identical values.
* The comparison used temporary hooks on both sides (a desktop
  `--preset-render`, Tab evidence intents); all removed, and the tree
  checked for them.

**Tree ready** — evidence `docs/evidence/step45b/`, decisions #79–#81.

# Follow-up: the Adreno spark drift (#80, #81 withdrawn)

The author asked for fewer, larger spark steps on Android after #80's
measurements. **The float field does not fix the drift** (`option3/`). On
the desktop a float field prints the same as the half-float field; on the
Tab it tears the strikes into a third arrangement. The drift comes from the
Adreno's filtering, not from the storage format. **A host quantum of 0.004
was built and tried** (`quantum/`): 37 steps in place of 147, with the same
total kick. It played well while active but stepped visibly as the episode
slowed (3.4 px jumps, pauses of up to 41 frames). A wait limit and a
decaying quantum were measured as smoothers (`quantum/rollback/`).

**The author chose the original look, and #81 is rolled back in full.**
The core, the regression test and the JNI/Kotlin call are gone; the
Android shell runs the core's default spark steps, like every other
platform.

| Check | Result | Evidence |
|---|---|---|
| Smoothers, headless | wait ≤ 8: 48 steps, pause 8, jumps 3.4 px; decaying (floor 0.002): 57 steps, 2.0 px | `quantum/rollback/tail_steps_headless.txt` |
| Tab repeatability | Three runs of the same 0.004 build counted 6, 3 and 5 charges: framed strikes follow the frame clock, so single-run counts do not rank variants | `quantum/rollback/tab_runs.txt`, `six_runs.png` |
| Rollback | `git diff` empty for `sumi_core.h`, `engine.cpp`, `voice_mapper.*`, `sumi_debug.h`, `normalizer_tests.cpp`; no quantum code in `android/`; ctest 5/5; the Tab's self-test passes | `tab/selftest.txt` |
