# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v5 — it absorbed
spec v2, the Phase-4 spec as §8, the Phase-5 spec as §9, and at the
Phase-10 fold (2026-10-13) the v2.0 arc's specs as shipped: §10 Media, §11
Quality of life, §12 Sound, §13 Synth, §14 Instruments, §15 the undone
items, §16 what carries past 2.0; §17 points at the roadmap). Decision
log: `docs/DECISIONS.md` (Part I = v1, Part II = v2, Part III = Phase 4,
Part IV = Phase 5, Part V = Phase 6, Part VI = Phase 7, Part VII = Phase 8,
Part VIII = Phase 9, Part IX = Phase 10; references written as
`DECISIONS_2 #n` … `DECISIONS_9 #n` mean Parts II … IX; the next phase
opens `_work/DECISIONS_10.md`, merged as Part X at its end). History:
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
steps 71–92, folded from the author's proposals in `_work/specs/`, in the
order the author chose on the fold's recommendation — harmony first,
`DECISIONS_10 #2`): Phase 11 Kessho — the third medium — and symplectic
harmony (71–75: the composite, the Arnol'd lock, Kirchhoff braiding, the
chord wave, the Poincaré strobe; libsumi 1.6.0 additive — THE CORE IS
OPEN FOR FEATURE WORK in steps 71–77 and frozen from step 78 to 92),
Phase 12 the carry-overs (76, 77: Anod's seed, the strain buffer, the
Chirikov grid and the spark reading `glow_mode`; the Boehm layout — moved
directly after Kessho on 2026-10-14, `DECISIONS_10 #13`), Phase 13 Suzu's
deferred physics (78–82), Phase 14 four new symplectic voices (83–86),
Phase 15 Voxo DS 2.0 (87–91), Phase 16 the lab, the books and the 3.0
release (92); `_work/DECISIONS_10.md` (Part X in flight,
Phase 11's — `#1` the fold and the facts it corrected against the tree,
`#2` the order; one file per phase, `_11` … `_15`);
`_work/TO_PROJECT_SPEC.md` (the arc's spec draft, opened at the START —
the proposals as the spec, each step correcting the subsection it ships;
`KESSHO §n` = `_work/specs/medium_specs_v3.md`). **Step 71 shipped on
2026-10-14 (`DECISIONS_10 #3`–`#5`):** `SUMI_MEDIUM_KESSHO` = 2, the composite
in `composite.glsl` (`kessho_col`), the chart `core/src/kessho_lut.h` from
`tools/kessho_lut.py`, the LUT built by the engine (`engine_kessho_lut`) and
uploaded by the renderer, the twelve Kessho palettes, `sumi_ctl_t` 20–23,
the params `polarizer_mode` / `braid_mode` / `kessho_*`, `slide_mode` 3,
`SUMI_EXPORT_KESSHO_CAUSTIC`; the bench's `--kessho-test` (12 checks),
`--kessho-flat <op|all>` (gate 2 as measured: a few thousandths is the
field's floor, the classes not separable — `#4`) and `--ab polarizer
--ab-out <dir>` (the K-1 renders, the author's viewing pending — `#5`);
the bench's `t42_print` DIPS (resets the field): shoot through the export
(`t71_shot`) when the field must stay. **Step 71b shipped on 2026-10-14
(`DECISIONS_10 #6`–`#7`; libsumi 1.7.0, additive):** A MINERAL REPLACES
KESSHO'S PALETTE — `sumi_mineral_t` (sized, 60 bytes), `sumi_set_mineral` /
`sumi_get_mineral` / `sumi_mineral_preset` (six, the OPTICAL half; the
pairing with a habit and a profile is the shells' — the desktop's
`app_mineral_preset` — and `presets/kessho_<mineral>.json`'s), the laws in
`composite.glsl` (`kessho_layer_a`, `kessho_brush`, the walk-off, the film,
the dispersion, the ring; the biaxial brush and the selenite plate are
π/2-periodic stylisations, #6), `core/src/minerals.cpp`; the params'
`drop_habit` / `habit_roundness` / `habit_blades` / `tine_profile` — the
crystal drop's R(θ) in `drop_fs` (exact by the theorem; habit 0 bitwise)
and the tine's bump profiles in `tine_fs` (profile 0 bitwise; profile 3 the
dev-only fault, `sumi_debug_set_tine_override`); the bench's `--mineral`,
`--habit-test`, `--habit-corner <dir>`, `--tine-fault`, `--soak
crystal_drop | twin_tine | terrace_tine` (the drop's (d) is informational:
its expansion carries ink off the sheet), `sumi_debug_pass_count`;
`tests/habit_det.c`. The desktop's Mineral section under Kessho, the INI's
`mineral_*`, the session's `mineral` block; the Medium radios one per line,
the dev key legend named for the live layout (`dev_key_name`; "Q" on the
author's AZERTY), the Kessho knob "Exposure". The tablets' medium pickers do
not offer Kessho or minerals yet (their own steps); the author's viewing of
the six at the exposure is pending; the habits ship as the closest real
ones at roundness 0, reviewed at the phase's end (`#8`). **Step 72 shipped
on 2026-10-14 (`DECISIONS_10 #9`; libsumi 1.8.0, additive):** THE ARNOL'D
TONGUE LOCK — `SUMI_DEFORM_ARNOLD` (`arnold_fs`: twist ∘ kick in ONE
lookup, the kick's inverse one Newton on θ, FIVE iterations; `backward` = 1
the pass's exact inverse), `core/src/displacement.cpp`'s
`sumi_arnold_tongue` / `_kmax` / `_window` / `_eval` (the CPU reference),
`sumi_add_arnold`, press_mode 3 live (Kessho's press default; the bass
keeps the swirl; the tempered fifth's two-cent beat precesses the islands —
the step's finding), `SUMI_CTL_ARNOLD_K`; the corrections to the spec
(K_max = 0.8/max|W′| without the q; W zero at the centre; the twist
windowed to 2.5 R; the convergents of order 2..6) are #9's; the bench's
`--arnold-test`, `--arnold-render <dir>`, `--arnold-probe <dir>`, `--soak
arnold`, `sumi_debug_add_arnold` / `_arnold_map`. **Step 73 shipped on
2026-10-14 (`DECISIONS_10 #10`; libsumi 1.9.0, additive):** KIRCHHOFF CHORD
BRAIDING — bend_mode 5 live (Kessho's bend default; the drop holds, the bend
moves the consonance), `sumi_consonance` (`midi_normalizer.cpp`: Sethares
on three anchors, the register floored at C4), the mapper's braid block
after the voice loop (the Strang halves of the swirl pass, 2N a frame, one
vortex a voice; `bend_s` beside the glide in every mode), the free vortices
`sumi_braid_step` / `_invariants` (`displacement.cpp`, double, the midpoint
rule to 1e-15), `params.braid_mode` and `SUMI_CTL_BRAID_COUPLE` live; the
bench's `--braid-test`, `--soak braid` (its (d) informational: 2N full-sheet
resamplings a frame are the composition's nature), `--ab braid --ab-out
<dir>`; the settings' *Braid consonance* and *Braid vortices*; B-1 decided
by the author: pinned the default, free the "dance" mode (the D key). **Step
74 shipped on 2026-10-14 (`DECISIONS_10 #11`; libsumi 1.10.0, additive):**
THE CHORD-WAVE LATTICE — the ripple pass with a window along s₂ (`win_c` /
`win_w` in `sumi_deform_ripple_t`, 0 = bitwise v2), the mapper's Kessho
poly-pressure branch (the axis by the pitch class, 15° a fifth-step; the
periods by the octave; delta-driven like the ripple bake, `wave_baked`),
`sumi_debug_add_chord_wave`; the bench's `--chord-wave-test`, `--soak
chord_wave`; route (b) the rosettes awaits the author's viewing. **Step 75
shipped on 2026-10-14 (`DECISIONS_10 #12`; Voxo 0.17.0 and `trace/`; NO core
change — the core stays open through step 77):** THE POINCARÉ
STROBE — Voxo's reference phasor at the bass (the lowest held voice, bend
included; per sub-step in double, the block's wraps precomputed), the
per-voice strobe ring, `voxo_set_strobe` (1 the bass, 2 the own drive: the
Duffing's phase wrap, the rotor's kick = Chirikov's section) /
`voxo_strobe_poll` / `voxo_strobe_hz`; `sumi_trace_config_t.mode` and the
bridge's `poincare_frame` (`OrbitDust`, the scopes' dust, the ink route's
micro-drop / charge speck (Anod, the continuous trace too, at the rate) /
micro-swirl through the hook at ~24 a second a voice by a fractional
stride — every return floods, an integer stride aliases); the
desktop's *Suzu trace mode*, `suzu_trace_mode`,
the preset's `trace_mode`; the suite's block 28, the bench's
`--strobe-test`, the fuzz's `--strobe-seconds`, `tools/strobe_chart.py`.
Found and fixed in Voxo by the strobe fuzz: a patch of another voice kind
over sounding notes (ends them now), the flute bent above rate2/16 (the
model's ceiling), and the bus's non-finite BREAKER (`breaker_trips`). Found
and carried: the rotor's section is the standard map only approximately
and sinks into a 4-cycle at K 2.5 (Phase 12). The tablets' *Synth trace*
mode rows are their own sub-steps. The Phase-11 close is the author's
(the commit, then the fold of `_work/DECISIONS_10.md` as Part X on the
author's word, the pre-release `v3.0.0-alpha.1`). **Step 76 shipped on
2026-10-14 (`DECISIONS_11 #1`; libsumi 1.11.0, additive; Phase 12, the
carry-overs, `_work/DECISIONS_11.md`):** ANOD FINISHED — `sumi_set_seed` /
`sumi_get_seed` (the strike-phase word; the set rebases the mapper's gesture
clocks too — measured necessary), the replay header's `seed` line; THE
STRAIN BUFFER (`composite.glsl`'s third program `strain`, an RGBA16F side
target: σ, the fast and slow ACTIVITY — the strain's rate integrated, since
σ never decays — and the exposure; `kessho_strain` a `@block` with three
readers; the plasma's inline copy bitwise by the new Anod golden
`tests/fixtures/composite_512_metal_anod.rgba`, `composite_gate.py
--medium 1`); THE SPARK READING (`params.anod_glow_mode` 0 plasma / 1
spark, `spark_afterglow`; ridges within a tenth of their neighbours across
ê_ψ⊥, the leader from the fast activity, the afterglow from the slow);
Kessho's Layer A reads the exposure (its gates against it open); the
bench's `--seed-test`, `--spark-reading-test`, `--chirikov-grid`,
`--dump-medium`; `sumi_debug_set_spark_nms` / `_read_strain`. **The
author's viewing (2026-10-15, `DECISIONS_11 #2`): a strike's σ is a
PLATEAU (the triangle stack's slope is piecewise constant), the reader
thins nothing on it, the branch trick is RETIRED (`spark_branch` gone
before ship) — the line has to be written.** **STEP 76b SHIPPED on
2026-10-15 (`DECISIONS_11 #3`; libsumi 1.11.0 still, `sumi_add_leader` and
`SUMI_DEFORM_LEADER` 17 added):** THE LEADER STROKE — under glow_mode 1 the
Anod note-on strikes a jagged tree walked from the key BESIDE its shear
episode (the author's viewing of 2026-10-18: replacing it lost the impact;
the strike's draw: the stroke's first, the blob's phase after) (the mapper's
`sumi_voice_mapper_add_leader`, a 32-bit LCG on the strike's draw; ℓ 0.012,
h 0.003, p_b 0.15, depth 3, N = 8 + 32·v), each segment a slip along itself
in a C² window across AND along (`leader_fs`, the ramps 2ℓ: ONE sub-step at
every velocity, class sub-stepped), THE CHANNEL CHARGED by the pass (the
strike's band and aux within h of the path — the reading lights charge only,
the proposal had the stroke walking into dark water), the episode at 120
sub-steps a second in ONE pass per stroke per frame (up to eight entries
composed last-first); `SPARK_AFTER_GAIN` 0.4; the bench's `--soak leader`,
`--leader-test` (`--ab-out`: the three-way viewing), `--leader-pb`,
`sumi_debug_add_leader_seg` / `_set_leader_pb` / `_set_leader_charge`; the
amended step-76 gates 12 and 13 in `--spark-reading-test`; the book's Leader
page; the desktop's *Drop & tine shape* section (the habit and the tine
profile for every medium, out of Kessho's Mineral panel — the author's).
Viewed by the author on 2026-10-18 ("this looks good"): the walk kept, PLASMA
STAYS ANOD'S DEFAULT glow_mode. The tablets'
recorders' `seed` and Glow-reading rows are their sub-steps. After 76b:
step 77 — the Boehm layout (the last core-open step).
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
