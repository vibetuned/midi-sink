# Evidence — Step 23: Desktop productization (macOS)

PHASE5 §2; ROADMAP_4 Step 23. Decisions: `_work/DECISIONS_4.md` #1–#8.
Machine: the author's Mac (macOS 26.6, Xcode 26.6, Apple Silicon). Core
untouched (Phase-5 working rule); the desktop harness became the product.

## What landed

* **Naming (#1):** `midi-sink` everywhere; bundle id `com.vibetuned.midi-sink`.
* **Shared settings window (#2):** Dear ImGui `v1.92.9b` in a second GLFW
  window with its own GL 3.2 context — `desktop/src/settings_ui.cpp`, one
  implementation for macOS/Windows/Linux. Sections: first-run hint (one
  dismissible line), Layout & look (6 layouts incl. Piano grid, 3 palettes,
  viscosity / ink feed / roughness, tempo + roll speed on the rolls,
  full-resolution toggle = sim_scale 1.0/0.75), Expression routing (note bend
  Glide/Ripple, channel pressure Feed/Swirl, CC 74 Hue/Pinch + Saddle/Crossed,
  vortex Exponential/Rankine), Ripple (amount / wavelength through the routed
  CCs, angle), **CC map editor** (table + add/remove + restore defaults),
  **MIDI inputs** (open-port list, seconds since the 1 Hz rescan, rescan now),
  Canvas (**Paper dip**, print folder, **Save last print as PNG**), About
  (app version · commit · `sumi_version()`), and — only with `--dev` — the
  Lab bench (key legend, ripple live/bake, smoothing, raw MIDI log).
  `Cmd ,` / `Ctrl ,` reopens it after closing.
* **Persistence (#4):** `~/Library/Application Support/midi-sink/settings.ini`
  written on first run and on every change (`settings.ini.sample`).
* **`--dev` (#5):** every debug key and scripted flag refused without it
  (`release_flags.txt`); `dev_tools.cpp` holds the whole lab bench, moved out
  of `main.cpp` verbatim.
* **macOS `.app` (#6):** `MACOSX_BUNDLE` + `packaging/macos/Info.plist.in`,
  `midi-sink.icns` from `tools/gen_icons.py --only macos` (Pillow ICNS),
  hardened-runtime entitlements, ad-hoc `codesign --options runtime` as a
  post-build step; the runtime Dock-tile hack and `app_icon_macos.h` deleted.
  `GLFW_COCOA_CHDIR_RESOURCES` disabled so relative paths stay the user's.
* **Version (#3):** `SUMI_APP_VERSION` cache var, default `git describe`;
  About/`--version` read `5ba722f-dirty (commit 5ba722f, libsumi 0.4.0)`
  pending the first tag; Info.plist carries `0.0.0` + `SumiBuildDescribe`.
* **CI (`.github/workflows/build.yml`):** ubuntu-24.04 / windows-latest /
  macos-latest configure + build + ctest, plus `codesign --verify` on macOS —
  Windows/Linux compile of the shared UI verified there, exercised in
  Steps 29/30.

## DONE verification

| DONE criterion | Result | Evidence |
|---|---|---|
| Launches to a playable instrument with zero debug keys active | **PASS** — release run opens the canvas + settings; MIDI inputs auto-open (`Réseau Session RTP 1`, GarageBand virtual out seen in the run log); the key callback returns before any binding without `--dev`; `--field-dump` and every lab flag are refused with a one-line pointer | `release_flags.txt`, `settings_window_first_run.png` |
| Every setting mouse-reachable | **PASS** — all iOS-sheet rows, the CC-map editor, MIDI list/rescan, dip and print export are widgets in the settings window; nothing requires a key | `settings_window_first_run.png` |
| `--dev` restores the lab bench | **PASS** — the step-19/20 battery through the bundled binary: wake 2/2 + flick/rankine/ripple-group/ripple-dip/ripple-permanence/swirl **17/17** (19 ok / 0 fail total); `--dev --exit-after` and the loop-riding scripted inputs work | `dev_battery.txt` |
| §4.6 field regression unchanged by the restructure | **PASS** — `--dev --field-dump` vs `tests/fixtures/field_512_metal.bin`: **bitwise identical** (max 0, mean 0) | `field_regression.txt` |
| Bundle passes `codesign --verify` ad-hoc | **PASS** — `valid on disk`, `satisfies its Designated Requirement`, flags `adhoc,runtime`, identifier `com.vibetuned.midi-sink` | `codesign_verify.txt`, `info_plist.txt` |
| Settings persist | **PASS** — first run writes defaults; an edited `layout=5` / `first_run_dismissed=1` loads back (`[settings] loaded …`) and is re-saved on a clean exit | `settings.ini.sample` |
| Headless suites | **PASS** — ctest 4/4 (ABI/C11 ×2, normalizer/mapper, hostmpe) | build log |
| Volunteer test (ROLI in two minutes from the hint alone) | **YOURS** — the human gate | — |

## Found on the way (recorded in DECISIONS_4)

* GLFW's Cocoa default `chdir()`s a bundled app into `Contents/Resources` —
  the first `--field-dump` landed inside the bundle. `GLFW_COCOA_CHDIR_RESOURCES`
  is now off (#6).
* A window positioned while hidden takes macOS's own frame on first show;
  and a late `glfwFocusWindow(canvas)` buried the settings window behind the
  canvas on a display too narrow for both. Placement is re-applied on first
  show, clamped to the monitor work area, and the canvas is not refocused
  after the settings appear (#2).
* ImGui's default font is Latin-1 only: the ⌘ and — glyphs rendered as `?`;
  UI strings use `Cmd ,` and ASCII dashes (#2).
