# Step 82 — The editable modal table, the Scala tuning and the unison pair (2026-10-19)

Voxo **0.22.0**, additive; the preset schema **2** (`DECISIONS_12 #6`;
`_work/ROADMAP_6.md` step 82; `_work/TO_PROJECT_SPEC.md` §13.12 as shipped).
The core is frozen: nothing under `core/` moved. Phase 13's last step.

## What shipped

* **The table** (`modal_preset` 5): `modal_ratio[16]`, `modal_t60[16]`,
  `modal_weight[16]` in `voxo_suzu_params_t` (a zero defers to the formula),
  read by the one table builder (`suzu::modal_table`, `PRESET_TABLE`);
  `voxo_suzu_table` returns any preset's resolved table. `modal_comp`: the
  coupling's compensation as a flag — off, the raw lattice, the load gate's
  bound read without the solve, and no swirl share on the coupling.
* **The tuning**: `voxo_set_tuning` (.scl and .kbm as text), `voxo_load_tuning`
  (paths), `voxo_clear_tuning`, `voxo_tuning_count / _hz / _name` — the
  128-note table double-buffered under `voice_retune`, the one site where a
  note becomes a frequency for Suzu and the sampler alike; the bend an
  equal-tempered offset on the tuned note; a malformed file refused with a
  reason.
* **The Unison pair** (patch 16, `presets/unison.json`, written by the
  serializer itself through `probes/make_unison.cpp`): the table in pairs,
  κ 0.006, the compensation off.
* **The preset library**: schema 2 — the suzu block's `modal_comp` and the
  three arrays (`r_farray`, `w_farr`; extras dropped, a shorter array leaves
  the rest; `modal_ratio[0]` 0 the sentinel); `sumi_preset_suzu_to_voxo`
  (`sumi_preset_voxo.c/.h`, in the SumiPreset module map): the one file-to-
  params mapping with every step's sentinel.
* **The desktop**: *Suzu preset* → *table (editable)*, the sixteen rows,
  *Start from the preset's table*, *Coupling compensated*; the INI's comma
  lists; the session file both ways; the tuning rows (*Tuning (.scl)*,
  *Keyboard mapping (.kbm)*, *Load tuning*, *Equal temperament*) applied with
  the params; the bench's `--preset` carries the table.
* **The tablets' Sound pages**: *Import a Suzu patch (.json)* (copied under
  Patches, applied over the picked patch through the helper; *Use the patch
  above* to return) and a *Tuning* section (*Import a Scala scale (.scl)*,
  copied under Tunings, `voxo_load_tuning`; *Equal temperament*; the scale's
  name and size as the status). iOS: `SoundController.patchFile /
  tuningFile`, two file importers; the Tab: `nativeVoxoSetPatchFile`,
  `nativeVoxoSetTuning`, two SAF pickers.
* The suite's gates 46–48, the field table's `modal_comp` and `oversample`,
  gate 28's seventeen and its array comparison; the web table's
  `modal_comp`; `voxo_c_compile.c` 0.22.0; the book's modal chapter, the
  patch table's seventeenth, the settings reference's two rows, SCHEMA.md.

## Findings (the decision entry has them in full)

* A unison pair has no separate targets for the compensation's solve: the
  load gate's bound ran the solve on the raw ratios, diverged (16.6 against
  4) and refused the patch until the raw lattice's bound was its own.
* Under the raw lattice the swirl must add nothing to the coupling — 0.5·κ
  would detune every mode the solve no longer protects.
* The split is the chain's: the first pair's two normal modes at
  +κ(3 − √5)/4 and +κ(3 + √5)/4 of the frequency (one mode has one
  neighbour, the other two), not the naive √(1 + 2κ).
* The unison file is the first preset written by the serializer itself, so
  it carries the serializer's full key set (the trace routes, `oversample`):
  the pinning gate skips the shell's keys and knows the oversampler.
* A generator linked statically against an older library wrote the file
  with the old κ once: relink before regenerating.

## Gates

| Gate | Result | File |
|---|---|---|
| the table as resolved (`voxo_suzu_table`) | six modes, the ratios 1 / 2.3 / 3.1 / 4.7 / 6.2 / 8.9, the decays and the weights as stated | `suite_step82_blocks.txt` |
| the table as inspected, at the oscillator | `s[k]` the ratios; the first four partials within 0.01 cent at A3 (coupling 0) | same |
| the table's decays | the mode energies' fall 0.3 → 1.3 s reads each T60 within 1 % (3 / 2 / 1.5 / 1 s) | same |
| the Scala table | 19-EDO within 0.0001 cent of 440·2^((n−69)/19) over 128 notes; the log names the scale | same |
| the Scala tuning at the oscillator | the cell at MIDI 60 / 64 / 69 / 81 within 0.0003 cent | same |
| the mapping and the ratios | Pythagoras in ratios with a .kbm (60 at 256 Hz): the fifth 1.5000000, MIDI 72 at 512 Hz | same |
| the refusal | a malformed degree refused with its reason, the tuning standing; cleared = 440 Hz at 69 | same |
| the Unison pair | admitted; at κ 0.01 the two lines +3.25 / +22.50 cents against the theory's +3.30 / +22.37; bit for bit with and without poly pressure (the swirl deaf) | same |
| the patch table | seventeen; patch 16 equals `unison.json` field for field, the arrays element for element | same |
| `preset_tests` | 40/40 (the schema 2, the suzu block round-tripped whole) | `gates/preset_tests.txt` |
| the Release suite | all 214 green (the membranes' headroom gate reds under a loaded machine: re-run idle) | `suite_release.txt` |
| the web gate | GREEN — ten sections ok (the engine unchanged where the script plays; `modal_comp` in the table) | `gates/suzu_web_gate.txt` |
| the lab gate (six pages) | GREEN | `gates/suzu_lab_gate.txt` |
| ctest (Debug) | 14/14 (`replay_tests` among them: the replay gate's format) | `gates/ctest.txt` |
| the storm on the Unison pair (the Release universal build, `--preset presets/unison.json`) | 0 XRuns, render max 0.37 ms, twelve voices | `gates/storm_unison.txt` |
| the unison's profile | the peak within 3.1 dB over A0..C8 (−29.5..−26.4 dBFS), the RMS within 0.7; the first three harmonics' pairs steady to C6 | `profiles/profile_unison.png` |

## The tablets

* **Android (the Galaxy Tab S8 Ultra, the release APK signed with the debug
  key)**: the Unison pair under the spike at the Tab's 4 ms low-latency
  burst — 5 250 callbacks, 0 proxy XRuns, 1 device XRun, render max
  1.87 ms (the Tab's edge at twelve voices, as step 81 recorded).
  `tablets/android_tab_patch16.txt`.
* **iOS (the iPad Air M4)**: the Unison pair under the spike — 7 982
  callbacks, 0 XRuns, render max 0.22 ms of 2.67, the CSV fresh (Voxo
  0.22.0's header). `tablets/ipad_voxo_spike_patch16.csv`.
* The pickers (a patch file, a scale) on both tablets are the author's to
  try by hand; the engine paths they reach are the suite's.

## Carried

* A sixteen-row table editor on the tablets (the file is the table there).
* The tuning inside the session file (a shell setting now).
* The lab's table (the web table is scalar).
* §14.6's microtonal mapping of the layouts (the core's; the roadmap's
  word).
* The Tab's wind and lattice headroom at twelve voices and 192 frames.
