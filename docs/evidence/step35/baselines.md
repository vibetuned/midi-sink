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

| Negative control | Outcome | Detail |
|---|---|---|
| `negative-inversion` | red as required | RED as required: offset (+k,-k) centres leave the pre-image 203.82 texel away (> 4.0; mass -99.65%) |
| `negative-fabrication` | red as required | RED as required: edge-clamp tines grew ink mass +13.99% over 300 passes (> 0.5%) |
| `negative-erosion` | red as required | RED as required: a 15-sub-step-per-frame wake stream erodes 1.45e-04/pass vs glide-tine 1.15e-05/pass (x12.6 > 2) |
