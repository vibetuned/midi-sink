# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v4 — it absorbed
spec v2, the Phase-4 spec as §8, the Phase-5 spec as §9, and every Part-III
and Part-IV decision; the Phase-6 medium and the shipped quality-of-life
items are drafted for it in `specs/TO_PROJECT_SPEC.md` until the author
transcribes them). Decision log: `docs/DECISIONS.md` (Part I = v1, Part II =
v2, Part III = Phase 4, Part IV = Phase 5, Part V = Phase 6; references
written as `DECISIONS_2 #n` … `DECISIONS_5 #n` mean Parts II … V). History:
`docs/CHANGELOG.md`; the completed roadmap is `docs/ROADMAP.md` (Parts 1–5).
Work items are fed one at a time by the user.

**Phases 1–6 are complete** (steps 1–46 folded into `docs/`). Step 34 shipped
`v1.0.0`: the release spine built the desktop three and the web, the App
Store and Google Play listings are public (linked from the README and the
install page), and the author uploads the iOS and Android builds by hand
(`ios/RELEASING.md`, `android/RELEASING.md`). Publishing the GitHub release
draft is the human act that fires the channel workflows (cask and winget
PRs) and opens the apt repository. The documentation site deploys from
`main` through `pages.yml`, the only Pages deployer (docs from the tree,
`/marble/` rebuilt from the newest stable tag, `/apt/` from published
releases — DECISIONS_4 #82); the release workflow deploys nothing to Pages.
Phase 6 (the Medium, `libsumi` 1.1.0) closed on 2026-09-26; its pre-release
tag `v2.0.0-alpha.1` is the author's, its notes the `v2.0.0` section of the
changelog.

**Phase 7 (Sound) is open** (step 47 shipped Voxo's skeleton; step 48 the
mobile latency spike — miniaudio confirmed on both tablets, DECISIONS_6
#7–#9; step 49 the sample player with Hermite pitch and the glide check,
#10–#12; step 50 the Decent Sampler front end and the compat report,
#13–#15; step 51 the voice's interior — layers, round robins, release
samples, loops, the filter, the bindings, #16–#18; step 52 the bus reverb
and delay, the advisory memory gate, the demo instrument's slot, #19–#21;
step 53 iOS — the session, foreground only, the Files import, the Dan Tranh
demo, #22–#27; step 54 Android — audio focus, the SAF import, the demo in
the assets, the tuner, #28–#29) — the open
roadmap is `_work/ROADMAP_5.md` (Phases 7–9: sound, instruments, publish;
steps 47–66, with 55b the field stored as a displacement before the
instruments); its decisions accumulate in `_work/DECISIONS_6.md`
(referenced as `DECISIONS_6 #n`, merged as Part VI at the phase's end); the
specs are `specs/SOUND_SPEC.md`, `specs/INSTRUMENT_SPEC.md` and
`specs/QUALITY_OF_LIFE_SPEC.md` (the undone items). Voxo is the sibling
library `voxo/` (pure C `voxo/include/voxo.h`, the callback contract at its
top; C++20 in `voxo/src/` compiling `core/src/midi_normalizer.cpp` from
source; miniaudio underneath; built on every native platform, the device
started only by the desktop setting and the tablets' spike hooks until steps
53/54); its headless suite is `tests/voxo_tests.cpp` and the desktop proxy
for the ROLI is `midi-sink --dev --voxo-storm <s>`. Android now runs on
this Mac (the Tab is plugged in, Gradle/NDK installed); the Linux box keeps
only the Linux desktop. Phase 7 never touches `libsumi` (the engine stays
audio-free); the phase invariant is that
`tests/fixtures/field_512_metal.bin` stays bitwise on Metal (DECISIONS_5 #12,
a Metal invariant — #87; GL, D3D11 and GLES hold their tiers). Every
operator declares its class and passes the four-part conservation gate
(`midi-sink --dev --soak <op>`); the composite gate runs per backend
(`tools/composite_gate.py --backend`). Operator-page drafts for step 63
wait in `site/drafts/operators/`. The user owns the specs and roadmaps:
agents do not edit them; where a spec and a decision entry disagree, flag
it — the entry is the record of what shipped.
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
