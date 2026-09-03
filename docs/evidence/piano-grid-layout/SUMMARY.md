# Evidence — `SUMI_LAYOUT_PIANO_GRID` (DECISIONS_3 #29)

A third playable lattice, added between Steps 17 and 18: the chroma grid's
frame (C1–B7, insets 0.08/0.10, out-of-range clamps to the edge octave
keeping pitch class), each octave drawn as a classical two-row keyboard —
5 accidentals on top at white-key-unit positions {1, 2, 4, 5, 6} (the E–F and
B–C gaps stay empty), 7 naturals below. 14 rows, one echo, enum value 5.

## What changed

* `core/include/sumi_core.h` — `SUMI_LAYOUT_PIANO_GRID = 5` (additive; ABI
  stays 0.3.0).
* `core/src/layouts.cpp` — `layout_piano_grid` (position) +
  `probe_piano_grid` (inverse, with honest dead zones in the accidental row:
  the two gaps and the row ends, same rule as the Jankó stagger ends). The
  semitone axis deliberately takes the generic DECISIONS_2 #7
  shortest-neighbor path — pitch is not a function of x alone here, unlike
  Jankó, so the honest per-note axis is the half-key diagonal toward the
  adjacent accidental/natural.
* `core/src/voice_mapper.cpp` — joined the true-step lattice set
  (DECISIONS_3 #20): glides render the uncapped lattice step.
* `tests/normalizer_tests.cpp` — independent golden replica (loop now covers
  layouts 0–5), landmark spot checks (C4/C#4 positions, edge-octave clamp),
  probe round-trip over all 84 notes at two aspects, semitone-axis golden at
  C4 (half key over, one row up), dead-zone refusals + adjacent white-row
  acceptance.
* `tests/abi_c_compile.c` — pure-C enum value check.
* Hosts: iOS picker entry "Piano grid" + playable checks
  (`SumiApp.swift`, `SumiCanvas.swift`); Android picker entry
  (`MainActivity.kt` — no Play mode until Step 18); desktop `L` key cycles 6
  layouts. The iOS play overlay needed no change — its lattice is a probe
  sweep (DECISIONS_3 #9).
* Docs: `_work/PROJECT_SPEC_NEW.md` §3.4 bullet + enum copy;
  `_work/PHASE4_SPEC.md` §1 playable-set sentence; `_work/DECISIONS_3.md`
  entry #29.

## Verification

* `ctest --test-dir build`: **4/4 pass** (abi_c_compile, normalizer_tests,
  hostmpe_tests, hostmpe_c_compile) after adding the new goldens.
* iOS: `cmake --build build-ios` clean; `xcodegen` + `xcodebuild`
  (CODE_SIGNING_ALLOWED=NO) **BUILD SUCCEEDED**.
* `probe_sweep.txt` (generator: `piano_sweep.c`, pure C against the built
  dylib): the full-canvas probe sweep prints the classical 2+3 disposition
  with dead zones exactly at the row ends and the E–F / B–C gaps, and the
  landmark table shows uniform cell radius (0.0571 at aspect 1 — see below),
  uniform semitone step (0.0829), and the per-note diagonal axes (C→C#
  up-right, E and B down-right — toward the nearest semitone neighbor).

## Revision after first device test (same day)

First play test on the iPad: the knobs (lattice circles + R_max rings) read
half the chroma grid's size — the inscribed single-row radius divides by 14
rows where chroma divides by 7. **R_max now uses the key's playable
footprint: half of min(key width, octave-pair height)** (DECISIONS_3 #29,
R_max bullet). The octave-pair height equals the chroma grid's row height
(0.8/7) exactly, so knob size, deadband scale and CC74 travel match the
chroma grid's feel; bend exactness is R_max-independent (identity beyond the
circle, #10). New golden in `normalizer_tests.cpp` pins the formula.
`lattice_preview.png` (headless SVG render of the probe-sweep lattice at
iPad-landscape aspect) shows the circles nesting diagonally between rows —
no two adjacent-row cells share an x, so the overlap reads as a honeycomb.
