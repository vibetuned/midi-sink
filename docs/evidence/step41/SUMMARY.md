# Evidence — Step 41: The ABI event, libsumi 1.0.0

ROADMAP_5 Step 41; MEDIUM §1, INSTRUMENT §1, QOL §1. Decisions:
`_work/DECISIONS_5.md` #45–#49. Machine: the author's Mac (Apple silicon,
Metal); the iOS shell compiled in-step against the 1.0.0 header; the wasm
rebuilt and gated; the Android JNI edited mechanically, to compile on the
Linux box as the first line of step 45. **The one break of the arc**;
behaviour unchanged by construction, and proved twice: the field gate for the
field, the new composite gate for the pixels.

## What landed (`sumi_version()` → 1.0.0)

* **`sumi_layout_probe(layout, params, aspect, state, x, y, out)`** — the
  `const sumi_layout_state_t*` of INSTRUMENT §1 (`buttons`, `slider`,
  `reserved[2]`; 16 bytes), NULL or zeros = stateless. Every layout shipping
  today ignores it (#45).
* **`sumi_cell_info_t.flags`** (`SUMI_CELL_CONTINUOUS` = bit 0, the
  theremin's; 0 today) — the `[ITERATE: sentinel vs flags]` closed as flags.
* **`sumi_params_t.medium`** (`SUMI_MEDIUM_SUMI` 0, `SUMI_MEDIUM_ANOD` 1),
  inert until the Anod composite (step 43); values above ANOD clamp to SUMI
  (#47).
* **`sumi_set_palette(inst, const sumi_palette_t*)`** and
  **`SUMI_PALETTE_CUSTOM`** (3): QOL §1's model as a POD — 2..8 linear-RGB
  stops along the ink-depth axis, the depth curve (γ, floor), the per-drop
  drift, the clear-water tone, four reserved words — validated on the way in
  and read by the composite through a branch that leaves the built-in path
  untouched (#46).
* **`sumi_layout_t` 8..12** named and RESERVED (TRUMPET, TROMBONE, WICKI,
  FRETS, THEREMIN); `sumi_set_params` clamps them to FIFTHS with a warning,
  the probe refuses them.
* **The migration note** in `sumi_core.h`, above `sumi_version`: five
  points, mechanical.
* **Call sites moved:** the desktop bench (3), the headless suite (25), the
  C11 ABI test (2 + the 1.0.0 checks: an explicit zero state answers as
  NULL, flags read 0, reserved layouts refused, enum values and POD sizes),
  the web shim, the iOS overlay (`nil`, 3), the Android JNI (`nullptr`, 2).
  `hostmpe` never probes. Web: a `medium` parameter id and the
  `_sumi_set_palette` export; desktop: `medium` persisted in the INI (the
  switch is step 43's UI).
* **The composite screenshot gate** (#48): `midi-sink --dev --composite-dump`
  writes the print of the §4.6 field script; `tools/composite_gate.py`
  compares it bitwise against `tests/fixtures/composite_512_metal.rgba` and
  proves red on a corrupted copy. The fixture was generated from the
  pre-break renderer before any header changed; two runs bitwise identical.
* `--palette-test` on the bench (2 checks).

## Measurements

| Check | Result |
|---|---|
| `sumi_version()` | 1.0.0 (`midi-sink 1.0.0-dirty … libsumi 1.0.0`) |
| Composite gate, 1.0.0 binary vs the pre-break fixture | max channel diff 0 over 1 048 576 samples; the negative control (a 32×32 block +64) rejected |
| Field gate, Metal | GREEN, fixture bitwise |
| `--palette-test` | a red-to-black palette changes 129 140 of 129 140 inked texels (mean \|ΔRGB\| 27.9) and 0 of 133 004 paper texels; palette 0 afterwards prints bitwise (0 texels differ); a degenerate POD (one stop, descending positions, NaN γ) is clamped and prints |
| C11 ABI test | 1.0.0 pin, 20 symbols, the state/flags/reserved/POD checks |

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| composite, Metal | bitwise: max diff 0 (`gates.log`) |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 16/16 |
| iOS shell | compiles against the 1.0.0 header (`xcodebuild … BUILD SUCCEEDED`) |
| Android JNI | edited (`nullptr` at both probes); compiles at step 45's first line, on device |

## DONE

| Criterion | Status |
|---|---|
| `sumi_version()` reads 1.0.0 | yes |
| all desktop suites green | ctest 4/4, `--palette-test` 2/2, both gates green |
| the iOS project compiles | BUILD SUCCEEDED |
| the web builds and `web_gate.mjs` passes | PASS, 16/16 |
| the fixture and the composite screenshot bitwise on Metal | both max diff 0 |
| `DECISIONS_5` records the break and why the probe state ships early | #45 |
| step 45's first line verifies on device | pending, by design (the Linux box) |

Not done here, on purpose (#46): QOL's unification of the built-in palettes
into the same model — it would touch the built-in arithmetic the composite
gate now holds to bitwise; it belongs with the palette editor, where the
presets are written out in the model under that gate. The roadmap's "bitwise
as 0.9.0" is read as the pre-break renderer, 0.14.0 (#48).
