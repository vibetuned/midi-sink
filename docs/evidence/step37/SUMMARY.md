# Evidence — Step 37: Chladni lattice

ROADMAP_5 Step 37; MEDIUM §2.2; SPEC §4.3 (the ripple's two insertion
points). Decisions: `_work/DECISIONS_5.md` #23–#27. Machine: the author's Mac
(Apple silicon, Metal); the wasm rebuilt and gated here; the iOS shell
compile-checked against the grown header. Core reopened by the roadmap; the
§4.6 fixture untouched and bitwise.

## What landed

* **The pass** `SUMI_DEFORM_CHLADNI` (`deform.glsl chladni_fs`): the
  quadrature kick-drift pair x₁ = x + a·cos(k_y·y); y₁ = y + b·cos(k_x·x₁),
  inverse lookup y first then x, an `inv_order` flag for the pair's exact
  inverse (reversed shear order), the §3.4 ingress rule. Class exact,
  declared in the shader header, the ABI comment, the soak table, the page
  draft (#23).
* **Live path** in the composite (`composite.glsl`, after the ripple's
  displacement, behind an `a == b == 0` branch), zeroed on the print path;
  **bake path** in the mapper, delta-driven like the ripple bake (#24).
* **Harmony as geometry** (#25): `SUMI_CTL_CHLADNI_A/_B` (16/17, COUNT 18),
  `params.chladni_bake / _k / _ratio_p / _ratio_q`, the just-intonation
  semitone table, retargeting on VoiceBegin/End with smoothing;
  `sumi_add_chladni(a, b, k_x, k_y)` as the gesture (a < 0 = the inverse);
  `sumi_debug_chladni_k` for tests. `sumi_version` **0.11.0**.
* **The negative**, headless: `test_chladni_kick_drift_order` in
  `tests/normalizer_tests.cpp` — the simultaneous form's |1 − det J| reaches
  0.38 and its sign flip does not invert; the kick-drift inverts to 10⁻⁹.
* Desktop: "Chladni lattice" settings section (live/bake, two amount sliders
  on CC 106/107 routes, base waves per canvas, ratio combo), INI keys, stock
  map v5 (+106/107) with the v4 stock map migrating, controller names,
  `--chladni-test`, `chladni` in the conservation gate.
* Web: scene `chladni` (interval, A, B, k, bake, pace), export
  `_sumi_add_chladni`, four parameter ids, the gate's sweep; `check.mjs`
  untouched on purpose (#22, #27).
* Page draft: `chladni.mdx` (this folder), for `site/…/operators/` at step 63.

## Measurements (`chladni_test.log`, 5/5)

| Check | Result |
|---|---|
| (a, b) then (−a, −b), 0.04 canvas on a 3:2 lattice | one pass moves the interior pre-image 22.2 texel; the pair leaves 0.178 (whole field 1.50 — the ingress bands) |
| Live path | an LFO on A and B leaves the field bitwise identical (2 097 152 bytes) |
| Live dip | equals the plain dip byte for byte; the fields before the dips bitwise identical |
| Harmony as geometry | fifth 1.5000, fourth 1.3333, one voice holds 1.3333, major third 1.2500 |
| Bake | centre column 2 waves (q), centre row 3 waves (p), peaks 28 and 16 texels |

## The gate (`soak_chladni.log`, 3/3)

| Part | Chladni |
|---|---|
| (a) | exact — two shears, the second at the displaced x₁ |
| (b) | 500 ±0.04 pairs: mass −0.48% / +0.00%, pre-image dev 3.43 texel (≤ 8; an oscillatory shear, as the torsion's 5.45) |
| (c) | growth +0.00%; edge band +196 / interior −9816; route alive (260 669 texels moved) |
| (d) | 1.57·10⁻⁵/pass vs glide-tine control 1.15·10⁻⁵ (×1.37 ≤ 2) |

Observation, not chased: the (c) split shows a small GAIN in the 16-texel
edge band (196 units of ~86 000) under a whole-canvas shear stream whose
ingress bands are fresh water — the bilinear clamp within the last
half-texel duplicates the edge column slightly; two orders below the
interior loss and three below the fabrication bar.

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 (ABI pin 0.11.0; the kick-drift order test inside the normalizer suite) |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 13/13 including `chladni` |
| iOS shell | compiles against the grown header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| exact declared | shader header, ABI, soak table, page draft |
| ±A inversion golden | interior 0.178 texel after the pair; the gate's (b) 3.43 over 500 pairs |
| the bake path passes the four-part gate (ingress rule applies) | 3/3; the pass carries the §3.4 ingress branch |
| the live path leaves the dip un-shimmered (the ripple's test, reused) | byte for byte |
| scene `chladni` with 3:2, 4:3, 5:4 presets | the interval slider: 7, 5, 4 semitones (and 12 for 2:1) |
| fixture bitwise | Metal max\|d\| 0 |
