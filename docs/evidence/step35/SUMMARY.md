# Evidence — Step 35: Phase 6 opening & the conservation gate

ROADMAP_5 Step 35; MEDIUM §2 (the class table); SPEC §4.1, §4.3(5);
DECISIONS_3 #33 (the four-part gate). Decisions: `_work/DECISIONS_5.md`
#1–#16 (#1–#12 the phase-opening resolutions the author confirmed in the
roadmap review; #13–#16 this step's). Machine: the author's Mac (Apple
silicon, Metal). **No core change** — the harness, a tool and the docs.

## What landed

* **`_work/DECISIONS_5.md`** opened: the medium is Anod (#1), four phases and
  version 2.0.0 (#2), the single ABI break carrying the probe state early
  (#3), the operator families (#4), the fingering CCs (#5), one preset
  serializer (#6), per-device presets offered (#7), replay with gestures and
  frame boundaries (#8), documentation timing (#9), fixed palette curve and
  the theremin flag (#10), `SUMI_LAYOUT_STRINGS` (#11), the phase invariant
  (#12).
* **The four-part conservation gate** in `desktop/src/dev_tools.cpp`
  (`--dev --soak <operator|all> [--soak-passes <n>]`, `--soak-negative`): the
  step-19 pinch soak generalised to every v1.0 pass — glide tine, pinch
  saddle, pinch crossed, wake doublet, wake Stokeslet, ripple bake,
  Lamb–Oseen swirl, exponential and Rankine vortex — each driven through its
  REAL ctl or gesture route on the fixed 512² scripted clock with one stream
  shape (a 0.5 Hz wobble at 120 Hz, at most one pass per frame). Verdicts:
  (a) the declared class with its det J = 1 line; (b) reversibility — exact
  operators: 500 strong ±k pairs hold mass within ±5% (the coarse guard) and
  return the pre-image within 4 texels (the sharp one); sub-stepped fields:
  one a/4 sub-step keeps the
  pre-image Jacobian > 0.5 outside the swept capsule with mean within 2·10⁻³
  of 1, the pair drift printed beside it; (c) the stream never grows mass
  past 0.5% and must have moved the field (a dead route cannot pass); (d)
  per-pass erosion ≤ 2× the glide-tine control from a fresh
  copy of the same scene, gated in the steady window only (#13). Each run
  ends with a `SUMMARY` line.
* **`tools/soak_report.py`**: the logs' `[soak]` lines as one Markdown table
  (the baselines below).
* **Red before green** (`--soak-negative`, #14): a non-inverting pinch pair,
  an edge-clamp tine fabrication, an over-stepped wake stream — each must
  trip its part, and the run asserts that it does.
* Docs: the lab-bench section of `docs/BUILD.md`, the `tools/README.md` row.

## Baselines — every v1.0 pass, 6000-pass streams, Metal (`soak_all.log`)

| Operator | Class (a) | (b) reversibility: ±k pairs mass lo / hi, pre-image dev (sub-stepped: one-step Jacobian) | (c) max growth | (d) erosion / pass vs glide-tine control | Verdicts b · c · d |
|---|---|---|---|---|---|
| `tine` | exact | -0.07% / +0.76%, 1.60 texel | +0.00% | 1.31e-05 vs 1.15e-05 (×1.14) | PASS · PASS · PASS |
| `pinch-saddle` | exact | -0.43% / +0.15%, 2.31 texel | +0.00% | 1.77e-05 vs 1.15e-05 (×1.53) | PASS · PASS · PASS |
| `pinch-cross` | exact | +0.00% / +25.25%, 92.08 texel | +0.00% | 9.38e-07 vs 1.15e-05 (×0.08) | FAIL · PASS · PASS |
| `wake-doublet` | sub-stepped | -4.71% / +0.00%, 2.20 texel (pairs informational); one sub-step det min 0.734, mean 0.99972 | +0.00% | 6.68e-06 vs 1.15e-05 (×0.58) | PASS · PASS · PASS |
| `wake-stokeslet` | sub-stepped | -2.97% / +0.98%, 3.51 texel (pairs informational); one sub-step det min 0.766, mean 1.00004 | +0.00% | 8.78e-06 vs 1.15e-05 (×0.76) | PASS · PASS · PASS |
| `ripple-bake` | exact | +0.00% / +2.69%, 2.63 texel | +0.00% | 5.89e-06 vs 1.15e-05 (×0.51) | PASS · PASS · PASS |
| `swirl` | exact | -0.22% / +0.31%, 1.45 texel | +0.00% | 9.77e-06 vs 1.15e-05 (×0.85) | PASS · PASS · PASS |
| `vortex-exp` | exact | -0.41% / +0.05%, 1.80 texel | +0.00% | 3.97e-07 vs 1.15e-05 (×0.03) | PASS · PASS · PASS |
| `vortex-rankine` | exact | -0.47% / +0.00%, 2.74 texel | +0.00% | 3.16e-06 vs 1.15e-05 (×0.27) | PASS · PASS · PASS |

`soak_all.log`: 26/27 checks — the one red is the crossed pinch's (b), a finding about the v1 operator, not the gate (#17). Both full runs printed identical numbers: the gate is deterministic on Metal.

Reading the table: the (b) numbers are the medium's floor for strong pairs
(#15 — the reference tine gains +0.76%, the whole-canvas ripple +2.69%, the
saddle pinch, swirl and vortices stay within ±0.5%; every exact pair returns
the pre-image to 1.5–2.7 texels, against 204 for a broken one), the (c)
column is the ingress rule holding on every route with the route proven
alive, and the (d) column is what every new operator's stream is compared
against (the glide tine's own steady fade is 1.15·10⁻⁵ per pass; the saddle
pinch reproduces #33's 1.5×). **The crossed pinch's (b) is red** (#17): the
variant composes two perpendicular tines and its sign flip keeps their order,
so ±k is not an inverse — 92 texels adrift and +25% fabricated through its
clamp-legacy tines under torture pairs, while its gesture-rate stream is
clean. A one-line core fix exists (reverse the order for k < 0); the author
weighs it against the look (#69 calls the untraced release "marbling").

## Negative controls (`negative.log`)

| Negative control | Outcome | Detail |
|---|---|---|
| `negative-inversion` | red as required | RED as required: offset (+k,-k) centres leave the pre-image 203.82 texel away (> 4.0; mass -99.65%) |
| `negative-fabrication` | red as required | RED as required: edge-clamp tines grew ink mass +13.99% over 300 passes (> 0.5%) |
| `negative-erosion` | red as required | RED as required: a 15-sub-step-per-frame wake stream erodes 1.45e-04/pass vs glide-tine 1.15e-05/pass (x12.6 > 2) |

`negative.log`: 3/3 — each part driven to fail, and the run asserts the failure.

## What was found on the way (recorded, not chased)

* The resampled medium is not mass-neutral in either direction: strong
  ±pairs GAIN at the ink/water boundaries (interior, not the edge band), a
  freshly laid drop gains under its first few hundred glide passes, and the
  steady glide regime fades (#15). #33's control ran on an aged field and
  never saw the early gain, which is why (d) is gated only at the 6000-pass
  window.
* The doublet's slip surface reads as det ≈ −1.4 to a finite difference
  across it and 0.73 outside — the flick test's exclusion, reused (#13).
* Bend-driven ripple bake drifts φ by design (#36), so the ripple's exactness
  is measured through its CC route (#16).

## DONE

| Criterion | Status |
|---|---|
| One command soaks any operator and prints the four verdicts | `midi-sink --dev --soak <op>`; `--soak all` runs the nine (`soak_all.log`) |
| The baseline table is in the evidence | above, from `tools/soak_report.py` |
| The red run is in the evidence | `negative.log`: 3/3 red as required (inversion 204 texels / −99.7%, fabrication +14.0% in 300 passes, over-stepped erosion ×12.6) |
| `DECISIONS_5.md` exists with the opening entries | #1–#12 (+ #13–#16 from this step) |
| The fixture is bitwise (nothing in the core changed) | `git diff --stat core/` empty; `tools/field_gate.py` on Metal: GREEN, max|d| 0, mean|d| 0 over 262 144 texels; the corrupted-fixture negative control still fails |
| Headless suites | `ctest`: 4/4 passed |
