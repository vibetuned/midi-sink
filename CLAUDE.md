# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v5 — it absorbed
spec v2, the Phase-4 spec as §8, the Phase-5 spec as §9, and at the
Phase-10 fold (2026-10-13) the v2.0 arc's specs as shipped: §10 Media, §11
Quality of life, §12 Sound, §13 Synth, §14 Instruments, §15 the undone
items, §16 what carries past 2.0; §17 points at the roadmap). Decision
log: `docs/DECISIONS.md` (Part I = v1, Part II = v2, Part III = Phase 4,
Part IV = Phase 5, Part V = Phase 6, Part VI = Phase 7, Part VII = Phase 8,
Part VIII = Phase 9, Part IX = Phase 10, Part X = Phase 11, Part XI =
Phase 12 — folded 2026-10-18; references written as `DECISIONS_2 #n` …
`DECISIONS_11 #n` mean Parts II … XI; Phase 13 opens
`_work/DECISIONS_12.md`, merged as Part XII at its end). History:
`docs/CHANGELOG.md`; the completed roadmap is `docs/ROADMAP.md` (Parts
1–9); **the open roadmap is `_work/ROADMAP_6.md`** (the v3.0 arc, below).
Work items are fed one at a time by the user.

**Phases 1–10 are complete** (steps 1–70 folded into `docs/`; the v2.0 arc
closed on 2026-10-13). Step 34 shipped
`v1.0.0`: the release spine built the desktop three and the web, the App
Store and Google Play listings are public (linked from the README and the
install page), and the author uploads the iOS and Android builds by hand
(`ios/RELEASING.md`, `android/RELEASING.md`). Publishing the GitHub release
draft is the human act that fires the channel workflows (cask and winget
PRs) and opens the apt repository. The documentation site deploys from
`main` through `pages.yml`, the only Pages deployer (docs from the tree,
`/marble/` rebuilt from the newest stable tag, `/apt/` from published
releases — DECISIONS_4 #82); the release workflow deploys nothing to Pages.
Phase 6 (the Medium, `libsumi` 1.1.0) closed on 2026-09-26 with the
author's pre-release tag `v2.0.0-alpha.1`; Phase 7 (Sound) closed on
2026-09-28 (DECISIONS_6 #42) — the pre-release tag `v2.0.0-alpha.2`, cut by
the author at the step-55b fold (2026-09-29), covers Phase 7 and step 55b;
its notes are the `v2.0.0` section of the changelog. Phase 8 (Suzu, the
synth; Voxo 0.15.0, `libsumi` 1.3.0) closed on 2026-10-04 (DECISIONS_7
#35) — the pre-release tag `v2.0.0-alpha.3` is the author's to cut at the
fold, the synth played on every device; its notes are the same section.
Phase 9 (Instruments; `libsumi` 1.5.0) closed on 2026-10-06 (DECISIONS_8
#26) — the pre-release tag `v2.0.0-alpha.4` is the author's to cut at the
fold; its notes are the same section. Phase 10 (Publish; Voxo 0.16.0)
closed on 2026-10-13 (DECISIONS_9 #15): the release candidates
`v2.0.0-rc.1`–`rc.5` were the author's from step 67 on, the beta wave came
back positive, and the store builds and the tag `v2.0.0` are the author's
— zero code changes from the final RC; its notes are the same section.

**Voxo** is the sound: the sibling library `voxo/` (pure C
`voxo/include/voxo.h`, the callback contract at its top; C++20 in
`voxo/src/` compiling `core/src/midi_normalizer.cpp` from source; miniaudio,
pugixml, miniz and dr_libs fetched by CMake and compiled in) plays Decent
Sampler presets and libraries from the same MIDI bytes the visuals draw, on
every native platform; each shell's Sound setting starts the device (the
desktop's settings window, the tablets' Sound pages), the Dan Tranh demo
(CC0, `voxo/demo/`) sounds at first launch on the tablets, and libraries are
the user's. Its headless suites are `tests/voxo_tests.cpp`,
`tests/voxo_preset_tests.cpp` and `tests/voxo_fuzz.cpp`; the desktop
acceptance suite is `midi-sink --dev --voxo-preset <heavy library>
--voxo-storm <s>` (pass = 0 XRuns, 0 dropped, the visuals at rate —
DECISIONS_6 #32); the tablets' spike runners live in `tools/voxo_spike/`.
`voxo/COMPAT_REPORT.md` is the compat report's copy, asserted by test.
**Suzu** (Phase 8; its spec as shipped is `docs/PROJECT_SPEC.md` §13 —
`SYNTH §n` in Part VII and in the code's comments means `git show
5111748:specs/SYNTH_SPEC.md`) is the synth inside
Voxo — a source beside the sampler (`voxo_set_source`), the symplectic
phase-space cells of `voxo/src/suzu.h` with their class table (step 56, Voxo 0.8.0; the modal voice and the bow are step 57, the strings and the chaos step 58, the flute 58b, the sax and the trumpet 58c, the orbit trace — the synth drawing its own phase portrait into the water through the gesture ABI, `desktop/src/orbit_trace.cpp`, the bench's `--trace-test` and `--trace-demo`; the scope view on the canvas is libsumi 1.3.0's `sumi_set_scope`, a live-composite pass, the phase's one core touch — step 59; Suzu on the web since step 59b: Voxo deviceless in a standalone wasm driven by an AudioWorklet, the lab in `web/suzu/` (`build-web/suzu-dist`), the inspection call, the gates `tools/suzu_web_gate.mjs` (bit for bit against `build/tests/suzu_web_reference`) and `tools/suzu_lab_gate.mjs` (headless Chrome: every page's `?gate=` check, `--shots`); the lab's panels for every family since step 59c — a page each on the shared `web/suzu/site/lab-core.js`, the red controls on the engine's lab parameters, the worklet muting full scale and restarting on a blow-up, Voxo 0.15.0's recent trace and the winds' ledger; voice kinds 0–8, Voxo 0.15.0, DECISIONS_7 #8–#34), the modal lattice — its coupling's detune compensated at
patch load and load-gated — and the breath bow (step 57, Voxo 0.9.0,
`DECISIONS_7 #10–#12`), the Verlet chain, the hybrid string, the Duffing
cell, the kicked rotor and the chaotic modulator with the CFL and passivity
gates (step 58, Voxo 0.10.0, `#13–#17`; the sampler and Suzu can sound
together, `VOXO_SOURCE_LAYERED`), the acoustic bore and the jet — the flute,
voice kind 6 (step 58b, Voxo 0.11.0, `#18–#19`; the reed and the lips are
58c; the orbit trace is step 59);
its suite is `tests/voxo_suzu_tests.cpp` and the storm runs on it with
`--voxo-source suzu`; the tablets offer Suzu's patches (Voxo's PATCH TABLE of
eleven — the five of step 63 and the six VCSL fits, `DECISIONS_9 #9–#10`;
the knobs are the desktop's Sound section's). **Every synth voice ships with its sound
profile** (the author's rule, DECISIONS_7 #9): `midi-sink --dev
--voxo-profile <dir>` plus `tools/sound_profile.py` — the figure goes in the
step's evidence and in the Suzu book (`site/src/content/docs/suzu/`).

**Phase 10's record is Part IX of `docs/DECISIONS.md`** (`DECISIONS_9
#1–#15`; `docs/ROADMAP.md` Part 9). What shipped, in brief: step 67, the
book — the operator book's five electric pages, Anod and the palettes; the
Suzu book (`site/src/content/docs/suzu/`, nine chapters) with the lab
embedded through `site/src/components/Lab.astro` — THE LAB'S ADDRESS IS
`/marble/suzu/`, composed from the tag's `suzu-dist` by `pages.yml` and
`site/scripts/compose-marble.mjs` (`#2`; `lab-core.js` reads its docs root
from its own address); the guide's instruments, medium, sound, presets and
replay pages; the settings reference; the chart's fingering and Voxo
sections against `tests/fixtures/bytelogs/` (`#3`); `check.mjs` requires
all seventeen scenes and the six lab pages, every scene carrying the medium
toggle with the medium's strike (`#6`); `pages.yml` embeds `/marble/rc/`
and names the RC in the footer while an RC is newer than the stable (`#2`);
the gallery's nine 2.0 cards "coming soon" until the author's captures land
(`#4`). The testers' rounds: the Windows lane (`#7`–`#8`: the performance
counter in `backend_none.cpp`, the INI loader's chain split, no
`__attribute__` or `M_PI` under MSVC), the VCSL fits (`#9`:
`tools/suzu_fingerprint.py`, `suzu_fit.py`, `suzu_compare.py`,
`presets/*_vcsl.json`), Voxo 0.16.0's PATCH TABLE
(`voxo_suzu_patch_count/_name/_patch`, `voxo/src/suzu_patches.cpp`, gate
28 pinning it to the JSON files — `#10`), the orbit trace as the host
library `trace/` (`sumi_trace.h`; the tablets' Sound pages carrying *Synth
trace* — `#11`), the Linux lane's deb count 29 (`#12`), the synth suite's
headroom gates reading the best of three seconds (`#13`), the Tab's
NETWORK SESSION — `rtpmidi/` (`sumi_rtpmidi.h`, RTP-MIDI with Apple's
session protocol, POSIX UDP, no threads; not built on Windows),
`tests/rtpmidi_tests.cpp` (`--serve`, `--connect`), the Tab's
`SINK_NETWORK` on the MIDI thread, Kotlin advertising `_apple-midi._udp`,
proven live over Wi-Fi both ways (`#14`). The 2.0 release round, after
the close (`#16`–`#17`, evidence `docs/evidence/step69`): Voxo 0.16.1 —
on the winds the press that blows is the mouth alone (the trumpet
saturated on the Tab under a hard finger: the mouth's +14 dB and the
gain's +9 dB compounded; gate 29); and the iOS `audio` background mode
STAYS — without it `MIDISourceCreate` returns kMIDINotPermitted on the
iPad (re-verified) and the Play surface has no virtual source; the store
metadata is 2.0's (`#18`: `ios/metadata/`, `android/metadata/`); the
Android build takes AGP 9's defaults — built-in Kotlin, the new DSL, no
`kotlin-android` plugin, the opt-out flags gone from `gradle.properties`
— and the root lists set `CMAKE_POLICY_VERSION_MINIMUM 3.10` for the
third-party scripts (`#19`; the one Gradle line left is AGP's own). What
carries past 2.0 is `docs/PROJECT_SPEC.md` §16, in one place. `v2.0.0`
shipped on 2026-10-14 (the Android build's warnings cleared the same
day, `#19`).
**The v3.0 arc is OPEN (2026-10-14)** — `_work/ROADMAP_6.md` (Phases 11–16,
steps 71–92, folded from the author's proposals in `_work/specs/`, harmony
first — `DECISIONS_10 #2`; the order re-resolved `DECISIONS_10 #13`): Phase
11 Kessho and symplectic harmony (71–75) and Phase 12 the carry-overs (76,
76b, 77) are CLOSED and folded (Parts X and XI, 2026-10-18; the pre-release
tags `v3.0.0-alpha.1` and `-alpha.2` are the author's to cut); Phase 13
Suzu's deferred physics (78–82), Phase 14 four new symplectic voices
(83–86), Phase 15 Voxo DS 2.0 (87–91), Phase 16 the lab, the books and the
3.0 release (92). **THE CORE IS FROZEN from step 78 to step 92** (libsumi
1.12.0 is the arc's last additive version): a core change is a bug →
regression test → fix. `_work/TO_PROJECT_SPEC.md` is the arc's spec draft
(the proposals as the spec, each step correcting the subsection it ships;
`KESSHO §n` = `_work/specs/medium_specs_v3.md`); its fold into
`docs/PROJECT_SPEC.md` (spec v6) is the release's. Evidence folders of
shipped steps are removed at the fold — `git show <commit>:docs/evidence/…`
keeps them (the author's open viewings live there: step 71's K-1 polarizer
A/B and the six minerals at the exposure, step 76's Chirikov grid, 76b's
`leader_ab/`).
**Phase 11's record is Part X of `docs/DECISIONS.md`** (`DECISIONS_10
#1–#13`). What shipped, in brief — every step libsumi additive, the field
fixture bitwise: step 71 (1.6.0) `SUMI_MEDIUM_KESSHO` = 2, the composite
`kessho_col` in `composite.glsl`, the Michel-Lévy LUT built by the engine
from `core/src/kessho_lut.h` (`tools/kessho_lut.py`), `sumi_ctl_t` 20–23,
`polarizer_mode` / `braid_mode` / `kessho_*`, `slide_mode` 3,
`SUMI_EXPORT_KESSHO_CAUSTIC`, the bench's `--kessho-test`, `--kessho-flat`
(gate 2 as measured: a few thousandths is the field's floor — #4), `--ab
polarizer` (K-1, the author's viewing pending — #5); the bench's
`t42_print` DIPS — shoot through the export (`t71_shot`). Step 71b (1.7.0)
A MINERAL REPLACES KESSHO'S PALETTE: `sumi_mineral_t` (60 bytes),
`sumi_set_mineral` / `_get_mineral` / `_mineral_preset` (six, the optical
half; the habit/profile pairing is the shells' — `app_mineral_preset`,
`presets/kessho_<mineral>.json`), the laws in `composite.glsl`,
`core/src/minerals.cpp`; `drop_habit` / `habit_roundness` / `habit_blades`
(the crystal drop's R(θ) in `drop_fs`, exact by the theorem; habit 0
bitwise) and `tine_profile` (bump profiles in `tine_fs`; 3 the dev-only
fault); `--mineral`, `--habit-test`, `--habit-corner`, `--tine-fault`,
`--soak crystal_drop | twin_tine | terrace_tine`, `sumi_debug_pass_count`,
`tests/habit_det.c`; the dev key legend named for the live layout
(`dev_key_name`; "Q" on the author's AZERTY). Step 72 (1.8.0) THE ARNOL'D
TONGUE LOCK — `SUMI_DEFORM_ARNOLD` (`arnold_fs`: twist ∘ kick in ONE lookup,
five Newton iterations; `backward` the exact inverse), `displacement.cpp`'s
`sumi_arnold_tongue` / `_kmax` / `_window` / `_eval`, `sumi_add_arnold`,
press_mode 3 (Kessho's press default; the tempered fifth's two-cent beat
precesses the islands), `SUMI_CTL_ARNOLD_K`; the spec's corrections are
#9's. Step 73 (1.9.0) KIRCHHOFF CHORD BRAIDING — bend_mode 5 (Kessho's
bend default), `sumi_consonance` (Sethares on three anchors), the mapper's
braid block (the Strang halves of the swirl pass, 2N a frame, one vortex a
voice), the free vortices `sumi_braid_step` / `_invariants`, `--soak braid`
((d) informational), B-1 pinned the default, free the dance (the D key).
Step 74 (1.10.0) THE CHORD-WAVE LATTICE — the ripple pass windowed along s₂
(`win_c` / `win_w`), the mapper's Kessho poly-pressure branch (the axis by
the pitch class, the periods by the octave, delta-driven), route (b) the
rosettes awaiting the author. Step 75 (NO core change; Voxo 0.17.0,
`trace/`) THE POINCARÉ STROBE — Voxo's reference phasor at the bass, the
per-voice strobe ring, `voxo_set_strobe` / `_strobe_poll` / `_strobe_hz`;
the bridge's `poincare_frame` (`OrbitDust`, the ink route's micro-drop /
charge speck / micro-swirl at ~24 a second a voice by a fractional stride),
the desktop's *Suzu trace mode*, the suite's block 28, `--strobe-test`, the
fuzz's `--strobe-seconds`, `tools/strobe_chart.py`; found and fixed in Voxo:
a kind change over sounding notes, the flute above rate2/16, the bus's
non-finite BREAKER (`breaker_trips`); carried: the rotor's section sinks
into a 4-cycle at K 2.5.
**Phase 12's record is Part XI of `docs/DECISIONS.md`** (`DECISIONS_11
#1–#4`). Step 76 (1.11.0) ANOD FINISHED — `sumi_set_seed` / `_get_seed`
(the strike-phase word; the set rebases the mapper's gesture clocks), the
replay header's `seed` line; THE STRAIN BUFFER (`composite.glsl`'s third
program `strain`, RGBA16F: σ, the fast and slow ACTIVITY, the exposure;
`kessho_strain` a `@block` with three readers; the plasma bitwise by the
Anod golden `tests/fixtures/composite_512_metal_anod.rgba`,
`composite_gate.py --medium 1`); THE SPARK READING (`anod_glow_mode` 0
plasma / 1 spark, `spark_afterglow`: the ridges across ê_ψ⊥, the leader
from the fast activity, the afterglow from the slow); `--seed-test`,
`--spark-reading-test` (with the amended gates 12 the plateau and 13 the
torsion scene), `--chirikov-grid`, `--dump-medium`,
`sumi_debug_set_spark_nms` / `_read_strain`. The author's viewing (#2): a
strike's σ is a PLATEAU, the branch trick RETIRED (`spark_branch` gone).
Step 76b (#3) THE LEADER STROKE — `SUMI_DEFORM_LEADER` 17 (`leader_fs`:
up to eight segments, each a slip along itself in a C² window across AND
along, the ramps 2ℓ — one sub-step at every velocity, class sub-stepped,
composed last-first), the mapper's walk (`sumi_voice_mapper_add_leader`, a
32-bit LCG on the strike's draw; ℓ 0.012, h 0.003, p_b 0.15, depth 3,
N = 8 + 32·v), struck BESIDE the shear episode under glow_mode 1 (the
author: replacing it lost the impact), THE CHANNEL CHARGED by the pass (the
reading lights charge only), the episode at 120 sub-steps a second in ONE
pass per stroke per frame; `SPARK_AFTER_GAIN` 0.4; the strain pass compares
σ in half precision (a step-76 bug: a permanent activity floor);
`sumi_add_leader`, `--soak leader`, `--leader-test`, `--leader-pb`; the
book's Leader page; the desktop's *Drop & tine shape* section (the habit and
the tine profile for every medium, out of the Mineral panel); the author's
viewing: the walk kept, PLASMA STAYS ANOD'S DEFAULT. Step 77 (#4; 1.12.0,
THE LAST CORE-OPEN STEP) `SUMI_LAYOUT_BOEHM` 13 — nine keys in `buttons`
bits 0..8 (L1 L2 L3 on CC 110–112, the thumb / G♯ / R1 / R2 / R3 / D♯ on
`SUMI_CC_KEY_*` 114–119), three register cells on the brass geometry
(+0/+12/+24), the chart `sumi_layout_boehm_note` (the vent rule with the
two linkages and the F♯ cross fingering, total over the 512 states →
D4..C♯5); THE SPLIT SURFACE — `split_mode` / `split_layout` / `split_note`
(two Play layouts at half width, the notes divided at the split note;
`layouts.cpp`'s public queries wrap the unsplit `*_one` bodies, which call
each other through those, never the public names); hostmpe's
`_key_press/_release/_keys/_key_cc`, the announce of fifteen,
`_preset_set/_count/_next`; the desktop's rows, the web's toggles. The
tablets' key panels, split surfaces, preset pads, recorders' `seed` and
Glow-reading rows are their own sub-steps. The Linux box is out of
commission since 2026-10-05 (Linux gates as a recipe).
**Phase 13 is OPEN (2026-10-18; `_work/DECISIONS_12.md`, Part XII in flight;
the core FROZEN).** Step 78 (`DECISIONS_12 #1`–`#2`; Voxo 0.18.0): THE
FOUR-LANE ENGINE — `voxo/src/simd.h` (NEON / SSE2 / WASM SIMD128 / a scalar
fallback, the helpers always_inline; `mul_add` fused only where the scalar
compiler fuses, plain under `VOXO_NO_FMA`), `VerletString::step` and
`lattice_rotate_simd` / `_contract_simd` (their scalar twins `step_scalar`,
`lattice_step_scalar` are the suite's), THE CONTRACT: `voxo_nofma` is the
scalar reference (`VOXO_NO_SIMD VOXO_NO_FMA`), the wasm SIMD128
(`-msimd128`), bit for bit — measured so, and the FMA desktop reference and
the strobe returns bit-identical before/after; THE ×4 OVERSAMPLER —
`voxo_suzu_params_t.oversample` (2 / 4, the cell family only), `suzu::HalfBand`
over `voxo/src/halfband.h` (GENERATED by `tools/halfband_design.py`, 47 taps),
the strike's and the block's constants at the section's rate (`rateC`), the
mode ceiling the OUTPUT Nyquist; `suzu::prewarp`. THE HEADROOM BARS ARE THE
RELEASE BUILD'S (`build-release`, arm64, `-DBUILD_TESTING` on; a Debug run
prints the figure); the storm from `build-universal` with a session preset
(`--preset <json> --voxo-source suzu --voxo-storm 20`). Found: step 75's bus
BREAKER silenced the lab's blow-up red controls — `sw_breaker_trips` and the
worklet count a trip as a blow-up. **Step 79 shipped on 2026-10-19
(`DECISIONS_12 #3`; Voxo 0.19.0):** THE MEMBRANE, voice_kind 9 —
`suzu::Membrane<CAP>` (`suzu.h`: the 2D leapfrog inside a SIMPLY-SUPPORTED
zero ring, four lanes a row, the gong's stretching E off the Laplacian pass,
the contraction γ₀ + γ₂Δ, the cap on the plain energy, the exact invariant
`energy()`), the params `drum_size` / `drum_kappa` (the biharmonic SHARE) /
`drum_alpha` (the SWELL) / `drum_cap` / the lab's `drum_cfl_gate`, `drum_cfl`;
the grid CUT PER NOTE under `MEMBRANE_CFL_GATE` 1.95 — THE GONG'S BAR IS
`MEMBRANE_CFL_GATE_GONG` 1.80 (measured: a swelling sheet at 1.95 diverged);
THE BUS PLATE (`plate_*`, one `Membrane<64>` on `voxo_t` at the OUTPUT rate,
the voices' blocks in `plate_src`, `voxo_set_strike_point` — the iPad's and
the Tab's touch-begins hand the cell's centre; `VOXO_MOBILE` caps 32; clamped
under the gate with a log line, never refused); the patch table's fourteen
(*Drum*, *Gong*, *Harp on a soundboard* ↔ `presets/drum.json`, `gong.json`,
`harp_soundboard.json`); the suite's gates 32–38; the book's
`suzu/membranes.mdx`. FOUND: the bench's `--preset` reached the engine
alone — it now reaches the Sound settings for the run (unsaved), so a storm
"on a preset" plays that preset (step 78's did not); the Android DEBUG APK's
engine is −O0 — timing evidence comes from the release APK signed with the
debug key. Next: step 80, the stick-slip bow.
**Phase 9's record is Part VIII of `docs/DECISIONS.md`** (`DECISIONS_8
#1–#26`; `docs/ROADMAP.md` Part 8). What shipped, in brief: the trumpet
and the trombone (step 60, libsumi 1.4.0 additive) — stateful layouts whose
partial cells sound their partial minus the valves' offset or the slide's
semitones, the fingering CCs (valves 110/111/112 ≥ 64, the slide 113,
global on the master channel, `SUMI_CC_*`) decoded by the normalizer into
the engine's layout state (`sumi_get_layout_state`), the probe pure under
the state it is handed, `params.trumpet_arc`; Wicki–Hayden, STRINGS (three
fixed tunings, `params.string_tuning`) and the theremin (`SUMI_CELL_CONTINUOUS`)
(step 61, libsumi 1.5.0: every named layout ships, 0–12); hostmpe's valve
buttons and positional slider, the brass retune (`hostmpe_voice_retune`,
`hostmpe_tick`), the theremin surface, CC 120/123 and the panic, the
quick-switch, the mirror, the device profile offered never applied (step
62); the iPad's play surface — the thirteen layouts, the large fingering
panel in two forms, the strip's valves and slide, the brass arc re-cut on a
ring, `SUMI_MAX_ECHOES` 12 (step 63, the author's fix rounds #14–#17); the
Tab's, one for one on its own architecture, the panel in a window of its
own (the Tab's input reader keeps the fingers from the S Pen: the pen plays
alone there, the brass with the pen is the wire's CCs — #21), the core's
default params fixed (#19) (step 64); session replay — `replay/` (the
recorder's staged frame boundary, the `.sumireplay` text file of
`replay/FORMAT.md`, the player, the apply unit), every shell recording and
playing, the desktop re-sounding, the bench's `--record-demo` / `--replay`
/ `--replay-wall` / `--replay-wav` / `--record-live` / `--replay-live`,
`tools/replay_gate.py`, `tests/replay_tests.c`; bitwise on the recording
device, the Pixel's GLES recording within the mobile tier on the Mac, the
iPad's Metal a few percent of displacement off the Mac's — #24's open
question, the two telling experiments named (step 65); the web's overlays
for every keyed layout with the probe shim carrying the fingering, the
brass and tuning rows, replay playback in the browser (`?replay=`),
`web_gate.mjs --replay` / `--fullshots` (step 66). What carries: #24 (the
iPad's gap: the simulator and the phone-replays-the-iPad experiments), the
pen voices' retune through hostmpe's glide (#21), the Tab's replay run
(`--es recordLab 20`), and the Linux box (out of commission since 2026-10-05) — all in
`docs/PROJECT_SPEC.md` §16.
Phase 8's record is Part VII of `docs/DECISIONS.md` (`DECISIONS_7 #1–#35`; what
carries is in #35 — the sax's quasi-periodic A3 #33, the bore grid's
damping #30, the D3D11 bench's variable dip frame count #7, Anod's
run-to-run reproducibility through the gesture ABI #25, the entries'
`[ITERATE]`s); the v2.0 arc's specs are `docs/PROJECT_SPEC.md` §10–§16
since the Phase-10 fold (2026-10-13): `INSTRUMENT §n`, `QOL §n` and
`TO_PROJECT_SPEC §n` in Parts V–IX and in the code's comments mean `git
show bae21c4:specs/INSTRUMENT_SPEC.md`, `…:specs/QUALITY_OF_LIFE_SPEC.md`
and `…:specs/TO_PROJECT_SPEC.md`; `MEDIUM §n` `git show
76e2f5e:specs/MEDIUM_SPEC.md`; `SOUND §n` in Part VI `git show
7ceb111:specs/SOUND_SPEC.md`; `SYNTH §n` in Part VII `git show
5111748:specs/SYNTH_SPEC.md`. `specs/` is gone.
Phase 9 reopened the core for feature work (the layouts' probe, INSTRUMENT
§1 — in the ABI since step 41) and it is frozen again; the phase invariant is that
`tests/fixtures/field_512_metal.bin` stays bitwise on Metal (DECISIONS_5 #12,
a Metal invariant — #87; GL, D3D11 and GLES hold their tiers) — re-captured
ONCE at step 55b, when the field's payload became a displacement (u − x,
v − y, ink, aux; libsumi 1.2.0, DECISIONS_7 #1); the invariant restarts from
that fixture and no later step touches it. Every
operator declares its class and passes the four-part conservation gate
(`midi-sink --dev --soak <op>`); the composite gate runs per backend
(`tools/composite_gate.py --backend`). The operator-page, Voxo and Suzu drafts landed in the book at step 67; the
drafts stay in `site/drafts/` until the author removes them.
Android runs on this Mac (the Tab plugged in, Gradle/NDK installed); the
Linux box keeps only the Linux desktop; Windows has its box. The user owns
the specs and roadmaps: agents do not edit them; where a spec and a
decision entry disagree, flag it — the entry is the record of what shipped.
Standing rules: the core is frozen again (a new phase reopens it under the
bug → regression-test → fix pattern); version strings come from the git tag
via CI injection, never hand-edited; store submissions and beta promotions
are human actions.

Working rules (apply to every task):

1. Read the spec sections referenced by the current task before writing code.
   Where the spec and the decision log conflict, the spec wins; flag any
   remaining conflict instead of silently picking one.
2. Keep every sokol call behind `core/src/renderer.cpp` /
   `core/src/swapchain_*.{mm,cpp}`; nothing above those files may include
   sokol headers.
3. No exceptions, no STL types, no callbacks-into-C++ across `sumi_core.h`.
4. Backend purity rule (spec §4.6): the deformation chain never contains
   backend-specific branches; orientation flips live only in the final
   swapchain composite and the print readback path.
5. Prefer the choice that keeps the core identical across all five platforms;
   log every newly resolved ambiguity as a new numbered entry in
   `docs/DECISIONS.md` (the current last part — a new phase starts a new
   part in `_work/DECISIONS_<n>.md` and merges it at phase end).
6. The user makes all git commits themselves — agents never commit. Prepare
   DONE evidence under `docs/evidence/<task>/` (transient: evidence folders
   are removed from the tree once a milestone ships — git history keeps them
   — and their SUMMARY.md is condensed into `docs/CHANGELOG.md`; scripts
   worth keeping move to `tools/`, fixtures to `tests/fixtures/`), and report
   when the tree is ready, with an evidence reference for the commit message.
7. Do not implement anything from a later planned task early, even if
   convenient.

License: AGPL-3.0 (`LICENSE`).

Build: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`
(Homebrew cmake/ninja live in /opt/homebrew/bin.)
iOS: `cmake -B build-ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS
-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_OSX_ARCHITECTURES=arm64
-DBUILD_TESTING=OFF && cmake --build build-ios`, then `ios/prepare_release.sh`
(which rebuilds the core with the tag's version and regenerates the Xcode
project) and build the generated project. NEVER bare `xcodegen` in `ios/`:
`project.yml` reads `MARKETING_VERSION`, `CURRENT_PROJECT_VERSION` and
`SUMI_BUILD_DESCRIBE` from the environment, and with them unset the
generated project carries the literal `${…}` — the app's
`CFBundleShortVersionString` and `CFBundleVersion` come out EMPTY (it
happened on 2026-10-14; a bare run must be followed by the script, or by
exporting the three values first). Android: Gradle in `android/` drives
the repo-root CMake via externalNativeBuild. Web: `emcmake cmake -B build-web
-G Ninja && cmake --build build-web` (Homebrew emscripten needs
`EMSDK_PYTHON=/opt/homebrew/bin/python3.14`), then `tools/web_gate.mjs`.
