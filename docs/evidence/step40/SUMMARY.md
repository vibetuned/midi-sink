# Evidence — Step 40: Chirikov standard map, the boss gate

ROADMAP_5 Step 40; MEDIUM §2.5. Decisions: `_work/DECISIONS_5.md` #40–#44.
Machine: the author's Mac (Apple silicon, Metal); the wasm rebuilt and gated
here; the iOS shell compile-checked against the final header. The §4.6
fixture untouched and bitwise. **Author input pending:** the erosion budget
against the depth into chaos — the table below is the call's material; the
core's route ceiling is set at the value recorded in #42 and moves at the
author's word.

## What landed

* **The pass** `SUMI_DEFORM_CHIRIKOV` (`deform.glsl chirikov_fs`): one stage
  of the scaled standard map — the kick y₁ = y + A·sin(k(x−x_c)+φ), then the
  drift x₁ = x + ε·(y₁−y_c); K = A·k·ε the step's chaos parameter; the §3.4
  ingress rule. CLASS EXACT; the inverse undoes the drift first (#40).
* **The gesture** `sumi_add_chirikov(x, y, K, periods, ε, φ)`: one full step
  (K < 0 the exact inverse), |K| ≤ 2.
* **The route** `SUMI_CTL_CHIRIKOV_K` (19, COUNT 20), delta-driven: a throw
  of δ is one step at δ²·`chirikov_kmax`, capped per step at the core's
  ceiling `SUMI_CHIRIKOV_K_CEIL` with the remainder following; a negative δ
  the exact inverse step (#41). Centred where the vortex is. Parameters
  `chirikov_kmax` (default 1), `chirikov_periods` (2), `chirikov_eps` (0.5).
  `sumi_version` **0.14.0**.
* **Headless** (`normalizer_tests`): the map in double inverts to 10⁻¹⁶ with
  det J = 1 to 10⁻¹⁰ and the same-order flip is not the inverse; the step
  helper's ordering; the route: a one-frame throw at K_max 2 is one step at
  the ceiling then the remainder, at K_max 0.5 one step at 0.5 and one
  inverse step (drift first) on the way down.
* Desktop: `--chirikov-test` (4 checks), `chirikov` in the conservation
  gate, `--soak chirikov-sweep` (the table), the I key, a "Chirikov"
  settings section (K max, periods, drift, the throw as a CC slider on the
  new CC 109 route), INI keys, stock CC map v7 (v6 migrates), the name
  "Chirikov throw".
* Web: scene `chirikov` (K through Greene's threshold, periods, ε,
  iterations), export `_sumi_add_chirikov`, three parameter ids, the gate's
  sweep; `check.mjs` untouched on purpose (#22).
* Page draft: `chirikov.mdx` (this folder), for step 63.

## Measurements (`chirikov_test.log`, 4/4)

| Check | Result |
|---|---|
| One step at K = 0.5 (periods 2, ε 0.5: a 41-texel kick) then its exact inverse | the central band's pre-image moves 32.9 texel; the pair leaves 0.042 |
| The pass vs the closed form | 25 texels match x = P.x − ε(P.y − y_c), y = P.y − A sin(k(x − x_c) + φ) to 0.123 texel |
| The KAM transition, 60 steps | rings on the hyperbolic point keep 53% of their mass at K = 0.5 and 16% at K = 1.5; rings on the elliptic island keep 101% at K = 1.5 (#43) |
| The delta route | a throw moves the band 42.6 texel through the smoother's gentle steps; the control home retraces to 2.86 |

## The boss gate — the erosion sweep (`soak_chirikov_sweep.log`)

One full step every other frame for 6000 frames per K (one pass a frame,
the tine control's density), the gate's scene, voice and window; (c) and
(d) as the four-part gate reads them; the visible ink kept and the
boundary length as the filamentation's witnesses.

Glide-tine control: 1.15·10⁻⁵/pass. **Whole-window (d) is RED at every K**:
45–60% of the ink is gone by frame 1000 at every K including 0.25 — the
drift's flush of the rotating orbits off a non-wrapping canvas (#43), the
operator's geometry, not resampling erosion. **After the flush** (frame 1000
as the base) the erosion of what stays is the gate's reading:

| K per step | flushed by 1000 | erosion/pass, whole | × tine | after the flush | × tine | visible ink kept | boundary × | (d) whole | (d) after |
|---|---|---|---|---|---|---|---|---|---|
| 0.25 | 58% | 1.04·10⁻⁴ | ×9.07 | 2.2·10⁻⁵ | ×1.88 | 67% | ×1.2 | RED | green |
| 0.5 | 47% | 8.33·10⁻⁵ | ×7.24 | 1.2·10⁻⁵ | ×1.02 | 96% | ×1.5 | RED | green |
| 0.75 | 42% | 7.60·10⁻⁵ | ×6.60 | 1.2·10⁻⁵ | ×1.00 | 109% | ×1.6 | RED | green |
| 0.9716 | 44% | 8.17·10⁻⁵ | ×7.10 | 1.7·10⁻⁵ | ×1.50 | 102% | ×1.8 | RED | green |
| 1.25 | 52% | 9.53·10⁻⁵ | ×8.29 | 2.0·10⁻⁵ | ×1.78 | 87% | ×1.6 | RED | green |
| 1.5 | 57% | 1.07·10⁻⁴ | ×9.33 | 3.4·10⁻⁵ | ×2.97 | 56% | ×1.3 | RED | RED |
| 2.0 | 69% | 1.24·10⁻⁴ | ×10.7 | 3.4·10⁻⁵ | ×2.94 | 29% | ×0.7 | RED | RED |

(c) is green at every K (mass only falls). **The ceiling** `SUMI_CHIRIKOV_K_CEIL`
= 1.25, the last K where (d) holds after the flush — above Greene's
threshold, so a hard throw reaches chaos; the gesture's hard limit 2.0. The
author's options are laid out in #42.

## The four-part gate through the route (`soak_chirikov.log`)

`--soak chirikov` (3/3), the operator through its delta route — the pairs a
full step at K = 0.5 and its exact inverse at the ink; the stream the CC 109
wobble, whose per-frame deltas are gentle steps (δ²·K_max, the pendulum
regime), the wheel home at the end retracing:

| Part | `chirikov` (exact) |
|---|---|
| (a) | exact — two shears, the kick then the drift; the inverse undoes the drift first |
| (b) | 500 pairs of a K = 0.5 step and its inverse: mass −0.11% / +2.66% (≤ 5%), pre-image dev 6.06 texel (≤ 8; a 41-texel kick's oscillatory field, the torsion's family) |
| (c) | growth +0.40% over 6000 passes (6.7·10⁻⁷/pass), edge +0 / interior +426; route alive (259 584 texels moved) |
| (d) | −6.8·10⁻⁷/pass (the medium gains) vs glide-tine control 1.15·10⁻⁵ (×−0.06 ≤ 2) |

The route's gentle regime erodes nothing measurable: the deltas of a
wobbling wheel are small steps of an integrable flow, and the kick-drift
pair is exact at any size. The chaos is reached only by throwing, and each
throw's step is capped at the ceiling above.

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 (ABI pin 0.14.0) |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 16/16 including `chirikov` |
| iOS shell | compiles against the final header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| exact declared | shader header, ABI comment, soak table, page draft; inverse 0.042 texel, headless 10⁻¹⁶ |
| the soak table in the evidence, the ceiling clamped and recorded | `soak_chirikov_sweep.log`, the table above; `SUMI_CHIRIKOV_K_CEIL` = 1.25 in `voice_mapper.h`, recorded in #42 with the author's options |
| the KAM transition visible in the scene | scene `chirikov`: rings at the hyperbolic point and on the island, K on a slider through 0.9716; the harness's mass-kept witness 53% → 16% across the threshold, the island 101% |
| fixture bitwise | Metal max\|d\| 0 |
| page draft | `chirikov.mdx` |

Author input requested (MEDIUM §2.5's `[ITERATE]`): the erosion budget
against the depth into chaos, with the table above in hand — the ceiling
is set where (d) still holds and moves at your word.
