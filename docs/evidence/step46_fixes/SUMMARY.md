# Evidence — after step 46: the fixes the author asked for, one by one

Written on the macOS machine (Metal), 2026-09-25, after the Linux (45a),
Android (45b) and Windows (46) agents handed back. Each fix below carries its
decision entry in `_work/DECISIONS_5.md`; the platforms that could not be
re-run here are named.

## 1. Silent background write failures (#86)

* `desktop/src/print_export.{h,cpp}`: one background PNG writer with an
  outcome queue (`print_write_async` / `print_write_poll`); the folder is
  checked first, the C runtime's reason otherwise.
* `desktop/src/print_ledger.{h,cpp}`: the ledger uses it and turns each
  outcome into its status row (`write_serial()`, `last_write_ok()`).
* `desktop/src/settings_ui.{h,cpp}`: the Canvas section shows the same line
  under its buttons (a failure stays 30 s); the dead `if (false)` block
  removed.
* Regression: `--print-test` 10/10 (`print_test.log`) — the two new checks
  are the last two.
* Sanity here: `ctest` 5/5, `--anod-test` 9/9, `--gesture-test` 6/6. Not
  re-run: Linux (GL) and Windows (D3D11) — the change is shell-side and
  platform-free (std::filesystem, as `app_settings.cpp` already uses).

## 2. The GL / D3D11 measurements as tools, not "by design" reds (#87)

* `tools/composite_gate.py --backend metal|gl|d3d11`: the tier per backend
  (0 / 1 / 1), `--max-diff` overrides, the summary line names the backend.
* `--palette-test`: a hash column per backend (Metal = step 43's proof; GL
  and D3D11 transcribed from `step45a/palette_test.txt` and
  `step46/palette_test.txt`); `DevOptions.backend` from `main.cpp`.
* Docs: `docs/BUILD.md`, `tools/README.md`. The Part-IV #35 "bitwise on
  real GPUs" note is corrected in the entry (the merged record is not
  edited).
* Here (Metal): `palette_test.log` 5/5 against the Metal column,
  `composite_gate_metal.log` bitwise. GL / D3D11: not re-run; the next
  visit to those boxes runs `--palette-test` (expect 5/5) and the gate with
  `--backend gl` / `--backend d3d11` (expect green within the tier).

## 3. The Adreno strike drift (#80) — measured, and the strike made the classic spark (#88)

Frame-locked six-strike prints, the author's Anod preset, 1480×924:

| Print | Charges (lit blobs ≥ 40 px) | Lit share |
|---|---|---|
| desktop, before (`tab_strikes_desktop_before.png`) | 6 | 1.93 % |
| Tab, before (`tab_strikes_tab_before.png`) | 2 fragments (151, 127 px) | 0.02 % |
| Tab, manual highp bilinear (experiment 1) | 0 | 0.00 % |
| Tab, RGBA32F field + manual bilinear (experiment 2) | 5, a fifth to a tenth the size | 0.36 % |
| desktop, after (`tab_strikes_desktop_after.png`) | 6 | 5.99 % |
| Tab, after (`tab_strikes_tab_after.png`) | 6 (two thinned) | 3.68 % |
| desktop, after, default strike params (`tab_strikes_desktop_after_defaults.png`) | 6 | 4.62 % |
| Tab, after, default strike params (`tab_strikes_tab_after_defaults.png`) | 6 | 4.95 % |

The §4.6 field gate on the Tab: stock max 1.51e-2 / mean 6.08e-4; with the
manual bilinear max 9.28e-3 / mean 6.01e-4 — the mean does not move, the
drift is arithmetic, not sampling. Shipped: the Anod strike as the classic
spark on its charge, `anod_drop` default 0.57 (#88).
`anod_strikes_default_after.png`: the lab's `--anod-strike-render` under the
new defaults, beside step 43's `anod_strike_after.png`.
