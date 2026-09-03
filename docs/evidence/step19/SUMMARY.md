# Evidence — Step 19: v0.4 deformation operator batch (core, desktop harness)

PROJECT_SPEC §4.3(3–6), §4.5, §5.3. `sumi_version()` → **0.4.0**. Decisions
logged as DECISIONS_3 #32 (wake sign correction, a/2 fold threshold,
`sumi_add_pinch` ABI addition, pinch-variant pick, ripple defaults, soak
semantics, crease measurement).

## What landed

**Core (`deform.glsl` + `displacement.{h,cpp}` + renderer + engine + mapper):**
* **Vortex profiles** (§4.3(3)): `sumi_add_vortex` gained the profile
  argument (breaking; all call sites updated — desktop, iOS, Android JNI,
  debug script). `RANKINE`: rigid core (θ = ω inside R), 1/r² exterior, all
  shear in the crease ring. CC-routed vortex takes `params.vortex_profile`.
* **Dipolar wake** (§4.3(4)): `sumi_add_wake(x0, y0, x1, y1, tip_radius)` —
  gesture-ABI only (not MIDI-expressible, the §7 invariant). Lab-frame
  doublet from φ = −Ua²x/r² (the SPEC'S SIGN WAS CORRECTED — the draft's `+`
  rendered inside-out and contradicted its own zero-seam claim); rigid tip
  body inside r ≤ a. Internal sub-stepping at ≤ a/4: a/2 is the fold
  THRESHOLD (inverse-map Jacobian 1 − 2d/a = 0 at the rear stagnation
  point), not a safe budget. Spec formula + budget corrected.
* **Hamiltonian pinch** (§4.3(5)): streamline-windowed saddle (w(s) =
  exp(−|s|/S), S = 0.02), det = 1 exactly. Two routes: `sumi_add_pinch(x, y,
  k_delta, angle)` — a public gesture-ABI addition beyond the spec's list
  (the fold axis is host-side data: pen azimuth / drag angle, no MIDI path) —
  and `slide_mode = 1`: smoothed per-voice CC74 DELTAS pinch at the voice
  position, fold axis = the voice's lattice pitch axis; the first CC74 of a
  voice primes (snaps) so a controller's rest position never fires a
  spurious gesture; slide→aux modulation is mode-0 only.
