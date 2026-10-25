# Step 90 — The modulator matrix, legato and glide, the compat report pruned

2026-10-26 · Voxo 0.30.0 (additive: `voxo_set_tempo`) · `DECISIONS_14 #4` · `TO_PROJECT_SPEC` §12.10–§12.11 as shipped · the core frozen.

## What shipped

* **The format, read first**: the `<lfo>` has `shape` (sine, square, saw; triangle from the SHAPE binding's list), `frequency` in Hz or a musical-time index under `frequencyFormat="musical_time"`, `delayTime`, `modAmount`, `scope` global | voice, `modBehavior` set | add | multiply | modulate, `enabled` — and **no offset, phase or sync attribute** (the proposal's); the `<envelope>` adds `delay` and `hold`; the groups carry `glideTime`, `glideMode` (legato | always | off), `polyphony`; the samples `trigger` first / legato, `previousNotes`, `legatoInterval`.
* **The matrix** (`Modulator` / `ModTarget`, eight of eight): every binding under an LFO or an envelope is a target — the volume, the pan, the tuning, the voice filter's cutoff and Q, a group effect's parameters, the bus effects' and the EQ stages' — read once a block at the block's start and end and folded into the ramps the voice already runs; the bus under a live copy of its parameters; modulate = the curve less its neutral. Step 89's filter envelope is one slot of it.
* **Musical time** on the host's tempo: `voxo_set_tempo`, passed by every shell from the session's BPM; the subdivision list reconstructed from the guide's four anchors (a quarter at 120 BPM is 2 Hz).
* **Legato and glide**: a new note in a gliding group walks from the previous note's current pitch along a straight line in semitones over `glideTime`; `polyphony` releases the group's oldest voice; the triggers and the previous-note attributes select samples in the dispatch.
* **The report**: the modulators sentence retired (eight sentences remain: the housekeeping and the four features genuinely out of scope).

## The gates

| gate | result |
|---|---|
| the preset suite, blocks 33–41 (Release) | `suite_preset_release.txt`, 256 ok: the tremolo's peak over its dip 1.994 (2); the global LFO at its peak and the voice LFO at its dip (0.501) 0.375 s after a note started a quarter second into the clock; the square's second half 0.0, the saw's ramp 3.958 (4), the triangle 1.996 (2); the vibrato 226.4 / 214.0 Hz (226.4 / 213.7); the pan 0.0010 and 956 left over right; a quarter note 1.994 at 120 BPM and 1.998 at 60; delayTime flat within 0.01 dB then 1.994; the modulated cutoff 13.6 dB between the LFO's top and bottom (the analytic 13.7); the bus peak's gain under a global LFO 5.99 dB (6); the envelope's delay silent, hold 0.1604, sustain 0.499, release −29.9 dB (−30.0); the glide 222.2 / 233.0 / 246.9 Hz on the semitone line, off 247.1, always from a released note 233.0, legato 247.1; the triggers and the previous-note attributes by the saw's second harmonic; zero allocation; the features preset's seven notes; the eight sentences |
| the sampler suite (Release) | `suite_voxo_release.txt`, 50 ok |
| the synth suite (Release) | `suite_suzu_release.txt`, 256 ok |
| the Debug ctest | `ctest_debug.txt`: 14 / 14 in 428 s |
| the five-minute fuzz seeded with the matrix fixtures | `fuzz_300s_debug.txt`: 1 132 760 loads in 300 s, 441 845 loaded, no crash |
| the storm on the Dan Tranh library — a per-note vibrato (voice scope, delayed), a tempo-synced tremolo and pan, a slow LFO on the bus reverb and the EQ's peak, a legato glide on every group, on top of step 89's effects (`storm_preset.dspreset`) | `storm.txt`: 88 783 messages, 7879 callbacks, 0 XRuns, render max 0.258 ms (0.196 at step 89), 13 voices at the end, 97.5 fps |
| the web and lab gates | `web_and_lab_gates.txt`: the web gate GREEN (every kind as at step 89), the lab gate GREEN, six pages |
| both tablets under the spike | `tablets.txt`, `tablets/`: the iPad clean (the sampler 0.066 ms of 2.67, Suzu patch 0 0.199 ms, 0 XRuns); the Tab's sampler 0 XRuns at 0.96 ms of 4 ms, Suzu patch 0 the carried placement item (11 proxy XRuns) |

## Found on the way

* **A saw through a swept filter is not a witness through 50 ms windows**: the first cutoff gate read ±5 dB of zigzag between consecutive windows (five and a half saw periods a window, the neighbouring harmonics leaking into the measured bin). A sine through the same sweep traced the analytic curve within half a decibel every 40 ms (`probe_cutoff_sine.txt`): the engine was right, the measurement was not. The fixture is the sine.
* **A scripted region cut anchored 1500 lines away emptied `voxo.cpp`** for a minute; it came back from HEAD (the author had committed step 89). The rule: the end anchor is the text that immediately follows the region, with a size assert and a backup first.
* The Tab's sampler reads 0.96 ms where step 89 read 1.2–1.4 ms on the same demo: run-to-run variance, as suspected then.

## Files

`voxo/src/ds_preset.h/.cpp` (the LFO's, the envelope's, the glide's and the legato attributes; the modulators note retired), `voxo/src/instrument.h/.cpp` (`Modulator` / `ModTarget`, `MOD_MAX`, `Target::AmpPan` / `GroupTuning` / `GlideTime`, `musical_whole_notes`, the glide and polyphony fields, the zones' legato fields, the matrix's compile), `voxo/src/voxo.cpp` (`lfo_raw`, `lfo_phase`, `menv_advance`, `mod_read`, `mod_apply`, `Voice::start_s` / `menv` / `glide_*`, `voxo_t::bpm` / `prev_note`, the dispatch's triggers, polyphony and glide, the matrix in the voice loop and on the bus, `voxo_set_tempo`; 0.30.0), `voxo/include/voxo.h` (`voxo_set_tempo`, the history), `voxo/COMPAT_REPORT.md`, `tests/voxo_preset_tests.cpp` (2, 6, 33–41), `tests/voxo_fuzz.cpp`, `tests/voxo_c_compile.c`, `tests/fixtures/dspresets/mod/` (eighteen presets, the two waves), `desktop/src/main.cpp`, `ios/Sources/SumiCanvas.swift`, `android/cpp/sumi_jni.cpp`, `site/src/content/docs/guide/sound.md`, `_work/DECISIONS_14.md`, `_work/TO_PROJECT_SPEC.md` §12.10–§12.11, `docs/CHANGELOG.md`, `CLAUDE.md`.
