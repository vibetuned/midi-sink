# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v4 — it absorbed
spec v2, the Phase-4 spec as §8, the Phase-5 spec as §9, and every Part-III
and Part-IV decision; the Phase-6 medium, the shipped quality-of-life
items and the Phase-7 sound are drafted for it in
`specs/TO_PROJECT_SPEC.md` until the author transcribes them). Decision log: `docs/DECISIONS.md` (Part I = v1, Part II =
v2, Part III = Phase 4, Part IV = Phase 5, Part V = Phase 6, Part VI =
Phase 7, Part VII = Phase 8; references written as `DECISIONS_2 #n` …
`DECISIONS_7 #n` mean Parts II … VII). History:
`docs/CHANGELOG.md`; the completed roadmap is `docs/ROADMAP.md` (Parts 1–5).
Work items are fed one at a time by the user.

**Phases 1–7 are complete** (steps 1–55 folded into `docs/`). Step 34 shipped
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
its notes are the `v2.0.0` section of the changelog.

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
**Suzu** (Phase 8, `specs/SYNTH_SPEC.md`, `SYNTH §n`) is the synth inside
Voxo — a source beside the sampler (`voxo_set_source`), the symplectic
phase-space cells of `voxo/src/suzu.h` with their class table (step 56,
Voxo 0.8.0; the modal voice, the bow, the strings and the orbit trace are
steps 57–59); its suite is `tests/voxo_suzu_tests.cpp` and the storm runs
on it with `--voxo-source suzu`. **Every synth voice ships with its sound
profile** (the author's rule, DECISIONS_7 #9): `midi-sink --dev
--voxo-profile <dir>` plus `tools/sound_profile.py` — the figure goes in the
step's evidence and is drafted for the docs (`site/drafts/suzu/`).

**Phase 8 (Suzu, the synth) is open** (step 55b shipped the field stored as a
displacement — libsumi 1.2.0, `DECISIONS_7 #1–#5`, the thin Anod strike the
strike again; the GL and D3D11 tiers held on the boxes and their Anod hash
columns are recaptured, #6–#7; the pre-release tag `v2.0.0-alpha.2` sits at
the 55b fold; the D3D11 bench's
variable dip frame count is the author's open item, #7) — the open roadmap
is `_work/ROADMAP_5.md`
(Phases 8–10: Suzu, instruments, publish; steps 56–70); the phase's
decisions accumulate in `_work/DECISIONS_7.md` from #8 (step 55b's #1–#7 are
already Part VII of `docs/DECISIONS.md`, merged at its fold; the file
continues that part and merges into it at the phase's end; all referenced as
`DECISIONS_7 #n`); the specs are `specs/SYNTH_SPEC.md`,
`specs/INSTRUMENT_SPEC.md` and `specs/QUALITY_OF_LIFE_SPEC.md` (the undone
items); the sound spec left the tree at the Phase-7 close — its content as
shipped is `specs/TO_PROJECT_SPEC.md` §12, and `SOUND §n` in Part VI means
`git show 7ceb111:specs/SOUND_SPEC.md`. Phase 8 reopens the core
for feature work; the phase invariant is that
`tests/fixtures/field_512_metal.bin` stays bitwise on Metal (DECISIONS_5 #12,
a Metal invariant — #87; GL, D3D11 and GLES hold their tiers) — re-captured
ONCE at step 55b, when the field's payload became a displacement (u − x,
v − y, ink, aux; libsumi 1.2.0, DECISIONS_7 #1); the invariant restarts from
that fixture and no later step touches it. Every
operator declares its class and passes the four-part conservation gate
(`midi-sink --dev --soak <op>`); the composite gate runs per backend
(`tools/composite_gate.py --backend`). Operator-page drafts for step 63 wait
in `site/drafts/operators/`, Voxo's licensing page in `site/drafts/voxo/`.
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
