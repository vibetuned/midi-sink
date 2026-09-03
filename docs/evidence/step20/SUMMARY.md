# Evidence — Step 20: Lamb–Oseen swirl & bipolar press (core + iOS)

PROJECT_SPEC §4.3(7), §3.3–§3.4, §2.1; PHASE4 §3.3/§4/§5.3. Decisions:
DECISIONS_3 #37. Version stays inside the uncommitted 0.4.0.

## What landed

**Core:** `SUMI_DEFORM_SWIRL` pass (θ(r) = S/(2πr²)·(1−e^(−r²/r_c²)), series
guard below x < 1e-3 — GLSL has no expm1); normalizer decodes 0xA0 →
`SUMI_MEV_POLY_PRESSURE`; mapper: the *swirl* dimension (coalesced, smoothed
like press/glide/slide), `press_mode` arbitration of 0xD0 (0 = v1 ink feed,
1 = swirl; one consumer; wind brush exempt), 0xA0 → swirl in either mode
keyed by the voice's note; Γ from core-angular accumulation (ω ≤ 2 rad/s ×
amount × expansion_rate, S = θ·2πr_c² at emission), r_c = the voice's
boundary R, band-parity sign, echo fan-out + budget merging. `press_mode`
param added.

**hostmpe:** bipolar Y (PHASE4 §3.3) — one radial knee, up → 0xD0 feed,
down → 0xA0 swirl on the voice's channel with its note; center = both zeros;
crossing releases the departing half through zero; lift releases an engaged
swirl half before Note Off; the limiter treats 0xA0 as a continuous
dimension (change-only + per-transport policies + BLE fairness slot).

**iOS:** the surface's down-pull plays the swirl (no new UI — hostmpe emits
it); settings gains "Pressure (aftertouch)" Feed/Swirl for 0xD0 hardware
routing. **Desktop:** `P` = press_mode, `J` = test voice, `W`/`E` = 0xA0
amount (through the §5.2 producer).

## DONE verification (`scripted_tests.txt` — 19 ok / 0 fail)

* **Small-r stability**: no NaN within 2 r_c, core bounded; the guarded
  shader path verified CPU-side against the analytic θ(0) limit (1e-3 rel,
  branch continuity 2.3e-5). Found & documented (#37): half-float ULP freeze
  near the exact center — protective, reads low never high.
* **Core coherence**: ring sharpness inside 0.7 r_c retained exactly
  (0.0143 → 0.0143) through ~4 rad of stirring while the 2–3 r_c annulus
  moved 29,014 texels — the drop IS the vortex core.
* **Counter-rotation**: consecutive strikes at two cells, signed swirl
  +1.5020 vs −0.6507 rad (band parity).
* **hostmpe bipolar** (unit, 516 checks total): center both zeros; up only
  0xD0 (127 at full radius); down only 0xA0 with the note; knee continuous
  through center; lift order 0xD0 0 → 0xA0 0 → Note Off; limiter decimates
  and change-only-filters 0xA0.
* **press_mode one-consumer** (unit): a scripted Osmose-style 0xD0 stream in
  mode 1 yields swirl passes and ZERO grow passes; mode 0 the reverse.
* **Byte-level 0xA0**: unit-asserted on the voice's member channel with its
  note number (hostmpe + mapper key-matching, stray notes ignored).
* **Regressions**: the full step-19 battery re-passes with the operator in
  the build (wake, flick, Rankine, ripple group/dip/permanence — all ok),
  field-dump fixture bitwise-identical, ctest 4/4.
