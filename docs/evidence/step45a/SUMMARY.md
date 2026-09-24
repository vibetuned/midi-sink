# Evidence — Step 45a: Phase 6 on the Linux desktop (GL)

Machine: the author's Linux box — Ubuntu 25.10, GNOME on Wayland (the UI walk
ran on the Xwayland session so XTest could drive it), NVIDIA RTX 5090,
driver 610.43, GL 4.1 core, GCC 15. Build from scratch at `libsumi 1.1.0`
(`midi-sink 1.0.0-26-g4da6121-dirty`, `version.txt`). Decisions:
`_work/DECISIONS_5.md` #76–#78. Two fixes landed, one of them a crash.

## Checklist

| # | Check | Result | Evidence |
|---|---|---|---|
| 1 | Build + ctest | **PASS after #76** — GCC stopped on a linkage mismatch Clang accepts (`destroy_export`, a `static` forward declaration outside the renderer's `extern "C"` block); one-line fix, no code change. ctest 5/5 | `ctest.log` |
| 2 | §4.6 field gate on GL | **PASS, unchanged** — mean 6.847619e-06, max 3.906e-03 (ink), exactly Part IV #44's numbers: Phase 6 changed nothing in the field on GL. Not bitwise, as recorded then; negative control red | `field_gate_gl.txt` |
| 3 | Composite gate on GL | **Measured: max 1 step** on 2.4 % of channels, 87 % of them over bare paper with a bitwise field — the washi's procedural float math on NVIDIA, not the palette path; deterministic. Green at `--max-diff 1` with the negative control red; GL tier proposed in #78, Metal stays 0, no workflow runs this gate | `composite_gate_gl.txt`, `composite_gate_gl_maxdiff1.txt`, `composite_analysis.txt` |
| 4 | Harness tests | **PASS** — anod 9/9, chladni 7/7, print **6/6** (5 + the #77 regression), gesture 6/6, burst 9/9, spark 5/5, torsion 8/8, chirikov 4/4; palette **4/5 by design**: its eight hashes are Metal's prints — GL's are in #78; Sumi at rest is exactly the composite-gate print, and GL's plasma-orange print against the Mac's own (which hashes to the table's entry) is visually identical, mean colour within 0.03 of a level, large pixel differences only where the grid's fine lines moved by a pixel. Soaks chladni, chladni-field, spark, burst 3/3 each | `*_test.txt`, `soak_*.txt`, `palette_prints_gl_vs_metal.txt`, `anod_orange_metal_vs_gl_diff.png` |
| 5 | Look | **PASS** — `--anod-strike-render` beside the Mac's step-43 renders: the six strikes, the bent six and the stir alone are the same (mean colours equal to 0.1 of a level); the settings window shows Substrate / Palette / Medium per medium (Sumi: tint, paper, roughness, fiber scale; Anod: glass darkness, grain, bloom, reach, strike charge, glow scale, grid lines); the author's preset loads to black glass | `anod_strike_renders_metal_vs_gl.jpg`, `settings_sections_sumi_anod.jpg`, `anod_preset_load_ledger.jpg` |
| 6 | Prints | **PASS after #77** — first pass: the Canvas dip logged `readback framebuffer incomplete` and kept no sheet, a re-export `PBO map failed`: the ledger called the core from the settings window's GL context. Deferred to `tick()`; now two dips land with thumbnails, a 4K re-export and a 4K Anod-over-alpha re-export write (alpha 0..255), no GL error. Regression in `--print-test`: the old ledger dies of SIGSEGV on that request | `anod_preset_load_ledger.jpg`, `ledger_export_4k.jpg`, `ledger_export_4k_alpha_preview.png`, `print_test.txt`, `print_test_without_fix.txt`, `ui_walk_anod.log` |
| 7 | Presets across machines | **PASS** — `desktop-anod.json` loaded from Settings › Presets, named `desktop-anod`, exported through the Export box: **byte-identical** to the Mac's 2040-byte file; also byte-identical through the serializer alone (glibc's float printing matches) | `preset_roundtrip_ui.txt`, `preset_roundtrip_serializer.txt` |
| 8 | Gestures (#75) | **PASS** in both media on defaults — Sumi: click drop, Shift-drag fold, right-drag vortex, Shift+right feed then swirl; Anod: click strike with the shear, Shift-drag burst, right-drag torsion rings, Shift+right push feeds torsion round the charge and the pull lights the Chladni discs. (The author's preset has the grid off, `anod_pitch` 0, so water-only gestures are invisible on it by design.) | `gestures_sumi_x11.jpg`, `gestures_anod_x11.jpg` |
| 9 | The ROLI over ALSA in Anod | **YOURS** — the ROLI was not attached this session (`amidi -l` lists only the tablet); the mapper's routes are covered headless (`test_medium_binding_tables`) | — |
| 10 | Packaging | **PASS** — CPack DEB of the 1.1.0 binary, 11 files, clean `ubuntu:24.04` container: installs, `--version` reads libsumi 1.1.0, launches through the desktop entry (622 frames), removes cleanly | `deb_container.log` |

## Found on the way

* The lab bench's `9` key dips straight through the core and does not feed
  the ledger (by design; the product dip is Settings › Canvas).
* This box's XKB layout is French while the X core keymap is US: scripted
  typing into ImGui goes through the clipboard (`automation/x11ui.py`).
* Metal and the web were not re-run here: #76 changes no code and #77 is
  desktop-shell only; the Mac and Windows owners re-run `--print-test`.

**Tree ready** — evidence `docs/evidence/step45a/`, decisions #76–#78. The
author commits.
