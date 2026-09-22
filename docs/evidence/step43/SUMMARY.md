# Evidence — Step 43: Palettes, substrate, presets & prints (in progress)

ROADMAP_5 Step 43; QOL §1 (palettes) landed first — this file grows as §2
(substrate), §3 (presets) and §4 (prints) land. Decisions:
`_work/DECISIONS_5.md` #63–#65. Machine: the author's Mac (Apple silicon,
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
1 `706461c151113da7`, 2 `a55dceb448a1eece`, 1 @ morph `b69689f2d0a5d22d`.

`test_palette_presets_and_ring` (headless, in `ctest`): the ring's pairs and
blends for every active id, twelve presets ascending and in range.

`preset_tests` (§3, headless C11, in `ctest`, 30/30): the round trip byte for
byte; a newer file's unknown keys ignored and its longer / shorter arrays
handled; malformed, truncated, non-object and empty inputs refused with the
target intact; the size contract; escapes.

## Gates (after §1, §2 and §3)

| Gate | Result |
|---|---|
| `ctest` | 5/5 (ABI: the POD's size 172 unchanged, the library; `preset_tests`) |
| composite, Metal (Sumi) | bitwise vs the 1.0.0 fixture: max diff 0 |
| `--anod-test` | 9/9 (the Anod palettes through the one path) |
| §4.6 field, web tier / `web_gate.mjs --scenes` | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹; 17/17 scenes |
| iOS shell | compiles against the header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE (step 43, running)

| Criterion | Status |
|---|---|
| the built-in palettes are bitwise through the new path | 8/8 hashes, the composite gate |
| a preset round-trips in ctest and the serializer has its own headless suite | `preset_tests` 30/30 |
| the ledger re-exports a dip at 4k | §4, pending |
| the schema is documented for the shells | `presets/SCHEMA.md` |
