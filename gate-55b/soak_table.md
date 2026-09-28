| Operator | Class (a) | (b) reversibility: ±k pairs mass lo / hi, pre-image dev (sub-stepped: one-step Jacobian) | (c) max growth | (d) erosion / pass vs glide-tine control | Verdicts b · c · d |
|---|---|---|---|---|---|
| `tine` | exact | -0.03% / +1.01%, 11.84 texel | +0.00% | 1.31e-05 vs 1.15e-05 (×1.14) | PASS · PASS · PASS |
| `pinch-saddle` | exact | -0.33% / +0.43%, 13.16 texel | +0.00% | 1.41e-05 vs 1.15e-05 (×1.22) | PASS · PASS · PASS |
| `pinch-cross` | exact | +0.00% / +26.40%, 73.10 texel | +0.00% | 9.62e-07 vs 1.15e-05 (×0.08) | FAIL · PASS · PASS |
| `wake-doublet` | sub-stepped | -4.28% / +0.00%, 12.97 texel (pairs informational); one sub-step det min 0.888, mean 0.99969 | +0.00% | 3.94e-06 vs 1.15e-05 (×0.34) | PASS · PASS · PASS |
| `wake-stokeslet` | sub-stepped | -2.53% / +1.09%, 14.18 texel (pairs informational); one sub-step det min 0.993, mean 0.99998 | +0.00% | 4.58e-06 vs 1.15e-05 (×0.40) | PASS · PASS · PASS |
| `ripple-bake` | exact | +0.00% / +2.69%, 8.11 texel | +0.00% | 5.89e-06 vs 1.15e-05 (×0.51) | PASS · PASS · PASS |
| `swirl` | exact | -0.11% / +0.58%, 12.07 texel | +0.00% | 8.19e-06 vs 1.15e-05 (×0.71) | PASS · PASS · PASS |
| `vortex-exp` | exact | -0.30% / +0.32%, 14.10 texel | +1.55% | -2.59e-06 vs 1.15e-05 (×-0.23) | PASS · PASS · PASS |
| `vortex-rankine` | exact | -0.37% / +0.06%, 15.91 texel | +0.35% | -5.71e-07 vs 1.15e-05 (×-0.05) | PASS · PASS · PASS |
| `torsion` | exact | -0.47% / +0.00%, 16.36 texel | +2.70% | -4.51e-06 vs 1.15e-05 (×-0.39) | PASS · PASS · PASS |
| `chladni` | exact | -0.10% / +1.92%, 21.98 texel | +0.00% | 1.32e-07 vs 1.15e-05 (×0.01) | PASS · PASS · PASS |
| `chladni-field` | sub-stepped | -0.37% / +0.26%, 6.63 texel (pairs informational); one sub-step det min 0.985, mean 1.00000 | +1.16% | -1.93e-06 vs 1.15e-05 (×-0.17) | PASS · PASS · PASS |
| `burst` | sub-stepped | -16.88% / +0.00%, 20.04 texel (pairs informational); one sub-step det min 0.976, mean 1.00000 | +0.00% | 9.18e-06 vs 1.15e-05 (×0.80) | PASS · PASS · PASS |
| `spark-shear` | exact | -0.25% / +0.37%, 20.21 texel | +0.00% | 9.95e-06 vs 1.15e-05 (×0.86) | PASS · PASS · PASS |
| `spark` | sub-stepped | -8.12% / +0.00%, 35.83 texel (pairs informational); one sub-step det min 0.976, mean 1.00000 | +0.19% | -3.32e-07 vs 1.15e-05 (×-0.03) | PASS · PASS · PASS |
| `chirikov` | exact | -0.11% / +2.66%, 17.97 texel | +0.40% | -6.72e-07 vs 1.15e-05 (×-0.06) | PASS · PASS · PASS |

| Negative control | Outcome | Detail |
|---|---|---|
| `negative-inversion` | red as required | RED as required: offset (+k,-k) centres leave the pre-image 198.47 texel away (> 32.0; mass -99.64%) |
| `negative-fabrication` | red as required | RED as required: edge-clamp tines grew ink mass +13.99% over 300 passes (> 0.5%; 4.7e-04/pass > 5e-05) |
| `negative-erosion` | red as required | RED as required: a 15-sub-step-per-frame wake stream erodes 6.73e-05/pass vs glide-tine 1.15e-05/pass (x5.8 > 2) |
