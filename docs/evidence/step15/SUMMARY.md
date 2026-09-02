# Step 15 — Layout probe ABI + Play-mode overlay skeleton (iOS): DONE evidence

Specs: `_work/PHASE4_SPEC.md` §1/§2/§6, `_work/ROADMAP_3.md` Step 15.
Decisions: `_work/DECISIONS_3.md` #6–#9. Core change: exactly the probe API
(working rule: core frozen otherwise).

| DONE criterion | Result | Evidence |
|---|---|---|
| Probe golden tests pass headlessly | **PASS** — suite grew 8,934 → **12,480 checks**: CHROMA_GRID round-trips all 84 cell centers (note + exact center + row-direction axis + step = one column width) at 2 aspects; Jankó round-trips **all three echo rows** of all 84 notes; semitone-step goldens against closed-form values; refusals (FIFTHS, rolls, unknown ids, outside the playable area, Jankó stagger dead zones); purity double-call | `tests/normalizer_tests.cpp` `test_layout_probe_golden`, ctest 4/4 |
| Lattice aligns pixel-perfect with where loopback-MIDI drops land | **PASS** — scripted `sumi_push_midi` note-ons (single-drop runs at the lattice extremes C1 / F#4 / B7 on CHROMA_GRID), dip-print export, measured drop centroids probed back through the REAL ABI: every centroid returns its note, offsets **≤ 0.0004 canvas (≈ ½ px at 2560×1440)**. (An 8-note run is included for context: multi-drop centroids drift by design — every Jaffer drop displaces its neighbors — so the undisplaced single-drop is the honest alignment measure.) | `alignment_check.txt`, `single_{24,66,107}.png`, `grid_alignment.png`, `run_alignment.log` |
| Indicator math unit tests cover the soft knee | **PASS** — g(0)=g(0.015)=g(0.03)=0; continuous from exactly 0 (g(0.03+1e-4)=1e-4/0.97 — no zipper jump); g(1)=1, clamps beyond; monotone over 200 samples; NaN/negative safe. Vector form: direction preserved, magnitude = g, deadband → (0,0), degenerate radius → (0,0). **221 checks**, plus `hostmpe_c_compile.c` proving the header is pure C11 (working rule) | `tests/hostmpe_tests.cpp`, `tests/hostmpe_c_compile.c`, ctest |
| Marble mode bit-identical to Step 13 | **PASS by construction + on-device check** — the Step-13 gesture recognizers and handlers are untouched; in Marble mode the Play overlay is hidden AND interaction-inert (`isUserInteractionEnabled = false`), so touches take the identical path. Play mode disables the recognizers and routes raw touches to the overlay only | `ios/Sources/SumiCanvas.swift` (applyMode), user-verified on the iPad |
| `sumi_version()` reads 0.3.0 | **PASS** — engine bumped; `abi_c_compile` asserts 0.3.0 in strict C11 and exercises the probe (grid center round-trip, FIFTHS refusal) with **no instance created at all** — the instance-free contract, proven in C | `tests/abi_c_compile.c` |

New this step:
- `sumi_layout_probe` + `sumi_cell_info_t` in the ABI (units contract
  documented field-by-field: centers normalized, radius/step in canvas-height
  units, direction aspect-corrected — DECISIONS_3 #6).
- `sumi_layout_semitone_delta` (internal): the single #7 implementation
  behind both the glide axis (capped) and the probe (true step) —
  DECISIONS_3 #7.
- `hostmpe/` seeded with the §3.2 soft knee (pure-C header + module map +
  ctest suite + C11 compile test) — DECISIONS_3 #8.
- iOS: Marble/Play segmented toggle (persisted, disabled with a footnote on
  non-playable layouts), `PlayOverlayView` — faint lattice built by sweeping
  the probe (DECISIONS_3 #9), per-touch joystick indicators (hairline R_max
  circle + thumb dot at Δ_eff via HostMPE), Jankó three-row same-note
  highlight, zero MIDI.

Regression state: ctest 4/4 (12,480 + 221 checks + both C11 ABI tests),
desktop harness untouched at 99.5 fps in the alignment runs, 0 dropped.
