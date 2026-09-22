# Evidence — Step 38: Viscous multipole burst

ROADMAP_5 Step 38; MEDIUM §2.3. Decisions: `_work/DECISIONS_5.md` #28–#33.
Machine: the author's Mac (Apple silicon, Metal); the wasm rebuilt and gated
here; the iOS shell compile-checked against the final header. The §4.6
fixture untouched and bitwise. **Order as the roadmap fixed it:** the
derivation script and the literature check first (`multipole_verify.log`,
`literature.md`), the pass second, the age envelope third.

## What landed

* **The derivation, checked** (`tools/multipole_verify.py`, 44 checks): the
  multipole heat kernel, its stream function with the spec's cutoff γ_m, the
  time integral Φ_m — E₁ at m = 1, elementary for m ≥ 2 and equal to the
  spec's form to 10⁻¹³, Φ₂ = χ — the divergence-free closed-form
  displacement, the hyperbolic-strain core (λ → 1.359 D/a), the far field
  (cos 2θ/r in the quadrupole's diffused zone; cos mθ/r^(m−1) in general,
  1/r^(m+1) beyond ℓ — a precision on §2.3, #28), the lobe peak (1.07–1.45 a,
  0.4–9% above D) and the per-pass fold budget β_m per order (#29).
* **The literature check** (`literature.md`): the velocity fields are
  classical (Voropayev & Afanasyev, Chan & Chwang, the Hermite modes), the
  time integration is Jaffer's method (arXiv:1810.04646), the displacement
  form for m ≥ 2 not found stated; the wording rule that follows.
* **The pass** `SUMI_DEFORM_BURST` (`deform.glsl burst_fs`): one age
  increment ℓ₀ → ℓ₁ of the impulse of core a, order m (2..8), axis θ₀; the
  plateau-deficit series below S = 1 and the closed form above (no expm1 in
  GLSL — the equivalent); the §3.4 ingress rule. `displacement.cpp` carries
  the same mathematics in double: Φ_m, γ_m/S, the amplitude from D, the
  peak on the axis, the budget β_m, the greedy march.
* **Class sub-stepped** (#29): peak displacement ≤ β_m × the pass's current
  core (|∇d| ≤ 0.25, the wake's a/4 criterion generalised); β₂ = 0.13.
* **The gesture is the strike with a lifetime** (#30): `sumi_add_burst(x, y,
  a, D, θ₀, m)` starts an episode in the mapper (32 slots); `burst_age`
  (ℓ_end/a, default 4) and `burst_life` (seconds, default 0.8) are the age
  envelope, ℓ² linear in time; `burst_order` (2..8, default 2) is the order
  a gesture with m = 0 takes; the emission floor is the field's half-float
  quantum (5·10⁻⁴). `sumi_version` **0.12.0**. `sumi_debug_burst_count`
  for tests.
* **Headless** (`normalizer_tests`): Φ and γ/S continuity at the series
  switch for m = 2..8, the normalisation and its sign, the peak, the march
  (D = 2a in 13 pieces, worst peak/budget 1.0000), the episode at life 0 (4
  passes tile a → 4a in one frame) and at life 0.5 s (passes on 10 of 70
  frames, the first age a·√1.25 exactly, over by the end).
* Desktop: `--burst-test` (9 checks), `burst` in the conservation gate, the
  U key at the cursor (the settings' order), a "Burst" settings section
  (age, life, order), INI keys.
* Web: scene `burst` (D of the core, a, θ₀, m, age, release, pace; two
  strikes a quarter turn apart), export `_sumi_add_burst`, three parameter
  ids, the gate's sweep; `check.mjs` untouched on purpose (#22).
* Page draft: `burst.mdx` (this folder) with the lineage line and the
  author's note verbatim from the roadmap, marked for the author to trim
  and sign; lands at step 63.

## Measurements (`burst_test.log`, 9/9)

| Check | Result |
|---|---|
| One budgeted pass, D = 0.0045 (2.30 texel) | centre 0.00 texel; lobes eject 2.25 / 2.25 on the axis, draw in 2.25 across it |
| First-order area | pre-image det min 0.844 everywhere, mean 1.00000 |
| Life 0 | the episode is over after the frame |
| The pair (+D, −D), D = a/4 | residual max 0.76 texel (the first-order \|∇d\|·d), mean 0.018 |
| θ₀ = π/4 | ejects 2.47 on its axis, draws in 2.39 across, 0.00 radially at θ = 0 |
| m = 3 | lobes 2.25 / 2.29 / 2.29 at 0°, 120°, 240°; 2.23 / 2.38 drawn in at 60°, 180° |
| Age envelope, t = life/6 | lobe at 2.62 of 4.10 texel (the envelope's share 0.70 → 2.89), episode running |
| Age envelope, after the release | lobe at 3.75 of 4.10 texel (the half-float quantum's toll, #30), episode over |
| The shader vs the closed form | d_r at a/2, a, 2a, 3a, 4a within 0.023 texel of the double reference; d(2a)/d(4a) = 1.76 = the 1/r law with the core and age corrections |

## The gate (`soak_burst.log`, 3/3)

| Part | Burst |
|---|---|
| (a) | sub-stepped — d = ∇⊥Ψ, div d = 0 as a field; passes within β_m × the current core |
| (b) | one budgeted pass: det min 0.812 everywhere, mean 1.00000; the pairs (informational): mass −17.62% / +0.00%, pre-image dev 2.29 texel over 500 pairs of FOUR passes (two pieces each way at D = a/4) |
| (c) | growth +0.00% over 6000 passes, edge band +0 / interior −5688; route alive (71 539 texels moved) — 2000 strikes, one every third frame with a three-frame release |
| (d) | 9.30·10⁻⁶/pass vs glide-tine control 1.15·10⁻⁵ (×0.81 ≤ 2) |

Against the wake's numbers (step 35): det min 0.734 / 0.766, growth +0.00%,
erosion ×0.58 / ×0.76. Observation, recorded: the burst's pair phase fades
faster than the wake's (−17.6% against −4.7% / −3.0% over 500 pairs) — a
pair here is four resampling passes, not two, and each pair's first-order
residual (0.76 texel at the core) reshuffles the ink boundary; per pass the
fade is 8.8·10⁻⁵, twice the doublet pair's. The gated erosion, on the
stream at about one pass a frame, is 0.81× the tine's.

## Gates

| Gate | Result |
|---|---|
| `tools/multipole_verify.py` | all checks passed (`multipole_verify.log`) |
| `ctest` | 4/4 (ABI pin 0.12.0; the burst test inside the normalizer suite, 18 523 checks) |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 14/14 including `burst` |
| iOS shell | compiles against the final header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| class sub-stepped declared | shader header, ABI comment, soak table, page draft |
| the four-part gate under a strike stream at gesture rate | 3/3; 2000 strikes over the 6000-frame window; the wake's numbers as the bar |
| the verify script and the literature note in the evidence | `multipole_verify.log`, `literature.md` (the note written before the page draft) |
| scene `burst` with the age slider | ships with D, a, θ₀, m, age, release |
| fixture bitwise | Metal max\|d\| 0 |
| the page draft carries the lineage line and the author's note | `burst.mdx` — the note verbatim, for the author to trim and sign |

Deferred to step 42 by the roadmap: the note-on strike route, θ₀ from the
pen azimuth or the glide direction, the pitch-class → m table.
