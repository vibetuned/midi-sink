# Suminagashi MPE Visualizer Engine — agent working rules (spec v2)

The full specification is in PROJECT_SPEC_2.md (v2 — supersedes
PROJECT_SPEC.md). Roadmap steps are fed one at a time by the user.

Working rules (apply to every step):

1. Read the spec sections referenced by the current step before writing code.
   Where the spec and DECISIONS.md conflict, the spec wins — it has absorbed
   the validated v1 decisions; flag any remaining conflict instead of silently
   picking one.
2. Keep every sokol call behind `core/src/renderer.cpp` /
   `core/src/swapchain_*.{mm,cpp}`; nothing above those files may include
   sokol headers.
3. No exceptions, no STL types, no callbacks-into-C++ across `sumi_core.h`.
4. Backend purity rule (spec §4.6): the deformation chain never contains
   backend-specific branches; orientation flips live only in the final
   swapchain composite and the print readback path.
5. Prefer the choice that keeps the core identical across all five platforms;
   log every newly resolved ambiguity in DECISIONS_2.md (v1 history stays in
   DECISIONS.md).
6. The user makes all git commits themselves — agents never commit. Prepare
   each step's DONE evidence under `docs/evidence/` and report when the tree
   is ready, with an evidence reference for the commit message.
7. Do not implement anything from a later step early, even if convenient.

Build: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`
(Homebrew cmake/ninja live in /opt/homebrew/bin.)
