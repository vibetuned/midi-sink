# Step 5 DONE evidence — full MPE mode (ROLI Piano, Osmose-ready)

Date: 2026-08-31/09-01. Same rig. All GUI runs `METAL_DEVICE_WRAPPER_TYPE=1`,
zero validation output, MPE mode auto-engaged via MCM in every log.

New: MCM (RPN 6) zone configuration (lower zone, v1 single-zone; upper
log-ignored), ±48 member bend default with RPN 0 override, per-member-channel
voice table with note steal, per-voice press/glide/slide with exponential
smoothing (smoothing_ms) and per-update coalescing, §4.4 incremental press
feeds (area-correct boundary growth, DECISIONS.md #28), per-voice glide tines
along the pitch axis, lift surfactant rings, slide→aux modulation, and the
per-frame deformation budget with overflow merging (DECISIONS.md #29).

| DONE check | Result | Evidence |
|---|---|---|
| Two simultaneous notes bent oppositely each drag their own drop | **PASS** — ch2/C4 (black) and ch3/D4 (white) bent ±10 semitones: each drop dragged along its own pitch axis with its own narrow wake; no global shear | `opposite_bends.png`, `run_bends.log` |
| Rising pressure grows one drop without disturbing neighbors | **PASS** — held C4 with 0→127 pressure ramp grows from r≈0.09 to canvas-scale, perfectly circular and sharp; the untouched F#4 neighbor is displaced outward (physically correct) but pristine | `press_early.png`, `press_growth.png`, `run_press.log` |
| Osmose stress: 10 voices × 200 press/s, 60 fps, 0 dropped, budget graceful | **PASS** — tests/osmose_stress.swift: 56 051 messages / 30.3 s (pressure + staggered bends + CC74 on ch 2–11 after MCM): **99.6 fps avg**, `dropped MIDI messages: 0`, coalescing held emissions ≈25/frame — under the 64 budget, zero merge events needed (graceful by construction); merging itself is proven by unit test (budget=3, 6 pressing voices → 3 emit + 3 merge, nothing lost) | `run_osmose_stress.log`, `osmose_stress.png`, tests |

Headless tests grew to 232 checks (ctest green): MCM zone config + upper-zone
ignore, ±48/RPN-0/master bend ranges, voice steal ordering + stolen-note-off
ignore, per-dimension coalescing (last value wins), §4.4 feed continuity
(same ink band, counter not advanced), glide tine narrowness + center
tracking, budget merging, lift ring (clear, lift-scaled), slide→aux.

**Bug found by the visual check:** spec-literal §4.4 radius steps produce
near-invisible growth (area conservation: a center expansion r moves boundary
R by only ~r²/2R). Fixed with the area-relation conversion (DECISIONS.md #28)
— before/after captures show frozen vs. canvas-filling growth.

Regressions: classic mode, mouse gestures, abi_c_compile all green;
~100 fps in all runs.
