# Evidence — Step 36: Wave torsion, the proof brick

ROADMAP_5 Step 36; MEDIUM §2.1. Decisions: `_work/DECISIONS_5.md` #18–#22.
Machine: the author's Mac (Apple silicon, Metal); the wasm rebuilt and gated
here too; the iOS shell compile-checked against the grown header. **Core
reopened by the roadmap** — the first Phase-6 operator; the §4.6 fixture
untouched and bitwise.

## What landed

* **The profile** (`SUMI_VORTEX_TORSION = 3`): θ′ = θ + A·sin(k·r − φ)·e^(−r/R),
  r′ = r, as a third branch of the vortex pass in `core/src/shaders/deform.glsl`
  (tested first so the exponential path is byte-identical), `k`/`phase` in the
  vortex uniform block and the queue payload, wired through the renderer.
  Class **exact**, declared in the shader header, the ABI comment, the soak
  table and the page draft (#18).
* **Two flavour controls** `SUMI_CTL_TORSION_K` / `_PHASE` (14/15, COUNT 16),
  ranges 2π·4…2π·40 per canvas height and 0…2π, rests 0.5 / 0, read smoothed
  by every vortex route (`sumi_voice_mapper_torsion_kphi`); the desktop maps
  CC 104/105 (stock map v4, v3 migrates) and names them in the CC editor (#19).
* **The sweep episode** (`params.torsion_sweep`, default off): per-voice,
  time-driven, per-frame rotation increments with φ advancing at 2π·1.5 rad/s
  and RATE·e^(−t/τ) amplitude (1.2 rad/s, τ 0.6 s, reach 3× the strike radius,
  over after 4τ), merging under the budget, outliving the note, re-armed by a
  new note (#20). Constants in `core/src/voice_mapper.cpp`.
* `sumi_version` **0.10.0**; `tests/abi_c_compile.c` pins it and the new
  enum values.
* Desktop: three-way profile picker + "Torsion sweep on note-on" checkbox in
  the settings window, INI keys `vortex_profile` (0/1/3) and `torsion_sweep`,
  dev key `V` cycles exponential → Rankine → torsion, `--torsion-test`, and
  `torsion` in the conservation gate (`--soak torsion`).
* Web: scene `torsion` (A, k, φ, R, sweep, pace) in `web/site/scenes.js`, the
  `torsion_sweep` param id, the profile clamp admits 3, `tools/web_gate.mjs`
  sweeps the scene. `site/scripts/check.mjs` untouched on purpose (#22).
* The gate itself (#21): (c) becomes "≤ 0.5% growth OR ≤ 5·10⁻⁵ per pass" with
  an edge/interior split printed; pair magnitudes follow one ~25-texel
  convention (the torsion's k set through CC 104 before its scene); the
  exact-class pre-image bar is 8 texels.
* Page draft: `torsion.mdx` (this folder), for `site/…/operators/` at step 63.

## Measurements (`torsion_test.log`, 8/8)

| Check | Result |
|---|---|
| One pass, θ(r) = A sin(k r) e^(−r/R), A 0.4, k 2π·4, R 1 | crest marker 0.364 rad (analytic 0.376), trough −0.320 (analytic −0.332) |
| Alternating shear | crest and trough turn opposite ways |
| (+A, −A) pair | markers back within 3·10⁻⁴ and 1·10⁻⁴ rad |
| Control, sweep off | a strike turns an outside marker by 0.0000 rad |
| Sweep fires | marker swing 0.087 rad |
| Deltas, never a total | largest 10-frame step 0.038 rad (bound 0.15; a whole pattern ≈ 0.7) |
| Ends on its own clock | angle at 3 s = angle at 4 s to 10⁻⁴; released at 2 s |
| Re-arm | second strike: swing 0.040, largest step 0.040 |

## The gate (`soak_torsion.log`, 3/3; `negative.log`, 3/3)

| Part | Torsion |
|---|---|
| (a) | exact — rotation by θ(r), r preserved |
| (b) | 500 ±0.5 rad pairs on a 39-texel wavelength: mass −0.63% / +0.00%, pre-image dev 5.45 texel (≤ 8) |
| (c) | +2.26% over 6000 passes = 3.8·10⁻⁶/pass (≤ 5·10⁻⁵), edge band +0, interior +2293; route alive (261 219 texels moved) |
| (d) | −3.8·10⁻⁶/pass (the medium gains) vs glide-tine control 1.15·10⁻⁵ |

Negative controls after the (c) change: inversion 204 texels / −99.7%,
fabrication +14.0% at 4.7·10⁻⁴/pass, over-stepped erosion ×12.6 — all red.

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 (ABI pin 0.10.0, normalizer/mapper, hostmpe) |
| §4.6 field, Metal | bitwise: max\|d\| 0, mean 0; corrupted fixture still refused |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ (tier 1e-2 / 1e-4) |
| `web_gate.mjs --scenes` | 12/12 including `torsion` |
| iOS shell | compiles against the grown header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| class exact declared | shader header, ABI comment, soak table, page draft |
| the ±A inversion golden holds to noise | markers return to 3·10⁻⁴ rad; the soak's (b) 5.45 texel drift is the resampler's (#21) |
| the episode emits deltas, never absolutes | 10-frame steps ≤ 0.038 rad; two strikes both run bounded |
| the scene runs | `torsion` in the sweep, 12/12 |
| the fixture is bitwise | Metal max\|d\| 0 |
| `sumi_version` bumped additively | 0.10.0 |