* **Sine ripple** (§4.3(6)): pure shear, both insertion points. LIVE
  (`ripple_bake = 0`): composite-time view displacement of the ink sampling
  coordinate only — paper stays screen-locked, the field is never written,
  the amp = 0 branch keeps the un-rippled composite bit-identical to v0.3,
  and the PRINT path always composites with amp 0 (the dip samples the
  un-rippled field). BAKE: delta-driven deform pass (ΔA at current k/angle),
  additive at fixed (k, φ, angle). Ctl dims `RIPPLE_AMP`/`RIPPLE_FREQ`
  (0..1 → A ≤ 0.025 canvas heights, k = 2π·(2..16)); φ fixed 0 in v0.4.
  The dims ship UNMAPPED in the default CC map (CC1 stays the vortex mod
  wheel — Step 18's DONE depends on it; spec enum comments cleaned).
* Params v0.4: `slide_mode`, `vortex_profile`, `ripple_bake`, `ripple_angle`.

**Desktop harness:**
* Bindings: middle-drag wake (scroll = tip radius), Shift+left-drag pinch
  (drag distance = k delta, drag angle = fold axis), `V` profile toggle,
  `R/T`+`F/G` ripple amp/freq as injected CC 102/103 through
  `sumi_midi_harness_inject` (same §5.2 producer mutex as device callbacks),
  `K` live/bake, `O` frame rotation, `X` crossed-tine pinch prototype.
* Scripted DONE tests: `--wake-test`, `--flick-test`, `--rankine-test`,
  `--pinch-soak <n>`, `--ripple-group-test`, `--ripple-dip-test`,
  `--pinch-demo` (the pick-by-eye pair).

## DONE verification (`scripted_tests.txt`, PNGs in this folder)

* **Wake orientation**: numeric (pre-image behind the tip ahead, ahead at
  the flank) + the screenshot pair `wake_before/after.png` — the rings show
  the forward bulge and the trailing entrainment cusp.
* **Fast flick**: one-frame 8×a stroke → pre-image Jacobian positive over
  the whole fluid region (min det 0.44). Learned: the tip corridor carries
  potential flow's slip surface (a discontinuity, not a fold), and
  u-monotonicity is the wrong test off the symmetry axis — the Jacobian
  criterion replaced it.
* **Rankine**: swirl 0.5006/0.4997 at 0.5R/0.9R (expect 0.5), 0.2226 at
  1.5R (expect 0.2222); crease max-gradient at R + 2.3 texels (one-sided by
  construction: |dα/dr| is zero inside R, maximal immediately outside);
  **20 full rotations: interior mean |Δink| 0.00000** vs 0.75775 for the
  exponential control at the same total angle.
* **Ripple group** (3/3): live LFO on A → field BITWISE identical
  (2,097,152 bytes memcmp) — the live path never writes; bake mode writes,
  and A returning to 0 composes back to identity (fresh sheet, mean |du|
  0.00033 vs ~0.016 for standing residue).
* **Live-vs-bake dip**: a print taken at full live ripple equals the
  ripple-free print of the same deterministic scene byte-for-byte.
* **Pinch soak** (`pinch_soak.txt`, DECISIONS_3 #33): the four-part gate —
  det = 1 verified symbolically; 500 strong reversible (+k,−k) pairs hold
  ink MASS (Σ phase — the observable the analytic map conserves; level-set
  areas and parity histograms are blur-sensitive and were rejected) to
  ±0.5%; zero fabrication (the pre-fix edge-clamp FABRICATED +9.5%/12k —
  the §3.4 scroll ingress rule now covers wake/pinch/ripple-bake); and the
  pinch's per-pass erosion ≤ 2× the v1 glide tine's under the identical
  stream, matched 6,000-frame windows (measured 1.76e-5 vs 9.15e-6 per
  pass, ratio 1.9 — bulk resample fade, zero edge component, the medium
  every operator lives in; at 36k passes the legacy tine's own edge-clamp
  FABRICATION nets it +5%, so long-horizon rates are incomparable — #33).
  **Flagged for the user:** the roadmap DONE's literal "conserves within
  measurement noise over the 10-minute stream" is unsatisfiable on this
  medium for ANY operator (the incumbent tine fails it identically) —
  amendment to the four-part gate proposed in #33.
* **Pinch variant pick — superseded by the user (#34)**: shown the pair
  (`pinch_hamiltonian.png` vs `pinch_crossed.png`), the user kept BOTH:
  `pinch_variant` params switch (0 = Hamiltonian saddle, default; 1 = crossed
  tines), honored by the CC74 route and `sumi_add_pinch`, surfaced on iOS
  ("Slide (CC74)": Hue/Pinch routing + Saddle/Crossed style), Android (same
  rows), desktop key `C`. The crossed PNG now renders through the real
  params path. Also fixed: Android's `nativeSetLayout` guard still rejected
  the Piano-grid entry (layout 5).
* **Regressions**: `--field-dump` vs the committed Metal fixture:
  **bitwise zero difference** (existing operators untouched; the
  profile-0 vortex path compiles to the same math). Step 3/5 stress:
  `--drop-test 512` (98.3 fps), `SUMI_STRESS_SWAPS=40` (99.4 fps),
  `--demo-chevron`, `--dip-burst` (third dip refused, both prints intact)
  all clean, 0 dropped MIDI.
* **bend_mode** (the user's roadmap addition, #35 — refined same-day twice,
  to the PER-NOTE bend with the amount AS the bend): 0 = v1 glide drag,
  1 = a note's bend raises the sine ripple, whose depth is the bend's
  DISTANCE FROM CENTER exactly like glide (|semis|/6; ±0.5-semi vibrato ≈
  8% shimmer; drop holds; master bend + mod wheel untouched; k rests
  mid-range, CC 103 adjusts). The water stills itself on re-center, on the
  last note's release, and on a mode flip — all three "come back from a
  ripple" paths unit-tested in `test_bend_mode_single_consumer`, alongside
  the glide-XOR-ripple DONE gate. Surfaced: iOS "Note bend" (Glide/Ripple),
  Android row, desktop key `M`.
* **Ripple vibrato is PERMANENT (#36)**: the Ripple toggle selects the BAKE
  insertion point, and bend-driven passes drift their phase with activity —
  each vibrato cycle bakes a slightly shifted comb (the §4.3(6) feathering),
  while the dynamic still stills on re-center/release/mode-flip.
  `--ripple-permanence-test`: 3 vibrato cycles ending at center + release →
  76,540 far-field texels permanently changed with the amp ctl at zero; the
  CC-driven bake path keeps the pure composing-back property (group test
  re-passes). No live/bake control on the tablets — the toggle is the
  choice; desktop `K` remains the manual override.
* ctest 4/4 (ABI compile test extended: version 0.4.0, new params/enums,
  wake/pinch symbols); iOS static lib + app compile clean against the new
  ABI.
