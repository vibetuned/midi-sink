# Step 87 — The sustain pedal v2: the half-pedal, re-pedalling, the pedal noises, the CC filter and the choke groups

2026-10-25 · Voxo 0.27.0 (the ABI unchanged) · `DECISIONS_14 #1` · `TO_PROJECT_SPEC` §12.7 as shipped · the core frozen · Phase 15 opens.

## What shipped

* **The format's attributes, read from the format** (the Decent Sampler developer guide's `<groups>` reference, fetched before the parser): `onLoCCN`/`onHiCCN` (the CC number in the attribute name) fire a sample once when the CC enters its range, as a voice of its own; `loCCN`/`hiCCN` admit a zone by the CC's last value; `silencedByTags` + `silencingMode` fast (the default, 5 ms) | normal (the release) + `silencingDecay`; `pitchKeyTrack`. The proposal's `trigger="cc"` does not exist; `sustainDecay` is Voxo's own.
* **The pedal is a position**: a note-off under it is held and damped at the rate (1 − p)·4.6/release + p·4.6/sustainDecay (absent = 0: the hold, bit for bit the pedal of 2.0); re-pedalling catches a releasing tail where it is (`ENV_HOLD`); a Suzu voice takes the rate through its contraction.
* **Nothing allocates in the callback**: the CC voices come from the pool, the tag dictionary (64 bits) and the CC-zone list are compiled on the shell thread.

## The gates

| gate | result |
|---|---|
| the preset suite, blocks 16–20 (`tests/voxo_preset_tests.cpp`, Release) | see `suite_preset_release.txt`: the hold 0.1414 → 0.1414 → 0.00000; the full pedal under sustainDecay 1.0: the ratio over a quarter second 0.317 (4.6 / s: 0.317); the half-pedal at 64: 0.081 over a tenth (25.1 / s: 0.081); the catch 0.0356 → 0.0356 half a second later → 0.00000 after the lift; the noises 0.0707 once / 0.0353 once, 127 / 100 / 30 again adding nothing; the filter 0.00000 up, 0.0707 down; the choke 0.2828 → 0.1789 (formula 0.1788) → 0.0707, one layer; zero allocation (blocks 19, 20) |
| the sampler suite (`tests/voxo_tests.cpp`, Release) | `suite_voxo_release.txt` — the two pedal gates and the MPE-zone pedal gate unchanged |
| the synth suite (`tests/voxo_suzu_tests.cpp`, Release) | `suite_suzu_release.txt` — unchanged (no Suzu change; the pedal's contraction path idle in the suite) |
| the Debug ctest | `ctest_debug.txt` |
| the fuzz through the pedal | `fuzz_60s_debug.txt`: 280 271 loads in 60 s, 108 776 loaded, 171 495 refused, no crash, no non-finite sample |
| the storm on the Dan Tranh library | `storm.txt`: 88 328 messages over 1937 frames, 7889 callbacks, 0 XRuns, render max 0.198 ms, 0 dropped, 96.8 fps |
| the web and lab gates | `web_and_lab_gates.txt`: the web gate GREEN (kind 4 declared tolerant for step 86's matrix section, −100.9 dB measured, see below); the lab gate GREEN, six pages |
| both tablets under the spike, the pedal in its storm | `tablets.txt`, `tablets/`: the sampler demo clean on both — the iPad 0 XRuns, render max 0.105 ms; the Tab (release APK) 0 XRuns, 0.801 ms of its 4 ms burst; Suzu patch 0 the iPad 0.210 ms, the Tab 21 proxy XRuns at 4.424 ms (the carried Tab thread-placement item, fifteen voices held by the pedal under the storm; 0 AAudio XRuns) |
| the Debug ctest | 14 / 14 in 410 s |

## Found on the way

* The first run of the contract check counted the suite's own measurement vectors (eleven and three): the blocks now render through one buffer allocated before arming.
* The format retriggers a CC-triggered sample on every message inside its range (the maker's note on a Pianobook piano: "the PEDALDOWN.wav will be triggered repeatedly while holding the pedal down"); Voxo fires on the crossing — a deliberate difference, recorded in the decision.
* The web gate read RED on the Duffing's matrix section (step 86's): its slide-to-pitch route goes through `exp2f` (libm), an ulp apart between the libraries; the audio bit-identical, the trace and the inspection within −100.9 dB; not the pedal, not the chaotic modulator (tested by script variants). Kind 4 joins the gate's tolerant list with the figure; step 86's evidence file held only the gate's tail, which is why the line was never seen.

## Files

`voxo/src/ds_preset.h/.cpp` (the attributes, `cc_range`, `SilencingMode`, `Trigger::Continuous`), `voxo/src/instrument.h/.cpp` (the compiled zone's pedal fields, the tag dictionary, `cc_zones`, `cc_trigger_mask`, `choke`), `voxo/src/voxo.cpp` (`Channel::pedal` / `cc_last`, `Layer::damp` / `damp_coef` / `choked`, `ENV_HOLD`, `Voice::cc_voice`, `pedal_damp_coef`, `pedal_set`, `pedal_apply`, `cc_fire`, `choke_layers`, the dispatch's filter, `render_layer`'s damping and key tracking; 0.27.0), `voxo/include/voxo.h` (the history comment), `voxo/COMPAT_REPORT.md` (the pedal paragraph), `tests/voxo_preset_tests.cpp` (16–20), `tests/voxo_fuzz.cpp` (the seeds and the pedal performance), `tests/voxo_c_compile.c`, `tests/fixtures/dspresets/pedal/` (+ `make_fixture_waves.py`), `tools/suzu_web_gate.mjs` (kind 4 tolerant), `site/src/content/docs/guide/sound.md`, `_work/DECISIONS_14.md`, `_work/TO_PROJECT_SPEC.md` §12.7, `docs/CHANGELOG.md`, `CLAUDE.md`.
