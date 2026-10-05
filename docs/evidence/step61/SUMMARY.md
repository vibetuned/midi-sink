# Step 61 — Stateless layouts: Wicki–Hayden, strings, the theremin — the Mac's evidence

`DECISIONS_8 #5–#8`. libsumi 1.5.0 (additive). Spec: INSTRUMENT §4, §5;
roadmap step 61. Machine: the author's Mac (Apple Silicon, Metal); the
desktop `build/` (Debug) for the suites and the gates.

## What changed in the tree

- `core/include/sumi_core.h`: layouts 10–12 unreserved; `SUMI_LAYOUT_STRINGS`
  (11; `SUMI_LAYOUT_FRETS` kept as an alias); `SUMI_STRINGS_*`;
  `sumi_params_t.string_tuning`; `sumi_version` 1.5.0.
- `core/src/layouts.cpp`: the Wicki–Hayden field, the string grids and
  their tuning tables, the theremin; the probe cases, the placements, the
  semitone axes (the Wicki gradient, the fret, the slot), the display cells.
- `core/src/voice_mapper.cpp`: the three join the lattices whose glide
  renders at the true step.
- `core/src/engine.cpp`: the unknown-layout warning; `string_tuning`
  clamped; the cells cache keyed by the tuning; 1.5.0.
- `desktop/src/`: the three names, the picker's thirteen entries, the INI's
  `layout % 13` and `string_tuning`, the tuning sub-picker, the bench's
  `--string-tuning` and the shot's scale and glide phrases.
- `tests/normalizer_tests.cpp` (`test_stateless_layouts`),
  `tests/abi_c_compile.c` (1.5.0).
- `_work/DECISIONS_8.md` #5–#8; `docs/CHANGELOG.md`; `CLAUDE.md`.

## The goldens (`normalizer_tests.txt`, `ctest.txt`)

| check | result |
|---|---|
| the headless suite | OK, 41 600 checks (from 24 483): the 90 buttons at two aspects, the bijection over C1..B7, the intervals, the dead half-buttons, the parity clamp; every (string, fret) under the three presets, every note's echoes, the edge clamps, the cell counts; the theremin across the field at three heights, the flag, the clamps; the mapper's true step on the three |
| `abi_c_compile` | OK: C11 compile + link, 28 ABI symbols, version 1.5.0; Wicki–Hayden answers, the theremin's cell is CONTINUOUS, the strings constants |
| ctest | 10 of 10 |

## The gates (`gates/`)

| gate | result |
|---|---|
| the field gate, Metal (`field_gate_metal.txt`) | GREEN: max 0.0, mean 0.0 — bitwise against `tests/fixtures/field_512_metal.bin`; the negative control red |
| the composite gate, Metal (`composite_gate_metal.txt`) | GREEN: 0 of 1 048 576 channel samples differ; the negative control red |
| the marble's web gate on the rebuilt `build-web` (`web_gate.txt`) | PASS: the WebGPU dump against the Metal fixture max 9.8e-4, mean 3.9e-9 (59c's numbers) |

No field pass changed; the fixture stands as step 55b captured it.

## The desktop draws the layouts (the shots)

`midi-sink --dev --layout <n> [--string-tuning <t>] --layout-shot <png>`: a
1024 × 640 sheet, the plate guide on, a scripted phrase, a dip, the print.

- `wicki.png`: the 90-button hex field (the accidentals' circles in the
  guide's second colour), a C major scale from C3 to C5 climbing its
  buttons — one drop per note, every note one button.
- `strings_guitar.png`, `strings_whole_tone.png`, `strings_fourths.png`: the
  6 × 25, 12 × 25 and 6 × 25 grids, the same scale as echoes along the
  strings (a note on its three lowest-fret strings at once).
- `theremin.png`: the 61 slots along the middle, C4 struck and glided up an
  octave — the drop travelling a sixth of the width under the bend.

## The other builds and the devices

| build | result |
|---|---|
| iOS `cmake --build build-ios`, xcodegen, xcodebuild into `ios/build-dd` | builds (libsumi 1.5.0) |
| Android `:app:assembleDebug` | BUILD SUCCESSFUL |
| web `cmake --build build-web` | builds; the marble's gate PASS |

The tablets' shells reach no new code path (their pickers list layouts
0–7; their shims probe stateless); both were installed and launched with
the rebuilt core — the Tab through adb, the iPad through devicectl — as the
core rule asks; the regression look is the author's.

## For the author

- **Wicki–Hayden's width** (`DECISIONS_8 #5`): six buttons a row — every
  note on one button — over the twelve-button rows some controllers use;
  its rows' ends reach G0 and F8.
- **STRINGS' echoes** (#6): the three lowest-fret sites, first position
  first; a note played high on a low string draws at its first-position
  site. A wider echo cap is a core change if wanted.
- **The theremin** (#7): five octaves C2–C7; Y's sign convention is the
  surface's (step 62) to settle by hand.
- **By eye at 63:** the three layouts' look on the tablets; the strings'
  cell size (half a fret).
