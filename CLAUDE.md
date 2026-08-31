# Suminagashi MPE Visualizer Engine — agent working rules

The full specification is in PROJECT_SPEC.md. Roadmap steps are fed one at a time by the user.

Working rules (apply to every step):

1. Read the spec sections referenced by the current step before writing code.
2. Keep every sokol call behind `core/src/renderer.cpp` / `core/src/swapchain_*.{mm,cpp}`; nothing above those files may include sokol headers.
3. No exceptions, no STL types, no callbacks-into-C++ across `sumi_core.h`. `sumi_create` failure path = NULL + log callback.
4. When a spec ambiguity is found, prefer the choice that keeps the core identical for iOS/Android — that is the project's reason for existing. Note the decision in DECISIONS.md at the repo root.
5. The user makes all git commits themselves — agents never commit. Prepare each step's DONE evidence (screenshots / test output) under `docs/evidence/` and report when the tree is ready.
6. Do not implement anything from a later step early, even if convenient.

Build: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`
(Homebrew cmake/ninja live in /opt/homebrew/bin.)
