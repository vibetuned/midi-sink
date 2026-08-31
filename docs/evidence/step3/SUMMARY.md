# Step 3 DONE evidence — Jaffer deformations via mouse

Date: 2026-08-31. Same rig (Apple Silicon, macOS 25.6.0). Window 1280×720
logical / 2560×1440 px — deliberately non-square. All runs with
`METAL_DEVICE_WRAPPER_TYPE=1`; zero validation output in every log.

New in this step: drop/tine/vortex fragment passes (§4.3 math verbatim, in
aspect-corrected space), continuous ink-phase encoding (§4.2), ring-banding
composite, mouse wiring (left click drop / left drag tine / right drag
vortex), app renamed to midi-sink (DECISIONS.md #16).

| DONE check | Result | Evidence |
|---|---|---|
| 500+ drops, ring boundaries pixel-sharp, no progressive blur | **PASS** — 10-drop and 500-drop captures + 1:1 600px center crops: the innermost boundary after 500 drops is exactly as crisp as after 10; no speckle at any seam | `drops_010.png`, `drops_500.png`, `*_centercrop.png`, `run_drops10/500.log` |
| Rings perfectly circular on a non-square window | **PASS** — all captures are 16:9; rings are visually circular (drop math runs in aspect-corrected space, using the field texture's own aspect) | `drops_010.png` |
| Tine dragged through concentric rings → classic marbled chevron | **PASS** — 40 drag segments produce the classic nested chevron/heart wake; composition verified exact (40 × z/40 == 1 × z) | `tine_chevron.png`, `run_chevron.log` |
| Vortex twists rings into spirals that stay sharp | **PASS** — off-center vortex (see DECISIONS.md #20) swirls the rings; edges stay hard | `vortex_spiral.png`, `run_vortex.log` |

Regressions: `ctest` C11 ABI test passes; resize during drop rendering
recreates targets cleanly (`run_resize.log`, no validation output); `leaks
--atExit` unchanged OS-XPC baseline (285 leaks / 18 560 B, zero core frames);
~99 fps in all runs.

**Bug found & fixed by this step's math:** every offscreen pass was
vertically flipping the field (fullscreen-triangle interpolant y vs Metal
texture row order), so consecutive deformations cancelled — masked until now
by flip-parity-even pass counts. All passes now work in a single y-down
texture-space convention emitted by the vertex shader (DECISIONS.md #17).
Also switched the ink phase to the RGBA16F-safe parity encoding after
observing seam speckle at high drop counts (DECISIONS.md #18).
