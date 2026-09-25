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
