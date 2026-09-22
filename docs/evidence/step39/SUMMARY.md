# Evidence — Step 39: Spark shear & the composed strike

ROADMAP_5 Step 39; MEDIUM §2.4. Decisions: `_work/DECISIONS_5.md` #34–#39.
Machine: the author's Mac (Apple silicon, Metal); the wasm rebuilt and gated
here; the iOS shell compile-checked against the final header. The §4.6
fixture untouched and bitwise.

## What landed

* **The pass** `SUMI_DEFORM_SPARK` (`deform.glsl spark_fs`): one stage of the
  piecewise kick-drift — rows slide by A·w(y)·f(y), then columns by
  B·w(x₁)·f(x₁) — with f a stack of triangle waves at k, 2k, 4k, 8k
  (`spark_stack`, 1..4, default 3) or piecewise-linear hash noise
  (`spark_profile`), w a Gaussian window across each shear, the frame
  rotated by θ₀ about the strike; the §3.4 ingress rule. CLASS EXACT for
  any profile (#34). The noise is an integer hash, bit-identical across
  backends.
* **The gesture** `sumi_add_spark_shear(x, y, band, A, B, k, φ, θ₀)`: one
  step; a zero amplitude skips its stage, so the exact inverse of (A, B) is
  the call (0, −B) then (−A, 0) — reversed order, negated.
* **The composed strike** `sumi_add_spark(x, y, r, D, θ₀, layer)` (#37): the
  drop (the Joule blast), the burst of core r and lobe displacement D along
  θ₀, the shear as a DECAYING EPISODE (#36): A = B = `spark_shear`·r spent
  as e^(−t/τ) over 4·`spark_tau`, dealt in kick-drift steps at the field's
  quantum, the window 2r, k from `SUMI_CTL_SPARK_K`, φ per strike. CLASS
  SUB-STEPPED BY INHERITANCE (the burst).
* **The wavenumber ctl** `SUMI_CTL_SPARK_K` (18, COUNT 19): base 2π·2 ..
  2π·24 per canvas height, octaves above; `slide_mode` 2 routes a voice's
  CC 74 to it (#35) — prepared, not the default. `sumi_version` **0.13.0**;
  `sumi_debug_spark_count` for tests.
* **Headless** (`normalizer_tests`): the kick-drift in double inverts to
  10⁻¹⁶ and keeps det J = 1 through the kinks for the triangle stack AND the
  noise profile; the same-order sign flip is not an inverse (0.03 residue);
  the episode deals 28 steps whose kicks decay 0.0024 → 0.0003 and sum to
  A(1 − e^(−t/τ)) to 10⁻⁶; slide_mode 2 sets the SPARK_K target from a
  member channel's CC 74, modes 0 and 1 do not.
* Desktop: `--spark-test` (5 checks), `spark-shear` and `spark` in the
  conservation gate, the Z key (the composed strike at the cursor), a
  "Spark" settings section (shear, decay, octaves, profile, frequency as a
  CC slider on the new CC 108 route), the slide radio's third option, INI
  keys, stock CC map v6 (v5 migrates), the name "Spark frequency".
* Web: scene `spark` (the composition by stages at A, the whole spark a
  quarter turn on at B; radius, D, shear, frequency, decay, octaves,
  profile, axis), exports `_sumi_add_spark_shear` / `_sumi_add_spark`, four
  parameter ids, the gate's sweep; `check.mjs` untouched on purpose (#22).
* Page draft: `spark.mdx` (this folder), for step 63.

## Measurements (`spark_test.log`, 5/5)

| Check | Result |
|---|---|
| Triangle stack, 15-texel kicks on a 43-texel base | a step moves the interior pre-image 7.35 texel; the step then its exact inverse leaves 0.212 (max 2.10 at the kinks); the same-order sign flip leaves 6.17 |
| Noise profile | 6.92 texel moved; the inverse leaves 0.009 (max 0.35); the flip 1.60 |
| One stage | a pure shear: no row moved in y (0.000 texel), in-row spread 0.039 texel, the largest row shift 11.7 texel |
| The composed strike | the shear episode registered and over after 4τ; the burst at life 0 done in its frame |
| Composition vs the drop alone | 5.51 texel mean difference over r..3r; the sub-stepped member inside the drop's field (shear off, #38) reads det min 0.734, mean 1.00002 — the drop alone 0.812 in the same region |

## The gate

`soak_spark_shear.log` (3/3) and `soak_spark.log` (3/3):

| Part | `spark-shear` (exact) | `spark` (sub-stepped by inheritance) |
|---|---|---|
| (a) | exact — two shears, any profile; the inverse (0, −B) then (−A, 0) | the drop and the shears exact, the burst first-order: the strictest member's class |
| (b) | 500 pairs of a 15-texel step and its inverse: mass −0.59% / +0.00%, pre-image dev 3.60 texel (≤ 8) | one composed strike (the drop then the burst, the shear off — #38): det min 0.750 outside the rim annulus, mean 1.00004; the pairs, informational: mass −9.53%, dev 20.6 texel (a −D strike negates the burst only; both strikes' shear episodes add) |
| (c) | growth +0.00% over 6000 passes, edge +446 / interior −11 601; route alive (240 990 texels moved) | growth +0.00%, edge +293 / interior −11 592; route alive (260 893 texels) — 1500 blast-less strikes, the axis turning |
| (d) | 1.83·10⁻⁵/pass vs glide-tine control 1.15·10⁻⁵ (×1.59 ≤ 2) | 1.85·10⁻⁵/pass vs 1.15·10⁻⁵ (×1.61 ≤ 2) |

Against the wake's numbers (step 35): det min 0.734 / 0.766, growth +0.00%,
erosion ×0.58 / ×0.76; the burst's (step 38): 0.812, +0.00%, ×0.81. Both
spark operators erode faster than the smooth operators and inside the bar:
kinks every few texels are fine structure, and fine structure is what the
bilinear medium fades first (#15).

**The first `spark` stream run struck with clear-water drops and read mass
0 at 500 pairs and at 6000 passes** (pre-image dev 145 texel, (d) ×14.5):
not erosion — each strike's exact expansion pushes the ink outward and
fifteen hundred of them push it off the canvas, which no mass observable
can tell from loss. Drops have been excluded from the gate by nature since
step 35 (#13); the composed strike inherits the exclusion for its blast, so
`SUMI_DROP_NONE` joined the layers and the stream runs blast-less (#39).

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 (ABI pin 0.13.0; 18 541 headless checks) |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 15/15 including `spark` |
| iOS shell | compiles against the final header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| the spark SHEAR declares exact (±A inversion golden, triangle and noise profiles) | shader header, ABI, soak table, page draft; 0.212 / 0.009 texel after a step and its inverse, headless 10⁻¹⁶ |
| `sumi_add_spark` gates as sub-stepped by inheritance under the wake-family numbers | det min 0.750 / mean 1.00004 on the composed strike (wake 0.734 / 0.766); (c) +0.00%; (d) ×1.61 |
| the four-part gate under episode streams | `spark`: 1500 blast-less strikes, each a burst episode and a shear episode, 3/3; `spark-shear`: 6000 exact steps, 3/3 |
| scene `spark` (the composition visible in it) | the stage slider shows the drop, then with the burst, then the whole spark |
| fixture bitwise | Metal max\|d\| 0 |
| page draft | `spark.mdx` |

Flagged: the spec's kick-drift line reads "y₁ = y₁ + B·tri(…)" — a typo
for y₁ = y + B·tri(k_x·x₁ + φ_x), which is what shipped. The pitch-class →
m table, the note-on strike route and the Anod defaults for CC 74 wait for
step 42 as the roadmap says.
