# Evidence — before Step 43: the Chladni cells

A debugging iteration on the Chladni operator between steps 42 and 43, at the
author's request, on the author's 2560×1440 window. Decisions:
`_work/DECISIONS_5.md` #59–#62 (and #55's version list). MEDIUM §2.2 is the
spec section; it describes a kick-drift lattice and now disagrees with what
shipped — flagged in #60, the spec is the author's to revise. This folder
supersedes `step37/chladni.mdx` as the page draft. Machine: the author's Mac
(Apple silicon, Metal); the wasm rebuilt and gated; the iOS shell compiled
against the header with `chladni_mode`.

## What landed (`sumi_version()` still 1.1.0, additive: `chladni_mode`)

* **The plate guide** (#59): the bench's N key draws the layout's display
  cells over the print — the circles the shells draw for the keys, from the
  same cell list the operator uses; dev-only, the composite bitwise with it
  off. It found the operator's lattice to be a second derivation of the
  layout, a grid: exact on the chromatic grid, doubled on the Jankó, a tenth
  of a row off on the piano grid, invented on the fifths and the rolls,
  which draw no keys at all.
* **The cells are the eddies** (#60): no lattice. `sumi_layout_cells`
  (layouts.cpp) is the one source — the probe's circles on the three key
  layouts, the largest circle at each note on the fifths (128, half a ring)
  and the rolls (128, half a semitone). One pass (`SUMI_DEFORM_CELLS`) turns
  every disc about its centre as a RING, θ·(4ρ²(1 − ρ²))² — the core and
  the water between the discs rest, det J = 1 inside every disc: class
  EXACT. The renderer holds an index map (RGBA16F at the field's
  resolution) of which discs own a texel. Balance sets the odd cells' sense
  on the layout's own checkerboard; cell size scales the disc, capped at 1
  in this mode. The gesture `sumi_add_chladni` keeps its two-wave lattice
  (generalised to any Bravais basis); the ABI is unchanged for it.
* **The emission floor** (#61): the stir banks its rotation and emits a pass
  only when it carries two half-float quanta of displacement on the smallest
  disc (`SUMI_CELLS_MIN_EMIT`); without it a fresh sheet did not turn at all.
  Clocks at 1440: every frame on the chromatic grid, ~50 ms on the Jankó,
  ~60 ms on the fifths.
* **`chladni_mode`** (#62): `SUMI_CHLADNI_FIELD`, the author's "inverse
  Chladni" — the rings summed into one divergence-free displacement field,
  sub-stepped, so the discs may grow to 1.5 of the key and overlap and the
  water between the keys is stirred. Off by default. Desktop "Mode" combo,
  INI key, web param 33; the soak op `chladni-field`.
* Tried and removed on the way (#60): a lattice fitted to the probe's cells
  (Cartesian, oblique for the Jankó) and a polar product flow for the
  fifths.
* Desktop: the Chladni section's help and "Mode"; `--chladni-test`
  rewritten to the new claims; the soak table's `chladni` row and the new
  `chladni-field` row; `sumi_debug_cells`, `sumi_debug_add_cells_pass`,
  `sumi_debug_set_chladni_overlay`. Tests: the binding-table test counts
  the cells pass as the stir.
* Page draft `chladni.mdx` (supersedes step 37's).

## Measurements

`--chladni-test` (`chladni_test.log`, 7/7; chromatic grid, 512² unless said):

| Check | Result |
|---|---|
| the gesture's step and inverse | moved 16.8 texel, the pair leaves 0.279 |
| fixed points after 150 stirred frames | cores (ρ 0.15) 0.00 texel, corners 0.00, the rings (ρ 0.745) 15.95 |
| types | a ring at ρ 0.7 round a centre rotates 1.45 rad, stretches 0.09; round a corner 0.00 / 0.00 |
| the eddies are the layout's cells | 84 discs = the probe's 84 keys, same centres, radius 0.0350 |
| cell size | 0.5 → radius 0.0175; 1.5 → 0.0350 (the key is the ceiling in the exact mode) |
| on a 16:9 field | cores 0.19, corners 0.19, rings 5.44; rings rotate 1.07 rad |
| imaginary cells | fifths 128 discs r 0.0160 between r = 0.10 and 0.42; a roll 128 discs r 0.00344 on x = 0.12 |

The four-part gate:

| Operator | class | (b) | (c) fabrication | (d) erosion |
|---|---|---|---|---|
| `chladni` (`soak_chladni.log`, 3/3) | exact | 500 pairs: mass −0.22 %/+1.21 %, pre-image dev 5.16 texel | +0.00 % over 6000 passes | 1.56·10⁻⁷/pass, ×0.01 the control |
| `chladni-field` (`soak_chladni_field.log`, 3/3) | sub-stepped | one 0.025-rad pass: det min 0.855, mean 1.00000; pairs −0.42 %/+0.17 %, dev 0.75 (informational) | 2.7·10⁻⁶/pass (+1.64 %) | −2.73·10⁻⁶/pass (grows), ×−0.24 |

The floor (#61), the first disc build on a fresh sheet: a ring rotated 0.07
rad in 150 frames of 0.0125-rad passes; with the floor and the ring profile,
1.45 rad.

## Renders (512², the print of the canonical script, the guide where drawn)

| File | What |
|---|---|
| `chroma_plate.png` | the chromatic grid's 84 display cells over the script |
| `chroma_stir.png` | 180 stirred frames, exact discs: every key wound from its edge, cores still |
| `piano_plate.png` | the piano grid's two key families as the shells draw them |
| `janko_plate.png` | the Jankó's 252 cells, six rows of forty-two |
| `fifths_stir.png` | the fifths' 128 imaginary discs stirred |
| `chroma_field_stir.png` | the inverse Chladni at cell size 1.5: the discs overlap, the water between the keys stirred |

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| composite, Metal (Sumi) | bitwise vs the 1.0.0 fixture (the guide off) |
| `--soak chladni`, `--soak chladni-field` | 3/3 each (above) |
| §4.6 field, web tier / `web_gate.mjs --scenes` | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹; 17/17 scenes |
| iOS shell | compiles against the header (`xcodebuild … BUILD SUCCEEDED`) |

## Carried

* MEDIUM §2.2 vs #60: the spec's kick-drift lattice with interval-set
  wavenumbers is not what ships; the author's revision.
* A 32-bit coordinate field (the renderer's quality flag) is the only true
  remedy for the emission floor's clock on small cells (#61); it would break
  the §4.6 fixture unless gated — roadmap.
* The field mode runs over its class budget on the Jankó and the fifths
  (#62); the soak on the chromatic grid is what was gated.
* The rolls' five-texel discs make the stir meaningless there; "Off" is
  a layout matter for step 43's presets, if at all.
