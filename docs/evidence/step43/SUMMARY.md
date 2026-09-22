# Evidence — Step 43: Palettes, substrate, presets & prints

ROADMAP_5 Step 43; QOL §1 (palettes), §2 (substrate), §3 (presets) and §4
(prints), landed in that order, then the author's glow (a bloom after the
Anod composite), the binding and strike revisions and the stir's ownership fix. Decisions: `_work/DECISIONS_5.md` #63–#72. Machine: the author's Mac (Apple silicon,
Metal); the wasm rebuilt and gated; the iOS shell compiled against the header.

## §1 Palettes — what landed (`sumi_version()` 1.1.0, additive)

* **One palette path** (#63): the composite's per-id colour tables are gone;
  every palette is a `sumi_palette_t` and `pal_ink_at` is the only colour
  path, for both media. The model's per-drop drift is now a blend toward a
  new `accent_rgb` (from the reserved words; the POD's size unchanged), which
  is what makes the built-ins expressible: a two-stop palette of one colour
  drifting 0.45 toward its accent. The morph ring is resolved on the CPU
  (`sumi_palette_ring`) with the shader's own float operations; the engine
  feeds two slots and the blend.
* **The preset library** (#64): `sumi_palette_preset_count` /
  `sumi_palette_preset`, six a medium — the three built-ins (the 0.x /
  step-42 literals verbatim) and three colour-blind-considerate proposals
  (Cobalt & amber, Viridis, Cividis). `sumi_get_palette` returns the custom
  slot as stored. The ring includes the custom slot only while it is active
  (custom → 0 → 1 → 2).
* **The editor** (#65): the desktop's "Palette" section — active slot,
  library with "Load into custom", the stop editor with sRGB pickers over the
  linear model, curve, floor, drift and its target, clear water; live to the
  core; persisted in the INI. Web: id 3 accepted, the library exported.
* QOL §1's two `[ITERATE]`s resolved: the curve fixed per medium (no
  advanced fold in 2.0); the drift a palette field with its target.

## §2 Substrate — what landed (#66)

* Four additive params, composite-side: `paper_tint[3]` (the 0.x cream by
  default), `fiber_scale` (0.5..2, ×1 = 0.x), `anod_dark` (0.5 = the step-42
  glass), `anod_grain` (0.5 = the step-42 speckle, its own knob now — no
  longer the paper roughness's). Bitwise at the defaults: the composite gate
  max diff 0 and the eight palette hashes unchanged after the change.
* Desktop "Substrate" section: Sumi tint presets (Cream / White / Toned) and
  picker, paper presets (Smooth / Washi / Coarse) over roughness and fiber
  scale; Anod glass darkness and phosphor grain. INI keys; web params 34..39.
* QOL §2's `[ITERATE: fiber angle drift?]` resolved as not exposed.

## §3 Presets — what landed (#67)

* `presets/` — `sumi_presets`, a pure-C11 static library beside hostmpe:
  `sumi_preset_t`, `sumi_preset_init` / `_write` / `_read`, and
  `sumi_preset_apply` in its own translation unit. No dependency but libc.
* The schema (`presets/SCHEMA.md`): schema 1, stamped with `sumi_version`;
  unknown keys ignored, missing keys defaulted, the core clamps on apply.
* The desktop: `<config>/last_session.json` written on every save and read
  before the INI; named presets in `<config>/presets/`; a "Presets" section
  with Load / Save as / Delete / Export / Import.
* QOL §3's `[ITERATE]`s: the schema rule as above; preset-next from the strip
  not in 2.0.

## §4 Prints — what landed (#68)

* `sumi_read_field` (public), `sumi_export_begin` / `sumi_export_poll`,
  `SUMI_EXPORT_MAX_DIM` 8192 and `SUMI_EXPORT_ANOD_ALPHA`: the composite over
  a field — kept or live — at any size, asynchronous on the one readback slot,
  with the params as they stand.
* The composite's `alpha_out` (0 on every shipped path): an Anod export as
  straight colour over alpha, the glass transparent.
* The desktop's print ledger: every dip keeps its field, look and thumbnail;
  re-export at Screen / 2K / 4K / 8K wide, Anod over alpha; "Save last print"
  from the ledger; eight entries or 384 MB.
* QOL §4's `[ITERATE]`s: the cap 8192 a side; TIFF-16 deferred pending demand.
* Evidence image `print_anod_alpha.png`: the script's discharge over alpha.

## The glow — what landed (#69)

* A screen-space bloom after the Anod composite (`bloom.glsl`): the linear
  emission at half resolution, thresholded with a soft knee, blurred down
  `anod_bloom_levels` octaves and back up (the 13-tap / tent chain), added
  back times `anod_bloom`, then a per-channel shoulder toward white. Prints,
  exports and the alpha export bloom the same. Two additive params; at 0 the
  composite is bitwise as before.
* The author's Anod defaults (2026-09-23): glass darkness 1, grain 0.5, glow
  scale 0.20, bloom 0.75 over 3 octaves. The four Anod palette hashes were
  recaptured at them; the Sumi four and the composite gate are unchanged.
* Renders `anod_glow_default_blue.png` and `anod_glow_default_orange.png`:
  the §4.6 script under the defaults.

## The binding — what landed (#70)

* The author's revision of MEDIUM §4's Anod column after playing: the
  per-note bend plays the Chladni stir (`bend_mode` 4, the Anod default now
  — its distance the rate, its sign the sense, so the banked rotation is
  signed and a vibrato stirs back and forth); the poly-pressure dimension
  plays the torsion's and the spark's wavenumbers from mid-range up, each
  unless the bend, the slide or a CC owns it. Additive: `bend_mode` 4, the
  desktop combo's "Chladni stir", the clamps at 4.
* `test_medium_binding_tables` (headless, in `ctest`) revised: Anod's bend
  → stir 1.00, 13 cells passes, no tine, torsion k untouched; poly pressure →
  torsion k 0.89, spark k the slide's 0.945, no Chladni pass, no swirl; the
  overridden table → both wavenumbers 0.89; Sumi as before.
* Flagged for the spec (the author's file): MEDIUM §4's "swirl — Chladni
  amplitude" and "per-note bend — torsion k / spark k" rows read the other
  way round now; `docs/evidence/step42/binding_table.md` carries the
  revised rows under the shipped table.

## The strike — what landed (#71)

* The Anod strike floods the canvas (the author): rendered side by side with
  the bench's new `--anod-strike-render <dir>` (six velocity-100 strikes on
  the circle of fifths at 1024²), the flood is the Sumi-sized drop, the
  shear and the burst all on a radius of 0.087 — not the burst alone. Ships:
  `anod_drop` (0.1..1, default 0.33), the strike's charge as a fraction of
  the Sumi drop, with the spark shear kept on the full Sumi radius, so the
  small charge is torn into long streamers; the burst left the Anod strike
  (it stays a gesture with its test and soak; `burst_order_by_class` stays
  in the ABI, unused by the strike). Desktop "Strike charge", INI, web id 42,
  the preset table (which also gained the glow's two keys, left out at #69).
* `anod_strike_before.png` (as shipped) / `anod_strike_after.png` (default).
* `test_medium_binding_tables`: the Anod strike's drop radius = anod_drop ×
  the Sumi radius, no burst piece, the shear's first step in the same frame.

## The stir's ownership — what landed (#72)

* The desktop's default CC map routes CC 106 / 104 / 108 to the stir and the
  wavenumbers, and #70's routes deferred to any mapped CC — the stir and the
  pressure routes were dead on the desktop (the headless test's mapper has
  no such handles). Now last writer wins on those dims; the poly-pressure
  route holds the wavenumbers while pressed and gives them back at release;
  the stir is stilled when the last voice lifts under mode 4 and on a flip
  away from it. The binding test maps CC 106/104 as the desktop does and
  covers the lift-while-bent and the flip.
* `anod_stir_alone.png`: a second of full-rate stir on a fresh Anod sheet —
  the whole circle-of-fifths lattice (128 discs, r 0.016) lights.
  `anod_strikes_bend.png`: the same stir under six bent strikes does not
  read, the spark shears having lit the grid everywhere — flagged in #72.

## §1 Measurements

`--palette-test` (`palette_test.log`, 5/5):

| Check | Result |
|---|---|
| a custom palette recolours the ink and only the ink | 129 140 of 129 140 inked texels changed, 0 of 133 004 paper texels; palette 0 prints bitwise after |
| a degenerate palette | clamped, prints |
| the built-ins through the one path | 8 prints — Sumi 0/1/2 and Anod 0/1/2 at rest, Sumi 0 and Anod 1 under morph 38/127 — hash as the legacy tables did (FNV-1a 64), bitwise |
| the preset library | 12 presets round-trip byte-equal through `sumi_set_palette` / `sumi_get_palette`; the built-ins carry the legacy literals |
| the substrate knobs (§2) | sheet mean 236.1 cream → 245.6 white tint; fibers ×2 mean 236.1, spread 10; Anod glass 27.5 → 0.0 at darkness 1; speckle spread 4 → 0 at grain 0 |

Legacy hashes (captured 2026-09-22 before the change, the expected table in
`dev_tools.cpp`): Sumi 0 `d7cc418955ac2e0e`, 1 `1ad837f3aa0a7324`, 2
`828d93044522a5af`, 0 @ morph `63d2e6377524170a`; Anod 0 `2e7d4887ade85c9c`,
1 `706461c151113da7`, 2 `a55dceb448a1eece`, 1 @ morph `b69689f2d0a5d22d` —
the Anod four recaptured under the author's glow defaults (#69):
`dc582051c8697b02`, `38799f2d9596d4d8`, `e617110f48b3b5f7`, `fb3f669b2d234944`.

`test_palette_presets_and_ring` (headless, in `ctest`): the ring's pairs and
blends for every active id, twelve presets ascending and in range.

`--print-test` (§4, `print_test.log`, 5/5):

| Check | Result |
|---|---|
| the export at the field's size is the dip's print | 0 of 1 048 576 bytes differ |
| the same field at 4k | 4096×4096; an 8×8 box average within 0.88 counts of the 512 print |
| the ledger's premise | a field kept before the dip re-exports after it bitwise, at 512 and at 4k |
| Anod over alpha | resting glass alpha 0 (768/768), every charged texel lit (41 169/41 169); the plain export opaque |
| the cap and the one readback | 9000 wide, zero height and a second export in flight refused |

`preset_tests` (§3, headless C11, in `ctest`, 30/30): the round trip byte for
byte; a newer file's unknown keys ignored and its longer / shorter arrays
handled; malformed, truncated, non-object and empty inputs refused with the
target intact; the size contract; escapes.

## Gates (after §1–§4, the glow, the binding, the strike and the stir's ownership — last run 2026-09-23)

| Gate | Result |
|---|---|
| `ctest` | 5/5 (ABI: the POD's size 172 unchanged, the library; `preset_tests` with the glow and strike keys; the binding-table test with the desktop's CC map, the lift-while-bent and the flip) |
| composite, Metal (Sumi) | bitwise vs the 1.0.0 fixture: max diff 0; the negative control red |
| §4.6 field, Metal | bitwise vs `field_512_metal.bin` (max 0, mean 0) |
| `--anod-test` / `--chladni-test` / `--palette-test` / `--print-test` | 9/9, 7/7, 5/5, 5/5 |
| `--burst-test` / `--spark-test` / `--torsion-test` / `--chirikov-test` | 9/9, 5/5, 8/8, 4/4 (the burst and the spark as gestures, untouched by the strike revision) |
| `--soak chladni` / `chladni-field` / `spark` / `burst` | 3/3 each |
| §4.6 field, web tier / `web_gate.mjs --scenes` | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹; 17/17 scenes |
| iOS shell | compiles against the header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE (step 43)

| Criterion | Status |
|---|---|
| the built-in palettes are bitwise through the new path | 8/8 hashes, the composite gate |
| a preset round-trips in ctest and the serializer has its own headless suite | `preset_tests` 30/30 |
| the ledger re-exports a dip at 4k | the prints test's third check: a kept field at 4096², bitwise |
| the schema is documented for the shells | `presets/SCHEMA.md` |
