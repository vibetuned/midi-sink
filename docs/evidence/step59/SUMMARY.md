# Step 59 — The orbit trace & the phase close — the Mac's evidence

Decisions `DECISIONS_7 #24–#27` (`_work/DECISIONS_7.md`, continuing Part
VII): Voxo's trace (#24), the shell's bridge and the scope (#25), the demo
and the phase's close (#26), the scope view in the live composite (#27,
the author's ask). Voxo is 0.13.0 (additive: `voxo_set_trace`,
`voxo_trace_poll`, `voxo_trace_t`); libsumi is 1.3.0 (additive:
`sumi_set_scope`, `SUMI_SCOPE_*` — the phase's one core touch, a composite
pass on the live path only).

Machine: the author's Mac (Apple Silicon); the Voxo gates headless through
`voxo_render`, the bench's through the desktop harness at 512 × 512 with the
scripted clock. Spec: SYNTH §2.7, §5; roadmap step 59.

## What changed in the tree

- `voxo/src/voxo.cpp`, `voxo/include/voxo.h`: the per-voice orbit ring
  (one point in eight sub-steps, 1024 points, lock-free), the capture per
  voice kind (the cell's and the lattice's (x, y), the Duffing's and the
  rotor's, the chains' (s, ṡ/ω)), the mask, the poll with the
  curvature-weighted decimation; 0.13.0.
- `desktop/src/orbit_trace.{h,cpp}` (new): the bridge — the layout probe's
  note → cell table, the placement, the tine/wake emission through the
  gesture ABI, the 24-a-frame budget with the in-voice merge, the scope's
  polylines and the stats.
- `desktop/src/main.cpp`: the bridge before each frame's update; the mask
  from the settings. `desktop/src/settings_ui.{h,cpp}`: the "Suzu trace"
  rows (ink, scope, this voice kind, scale, segments, stroke) and the scope
  miniature. `desktop/src/app_settings.{h,cpp}`: the six settings, the INI,
  the preset mapping. `desktop/src/dev_tools.{h,cpp}`: `--trace-test`,
  `--trace-demo <dir>`.
- `presets/`: the `suzu` object's `trace_*` fields (`SCHEMA.md`);
  `tests/preset_tests.c` the round trip; `tests/voxo_suzu_tests.cpp` gate 24
  (five checks); `tests/voxo_c_compile.c` 0.13.0 and the ABI from C.
- `core/src/shaders/composite.glsl`, `core/src/renderer.{h,cpp}`,
  `core/src/engine.cpp`, `core/include/sumi_core.h`: the scope view (#27) —
  the segments as uniforms in the plate guide's pattern, drawn at the end of
  the live composite only; `sumi_set_scope`; 1.3.0; `tests/abi_c_compile.c`.
- `docs/CHANGELOG.md`, `CLAUDE.md`, `_work/DECISIONS_7.md` #24–#27,
  `site/drafts/suzu/orbit-trace.md` with the still.

## The gates

Voxo (`voxo_suzu_tests.txt`, gate 24):

| gate (SYNTH §2.7, §5) | bound | measured |
|---|---|---|
| the single cell's trace: A3 held 200 ms, 8 segments asked | the polyline on the unit circle within 5 %, ≤ 9 points | 9 points, within 0.4 %; the amplitude 0.175 (the level's) |
| the mask: off; a kind not in it | nothing polled | 0 and 0 |
| the toggle at the source: the rendering with the trace on against without, kinds 0, 5, 3, 7 | bit-identical | 14 400 samples equal, each kind |
| ten rotor voices polled at 4 segments | all answer, none over 5 points, finite | 10, 0 over, 0 empty, finite |
| the hybrid string's phase plane | a polyline with the note's amplitude | 9 points, amplitude 0.179 |

The bench (`trace_test.txt`, `--trace-test`: ten rotor notes struck at their
cells through the gesture ABI and held under the press for 240 frames, K
swept by the wheel on Voxo's side; three runs of the script — no trace
object, the trace OFF, the trace ON):

| gate (roadmap DONE) | bound | measured |
|---|---|---|
| the script's own determinism (two untraced runs) | — | Sumi: 0 of 2 097 152 bytes differ; Anod: 1 116 577 differ (a core finding, #25) — the gates run in Sumi |
| trace OFF is bit-identical to no-trace | the field bytes equal | 2 097 152 bytes equal |
| the ink route marbles | the traced field differs | 1 972 336 bytes differ |
| ten voices with traces on hold the budget | ≤ 24 segments a frame, none starved | peak 20 of 24 (ten × ⌊6 × 0.4⌋), 4 448 inked, 9 600 merged, 0 unplaced; the mapper's 64 untouched by construction (the queue holds 4 096) |
| the bridge's own cost | under a millisecond a frame | 0.04–0.17 ms a frame |
| the scope view instead of the water (ten rotor voices, the ink off) | the field and the dip's print bit-identical to the run without it | 2 097 152 and 1 048 576 bytes equal |
| the composite gate on Metal (`tools/composite_gate.py`) | bitwise with the scope off (the shipped composite) | 0 of 1 048 576 channel samples differ; the negative control red |
| the field gate on Metal (`tools/field_gate.py`) | the phase invariant | holds (the log in this step's run) |
| the earlier steps' gates | unchanged | all green (`ctest` 10 of 10) |

## The demo — `rotor_anod.mp4`, `rotor_anod_frame.png`

`--trace-demo <dir>`: Anod on the chroma grid, four rotor voices struck in
turn (E3, B3, E4, B4) and held under the press while the wheel sweeps K
from 0 to 2.5 over twelve seconds, then released; the orbits inked as tines
at their cells, eight segments a voice, scale 0.35. 1 200 frames at 60 fps
in 94 s of wall time (the export at 720 × 720 lands every other frame), 591
frames exported, encoded at 30 fps (540 × 540, 20 s): the rotor scribbling
its chaos into Anod — a circle's comb at K 0, the scribble past K_c. The
trace inked 5 910 segments (peak 24 a frame, 2 624 merged).

## The tablets

Voxo 0.13.0 compiles into both tablet shells unchanged (iOS `cmake --build
build-ios`, Android Gradle — the run is in this step's log); neither
exposes Suzu or the trace.

## For the author

- **Taste sign-off (the step's inputs):** the trace scale (0.25 canvas
  heights per unit amplitude: a full-velocity cell traces a 0.06-height
  orbit) and the rotor's trace ON by default; the stroke (tine by default;
  wake is the sub-stepped alternative); all in the Sound section under
  "Suzu trace". The scope is the Sound section's miniature and, since #27,
  the canvas itself: "Suzu trace on the canvas" — off (the default), over
  the water, or the scope alone with the water hidden (your ask); the
  amber and the line width are the composite's to sign by eye.
- **Core finding (frozen this phase, for Phase 10):** Anod's strikes through
  the gesture ABI are not bit-reproducible run to run (1.1 million of
  2 097 152 field bytes differ between two identical untraced runs with a
  600-frame quiet start and a dip between); Sumi's are. The toggle's gate
  runs in Sumi.
- **Phase end (the author's):** the tag `v2.0.0-alpha.3`; the synth played on
  the desktops, the tablets regression-checked; the fold of
  `_work/DECISIONS_7.md` (#8–#26) into `docs/DECISIONS.md` Part VII, the
  evidence folders condensed into the changelog and removed, the drafts
  kept for step 63.
