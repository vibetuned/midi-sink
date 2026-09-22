# Evidence — Step 37: Chladni cellular flow

ROADMAP_5 Step 37; MEDIUM §2.2; SPEC §4.3. Decisions: `_work/DECISIONS_5.md`
#23–#27. Machine: the author's Mac (Apple silicon, Metal); the wasm rebuilt
and gated here; the iOS shell compile-checked against the final header. Core
reopened by the roadmap; the §4.6 fixture untouched and bitwise. **The author
redirected the design three times in review** (#23): from the roadmap's
separable kick-drift with an interval ratio, through cells as nodes and cells
as still islands, to the Taylor–Green cellular flow on the layout's cells —
bake only, steadily driven — once the physics was settled: an area-preserving
map cannot gather ink (Liouville), it can stretch it along the separatrices,
and the Taylor–Green separatrices are the plate mode's nodal lines. The
author's derivation of the ponderomotive potential is `chladni.md`.

## What landed

* **The pass** `SUMI_DEFORM_CHLADNI` (`deform.glsl chladni_fs`): one stage of
  the Taylor–Green splitting ψ = Ψ·cos u·cos v = ½Ψ[cos(u−v) + cos(u+v)] — a
  pure shear along (k_y, k_x) for the first wave and (−k_y, k_x) for the
  second, each exact; a step of the flow is the two stages in order, the
  exact inverse both negated in reversed order (`sumi_chladni_emit_step`,
  shared by the gesture and the mapper); the §3.4 ingress rule. Class exact,
  declared in the shader header, the ABI comment, the soak table, the page
  draft.
* **Bake only, steady** (#24): while `SUMI_CTL_CHLADNI_A` is up the mapper
  emits one step per frame with Ψ = rate/(k_x·k_y), `SUMI_CHLADNI_RATE` 1.5
  rad/s of cell rotation at ctl 1 — the vortex's pattern, never an absolute;
  `_B` balances the two diagonal waves. No live path (the composite's block
  removed), no quadrature; `chladni_bake` and `chladni_channel` gone.
* **The layout is the plate** (#25): `sumi_layout_cell_lattice` (half pitch
  along x for the Jankó and the piano grid); x converted through the aspect
  normalize() last saw; `chladni_cell` (0.5..1.5) scales the pitch about the
  layout's cells; the layouts without drawn cells take the largest
  imaginary cell that does not touch a neighbour's — the fifths' octave-ring
  spacing (0.032), a roll's semitone (0.0069). `sumi_add_chladni(psi,
  balance, s_x, x_0, s_y, y_0)` the gesture;
  `sumi_debug_chladni_lattice` for tests. `sumi_version` **0.11.0**. A
  Faraday half-cell shift was built, measured (the fixed points swap type
  exactly) and removed on closing the step at the author's request — the
  real inverse effect is boundary-layer acoustic streaming, deferred as the
  author's idea for another day.
* **The negative**, headless: `test_chladni_kick_drift_order` — the
  simultaneous form's |1 − det J| reaches 0.38 and its sign flip does not
  invert; the kick-drift inverts to 10⁻⁹.
* Desktop: "Chladni" settings section (stir and balance as CC sliders on the
  106/107 routes, the cell-size slider), INI keys, stock map v5 (+106/107;
  v4 migrates), names, `--chladni-test`, `chladni` in the conservation gate.
* Web: scene `chladni` (stir, balance, cell size, frames, pace), export
  `_sumi_add_chladni`, one parameter id, the gate's sweep; `check.mjs`
  untouched on purpose.
* Page draft: `chladni.mdx` (this folder), for `site/…/operators/` at step 63.
* Also this step, from the author: `tools/coremidi_recover.sh` (a wedged
  CoreMIDI setup store on macOS), listed in `tools/README.md`.

## Measurements (`chladni_test.log`, 7/7)

| Check | Result |
|---|---|
| A step then its inverse, Ψ 0.006 on a 3 × 2 lattice | the step moves the interior pre-image 16.8 texel; the pair leaves 0.284 (whole field 2.17 — the ingress bands) |
| Fixed points (chromatic grid, 150 stirred frames) | the 84 cell centres moved 0.00 texel, the 66 corners 0.03; the boundary midpoints 9.17 |
| Types | a ring round a cell centre rotates 0.33 rad (an eddy); round a corner 0.00 rad with stretch 0.38 (a saddle) |
| Cell size 1.5 | lattice pitch 0.1050 / 0.1714 = 1.5 × the probe's, the probe's centre cell still a lattice centre |
| 16:9 field (768 × 432) | centres 0.18 / corners 0.22 texel against midpoints 7.94; centres rotate 0.34 rad, corners 0.00 |
| Lattice = layout | pitch 0.0700 / 0.1143 = the probe's; the probe's cell centre 2·10⁻⁷ of a pitch off a lattice centre |
| Imaginary cells | the fifths 0.0320 / 0.0320 centred on (0.5, 0.5); a roll 0.00688 from the now-line |

## The gate (`soak_chladni.log`, 3/3; the chromatic grid's lattice)

| Part | Chladni |
|---|---|
| (a) | exact — two diagonal shears per step |
| (b) | 500 ±Ψ pairs: mass −0.20% / +1.08%, pre-image dev 5.04 texel (≤ 8; an oscillatory field, as the torsion's 5.45) |
| (c) | growth +5.08% over 6000 passes = 8.5·10⁻⁶/pass (≤ 5·10⁻⁵), edge band +425 / interior +4801; route alive (156 856 texels moved) |
| (d) | −8.5·10⁻⁶/pass (the medium gains) vs glide-tine control 1.15·10⁻⁵ |

Observation, recorded: a steady cellular flow stretches ink continuously, so
the medium's boundary gain (#15) is the largest measured so far — five
percent over the window, all interior, six times below the fabrication rate
bar. Chaotic advection is exactly the regime #15 predicted would show it.

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 (ABI pin 0.11.0; the kick-drift order test inside the normalizer suite) |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 13/13 including `chladni` |
| iOS shell | compiles against the final header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| exact declared | shader header, ABI, soak table, page draft |
| ±A inversion golden | 0.284 texel interior after a step and its inverse; the gate's (b) 5.04 over 500 pairs |
| the bake path passes the four-part gate (ingress rule applies) | 3/3; the pass carries the §3.4 ingress branch |
| the live path leaves the dip un-shimmered | superseded: no live path by the author's decision (#24) |
| scene `chladni` | ships with stir, balance, cell size and duration; the roadmap's ratio presets are superseded (#23) |
| fixture bitwise | Metal max\|d\| 0 |
