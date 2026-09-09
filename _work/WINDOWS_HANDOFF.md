# Handoff — Windows machine: Step 33 verification (the fix batches from the Mac)

Written on the macOS machine. You are the agent on the author's Windows 11
box (real GPU, 5120×2160 @ 125 %, MSVC 2022, CMake, Ninja, loopMIDI). Your
own step, **Step 29 (Windows release lane), is DONE** — `docs/evidence/step29/`,
DECISIONS_4 #39–#43; nothing in this handoff reopens it. What is left of
Step 29 is the author's (a runner run with `dry_run`, the clean-VM install,
the winget RC check, the signing secret) — leave those alone.

This handoff is **Step 33 on Windows**: ten fix batches landed on the Mac,
some of them in the core, and the desktop shell is one build for three
platforms. Verify them here, fix anything Windows-only, and record what you
find. Do not touch the macOS, Linux or web lanes, nor the tablets.

## Read first, in this order

1. `CLAUDE.md` — the working rules. **Agents never commit** (prepare
   evidence, report "tree ready"); **agents never edit specs or roadmaps** —
   new decisions go to `_work/DECISIONS_4.md` as numbered entries (**next
   number: 67**; the Linux box may be writing in parallel — coordinate
   numbering through the author), and where a spec line and reality disagree
   you flag it in the entry, you do not edit the spec.
2. `docs/evidence/step33/SUMMARY.md` — batches 1–10, what each changed and
   how it was verified on the Mac. Append your section there.
3. `_work/DECISIONS_4.md` #49–#66 — the record of what shipped. The ones
   that touch Windows directly: #57 (`--window`), #58/#59 (no title bar is
   the default; fullscreen is a setting), #60 (input mode is a setting),
   #64 (eight layouts).
4. `README.md` "Build & run" (Windows paragraph) and `site/src/content/docs/
   guide/desktop.md` — the user-facing description of what you are checking.

## What changed since Step 29 (all of it on the Mac)

* **Core ABI 0.5 → 0.8.0** — rebuild everything from scratch (`cmake -B build
  -G Ninja && cmake --build build && ctest --test-dir build`; four suites,
  18 377 mapper checks). `sumi_params_t` grew (`wake_profile`, `wake_spread`,
  #53); `sumi_layout_t` grew two rolls (#64); new drop layer and vortex
  profile enums (#49). The §4.6 field fixture is unchanged and must still
  hold **bitwise** on your GPU (`tools/field_gate.py` defaults).
* **Desktop shell:** Marble-mode pressure gesture, Shift + right drag (#49);
  Stylus wake row + spread (#53); `--window <w>x<h>` (#57); Settings › Window
  › Fullscreen, F11, `--fullscreen` (#58); **the canvas has no title bar by
  default — on Windows it is created with `GLFW_DECORATED` off** (#59); an
  **Input** row (MPE default · Classic keyboard · Wind) in Expression routing
  (#60); eight layouts in the picker, named by the roll's now-line edge (#64).
* **Behaviour every platform shares:** MPE mode gives a channel-1 keyboard
  per-note voices (chords) (#60); the palette-morph CC travels the ring (#61);
  **CC 64 never dips the paper in any mode** (#62); wind mode is MPE plus a
  wake between notes, breath unbounded (#63); the bend-driven ripple is 4×
  more sensitive (#66); the measured Airwave CC map (#50).

## Checklist — Windows

Build first; everything below runs the release binary from your tree.

1. **Build + suites + field gate.** Clean configure, `ctest` 4/4,
   `tools/field_gate.py` bitwise against `tests/fixtures/field_512_metal.bin`
   on the D3D11 device. Then `--dev --pressure-test` (6/6), `--stokeslet-test`
   (4/4), `--ripple-group-test`, `--ripple-permanence-test`.
2. **The borderless canvas (#59) — the one that can bite here.** Launch
   plain: no title bar. Can you live with it? Win + arrows move/snap it,
   Alt + F4 closes, Ctrl , opens the framed settings window, the taskbar entry
   and icon are still right, minimise/restore from the taskbar works, and a
   second monitor + 125 % DPI keep the framebuffer size honest (the `--dev`
   bench / `[settings]` log line prints it). If the borderless canvas is
   unmanageable on this desktop, say so with the concrete failure — the
   recorded fallback is a **per-platform default, not a setting** (#59).
3. **Fullscreen (#58).** Settings › Window › Fullscreen and F11: fills the
   monitor the canvas is mostly on, comes back to the same windowed position
   and size, survives the DPI change between monitors; `--fullscreen` writes
   `fullscreen=1` to `%APPDATA%\midi-sink\settings.ini`; `--window 1920x1080`
   opens exactly that (points) and the INI is untouched by it.
4. **Input mode (#60/#62/#63)** with loopMIDI + a keyboard or
   `build\tests\mpe_stress_win.exe`: MPE (default) — a chord on channel 1
   paints one drop per note, CC 64 does nothing; Classic — per-note voices,
   bend = global shear; Wind — one voice, legato drags the sounding drop to
   the next note with a wake and CC 2 grows it (unbounded). The row persists
   as `input_mode=` in the INI.
5. **Layouts (#64).** The combo lists eight; Piano roll (left / right / top /
   bottom) scroll away from their now-line; the tempo and roll-speed rows
   appear for all four.
6. **Gestures.** Shift + right drag (press: hold/push up = feed, pull back =
   swirl), middle drag wake with the Stylus wake row flipped to Viscous
   stroke, right drag with Vortex profile Rankine.
7. **Settings window (#56 is desktop-parity, so nothing new here)** — a quick
   pass that the CC map editor, palette and ripple rows still behave after
   the ABI bump; Palette morph on CC 31 now reaches Ochre from Sumi (#61).
8. **Installer unaffected?** `packaging/windows/midi-sink.iss` did not change;
   one local build of the setup exe, install, launch, uninstall — to be sure
   the new flags and the borderless window do not upset the Start-menu launch.

## Reporting

Append **"Windows verification (Step 33)"** to `docs/evidence/step33/SUMMARY.md`:
a table of the eight items above (PASS / FAIL / YOURS with the file that
proves each), raw outputs beside it (`ctest.log`, `field_gate_d3d11.txt`, a
screenshot of the borderless canvas + settings window, the INI after item 3).
Any Windows-only fix goes in the shell, never the core, with a DECISIONS_4
entry from #67. The author commits; end with "tree ready" and the evidence
path.
