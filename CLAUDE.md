# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v5 — it absorbed
spec v2, the Phase-4 spec as §8, the Phase-5 spec as §9, and at the
Phase-10 fold (2026-10-13) the v2.0 arc's specs as shipped: §10 Media, §11
Quality of life, §12 Sound, §13 Synth, §14 Instruments, §15 the undone
items, §16 what carries past 2.0; §17 points at the roadmap). Decision
log: `docs/DECISIONS.md` (Part I = v1, Part II = v2, Part III = Phase 4,
Part IV = Phase 5, Part V = Phase 6, Part VI = Phase 7, Part VII = Phase 8,
Part VIII = Phase 9, Part IX = Phase 10, Part X = Phase 11, Part XI =
Phase 12, Part XII = Phase 13, Part XIII = Phase 14 — folded 2026-10-25;
references written as `DECISIONS_2 #n` … `DECISIONS_13 #n` mean Parts II …
XIII; Phase 15 opens `_work/DECISIONS_14.md`, merged as Part XIV at its
end). History:
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
11 Kessho and symplectic harmony (71–75), Phase 12 the carry-overs (76,
76b, 77), Phase 13 Suzu's deferred physics (78–82) and Phase 14 the four
new symplectic voices (83–86) are CLOSED and folded (Parts X, XI, XII and
XIII — 2026-10-18, 2026-10-19 and 2026-10-25; the pre-release tags
`v3.0.0-alpha.1` … `-alpha.4` are the author's to cut); Phase 15 Voxo DS
2.0 (87–91) is CLOSED (steps 87–91 shipped; its fold and the tag `v3.0.0-alpha.5` are the
author's), and Phase 16 the lab, the books and the 3.0 release is OPEN
(92 — its first session shipped 2026-10-26, `_work/DECISIONS_15.md #1`; the step also folds the operator drafts and THE LESSONS, the six modules
of `site/drafts/lessons/`, into the book, applying the review's checks,
links and fixes: `site/drafts/lessons/REVIEW_modules_02-06.md` and module
1's `ADAPTATION_NOTES.md`; the author's word, 2026-10-25). **THE CORE IS FROZEN from step 78 to step 92** (libsumi
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
**Phase 13's record is Part XII of `docs/DECISIONS.md`** (`DECISIONS_12
#1`–`#6`; folded 2026-10-19). What shipped, in brief — every step Voxo
additive, the core frozen: step 78 (`DECISIONS_12 #1`–`#2`; Voxo 0.18.0): THE
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
debug key. **Step 80 shipped on 2026-10-19 (`DECISIONS_12 #4`; Voxo 0.20.0):**
THE STICK-SLIP BOW — `suzu::StickSlip` (`suzu.h`: the hyperbolic friction
curve, three Newton steps, THE BOUND and THE HYSTERESIS; `solve_naive` the raw
steps, the ledger's red), `bow_mode` 1 on the lattice's point, the chain's
node (BETWEEN `VerletString::kick` and `drift`) and the hybrid's loop
(`HybridString::bow`: two taps against Z = 1 — the clean Helmholtz; the chain's
dispersion smears the corner), `bow_force` / `bow_speed` / the lab's `bow_naive`;
the mouth the speed, the press and the velocity the force; the ledger's gain at
the SYNCHRONIZED velocity (`bow_work`, `bow_gain` in the inspection); the
strike position from the cell `voxo_set_strike_pluck` (the tablets' touch-begins
hand it); the patch table's sixteen (*Bowed string (stick-slip)* = the waveguide,
*Bowed chain*); the web gate drops the bowed LATTICE (the branch flips under the
lattice's sinf); the chart's `bow_force` (mode 5: the press ramps the force).
**Step 81 shipped on 2026-10-19 (`DECISIONS_12 #5`; Voxo 0.21.0):** THE
ACOUSTIC BORE V2 — THE TONEHOLES (`tonehole_mode` 1 on the flute and the sax:
`Bore::holes_set/_open/_energy`, a hole an inertance kicked at the half step +
a one-pole resistance at a fractional cell, 1/L_h = S/8 per `hole_size`,
the vent 90 cells' at an eighth; THE PICKUP = the end's velocity + the open
holes' flows — the far end alone sits past the lattice; the chart
`boehm_note_of` copied from the core, the keys from CC 110–112/114–119 on the
master channel into `v->keys`; `suzu_holes_place` (a cone counts its apex),
the calibration per register at patch load — pulsed on the flute, BLOWN on
the sax (`suzu_blow_offset`), 228 / 770 ms, cached in `suzu_hole[2]`; two
registers, the third and a disagreeing key take the v1 cut; `k[11..13]`),
THE LEAK (`bore_leak`, THE SAX'S BELL'S ALONE — on the flute and the trumpet
it ate the bass; `Bore::leak` + `leak_correction`, the reactance counted in
the cut; the quasi-periodic A3 cured: −60 dB sidebands, the red +13),
`reed_inertance` (as proposed, dflt 0, partial), THE SHOCK (`brass_shock`, THE TRUMPET'S:
the local Courant number on both sweeps, saturated; the Lapidus flux limiter
`shock_limiter`, its loss `k[10]` in the ledger; the red = the bore's roughness at κ, the valve gate's refusal at 2κ), THE
TWO-MODE LIPS (`lip_shear`, `lip_shear_ratio`, `Valve::setup_shear`; dflt 0,
bit for bit); the web gate's sax in the libm class; gates 42–45. Found: a
flat hole resistance eats the fundamental; the hole mode needs CFL room
(Gershgorin); the profiles catch what the gates do not (the flute's bass); a running
sum in a vectorised pass serialises it (the limiter's loss: the trumpet's
storm threw XRuns until the terms were summed after the passes).
**Step 82 shipped on 2026-10-19 (`DECISIONS_12 #6`; Voxo 0.22.0; the preset
schema 2):** THE EDITABLE TABLE — `modal_preset` 5 with `modal_ratio[16]` /
`modal_t60[16]` / `modal_weight[16]` (a zero defers to the formula; `suzu.h`'s
`PRESET_TABLE` in the one builder), `modal_comp` (0 = the raw lattice: NO
swirl share, the load gate's bound WITHOUT the solve —
`lattice_lambda_max_over_pitch(…, compensated)`), `voxo_suzu_table` (the
resolved table for any preset); THE TUNING — `voxo_set_tuning` (.scl + .kbm
text) / `_load_tuning` (paths) / `_clear_tuning` / `_tuning_count` / `_hz` /
`_name`, the 128-note table double-buffered under `voice_retune` (the ONE
note-to-frequency site, Suzu and the sampler alike; the bend an ET offset);
THE UNISON PAIR (patch 16, `presets/unison.json` — written by the serializer
itself through `make_unison`: the first file carrying the serializer's full
key set, so gate 28 skips `trace_*` and knows `oversample`): the split is the
chain's 2×2 block, +κ(3∓√5)/4, not √(1+2κ); `presets/`: schema 2, the arrays
(`r_farray`, `w_farr`), `sumi_preset_suzu_to_voxo` (`sumi_preset_voxo.c/.h`,
the SumiPreset module map: the tablets' patch files); the desktop's table
rows, INI comma lists, tuning rows; the tablets' *Import a Suzu patch* /
*Import a Scala scale* through the pickers (iOS `patchesDir` / `tuningsDir`,
the Tab's `nativeVoxoSetPatchFile` / `nativeVoxoSetTuning`); gates 46–48.
Phase 13 closed with this step (folded 2026-10-19); the tag `v3.0.0-alpha.3`
is the author's.
**Phase 14's record is Part XIII of `docs/DECISIONS.md`** (`DECISIONS_13
#1`–`#4`; folded 2026-10-25): four new symplectic voices, every one in the
§13.1 class table with its red control, its profile and its strobe
portrait — step 83 the Toda lattice soliton ring (`voice_kind` 10), step 84
the Boris–leapfrog magnetic gyroscope (11), step 85 the port-Hamiltonian
Dirac network (12) and the billiard (13), step 86 the per-voice modulator
and the mod matrix; the spec draft's §13.13–§13.14 as shipped. What
shipped, in brief — every step Voxo additive, the core frozen:
**Step 83 shipped on 2026-10-19 (`DECISIONS_13 #1`; Voxo 0.23.0):** THE TODA
LATTICE SOLITON RING, voice_kind 10 — `suzu::TodaRing` (`suzu.h`: N ≤ 64 nodes
under H = Σp²/2 + (a/b)Σ(e^{−br} + br − 1), the kick–drift leapfrog WITH THE
STATE IN DOUBLE and THE RING'S OWN EXPONENTIAL (`suzu::toda_exp`, a degree-11
Taylor after Cody–Waite, 10⁻¹⁴, the same bits everywhere — libm's exp was the
Tab's whole budget), k set so the first mode is the note at the step, the C¹ SOFT
CAP E = e up to cap/2 then cap − cap²/4e, the strike a mean-free Gaussian
compression launched with p = −√k·r, `kick_all` the swirl's uniform momentum,
THE PICKUP STANDING IN THE LAB against the mean drift — ξ runs back at Σp/N a
step, a Catmull–Rom cubic over the bonds; a fully Eulerian pickup read a hard
soliton's density spike at −43 dB in the top octave), the params `toda_nodes`
16 / `toda_b` 0.6 / `toda_cap` 6 / `toda_width` 1.5 / `toda_gate`; THE
RESOLUTION RULE (`suzu_toda_fit`): √k·ĉ·b ≤ `SUZU_TODA_RES` 0.15 by two to
eight inner steps a 2× sub-step (`toda_m`; four at most under `VOXO_MOBILE`;
the momenta rescaled on a change;
the pickup read twice a sub-step at ×4) and, where eight cannot, THE
NONLINEARITY CAPPED FOR THE NOTE at the strike (`toda_b_cap`, the 1.3×
self-focusing allowance; lowered never raised — a limiter following the live
peak PUMPED the pulse); the inspection k[0..8] (H in the ×4 step's units
whatever m); the patch table's eighteenth *Toda ring* (`presets/toda.json`,
`tools/make_patch_preset.cpp`); the preset schema's four fields; the desktop's
*Toda ring (solitons)* voice and rows; the suite's gates 49–54 (the linear
modes exact, the press's train +427 cents, the ×4 path −132 dB, TEN LOSSLESS
MINUTES on the ring alone — the MINUTE-MEAN H drifts 0.000017 dB (the
instantaneous H breathes ±0.06 dB: the modified H is the conserved one), the
soliton's amplitude × width within 0.72 %, the swirl's Doppler +19.7 %, the
red control +19 dB a second at C8, the strobe 440/440, sixteen rings C3–C7
18.6 % of the period); the storm 0 XRuns; the iPad's spike clean (0.42 ms of
2.67); THE TAB OVER ITS 4 ms BURST (13.9 ms — its audio thread ~40× the Mac's
cost on any voice, the step-82 unison at 1.9 ms there vs the Mac's 0.05; the
same APK on the emulator at the Mac's speed clean: a shell thread-placement
item, ADPF/affinity, CARRIED for the author; the tier caps m ≤ 4, N ≤ 32); the
book's `suzu/toda-ring.mdx`. **Step 84 shipped on 2026-10-21 (`DECISIONS_13
#2`; Voxo 0.24.0):** THE BORIS–LEAPFROG MAGNETIC GYROSCOPE, voice_kind 11 —
`suzu::Gyro` (`suzu.h`: (x₁, x₂, y₁, y₂) in double, the half kick, THE CAYLEY
ROTATION AS THREE SHEARS (a cos/sin matrix's c² + s² ≠ 1 is a drift of e^{±1.7}
over ten minutes), the half kick, the drift; `angles()` the doublet θ/2 ± ψ,
cos ψ = cos(θ/2)(1 − ε²/2); the strike a displacement by the equal-power pan
law from the pluck), `boris_b` / `boris_b_rest` (THE FIELD IN HERTZ, the poly
pressure's: rest + (full − rest)·swirl, 0..50) / `boris_prewarp` (0 the red:
1.208 cents at B6), `suzu_boris_fit` every block; THE FIRST STEREO VOICE of
`render_suzu_other` ((x₁, x₂) → (L, R), `svf2`, the plate fed the mean); the
own-clock strobe = the cyclotron period; the trace's pair (x₁, x₂); the bench's
`--boris-demo <dir>` (the spirograph into Kessho and Anod's spark); the patch
table's nineteenth *Gyroscope (Zeeman stereo)* (`presets/gyroscope.json`); the
preset schema's two fields; the desktop's voice and rows; gates 55–58 (the
doublet 0.001 cent from the struct's own complex map over A2..B6 × 0..50 Hz,
the red 1.208, the mono line bit for bit, the rendered pair 0.000 cent, the
energy ±0.031 dB with 0.000000 dB of drift over ten minutes, the own clock 39
returns at 20 Hz); `tools/boris_chart.py`; the web gate bit-identical at 0.24 %
of the quantum; the storm 0 XRuns; the iPad clean (the Tab UNPLUGGED at the
step — the emulator stood in); the book's `suzu/gyroscope.mdx`. **Step 85
shipped on 2026-10-21 (`DECISIONS_13 #3`; Voxo 0.25.0):** THE PORT-HAMILTONIAN
DIRAC NETWORK, voice_kind 12 — `suzu::Dirac` (`suzu.h`: n ≤ 36 states in
double, H = ½xᵀQx with Q diagonal, THE CAYLEY STEP M = (I − h/2·A)⁻¹(I + h/2·A)
by Gauss–Jordan at the strike, its Q-NORM GAIN by power iteration (`gain_q`,
k[6]: 1 to 2e-16 lossless over random skew J — gate 59), `chain()` the
prepared network: the springs' EXTENSIONS and the masses' momenta as the
states, J the incidence, the bridge (m_b, k_b) on the last spring, the chain's
lowest mode on the note by inverse iteration with the bridge's loading, the
stick-slip bow on a mass as a port (`SUZU_DIRAC_KF` 0.18 — the chain's scale
threw the mass to the bow), `rattle()` the stop under the bridge with its
declared loss in the ledger; the ledger H + losses − gain = H₀ to 1e-6, gate
60; THE VOICE IS THE FORCE ON THE BRIDGE (the bridge's velocity tilted 6 dB an
octave), the kick and the bow's reference × 220/f, the plucked profile 14 dB over the
keyboard — the body's dip; at 2500 Hz 9 dB but the bow does not sing), the
params `dirac_masses` 8 / `dirac_bridge_hz` 700 / `dirac_bridge_mass` 0.5 / `dirac_bridge_decay`
0.5 / `dirac_stop` 2 (in the pluck's static deflection) / `dirac_stop_loss`
0.3; THE BILLIARD, voice_kind 13 — `suzu::Billiard` (the exact rotation at ×4,
the stadium wall (a, b = 1, hl), the collision's fraction by bisection, the
reflection, det J = +1 to 2e-12 dB over 66k collisions, THE POLYBLAMP of each
kink through a one-sample delay: the alias −80.5 dB (gate 61); the fold raises
the bounce to (π/2)/asin(1/v) × the note; `bill_exact` 0 the red — the bounce
quantised to whole steps, 3.8 % off), the params `bill_drive` 1.4 / `bill_aspect`
1 (CC 74 ×½..×2) / `bill_length` 2 (the wheel) / `bill_length_rest` 0 /
`bill_exact`; (x₁, x₂) → (L, R) with `hbr[2]`; the strobe from the circle's
LINE (a central kick is radial; the roadmap's ring wants an off-centre strike,
flagged) to the stadium's dust (gate 62); the patches *Prepared network* and *Billiard (stadium)* (20, 21); the
preset schema's nine fields; the desktop's two voices; `--voxo-chart`'s
`billiard_sweep`; the strobe chart's two billiard panels; the book's
`suzu/dirac-network.mdx` and `suzu/billiard.mdx`. **Step 86 shipped on
2026-10-25 (`DECISIONS_13 #4`; Voxo 0.26.0):** THE PER-VOICE CHAOTIC MODULATOR
(`SuzuVoice::pend`, kicked at the voice's strike, stepped in its block when a
slot reads it; the instance's pendulum gone) AND THE FOUR-SLOT MOD MATRIX —
`mod_src[4]` / `mod_dst[4]` / `mod_amt[4]` (sources 1 the modulator, 2 press, 3
swirl, 4 slide centred, 5 wheel, 6 breath, 7 velocity; targets 1 cutoff ±3 oct,
2 coupling +0.5, 3 K +1, 4 drive ×, 5 LEVEL ×, 6 PITCH ±1 semitone; the depths
signed), `suzu_mod_compute` summing per target each block (the inspection's
`mod_out`, `mod_sum[8]`), THE LEGACY PAIR FOLDED INTO SLOT 0 at
`voxo_set_suzu_params` (bit for bit on the rotor; the desktop folds and clears
it); the cutoff and the coupling targets ROUTE FOR THE FIRST TIME (step 58
wired K and the drive only); the preset schema's three arrays of four
(`r_farray` reads all three), the INI's lists, the desktop's *Suzu mod matrix*
rows, the web's twelve scalars, the generator's `--slot`; gates 63–64.
FOUND IN VOXO SINCE STEP 56: a cutoff under the ceiling at ZERO resonance
blew every voice — `svf_f_for`'s ringing compensation ran past stability
where an overdamped filter has no ring (q 2 at 4 kHz: f 3.6 → nan → the
breaker); it falls back to the plain coefficient outside |cos θ| < 1, fq < 1
(gated across the keyboard); AND the SVF's fs/6 cutoff ceiling (step 57) sat
past the Jury bound f² + 2qf < 4 at low resonance (the storm's cutoff slot
tripped the breaker 44×) — THE CEILING FOLLOWS q (0.95 of the bound, the fs/6
above it), the eigenvalues at the ceiling gated under 1. Phase 14 closed
with this step (folded 2026-10-25); the tag `v3.0.0-alpha.4` is the
author's; carried: the Tab's audio-thread placement (ADPF, affinity — a
shell item), the Tab unplugged since step 84 (the emulator stands in), the
billiard's off-centre strike for a ring section.
**Phase 15 is OPEN (2026-10-25; `_work/DECISIONS_14.md`, Part XIV in
flight; the core FROZEN):** Voxo DS 2.0 — the Decent Sampler coverage's
second generation (steps 87–91: the sustain pedal v2, chorus and phaser
with the CC-map targets, the filter family and filter envelopes with the
bus EQ and the waveshaper, the `<modulators>` matrix with legato and glide
and the compat report pruned, the instrument slot, the offline bounce and
the device picker; the spec draft's §12.7–§12.12; the author's Pianobook
libraries are the acceptance material). **Step 87 shipped on 2026-10-25
(`DECISIONS_14 #1`; Voxo 0.27.0, the ABI unchanged):** THE SUSTAIN PEDAL
V2 — the format's attributes READ FROM THE DECENT SAMPLER GUIDE, not the
proposal (no `trigger="cc"`: a pedal noise is `onLoCC64`/`onHiCC64` on the
sample with `pitchKeyTrack="0"`, the CC number in the attribute name,
`ds_preset.cpp`'s `cc_range`; the note-on filter `loCCN`/`hiCCN`; the choke
`silencedByTags` + `silencingMode` fast (THE DEFAULT) | normal +
`silencingDecay`; `sustainDecay` is VOXO'S OWN); THE PEDAL IS A POSITION
(`Channel::pedal`, `cc_last[128]`): a note-off at p > 0 is held and its
layers take a second envelope factor `damp` decaying at the rate
(1 − p)·4.6/release + p·4.6/sustainDecay (no attribute → 0 → the hold, BIT
FOR BIT the old pedal; `pedal_damp_coef`, `pedal_set`); RE-PEDALLING catches
a releasing tail at its level (`ENV_HOLD`, `pedal_apply`); THE PEDAL NOISES
fire ONCE PER CROSSING as voices of their own (`cc_fire`, `Voice::cc_voice`,
note 0xFF, never pedalled; the format retriggers on every message — recorded
difference); THE CHOKE runs before the new layers start (`choke_layers`, the
instrument's 64-tag dictionary, `Zone::silenced_by`, `silence_s`); a Suzu
voice under the pedal takes the rate through its contraction. Gates: the
preset suite's 16–20 on `tests/fixtures/dspresets/pedal/` (the hold bitwise,
4.6 / s and 25.1 / s within the formula, the catch flat for half a second, the
noises once, the filter, the choke in 5 ms and through the release, zero
allocation), the fuzz through the pedal (280k loads a minute, no crash), the
storm on the Dan Tranh library 0 XRuns, the web and lab gates green; BOTH
TABLETS under the spike (its storm now presses the pedal — `VoxoSpike.swift`,
`sumi_jni.cpp`): the sampler demo clean on both (the iPad 0.105 ms, the Tab's
release APK 0.801 ms of its 4 ms, 0 XRuns), Suzu patch 0 on the Tab 21 proxy
XRuns at 4.4 ms — the carried thread-placement item, fifteen voices held by
the pedal. The compat report retires no sentence (none covered these). FOUND: step 86's
matrix section was never bit for bit on the web (its slide-to-pitch route
through libm's exp2f; the Duffing joins the gate's TOLERANT list at −100.9 dB)
and its evidence file held the gate's tail only — AN EVIDENCE FILE IS THE
WHOLE OUTPUT, never a tail. **Step 88 shipped on 2026-10-25 (`DECISIONS_14
#2`; Voxo 0.28.0, additive):** THE CHORUS AND THE PHASER — read from the
guide's `<effects>` reference (the chorus: `mix`, `modDepth`, `modRate` and
nothing else; the phaser: + `centerFrequency`, `feedback`, no `stages`; group
effects are PER VOICE in the format, the phaser bus-only; `enabled` on any
effect). `bus.h/.cpp`: `voxo_bus::Chorus` (two lines at a 7 ms centre ± 4 ms ×
depth, the LFO a magic-circle cell in quadrature, `hermite4` reads, FIXED
buffers — an instance in every `Voice` and one on the bus, `voice_scratch` for
a chorused voice's sum) and `voxo_bus::Phaser` (six all-passes, ±2 octaves,
the coefficient every sixteen samples, feedback within ±0.95); both under
`#pragma clang fp contract(off)` — THE SAME BITS IN EVERY NATIVE BUILD;
`target_of(parameter, kind)` + `binding_kind` resolve `FX_MIX` / `FX_MOD_*` /
`FX_CENTER_FREQUENCY` / `FX_FEEDBACK` by the effect's position; `Target::Chorus*`
group-or-bus (`BUS_GROUP` −2), `Target::Phaser*` bus; `VOXO_CTL_CHORUS_MIX`
1006 / `_RATE` 1007 / `_DEPTH` 1008 (`VOXO_CTL_COUNT` 9; the desktop's,
iPad's and Tab's tables, `presets/SCHEMA.md`); `VOXO_NOTE_CHORUS` RETIRED (the
bit stays, its copy empty; `features.dspreset` carries a `bit_crusher` as the
unknown-effect witness). Gates 21–25 on `tests/fixtures/dspresets/fx/` (the
exact 336-sample delay bit for bit, the ±11 Hz swing and the quadrature, the
group chorus = the bus chorus on one note bit for bit, the 132 dB notch, the
sweep 20 dB over 50 ms windows, zero allocation); the fuzz seeded with the
effects. FOUND: THE DEBUG BUILD FUSES TOO (clang's fmuladd at -O0 lowers to
fmadd on arm64) — the sampler's mix of two voices is FMA-dependent; the
two-note chorus gate is a −120 dB tolerance (measured −143.6). **Step 89
shipped on 2026-10-25 (`DECISIONS_14 #3`; Voxo 0.29.0, no new entry point):**
THE FILTER FAMILY, THE FILTER ENVELOPE, THE BUS EQ, THE GAIN AND THE WAVE
SHAPER — the voice's SVF plays the format's lowpass (`lowpass_4pl`),
`lowpass_1pl`, highpass, bandpass and notch as its own outputs
(`Group::filter_mode`, one loop); THE `resonance` ATTRIBUTE IS THE Q (k =
1/Q, 0.1..18 — step 51 read it as an amount above 0.7: every lowpass preset
is 3 dB brighter at its cutoff; gate 11's margin 1.3); the first
`<modulators><envelope>` bound to `FX_FILTER_FREQUENCY` / `FX_FILTER_Q` is
the group's filter envelope, PER VOICE whatever its scope, stepped ONCE A
BLOCK into the cutoff's two coefficients (`Layer::fenv`, `fenv_advance`,
`fenv_apply`; modAmount, modBehavior set | add | multiply, `modulate` read as
multiply); the bus EQ `voxo_bus::Biquad` / `EqChain` (RBJ, the peak, the
shelves by Q, an instrument-level notch; eight stages in listed order,
`Params::eq[8]`, set every block at the device's rate; `Target::Eq*` with the
curve's group the stage index, `eq_stage_of`); `<effect type="gain">` (dB,
"-6dB", `levelUnit="linear"`; the bus's folded into `Params::gain_lin`, a
group's into its gain); `<effect type="wave_shaper">` (`shaper_tanh`:
Eigen's (13,6) rational, 3e-7; `shape` AT THE FORMAT'S SCALE — divided by
0.40 in, multiplied out; the bus's first, a group's per voice through the
scratch path before the chorus; `Target::Shaper*`); the bus order from here:
shaper, EQ, gain, chorus, phaser, reverb, delay. THE "OTHER FILTERS" NOTE IS
RETIRED (its copy empty; nine sentences), the modulators sentence names
LFOs and sequences. Gates 26–32 on twenty-seven `fx/` fixtures (every mode
within 0.001 dB of the analytic bilinear curve, every EQ stage within 0.002
dB of the cookbook, the gain and the shaper sample for sample, the
envelope's brightness, the Q's ring, zero allocation). FOUND: A LOOP WITHOUT
A CROSSFADE CLICKED ONCE A PASS since step 51 (its edge taps read the
sample's padding) — `read_frame_wrapped` / `Layer::looped` wrap them into the
loop (gate 27; the first version's unsigned `i0 − 1` crashed at the loop's
first frame — the index is signed); the filter gates measure 110 cycles
ending before the loop point. **Step 90 shipped on 2026-10-26
(`DECISIONS_14 #4`; Voxo 0.30.0, additive: `voxo_set_tempo`):** THE
MODULATOR MATRIX, LEGATO AND GLIDE — read from the guide's modulators and
groups pages (the `<lfo>` has NO offset / phase / sync: `shape` sine |
triangle | square | saw, `frequency` in Hz or a musical-time INDEX under
`frequencyFormat="musical_time"` — the list reconstructed from the page's
four anchors, `musical_whole_notes` — `delayTime`, `modAmount`, `scope`
global | voice, `modBehavior` set | add | multiply | modulate; the envelope
+ `delay`, `hold`). `instrument.h`'s `Modulator` / `ModTarget` (eight of
eight; the targets AmpVolume, AmpPan, GroupTuning, FilterCutoff /
Resonance, the group effects', the bus's and the EQ stages'), `voxo.cpp`'s
`lfo_raw` / `lfo_phase` / `menv_advance` / `mod_read` / `mod_apply` (ONCE A
BLOCK at the block's start and end, folded into the gain, pitch, pan,
cutoff and Q ramps; MODULATE = the curve less its neutral; the bus under a
LIVE COPY of its params every block; step 89's `fenv` folded into the
matrix), `Voice::start_s` / `menv[]` / `glide_*`, `voxo_t::bpm` /
`prev_note`; the shells pass the session's BPM (`sound_apply`,
`SumiCanvas.swift`, `sumi_jni.cpp`). THE GLIDE: a straight line in
semitones over `glideTime` from the previous note's CURRENT pitch
(`glideMode` legato | always | off; `GroupState::last_hz`); `polyphony`
releases the group's oldest voice; `trigger` first / legato,
`previousNotes`, `legatoInterval` in the dispatch's `matches` (Suzu does not
glide — recorded). THE MODULATORS NOTE RETIRED (eight sentences remain; an
unknown modulator element is named with the unknown bindings). Gates 33–41
on eighteen `mod/` fixtures; the fuzz five minutes. FOUND: a saw through a
swept filter read ±5 dB of zigzag through 50 ms windows (five and a half
saw periods: the neighbouring harmonics' leakage) — the sine is the witness
for a moving cutoff; and a scripted region cut anchored 1500 lines away
emptied `voxo.cpp` for a minute (restored from HEAD: the author commits each
step before the next — a near anchor and a size assert from here). **Step
91 shipped on 2026-10-26 (`DECISIONS_14 #5`; Voxo 0.31.0, additive; the
preset schema 3):** THE INSTRUMENT SLOT — `sumi_preset_t`'s `library_*`
(name, the FNV-1a 64 of the preset XML, the path as a hint; `presets/SCHEMA.md`
`library`), written by a shell whose sound holds a library (the desktop's
`app_sound_library_set` as a load lands, the iPad's `libraryIdentity`, the
Tab's `nativeVoxoLoad`), resolved on load by name then by hash
(`voxo_preset_file_hash` reads the XML without a sample; the desktop's
`app_resolve_library` over the hint, the current instrument's folder,
`~/Music/midi-sink`; the tablets' `resolveLibrary` over Instruments), the
SENTENCE when absent ("Library '…' is not on this computer: the preset
plays without it."); `voxo_preset_hash` / `voxo_preset_name` kept on the
instance at load (the swap is the callback's). THE OFFLINE BOUNCE —
`replay/src/sumi_replay_bounce.c` `sumi_replay_bounce` (the walker: a
frame's bytes pushed, its samples rendered in 128-frame calls, a float32
stereo WAV; no engine dependency) over a DEVICELESS Voxo configured as the
live one — the desktop's `app_sound_configure` (ONE body: `sound_apply` and
`ReplayHost::bounce` take it; `app_settings_suzu_params` factored out of
main.cpp), the iPad's `SoundController.bounce`, the Tab's
`nativeReplayBounce` from `g.vcfg`; the bench's `--replay-wav` shares the
walker; THE GATE AS READ: the live device's output is not byte-alignable to
the frame clock, so the bounce IS the deterministic rendering —
`--bounce-test` (two bounces through the host path byte for byte), the
replay suite's synthetic engine. THE OUTPUT DEVICE — `voxo_device_count` /
`_name` (miniaudio's enumeration), `voxo_set_device` / `_get_device` (a
running instance reopens its backend on the name without a stop; a name no
longer present opens the default), `voxo_backend_start(..., want_device)`,
the desktop's *Output device* row and `sound_device` INI key,
`--device-test`. Phase 15's steps 87–91 are done; the phase's fold and the
tag `v3.0.0-alpha.5` are the author's.
**Phase 16 is OPEN (2026-10-26; `_work/DECISIONS_15.md`, Part XV in flight; the
core and Voxo FROZEN — no engine change in step 92):** the lab, the books and
the 3.0 release. **Step 92's first session shipped on 2026-10-26 (`DECISIONS_15
#1`):** THE LESSONS in the book — `site/src/content/docs/lessons/` (the index
and six modules, the labs as static pages in `site/public/lessons/`, a
*Lessons* sidebar group, a home card), folded by script from
`site/drafts/lessons/` with EVERY item of `REVIEW_modules_02-06.md` applied
(the drafts stay: the author removes drafts; open for the author: the cells
chapter's 0.63 dB glide figure against the suite's 0.057 / 0.062, the spec
draft's §10.7.8 rosette formula against the shader's). KESSHO ON THE WEB —
`sumi_web_mineral_preset` / `_mineral_get` (the optical half through
`sumi_set_mineral`, the mechanical half as the desktop pairs it: the table
COPIED into `web/sumi_web.cpp`, a shared home being frozen; the preset
capture carries the mineral), the web ids 45–55 (`polarizer_mode` …
`tine_profile`, the struct's order), the host's Kessho rows and *Mineral*
row, the routing menus' three consumers, `?mineral=<id>` on any scene, the
scenes `arnold` / `braid` / `chord_wave` (the mapper's routes driven by MIDI
under the medium's defaults — no new gesture export), `check.mjs` at twenty.
THE LAB V2 — `sw_strobe` / `sw_strobe_poll` / `_ptr` / `_stride` / `_hz`
(flat records, 8 + 4 × 32 floats), `suzu-engine.js`'s `strobePoll`, the
worklet's `strobe` option and snapshot field, `Lab`'s `strobe` / `setStrobe`,
`lab-core.js`'s `peakScan` and five `PAGES`; `membrane.html/.js`,
`toda.*`, `gyro.*`, `billiard.*`, `dirac.*` (`?gate=` each; GATES in
`tools/suzu_lab_gate.mjs`; `Lab.astro`'s union and `check.mjs`'s
`LAB_PAGES` at eleven), the export list in `web/CMakeLists.txt` naming the
strike exports of steps 79–80 at last. THE BOOKS — `reference/compat.md`, the
chart's mode and CTL 20–23 rows (`chart_check.py` 52 ok on the existing logs;
fresh logs are the RC round's), the Minerals page's proof and selenite
exercise with a live Selenite drop, the three harmony embeds, the five `<Lab>`
embeds, `lab.mdx`'s rows, `guide/medium.md`'s third medium, `guide/web.mdx`'s
scenes and `mineral=`; `check.mjs` 96 pages, 20/20 scenes, 11/11 lab pages.
THE WEB FLATNESS READING (`tools/kessho_flat_web.mjs`, the desktop's
estimator on the marble's field dump; the rims by parity, fresh ink and the
sampler's clamp masked and counted): the ±3 median 3.4e-3 against the
roadmap's 1e-3 — THE TIER'S FLOOR, as measured (the desktop's a few
thousandths, `DECISIONS_10 #4`), for the author. FOUND: a brace behind a
comment loads nothing and reads as "no field dump" (syntax-check web
scripts after a scripted edit); the Toda ring's pitch is the spectral line
(the suite's measure), never a period; the billiard's quantised bounce is
worst high (4.7 % at A5); THE DIRAC NETWORK HAS NO RED OF ITS OWN and the
strings' naive solve stays in bound on it (`SUZU_DIRAC_KF`). The release —
`v3.0.0-rc.N` on both tracks, the wave, the stores, the fold of Parts X–XV
and the spec v6 — is the author's.
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
