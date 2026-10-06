# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v4 — it absorbed
spec v2, the Phase-4 spec as §8, the Phase-5 spec as §9, and every Part-III
and Part-IV decision; the Phase-6 medium, the shipped quality-of-life
items, the Phase-7 sound and the Phase-8 synth are drafted for it in
`specs/TO_PROJECT_SPEC.md` until the author transcribes them). Decision log: `docs/DECISIONS.md` (Part I = v1, Part II =
v2, Part III = Phase 4, Part IV = Phase 5, Part V = Phase 6, Part VI =
Phase 7, Part VII = Phase 8, Part VIII = Phase 9; references written as
`DECISIONS_2 #n` … `DECISIONS_8 #n` mean Parts II … VIII; Phase 10's
`DECISIONS_9 #n` will be `_work/DECISIONS_9.md`, opened at step 67).
History: `docs/CHANGELOG.md`; the completed roadmap is `docs/ROADMAP.md`
(Parts 1–8).
Work items are fed one at a time by the user.

**Phases 1–9 are complete** (steps 1–66 folded into `docs/`). Step 34 shipped
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
fold; its notes are the same section.

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
**Suzu** (Phase 8; its spec as shipped is `specs/TO_PROJECT_SPEC.md` §13 —
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
`--voxo-source suzu`; the tablets stay on the sampler (Suzu's knobs are
the desktop's Sound section's). **Every synth voice ships with its sound
profile** (the author's rule, DECISIONS_7 #9): `midi-sink --dev
--voxo-profile <dir>` plus `tools/sound_profile.py` — the figure goes in the
step's evidence and is drafted for the docs (`site/drafts/suzu/`).

**Phase 10 (Publish, steps 67–70) is open; step 67 (the documentation)
shipped on 2026-10-06** — the open roadmap is `_work/ROADMAP_5.md` (Phase
10 alone); `_work/DECISIONS_9.md` is Part IX in flight (`DECISIONS_9 #n`),
merged as Part IX at the phase's end. Step 67 (`DECISIONS_9 #1–#5`): the
operator book's five electric pages, Anod and the palettes from the drafts
(the drafts stay in `site/drafts/` until the author removes them); the Suzu
book (`site/src/content/docs/suzu/`, nine chapters, the figures in
`site/src/assets/suzu/`) with the lab embedded through
`site/src/components/Lab.astro` — THE LAB'S ADDRESS IS `/marble/suzu/`,
composed from the tag's `suzu-dist` by `pages.yml` and
`site/scripts/compose-marble.mjs` (`DECISIONS_9 #2`; `lab-core.js`
`DOCS_ROOT='../../'`); the guide's instruments, medium, sound, presets and
replay pages; the settings reference rewritten; the chart's new sections
against `tests/fixtures/bytelogs/{trumpet,trombone,theremin}_byte_log.csv`
(`#3`); `check.mjs` requires all seventeen scenes and the six lab pages;
the gallery's three new cards are PENDING with `replay` links — the
recordings and their captions are the author's (`#4`); every `[ITERATE]`
of the four specs resolved or documented as a limit (`#1`). The live site's
timing (`#2`): the author cuts the first v2 RC with this step so the
testers can proofread the docs live — `pages.yml` embeds `/marble/rc/`
(and names the RC in the footer) while an RC is newer than the stable;
the gallery's nine 2.0 cards are "coming soon" until the author's captures
land. Next: step 68 (the store beta wave).
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
(`--es recordLab 20`), the Linux box (out of commission since 2026-10-05),
and the transcription of `specs/INSTRUMENT_SPEC.md` and the shipped QOL
items into `specs/TO_PROJECT_SPEC.md` — the author's word, as for the
synth spec.
Phase 8's record is Part VII of `docs/DECISIONS.md` (`DECISIONS_7 #1–#35`; what
carries is in #35 — the sax's quasi-periodic A3 #33, the bore grid's
damping #30, the D3D11 bench's variable dip frame count #7, Anod's
run-to-run reproducibility through the gesture ABI #25, the entries'
`[ITERATE]`s); the specs are `specs/INSTRUMENT_SPEC.md` and
`specs/QUALITY_OF_LIFE_SPEC.md` (the undone items); the sound spec left the
tree at the Phase-7 close and the synth spec the day after the Phase-8 fold
(2026-10-05, the author's word) — their content as shipped is
`specs/TO_PROJECT_SPEC.md` §12 and §13, `SOUND §n` in Part VI means `git show
7ceb111:specs/SOUND_SPEC.md` and `SYNTH §n` in Part VII `git show
5111748:specs/SYNTH_SPEC.md`.
Phase 9 reopened the core for feature work (the layouts' probe, INSTRUMENT
§1 — in the ABI since step 41) and it is frozen again; the phase invariant is that
`tests/fixtures/field_512_metal.bin` stays bitwise on Metal (DECISIONS_5 #12,
a Metal invariant — #87; GL, D3D11 and GLES hold their tiers) — re-captured
ONCE at step 55b, when the field's payload became a displacement (u − x,
v − y, ink, aux; libsumi 1.2.0, DECISIONS_7 #1); the invariant restarts from
that fixture and no later step touches it. Every
operator declares its class and passes the four-part conservation gate
(`midi-sink --dev --soak <op>`); the composite gate runs per backend
(`tools/composite_gate.py --backend`). Operator-page drafts for the docs step
(67 on the open roadmap; the drafts say 63) wait in `site/drafts/operators/`,
Voxo's licensing page in `site/drafts/voxo/`, Suzu's in `site/drafts/suzu/`.
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
-DBUILD_TESTING=OFF && cmake --build build-ios`, then `cd ios && xcodegen`
and build the generated project (or `ios/prepare_release.sh`, which does
both with the version from the tag). Android: Gradle in `android/` drives
the repo-root CMake via externalNativeBuild. Web: `emcmake cmake -B build-web
-G Ninja && cmake --build build-web` (Homebrew emscripten needs
`EMSDK_PYTHON=/opt/homebrew/bin/python3.14`), then `tools/web_gate.mjs`.
