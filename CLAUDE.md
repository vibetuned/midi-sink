# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is `docs/PROJECT_SPEC.md` (spec v3 — it absorbed
spec v2, the Phase-4 spec as §8, and every Part-III decision). Decision log:
`docs/DECISIONS.md` (Part I = v1, Part II = v2, Part III = Phase 4;
references written as `DECISIONS_2 #n` / `DECISIONS_3 #n` mean Parts II /
III). History: `docs/CHANGELOG.md`; the completed roadmap is
`docs/ROADMAP.md` (Parts 1–3). Work items are fed one at a time by the user.

**Active phase: 5 — Packaging, Release, Web & Documentation — lives in
`_work/`**: `PHASE5_SPEC.md`, `ROADMAP_4.md` (steps 23–33, ONE PLATFORM PER
STEP) and `DECISIONS_4.md` (new entries go here; merges into
`docs/DECISIONS.md` as Part IV at phase end). Phase 4 is fully folded into
`docs/`. The user owns the specs and roadmaps: agents do not edit them; where
a spec and a decision entry disagree, flag it — the entry is the record of
what shipped. Phase-5 rules: the core stays frozen except Step 30's WebGPU
seam; version strings come from the git tag via CI injection, never
hand-edited; store submissions and beta promotions are human actions.

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
   `docs/DECISIONS.md` (the current last part).
6. The user makes all git commits themselves — agents never commit. Prepare
   DONE evidence under `docs/evidence/<task>/` (transient: evidence folders
   are removed from the tree once a milestone ships — git history keeps them
   — and their SUMMARY.md is condensed into `docs/CHANGELOG.md`), and report
   when the tree is ready, with an evidence reference for the commit message.
7. Do not implement anything from a later planned task early, even if
   convenient.

License: AGPL-3.0 (`LICENSE`).

Build: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`
(Homebrew cmake/ninja live in /opt/homebrew/bin.)
iOS: `cmake -B build-ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS
-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_OSX_ARCHITECTURES=arm64
-DBUILD_TESTING=OFF && cmake --build build-ios`, then `cd ios && xcodegen`
and build the generated project. Android: Gradle in `android/` drives the
repo-root CMake via externalNativeBuild.
