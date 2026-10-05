# Step 60 — Stateful layout cores: the trumpet and the trombone — the Mac's evidence

Phase 9 opens (`_work/DECISIONS_8.md`, `DECISIONS_8 #1–#4`). libsumi 1.4.0
(additive). Spec: INSTRUMENT §1, §2, §3, §5; roadmap step 60. Machine: the
author's Mac (Apple Silicon, Metal); the desktop `build/` (Debug) for the
suites and the gates.

## What changed in the tree

- `core/include/sumi_core.h`: layouts 8 and 9 unreserved (10–12 still
  clamp); `sumi_params_t.trumpet_arc`; `SUMI_CC_VALVE_1/2/3`, `SUMI_CC_SLIDE`;
  `sumi_get_layout_state`; `sumi_version` 1.4.0.
- `core/src/layouts.cpp` / `.h`: the brass layouts — the partials, the
  valve offsets, the slide, the column and the arc, the hit test, the
  placement rule, the probe's answer, the display cells; the internal
  `sumi_layout_position` / `sumi_layout_semitone_delta` take the state.
- `core/src/midi_normalizer.cpp` / `.h`: the fingering CCs decoded into the
  layout state (the master channel in MPE mode, any channel otherwise), the
  slide's 10 ms one-pole per drain, `sumi_normalizer_layout_state`.
- `core/src/voice_mapper.cpp` / `.h`: `sumi_voice_mapper_set_layout_state`;
  the placement and the pitch axis read the state.
- `core/src/engine.cpp`: the state taken from the drain before the mapper
  places the frame's notes; the getter; the cells' cache keyed by the
  arrangement; the spark gesture probing with the state.
- `desktop/src/`: the two layouts in the picker and the INI (`layout % 10`,
  `trumpet_arc`), the arc checkbox, the orbit trace's placement table keyed
  by the engine's state on the brass layouts, the bench's `--layout-shot`
  and `--trumpet-arc`.
- `tests/normalizer_tests.cpp` (`test_brass_layouts_and_fingering`, the 28
  position calls moved), `tests/abi_c_compile.c` (1.4.0).
- `_work/DECISIONS_8.md` (new); `docs/CHANGELOG.md`; `CLAUDE.md`.

## The goldens (`normalizer_tests.txt`, `ctest.txt`)

| check | result |
|---|---|
| the headless suite | OK, 24 483 checks (every valve combination × every partial at aspects 1.0 and 16:9, column and arc; the slide at its seven positions and the six midpoints × seven partials; the standard fingerings and the gaps; the refusals; the normalizer's decode; the replay) |
| the replay | two normalizers fed one byte stream bitwise equal at every drain; the shell's mirror agrees at every cell's probe; the mapper lands the ten fingered notes and the seven slide positions in their partial cells |
| `abi_c_compile` | OK: C11 compile + link, 28 ABI symbols, version 1.4.0; the trumpet answers, 10–12 refused |
| ctest | 10 of 10 |

## The gates (`gates/`)

| gate | result |
|---|---|
| the field gate, Metal (`field_gate_metal.txt`) | GREEN: max 0.0, mean 0.0 against `tests/fixtures/field_512_metal.bin` — bitwise; the negative control red |
| the composite gate, Metal (`composite_gate_metal.txt`) | GREEN: 0 of 1 048 576 channel samples differ (the Metal tier is 0); the negative control red |
| the marble's web gate on the rebuilt `build-web` (`web_gate.txt`) | PASS: the WebGPU dump against the Metal fixture max 9.8e-4, mean 3.9e-9 (59c's numbers — the web tree's field unchanged) |

The phase invariant holds: no field pass changed, the fixture stands as
step 55b captured it.

## The desktop draws both layouts (the shots)

`midi-sink --dev --layout <n> [--trumpet-arc] --layout-shot <png>`: a
1024 × 640 sheet, the plate guide on (the display cells, cyan), a scripted
fingering phrase through the normalizer, a dip, the print.

- `trumpet_column.png`: the open series up the eight cells (B♭1 to B♭4),
  then the chromatic run A3 → E3 fingered on the 4th partial — seven
  strikes stacked as concentric rings on the B♭3 cell, where the valves put
  them.
- `trumpet_arc.png`: the same phrase on the arc.
- `trombone.png`: the seven partials at the 1st position, then B♭3 held
  through a slide out to the 7th with the bend following — the drop nudged
  left under the mapper's cap (the canvas stays ink).

## The other builds

| build | result |
|---|---|
| iOS `cmake --build build-ios` | builds (libsumi 1.4.0) |
| Android `:app:assembleDebug` | BUILD SUCCESSFUL |
| web `cmake --build build-web` | builds; the marble's gate PASS (max 9.8e-4, mean 3.9e-9) |

The tablets' shells reach no new code path (their pickers list layouts
0–7; their shims probe stateless). Both were installed and launched once
the author plugged them in (the tablets are tested on every core change):
the Tab (`adb install -r`, `am start`: the shell logs "sumi 1.4.0 ready",
Play mode on the piano grid, the session config sent, the Dan Tranh loaded,
the process alive) and the iPad (`build-ios` → xcodegen → xcodebuild into
`ios/build-dd` → devicectl install and launch, both exit 0 — the app with
the rebuilt `libsumi.a`). The regression look is the author's.

## For the author

- **The fingering numbers** (`DECISIONS_8 #1`): CC 110/111/112 and 113 on
  the master channel, as Part V fixed them — confirm or override in #1.
- **By eye at step 63:** the column or the arc; the lip bend's ±1 semitone
  per cell radius (#2).
- **The partials are sounding pitches of a B♭ instrument** (#4's flag 3).
- **The devices:** both installed and launched; a look at the piano grid on
  each is the regression check (nothing in their shells changed).
