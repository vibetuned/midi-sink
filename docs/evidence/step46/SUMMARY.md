# Evidence — Step 46: Phase 6 on the Windows desktop (D3D11)

Machine: the author's Windows 11 Pro box (26220) — NVIDIA GeForce RTX 5090
(driver 32.0.16.1664; an Intel iGPU beside it), 5120×2160 @ 165 Hz + 3840×2160
@ 120 Hz, both at 125 %, MSVC 19.44 (VS 2022), CMake 4.4.3, Ninja, loopMIDI;
the ROLI Airwave attached, the ROLI Piano not. Build from scratch at
`libsumi 1.1.0` (`midi-sink 1.0.0-27-gda750be`, `version.txt`). Decisions:
`_work/DECISIONS_5.md` #82–#85. Three fixes landed, all in the desktop shell
(the core is untouched): one compile error, two Windows bugs that broke
print export.

## Checklist

| # | Check | Result | Evidence |
|---|---|---|---|
| 1 | Build + ctest | **PASS after #82** — MSVC refused one line of the lab bench (`0.0f / 0.0f`, C2124; now `std::nanf("")`); otherwise clean, the only warning in our code a CRT deprecation notice. ctest **6/6** (the ABI test runs against both artifacts on Windows), 19 004 mapper checks, `preset_tests` 30/30 under MSVC /W4 | `ctest.log`, `version.txt` |
| 2 | §4.6 field gate on D3D11 | **PASS, and bit-identical to Step 11** — max 3.906e-03 (ink), mean 6.845268e-06, negative control red. Not bitwise with the Metal fixture, and it **never was on this box**: today's dump equals the Step-11 D3D11 dump in git history byte for byte, and that dump already sat at this mean. Part IV #35's "bitwise on real GPUs" is flagged in #83 | `field_gate_d3d11.txt` |
| 3 | Composite gate on D3D11 | **Measured: max 1 step** on 2.44 % of channel samples, alpha never, deterministic — GL's #78 profile to the pixel (25 257 vs 25 312 differing pixels, the same sample pixels and values, 87 % over bare paper, every difference one step darker). Both boxes are RTX 5090s: NVIDIA's arithmetic through FXC and through GLSL, not a backend property. Green at `--max-diff 1` with the negative control red (max 65); **D3D11 tier `--max-diff 1` proposed in #83**, Metal stays 0 | `composite_gate_d3d11.txt`, `composite_gate_d3d11_maxdiff1.txt`, `composite_analysis.txt`, `composite_512_d3d11.rgba` |
| 4 | Harness tests | **PASS** — anod 9/9, chladni 7/7, print **9/9** (6 + the #84/#85 regressions: the D3D11 readback at 4K/8K, Anod over alpha and #77's other-context request all pass), gesture 6/6, burst 9/9, spark 5/5, torsion 8/8, chirikov 4/4; palette **4/5 by design** — D3D11's eight hashes are in #83; its Sumi at-rest print IS the composite-gate print, its Anod prints have GL's profile against the Mac's own (mean colour equal to GL's to the hundredth). Soaks chladni, chladni-field, spark, burst **3/3** each, their SUMMARY lines equal to GL's to the last digit but for two erosion rates in the third figure | `*_test.txt`, `palette_prints_d3d11_vs_metal.txt`, `soak_*.txt` |
| 5 | Look | **PASS** — `--anod-strike-render` beside the Mac's step-43 renders: the six strikes, the bent six and the stir alone are the same (means within 0.04 of a level, lit share 34.11 % vs 34.10 %). Live: Sumi's washi and fibres, the Anod black glass, grid and bloom on the switch; Settings shows Substrate / Palette / Medium per medium as on Linux; the custom palette editor (stops, depth curve and floor, hue drift). No sokol validation error — only FXC's compile warnings (#82) | `anod_strike_renders_metal_vs_d3d11.jpg`, `anod_strike_renders.txt`, `look_sumi_then_anod_live.jpg`, `settings_sections_sumi_anod.jpg`, `palette_custom_editor.jpg` |
| 6 | Prints | **PASS after #84 and #85** — two dips with thumbnails (GL textures in the ImGui window beside the D3D11 canvas; #77's deferral works here). First pass: every re-export failed silently — a CRLF `settings.ini` had left a carriage return inside the print folder (#84); then an accented folder failed too — the narrow CRT read UTF-8 paths in cp1252 (#85). After the fixes: a 4096×2304 export (10.2 MB), the same over alpha (alpha 0–255, 38 % transparent glass), and a 4K export into `…/midi-sink-été` shown correctly in the field | `ledger_two_dips_thumbnails.jpg`, `ledger_export_4k.jpg`, `ledger_export_4k_alpha_preview.jpg`, `print_folder_utf8.png`, `print_test_without_fix.txt`, `print_test_without_utf8_manifest.txt` |
| 7 | Presets across machines | **PASS** — `desktop-anod.json` installed from the git blob (this checkout's autocrlf makes the working copy 2102 bytes CRLF; the Mac's file is 2040 bytes LF), loaded from Settings › Presets, named `desktop-anod`, exported through the Export box: **byte-identical** to the Mac's (same SHA-256, zero CR). Through the MSVC serializer alone too — and the CRLF copy reads cleanly and writes the Mac's LF bytes back | `preset_roundtrip_ui.txt`, `preset_roundtrip_serializer.txt`, `desktop-anod.written-by-windows.json` |
| 8 | Gestures (#75) | **PASS** in both media on defaults — Sumi: click drop, Shift-drag fold, right-drag vortex (both shown over a row of drops, water is invisible in Sumi), Shift+right feed then swirl; Anod: click strike with the shear, Shift-drag burst, right-drag torsion rings, Shift+right push feeds torsion round a charge and the pull lights the Chladni discs | `gestures_sumi_d3d11.jpg`, `gestures_anod_d3d11.jpg` |
| 9 | The ROLI in Anod | **PASS simulated, hands-on YOURS** — the Piano was not attached; the same MPE stream through loopMIDI/WinMM (MCM, three strikes, per-note bend ±, poly pressure, mod-wheel throws) drives spark charges, the stir and its reverse, the pressure's wavenumbers and the Chirikov shear. The Airwave map is #69's, identical in the shell's INI and the core, Flex free | `anod_mpe_bindings_loopmidi.jpg` |
| 10 | The Windows channels | **PASS** — the lane files are untouched since Phase 5; `packaging/windows/build_installer.bat` builds the lane's Inno Setup installer (3.1 MB, the #85 manifest inside the exe); silent per-user install, `--version` reads libsumi 1.1.0, the Start-menu entry launches both windows, uninstall removes the app, settings and session survive | `installer_roundtrip.log`, `installed_launch.jpg` |

## Found on the way

* **The desktop's 0.x migration works on Windows**: the first 1.1 launch took
  the Step-33 `settings.ini` (layout, palette, input, bend ripple, the #69 CC
  map recognised as stock and upgraded) into `last_session.json`.
* **A failed background PNG write is invisible in the UI** — the status
  line reads "writing the PNG in the background" and the failure goes to
  stdout only. Both #84 and #85 hid behind it. Flagged in #84, not changed.
* **The harness's FNV-1a seed is one digit short** of the standard basis —
  harmless and left alone (#83); re-hashing outside the harness needs it.
* **MSVC reads the sources as cp1252** — narrow literals survive byte for
  byte, a raw `é` in a wide literal does not; the first #85 test passed
  falsely because of it (#85).
* The #84 poisoned INI on this box came from my own Step-33 restore through
  PowerShell (CRLF) — exactly the path a user's Notepad edit takes.

## Not re-run here

Metal, GL and the web. #82 is one lab-bench line (Clang and GCC accepted
the old form; `std::nanf` is the same NaN); #84 and the legacy half of #85
are desktop-shell code on every platform; the manifest is Windows-only.
`--print-test` becomes **8/8** on macOS and Linux (the wide-API check is
Windows-only) — the Mac and Linux owners re-run it.

## YOURS

* The ROLI Piano in Anod by hand (item 9's simulation covers the bindings).
* The tier proposal in #83 and the invisible-write-failure flag in #84.
* Your `%APPDATA%\midi-sink` is restored byte for byte from before the
  session; the working copies sit beside it (`midi-sink-backup-step46`,
  `midi-sink-s46-session1`, `midi-sink-s46-defaults`) and my test exports
  are gathered in `Pictures\midi-sink-step46-test-exports` — delete them
  when you like.

**Tree ready** — evidence `docs/evidence/step46/`, decisions #82–#85. The
author commits.
