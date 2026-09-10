# Evidence — Step 33, fix batches 1–2 (opened early, while the beta runs)

ROADMAP_4 Step 33 (scoped unfreeze: fixes with their tests). Decisions:
`_work/DECISIONS_4.md` #49–#52. Machine: the author's Mac. The author's
list: the missing iOS paper-dip discard button, a Marble-mode gesture for the
two pressure operators (long press on the tablets, Shift + right drag on
desktop), the Airwave's missing and mis-assigned inputs, and Jaffer's three
remaining patterns (Oseen stroke, Spanish wave, Turkish moiré).

## What landed

* **Core, ABI 0.6.0 (#49, #51)** — two additive enum values:
  `SUMI_VORTEX_LAMB_OSEEN` (the §4.3(7) swirl pass as a gesture, host
  supplies Γ·Δt and r_c) and `SUMI_DROP_FEED` (the drop shader's interior
  copies the centre texel: the band under the press WIDENS, no new ring). The
  print double-buffer **recycles the older unread print** instead of refusing
  a third dip (`sumi_read_print` is a synchronous copy, so the only unsafe
  overwrite is a readback in flight). The §4.6 field script is untouched:
  every fixture stands.
* **The pressure gesture on every shell (#49)**, same constants everywhere:
  long press 250 ms (Shift + right button with a mouse) lays a drop and
  becomes Play mode's bipolar Y — hold / push up = feed (0.12·(0.35 + up)
  canvas heights/s), pull back = swirl (3 rad/s core rotation at full pull,
  r_c = R), travel 0.15. Desktop `main.cpp`, web `sumi-host.js` (mouse and
  touch), iOS `SumiCanvas.swift` (`UILongPressGestureRecognizer`), Android
  `MainActivity.kt` + `NativeBridge` + JNI (**written, not compiled here**).
* **Airwave default map = the measured one (#50)**: 26 Raise L → vortex
  strength, 24 Glide L → centre X, 22 Slide L → centre Y, 30 Flex L →
  roughness, 28 Tilt L → ripple wavelength; 29 Tilt R → viscosity, 31 Flex R
  → palette, 27 Raise R → ripple amount; 20/21/23/25 free. Core table,
  desktop mirror, README, devices page, chart JSON; normalizer goldens moved.
* **iOS paper dip (#51)**: two buttons like Android — *save the print* (RGBA8
  → Photos, `NSPhotoLibraryAddUsageDescription`) and *discard*.
* **Regression test for the new passes**: `midi-sink --dev --pressure-test`
  (feed widens one parity band to the analytic area, no rings, vs. a control
  that lays rings; the LAMB_OSEEN gesture rotates a far-field marker by the
  analytic θ(r); three unread dips all accepted, two prints read back).
* Docs: Marble gestures table, README, drop and swirl pages, paper-and-prints,
  C-ABI version history. Decision #52 records the paper-derived design for the
  Oseen stroke (velocity field closed-form, displacement iterated), the
  Spanish wave (a TRANSFER-TIME mapping with pigment tint — composite/print,
  not a field pass) and the Turkish moiré (its dark-paper variant).

## Verification

| Check | Result | Evidence |
|---|---|---|
| Headless suites | 4/4 (ABI pin 0.6.0; goldens on the measured map) | `ctest_v0.6.txt` |
| §4.6 field gate, Metal (reference tier) | GREEN, bitwise, negative control red | `field_gate_metal_v0.6.txt` |
| §4.6 field gate, WebGPU (rebuilt wasm) | PASS, max 9.8e-4 unchanged | `field_gate_webgpu_v0.6.txt` |
| Web scene sweep | 10/10 | `scenes_sweep_v0.6.txt` |
| Pressure test (Metal) | 6/6 — feed area 12256 vs π(R·H)² 11859, one band, control 11 transitions, swirl 0.320 rad vs analytic 0.319, recycle 2 prints after 3 dips | `pressure_test_metal.txt` |
| iOS | `ios/prepare_release.sh` + unsigned `generic/platform=iOS` Release build: BUILD SUCCEEDED | (log) |
| Docs site | 37 pages, drift check ok, chart check 32/32 | (site build) |
| Android | **not compiled on this machine** — `_work/LINUX_HANDOFF.md` addendum lists what to verify on the tablet | — |

## Human

* Airwave hand assignment is a taste call (#50): every row is a one-line
  remap in the settings window; say the word and I move it.
* The gesture feel (rates, travel) is tunable in one constants block per
  shell; try it on the iPad and desktop.
* Spec is stale on §5.3 (header copy), §8.1 (Marble gestures), §2.2/§3.4
  (Airwave defaults) — yours to fold.


# Batch 2 — the viscous stylus stroke (DECISIONS_4 #53)

The author proposed the impulsive point force in unsteady Stokes/Oseen flow
as a closed-form displacement operator. Derived for the 2-D layer, regularised
over the tip radius as the difference of two point kernels (exactly
divergence-free), normalised so the tip moves by d, verified numerically, then
built as the wake's second profile. ABI **0.7.0** (params grew
`wake_profile`, `wake_spread`).

| Check | Result | Evidence |
|---|---|---|
| Kernel derivation vs numerical time integration | closed form = ∫u dτ at three points, both components (a sign slip in d_y was caught here); E₁ to 1e-10; blob divergence 1e-11; mirror; d(0) = d; 1/r² far field; fold budget d ≤ a/4 for ℓ/a ≥ 1.5 | `stokeslet_verify.py`, `stokeslet_verify.txt` |
| Headless suites | 4/4 (ABI pin 0.7.0) | `ctest_v0.7.txt` |
| §4.6 field gate, Metal | GREEN, bitwise (script untouched) | `field_gate_metal_v0.7.txt` |
| §4.6 field gate, WebGPU (rebuilt wasm) | PASS, unchanged | `field_gate_webgpu_v0.7.txt` |
| Web scene sweep | 11/11 (new `viscous` scene) | `scenes_sweep_v0.7.txt` |
| `--stokeslet-test` (Metal) | 4/4: tip moves by d (0.0100), mirror to a half-float ULP, one a/4 pass det ≥ 0.75 / mean 1.00012, a 10a stroke fold-free outside the swept corridor | `stokeslet_test_metal.txt` |
| iOS | picker + spread in the sheet; unsigned Release build succeeds | (log) |
| Docs | wake page: derivation, formula, invariants, the `viscous` embed; operators table; C-ABI history; 38 pages, drift check 11/11 scenes | (site build) |
| Look | bands compressed ahead into a point, spread perpendicular, a V trailing — Jaffer's tank observation; the doublet for comparison | `scene_viscous_stroke.png`, `scene_wake_doublet_for_comparison.png` |
| Android | bridge `nativeSetWakeProfile` written; **no sheet row, not compiled here** (handoff addendum) | — |

Deferred by the author: the Spanish wave (print-time UI). Agreed in principle,
not built: the Airwave right hand as a hand in the water delivering these
impulses (#53, last paragraph).

# Batch 3 — the stylus in Marble mode (DECISIONS_4 #54)

Author's report: the stylus does nothing useful in Marble mode on the tablets.
Cause: the pen lived only in the Play overlay, hidden in Marble mode, so a
Pencil / S-Pen fell through to the finger path (tine, drop, pressure press).
Spec §8.7 wants the wake in both modes. Fix: iOS recognizers accept direct
touches only and the canvas view handles `.pencil` touches with
`sumi_add_wake` (the overlay's tip mapping); Android routes stylus tool types
to `nativeAddWake` and cancels the long press. iOS compiles (unsigned Release
build); Android written, not compiled here. The behavioural check is the
author's: a Pencil stroke in Marble mode threads the rings as on desktop's
middle-drag, with no tine and no drop on lift.

# Batch 4 — iOS MIDI inputs list + fallback rescan (DECISIONS_4 #55)

Author's report: the iPad sends MIDI over USB but receives nothing. The shell
already connected every source; nothing surfaced which link failed. Added the
desktop's "MIDI inputs" list to the iOS Settings (names, received counter with
the last message, skipped-source count, Rescan now), status logging on every
connect, and a 1 Hz rescan beside the CoreMIDI notification. iOS compiles
(unsigned Release build). Reading the list on the iPad with the USB device
attached tells which case it is: not listed → CoreMIDI/USB level (cable, hub
power, non-class-compliant device); listed, counter still → bytes never leave
the device (its USB mode, or it only sends on a port we cannot see); counter
moving, canvas still → core-side routing (report the last message shown).

# Batch 5 — settings parity on the tablets (DECISIONS_4 #56)

Feedback: no palette (and other rows) in the iOS/Android settings. Both sheets
now carry the desktop window's rows: palette, viscosity, ink feed, roughness,
tempo/roll speed on the rolls, vortex profile (the twist follows it, as the
desktop's right drag), stylus wake on Android, ripple amount/wavelength (as the
routed CCs through the MIDI path) and angle, and the CC map editor with restore.
iOS compiles (unsigned Release build). Android: JNI setters, CC map apply,
`play_send_cc` with replay at create, Compose rows — written, uncompiled here
(LINUX_HANDOFF). Guide: marble-mode.md gains "The same settings everywhere"
and the vortex row's "Rankine by default" is corrected; desktop.md links it.
Docs follow-up: `site/src/content/docs/reference/settings.md` — every setting on
every platform with range, default, what it drives and how it reaches the core
(params field / CC / action), the default CC map with consumers, and where each
platform stores settings. Sidebar: Reference → Settings reference. Site check ok.

# Batch 6 — `--window <w>x<h>` (DECISIONS_4 #57)

Public desktop flag to open the canvas at an exact size for resolution-specific
reports. `--window 1920x1080` opens 1920×1080 (verified below); a bad value
exits 2 with the usage line. Docs: guide/desktop.md "Command line", README.

# Batch 7 — title bar and fullscreen (DECISIONS_4 #58)

Settings › Window: "Hide the title bar" (macOS: hiddenTitleBar look via the Cocoa
glue; Windows/Linux: GLFW_DECORATED off) and "Fullscreen" (glfwSetWindowMonitor
on the canvas's monitor, geometry restored on exit), persisted in the INI as
`hide_titlebar` / `fullscreen`; flags `--no-titlebar` / `--fullscreen`; chord
⌃⌘F / F11. Runs on the Mac: `--fullscreen --dev --exit-after 3` logs
`[window] fullscreen on "LG ULTRAGEAR+" 3008x1269@100Hz` and the core resizes
to 3008×1270; `--no-titlebar` writes `hide_titlebar=1` and reloads. ctest green.
Docs: guide/desktop.md (Window row, Command line), reference/settings.md
(Window section), README. Windows/Linux paths are the handoffs' to verify.
Follow-up (DECISIONS_4 #59): the title bar is gone by default — the checkbox,
INI key and `--no-titlebar` flag are removed; macOS applies the hidden-title-bar
style at creation, Windows/Linux create the window undecorated. Build + ctest
green; the Mac run opens edge to edge with the traffic lights.

# Batch 8 — input mode as a setting; MPE/Wind layers; ring morph (DECISIONS_4 #60, #61)

Core 0.7.1 (ABI unchanged): non-member notes in MPE mode are per-(channel, note)
voices (a channel-1 keyboard keeps its chords); wind reads CC 74 / poly pressure /
member-channel bend on the brush; palette morph spans the ring (0 active, ½ next,
1 third). New tests `test_mpe_master_channel_keyboard`, `test_wind_expression_layer`;
ctest 4/4. Shells: an Input row (MPE default · Classic keyboard · Wind) on desktop
(INI `input_mode`), web (localStorage, `sumi_set_input_mode` already exported),
iOS (UserDefaults, compiles), Android (SharedPreferences + `nativeSetInputMode`,
uncompiled here). Docs: guide/devices.md rewritten (setting, not detection),
midi-chart.mdx intro, reference/settings.md (Input row, palette morph ring).
Spec flags: §2.5 stale (detection → setting), §2.3/§2.4 gain the layers.

# Batch 9 — CC 64 out; wind = MPE + wake legato (DECISIONS_4 #62, #63)

Core 0.7.2 (ABI unchanged). CC 64 never dips in any mode (falls to the CC map).
Wind: one voice played as MPE — MPE-radius strike drops, unbounded breath feed,
press_mode honoured — plus a wake (profile/spread from the stylus settings, tip =
the drop's radius, ≤ a/4 sub-steps) dragging the sounding drop to the next note on
legato, then a silent end and the new strike. Tests: `test_sustain_never_dips`,
`test_wind_mode_wake_legato`; ctest 4/4 (15812 checks). Shell footnotes, guide
(devices, marble-mode, index), operators index/tine/wake, settings reference,
MIDI chart JSON and README updated. Spec flags: §2.3, §2.4, §3.3, §4.4 stale.

# Batch 10 — rolls right/bottom, "(playable)" labels, ripple ×4 (DECISIONS_4 #64–#66)

ABI 0.8.0: `SUMI_LAYOUT_ROLL_H_RIGHT = 6`, `SUMI_LAYOUT_ROLL_V_BOTTOM = 7` (mirrors
of 3/4; drift away from the now-line). Golden positions for 8 layouts + a drift
direction test; every shell lists eight layouts named by the now-line's edge; iOS
and Android label the three lattices "(playable)". Bend → ripple amplitude
saturates at |±1.5| semitones instead of |±6|. Gates below.
Gates: ctest 4/4 (18377 checks); Metal field bitwise; wasm rebuilt, 11/11 scenes, WebGPU field PASS; `--ripple-group-test` 3/3, `--ripple-permanence-test` 1/1; layouts 6 and 7 run on the desktop; iOS compiles; site check ok. Android uncompiled here (handoff).

# Windows verification (Step 33) — DECISIONS_4 #67, #68

Machine: the author's Windows 11 box (real GPU, 5120×2160@165 Hz + 3840×2160
@120 Hz, both 125 % DPI, MSVC 2022, loopMIDI). Clean configure at ABI 0.8.0.
Two fixes landed, both shell-only: the Windows canvas keeps its title bar
(#67 — #59's recorded per-platform fallback, taken with the measured
failure) and the settings loader's `layout % 6` became `% 8` (#68 — rolls
6/7 reloaded as 0/1 on every desktop platform). Raw outputs:
`windows_checks.log`; per-item files below.

| # | Check | Result | Evidence |
|---|---|---|---|
| 1 | Build + suites + field gate + scripted tests | **PASS** — 5/5 ctest (18 377 mapper checks, ABI pin 0.8.0); §4.6 gate GREEN at the reference defaults, negative control red; `--pressure-test` 6/6, `--stokeslet-test` 4/4 (mirror to a half-float ULP, det min 0.848 / mean 1.00012), `--ripple-group-test` 3/3, `--ripple-permanence-test` 1/1. **Not bitwise vs the Metal fixture** — this GPU has sat at max 3.9e-3 (ink) / mean 6.8e-6 since Step 29 (driver drift, recorded there); the D3D11 dump is **bit-identical to its own Step-29/pre-batch output**, i.e. the ten batches changed nothing in the field script on this backend, which is the intent of "the fixture is unchanged" | `ctest_win.log`, `field_gate_d3d11_win.txt`, `pressure_test_win.txt`, `stokeslet_test_win.txt`, `ripple_*_win.txt` |
| 2 | Borderless canvas (#59) | **FAIL as shipped → #67 fallback applied.** No `WS_CAPTION`, no `WS_THICKFRAME`: Windows Snap refuses it — **Win + arrows do nothing** (a framed control window snapped fine in the same session) — and there is no drag and no Alt+Space Move; only Win+Shift+arrow monitor hops, taskbar minimize/restore, Alt+F4 and Ctrl , worked. A window that cannot be placed is unmanageable, so Windows now creates the canvas decorated (Win+Right verified snapping after); macOS/Linux untouched, README + guide updated | `windows_checks.log`, `borderless_canvas_win.png` |
| 3 | Fullscreen + `--window` (#57/#58) | **PASS** — F11 fills the monitor holding the canvas (verified on both monitors, log names each mode: 5120×2160@165 / 3840×2160@120), exact windowed rect restored, `fullscreen=` toggles live in the INI; `--fullscreen` writes `fullscreen=1`; `--window 1920x1080` opens exactly that (sim targets 1920×1080) and writes nothing to the INI; `--window 12x7` exits 2 with usage. Both monitors here are 125 %, so a DPI *change* across the move could not be exercised | `windows_checks.log`, `settings_ini_after_tests.ini` |
| 4 | Input mode (#60/#62/#63) | **PASS** — MPE: ch-1 chord = per-note drops, CC 64 dips nothing (zero dip lines); Classic: `override -> classic` logged, per-note drops; Wind: one voice, breath-grown drop dragged across two legato changes with wake threading; `input_mode=` persists, and the row applies live (the author's own mid-test click logged the switch immediately) | `mode_mpe_chord.png`, `mode_wind_legato.png`, `run_*_err.log` |
| 5 | Eight layouts (#64) | **PASS after the #68 fix** — picker lists eight named by the now-line edge; all four rolls drift away from their now-lines (~0.5 canvas in 4 s at the defaults: +x/+y/−x/−y measured); tempo + roll-speed rows shown on the new rolls. Found here: the INI loader clamped `% 6`, so rolls 6/7 never survived a restart — fixed, both reload | `layout_picker_eight.png`, `roll_layout_{3,4,6,7}.png`, `roll6_tempo_rows.png` |
| 6 | Gestures (#49/#53) | **PASS** — Shift+right hold fed a drop large; pull-back laid + swirled one; middle drag with **Viscous stroke** compressed the boundary ahead into the tip with the trailing V (Jaffer's look, as the Mac captures); right drag with **Rankine** rotated the drop as a rigid piece | `gesture_pressure.png`, `gesture_wake_viscous.png`, `gesture_rankine.png` |
| 7 | Settings parity + ring morph (#56/#61) | **PASS** — all rows render post-ABI (Input, Stylus wake, Pinch style included) at 125 % DPI; with the #50 default map, CC 31 = 127 morphs Sumi → **Ochre** and 64 → **Indigo** (the ring), live on existing ink | `layout_picker_eight.png`, `morph_cc31_{sumi,half,ochre}.png` |
| 8 | Installer unaffected | **PASS** — unchanged `.iss` builds against the 0.8.0 binary; silent per-user install, **Start-menu launch runs the decorated canvas**, `--version` reads the injected version, silent uninstall removes app + shortcut, settings survive | (transcript; step-29 `installer_roundtrip.log` shape) |

Flagged for the author: the handoff's item-1 wording expects the field gate
"bitwise" on this GPU — it has not been bitwise since Step 29 (recorded
there); the stable cross-batch bit-identity of the D3D11 dump is the
stronger equivalent this box can offer. The #68 fix is shared shell code:
macOS and Linux inherit it, and their INI-reload of layouts 6/7 is worth a
one-line re-check on each.

## Airwave remap — each hand stirs (DECISIONS_4 #69, ABI 0.9.0)

Author's direction after playing the #50 map ("hard to use"): symmetric
hands — Raise/Glide/Slide = strength/X/Y per hand (Y REVERSED: hand up =
centre up), left the vortex, right a new **Lamb-Oseen swirl** ctl trio;
Grasp = saddle/crossed pinch at its own hand's centre (delta-driven, nets
out on release); Tilt = ripple wavelength/amount; **Flex free** (unplayable
without disturbing the rest); viscosity/roughness/palette lose their routes
(settings sliders). `sumi_ctl_t` +5 dims → 0.9.0.

| Check | Result | Evidence |
|---|---|---|
| Suites | 5/5, **18 455** checks (new `test_global_ctl_swirl_and_pinches`: swirl trio + reversed Y + delta-in/out pinch + crossed pair; goldens moved) | ctest run in the step log |
| §4.6 field gate | GREEN, **bit-stable** (mean 6.845268e-06, unchanged — ctl dims never enter the fixture script) | gate run in the step log |
| Live, simulated Airwave CCs via loopMIDI | Raise R at an offset Glide/Slide R centre carried the drop in a long arc (the 1/r² far field); Grasp L squeeze-and-release folded it at the vortex centre | `airwave69_swirl.png`, `airwave69_grasp.png` |
| Chart | `midi-chart.json` GlobalCtl row rewritten; chart_check **32/32** | step log |
| Docs | README table, guide/devices.md, reference/settings.md (14 dims) | diffs |
| Tablets | iOS/Android name tables + defaults updated, **not compiled here** (standing pattern — next iOS/Android session verifies) | `SumiCanvas.swift`, `MainActivity.kt` |

Tuning knobs for the author's next session: `SWIRL_CTL_RATE` (3 rad/s at
full Raise — the live test read strong), `SWIRL_CTL_CORE_R` (0.15),
pinch `PINCH_K_SCALE` (shared with the CC 74 route). The user's INI was
updated to the new map (old persisted maps override defaults — the upgrade
migration question flagged after the #50 rollout stands, now sharper).

# Batch 11 — title bar everywhere; stock CC map follows #69; local DMG (DECISIONS_4 #70–#72)

Desktop: the canvas keeps its title bar on every platform (macOS call and Linux
hint removed); README/guide/reference updated; the Linux handoff drops its
borderless item. `packaging/macos/release.sh` accepts a single-arch bundle for a
`*-local` dry run (#72; verified below). CC map: an INI /
UserDefaults / SharedPreferences map equal as a set to the pre-#50 or #50 stock
map is upgraded to the #69 defaults on load (`ccmap_version=3` in the INI);
customised maps untouched. Verified on the Mac: the author's INI carried the pre-#50 stock
map and reloaded as the #69 layout (`[settings] CC map was the stock map of an
older version - upgraded …`); ctest 4/4; iOS compiles
against the rebuilt 0.9.0 libsumi; site check ok. Android uncompiled (handoff).
Local DMG dry run after #72: `release.sh build/desktop/midi-sink.app 0.0.0-local dist` → warning, ad-hoc sign, `dist/midi-sink-0.0.0-local-macos-arm64.dmg` (4.2 MB), codesign valid.

# Linux verification (Step 33) — DECISIONS_4 #73

Machine: the author's Linux box (Ubuntu 25.10, GNOME on Wayland, NVIDIA RTX
5090 / GL 4.1, 3840×2160 + 5120×2160@165). Clean configure at ABI **0.9.0**
(`build/` removed and rebuilt, `-j6`). Everything that needs a pointer or a
key was driven through XTest on the **X11** (Xwayland) session — a Wayland
surface takes no injected input, the Step-30 capture problem stands — and
MIDI was played into the ALSA **Midi Through** port the harness subscribes
to (`linux_automation/`: the helper, the three case scripts, the MIDI
file generator; `linux_checks_results.json` holds every measurement). One
Linux-only fix landed (#73); the author's `settings.ini` was backed up and
restored around the runs.

| # | Check | Result | Evidence |
|---|---|---|---|
| 1 | Build + suites + field gate + scripted tests | **PASS** — `--version` reads `libsumi 0.9.0`; ctest 4/4; §4.6 gate GREEN at the reference defaults (mean 6.85e-6, max 3.9e-3 — the same numbers this GPU has produced since Step 30, so the ten batches changed nothing in the field on GL; **not bitwise**, as recorded there — the handoff's "must still hold bitwise" line does not hold on this box and never did), negative control red; `--pressure-test` 6/6, `--stokeslet-test` 4/4 (mirror to a half-float ULP, det min 0.848 / mean 1.00012), `--ripple-group-test` 3/3, `--ripple-permanence-test` 1/1 | `version_linux.txt`, `ctest_linux.log`, `field_gate_gl_linux.txt`, `pressure_test_linux.txt`, `stokeslet_test_linux.txt`, `ripple_group_test_linux.txt`, `ripple_permanence_test_linux.txt` |
| 2 | Fullscreen + `--window` (#57/#58) | **PASS after #73.** X11: F11 fills the monitor holding the canvas (`[window] fullscreen on "DP-5" 5120x2160@165Hz`, core resized), `fullscreen=` toggles live in the INI (1 then 0); **the way back was wrong** — 1280×720 at y=755 came back 1280×683 at y=792 (mutter re-frames GLFW's restore) — fixed by re-asserting the remembered geometry for a second: two round trips, exact restore. Wayland: `--fullscreen` fills HDMI-1 3840×2160 and writes `fullscreen=1`; the F11 round trip there is the author's key press (no injection possible). `--window 1920x1080` opens exactly that on Wayland; `--window 12x7` exits 2 with the usage line | `linux_f11_before_fix.log`, `linux_f11_after_fix.log`, `linux_fullscreen_x11.jpg`, `linux_back_windowed_x11.jpg`, `linux_run_wayland_fullscreen_flag.log`, `linux_window_flag.log` |
| 3 | Input mode (#60/#62/#63) | **PASS** (MIDI files through Midi Through; the ROLI was not attached during the run — hardware pass is the author's). MPE: `normalizer: input mode override -> MPE`, a channel-1 C-E-G chord = three drops, CC 64 on/off changes nothing (ink count identical before/after). Classic: `override -> classic`, three per-note drops, a channel-1 bend sweep shears the picture, CC 64 nothing. Wind: `override -> wind`, one drop grown by CC 2 (ink 19 565 → 38 836 px), each legato change drags the sounding drop to the next note with a wake, CC 64 nothing. `input_mode=` persists in the INI (the runs were driven by it) | `linux_input_modes_and_rolls.jpg` (rows 1–3), `linux_input_mode_{mpe,classic,wind}.log` |
| 4 | Eight layouts (#64, #68) | **PASS** — `app_settings.cpp` names eight (rolls left/top/right/bottom) and the loader clamps `% 8` (#68 inherited); each roll set in the INI survives a launch (`layout=3/4/6/7` read back unchanged); the ink laid on the now-line drifts away from it in 3 s: layout 3 x 0.22→0.70, 4 y 0.22→0.70, 6 x 0.78→0.30, 7 y 0.78→0.30 (centroids) | `linux_input_modes_and_rolls.jpg` (row 4: layout 6), `linux_checks_results.json` |
| 5 | Gestures (#49/#53) | **PASS** — Shift+right hold 3 s lays a drop and feeds it to a large disk (5 803 px per plain click → 54 034 px); Shift+right pull on the disk's rim lays a drop there and swirls the rim into a paisley (Lamb-Oseen); middle drag with **Viscous stroke** pulls the disk into a point with the trailing V; right drag with **Rankine** turns it as a rigid piece with the crease | `linux_gestures.jpg`, `linux_gesture_swirl.jpg`, `linux_gestures.log` |
| 6 | Packaging unaffected | **PASS** — `cpack -G DEB` from the 0.9.0 build: 11 files as in Step 30; clean `ubuntu:24.04` container installs it, `--version` reads the injected version, `desktop-file-validate` OK, launching through the `.desktop`'s `Exec=/usr/bin/midi-sink` renders 634 frames, `apt-get remove` cleans up. The canvas keeps its title bar (#70) | `deb_container_linux.log` |

Found on the way: the settings window takes keyboard focus at launch on X11
(the canvas is opened first, the settings window second), so F11 and Shift
pressed right after launch go to the settings window until the canvas is
clicked — the same on every platform by construction (GLFW focus follows the
last-created window) and consistent with the "do NOT refocus" comment in
`main.cpp`; a user clicks the canvas first anyway. Not changed.

## Wayland, driven through mutter's remote-desktop API

The author's two live reports while this ran — "the image only takes a tiny
square", then "the click with the mouse is not working" — were chased on the
Wayland session itself once an input path existed: GNOME denies every
screenshot/injection route to a plain client, but **mutter's own session-bus
API** (`org.gnome.Mutter.RemoteDesktop` + `ScreenCast`, what gnome-remote-
desktop uses) needs no dialog; `linux_automation/mutterrd.py` wraps it
(pointer, keyboard, one PipeWire frame through `gst-launch-1.0`).

| Check | Result | Evidence |
|---|---|---|
| Wayland F11 round trip | **PASS** — `[window] fullscreen on "HDMI-1" 3840x2160@60Hz`, the core follows (3840×2160), the INI toggles 1 → 0, the canvas comes back 1280×720 (Wayland has no positions to restore; the #73 settle re-asserts the size only) | `linux_wayland_results.json`, `linux_wayland_session.jpg` |
| Wayland pointer: Shift+right hold, left drag, right drag | **PASS** on the canvas — the pressure gesture fed a large drop where the canvas was exposed | `linux_wayland_session.jpg` (frame 5) |
| **"Click not working"** | **Explained, not a bug in the click path.** On Wayland GLFW cannot place windows, so GNOME centres both: the settings window (created second) lands ON TOP of the canvas's middle. Every injected click at the canvas centre went to the settings window (frame 2: the Ripple *Amount* slider moved to 16 under the pointer) and the `--dev` mouse log recorded no canvas button at all; a click on the exposed part of the canvas lays a drop and raises the canvas over the settings, which is when "it works now". On X11 the two windows sit side by side (`[settings] window at 7056,755 (canvas 5760,755 …)`), as on macOS/Windows | `linux_wayland_session.jpg` (frames 0, 2), `linux_wayland_click.log` |
| "Tiny square" | **Not reproduced** — most likely the same overlap seen from the other side (the canvas mostly hidden behind the settings window), or a launch that inherited `fullscreen=1` from my `--fullscreen` run while the author was watching (restored to 0 since). The framebuffer path is right on both sessions: every frame fills its window (X11 capture at 3840×2160, Wayland frames) | `linux_fullscreen_flag_x11.jpg`, `linux_wayland_session.jpg` |

**Author's call: leave it (option b).** On Wayland the app cannot arrange
its two windows (no positions, no focus requests); re-mapping the canvas to
put it on top would hide the settings window behind it instead. Recorded as
DECISIONS_4 #76. `--dev` now logs every canvas mouse button (`[mouse] button
…`) so the next report can be read off the log.

# Android verification (Step 33) — build

The Mac's uncompiled Android code **compiles** (`assembleDebug`, AGP 9.3.2 /
Gradle 9.5 / Kotlin 2.2.10 as the tree now pins them, NDK r27, `-j6`) after
two mismatches found by reading it before the first build:

* `sumi_jni.cpp` — the "Restore default map" table (an empty CC map from
  Kotlin) was still the **#50** layout (viscosity / roughness / palette on
  29–31, 27/28 the ripple); replaced by the #69 symmetric-hands map, verbatim
  `app_settings_default_routes`, so the three copies (core, desktop, Android)
  agree again.
* `MainActivity.kt` — the CC editor's "Dimension" cycle stopped at 9 of the
  14 dimensions (the swirl trio and the two pinches were unreachable) and
  the footnote still named Flex; now `% CcMap.ctlCount` and the #69 wording.

Build-environment findings: the tree pins CMake **4.4.3** (the Mac's
Homebrew CMake) in `app/build.gradle.kts`; the Android SDK manager offers
3.31.6 at most, so this box needed Kitware's 4.4.3 tarball under
`~/.local/opt` and `cmake.dir=` in the untracked `local.properties` (noted in
`android/RELEASING.md`). The Gradle daemon JVM criteria from Step 31 still
apply (JDK 21). Build logs: `gradle_build_linux.log`, `gradle_build_16k_linux.log`.

# Android verification (Step 33) — on the device — DECISIONS_4 #74, #75, #77

Device: the author attached a **Pixel 9 Pro** (Android 17, 960×2142 @2.25×,
a phone) — not the Galaxy Tab. Everything below ran on it through adb
(`android_automation/`: uiautomator dumps, `input tap/swipe/motionevent`,
`run-as` for the prefs and the byte log; the phone auto-rotated to portrait
mid-session, so the driver reads the orientation before every gesture).
What a phone cannot show is marked YOURS for the Tab. Three Android fixes
landed here beyond the two compile-time ones above:

* **16 KB pages (#74)** — the first launch opened the system warning
  "`libsumi-shell.so`: LOAD segment not aligned"; `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`
  in the CMake arguments; `readelf` 0x1000 → 0x4000, warning gone.
* **Control strip on phones (#75)** — author's request: "Show the control
  strip" (CONTROL STRIP section), default off below 600 dp, on for tablets.
* **Startup ripple replay (#77)** — the persisted ripple sliders were never
  re-sent on a cold start (`onCreate` filled nothing into `cc_replay`);
  `sendRipple()` added. Plus, at the author's request, the **CANVAS section
  (paper dip) leads the sheet**.

| Batch | Check | Result | Evidence |
|---|---|---|---|
| build | The Mac's uncompiled code | **PASS** — compiles after the two mismatches above (JNI default map, 14-dimension cycle); installs and runs (`sumi 0.9.0 ready`, `input mode override -> MPE`, MCM lower zone 15 members) | `gradle_build_linux.log` |
| #49 pressure gesture | Long press lays a drop; hold / push up grows it; no drop on lift; tines unchanged | **PASS** — tap = one drop; `motionevent DOWN` + 0.7 s = a second drop under the finger (paper-coloured by parity, the ring in the sheet); MOVE up 250 px + 1.3 s = the pressed band grows to the screen's width, pushing the first drop out (the FEED pass); pull back + lift = no new drop (frame identical to the uniform sheet); a swipe after = a tine on a uniform sheet, invisible as expected. Metrics are change-vs-blank (dark-pixel counts fail on paper-coloured drops) | `android_pressure_gesture_and_rolls.jpg` (g1–g6), `android_gesture_metrics.json` |
| #53 stylus wake fluid | Viscous stroke changes the pen wake; Spread row; persists | **PARTIAL** — the row toggles (doublet ↔ viscous), the Spread row appears only for viscous, its «‹›» steps move the value (3.0 → 5.0), `wakeViscous`/`wakeSpread` persist in the prefs and reload; **the pen's look is YOURS** (a phone has no S-Pen) | `android_final_checks.json`, `android_final2_checks.json` |
| #54 S-Pen in Marble mode | Pen draws the wake, no tine, no drop, no long-press | **YOURS** — no stylus on the phone; the code path is the `TOOL_TYPE_STYLUS` branch of `onTouchEvent` | — |
| #56 settings parity | Every desktop row present; palette / viscosity / feed / roughness step; tempo rows on rolls; Vortex row; Ripple rows ride CC 102/103 as source 2; removing the route hides Amount; restore; persistence | **PASS** — all 36 expected texts present (8 layouts, INPUT ×3, VORTEX, STYLUS WAKE, RIPPLE ×3, CC MAP with the 14 names, Add / Restore, CANVAS, ABOUT `midi-sink 0.5.0 (52) · 0.5.0-rc.5-11-g04b78e5-dirty` / `libsumi 0.9.0`); Palette cycles Sumi → Indigo → Ochre; Viscosity « » 0,50 → 0,70 (French locale decimal); Tempo + Roll speed rows appear on Piano roll (right); Vortex Exponential ↔ Rankine with its footnote; Amount » = **CC 29 as source 2** in `midi_log.csv` (`176,29,48,2` + `176,28,50,2` — the #69 default routes, footnote "Sent as CC 29 / CC 28"); removing the CC 29 route keeps Amount (CC 102 still maps it: footnote "CC 102 / CC 28"), Restore brings CC 29 back; kill + relaunch: the prefs hold every value and the sheet re-reads them; **ripple values re-sent at startup only after #77** (first two lines of a fresh log: `176,29,48,2`, `176,28,50,2`, then again at instance creation) | `android_settings_rows.json`, `android_final_checks.json`, `android_final2_checks.json`, `android_midi_log_ripple.csv`, `android_startup_replay.json`, `android_midi_log_startup.csv`, `android_settings_bottom.jpg` |
| #60 input mode | MPE / Classic / Wind with a channel-1 keyboard over USB; CC 64 nothing | **PARTIAL** — the INPUT rows exist, the default logs `input mode override -> MPE` at every start and the setting persists; **the keyboard-over-USB behaviour is YOURS** (nothing to host on the phone here; the desktop proved the three modes with the same core) | logcat, `android_settings_rows.json` |
| #64/#65 layouts | Eight entries, "(playable)" labels, rolls right / bottom scroll from their edge, tempo rows | **PASS** — eight names with "(playable)" on the three lattices; `--ei layout 6/7` accepted (`layout -> 6/7`); a drop laid at the centre drifts −x (right roll: x 0.5 → 0.39 in 0.6 s) and −y (bottom roll: y 0.5 → 0.34) and is off-screen after 3.6 s — the roll speed is in canvas heights, so a portrait phone's horizontal roll clears in ~3.6 s; tempo + roll-speed rows show | `android_pressure_gesture_and_rolls.jpg` (r6, r7), `android_final_checks.json` |
| #69/#71 CC map | The 14 names, #69 defaults, an older stock map upgrades on load | **PASS** (migration) / **YOURS** (Airwave) — the CC MAP rows list the #69 routes (27 Swirl strength, 25/23 swirl X/Y, 20/21 pinches, 28/29 ripple); with the **#50 stock map written into the prefs** and a cold start the sheet shows the #69 routes without "Restore" (`prefs_ccMap_stored` = the #50 string, rows = #69 names); the Airwave was not attached (and needs ROLI's host software, unlikely to speak to a phone) | `android_final2_checks.json` |
| suites | `--es hostmpeTests 1` | **PASS** — hostmpe 1569 checks, normalizer/mapper **18 455** checks on arm64 | `android_final_checks.json` |
| byte log | `tools/midi_asserts.py device` on a Play-mode session (8 touches, 2 swipes) | **PASS — ALL ASSERTS PASS**: MCM ordered, RPN 0 on 15/15, every Note On preceded by its bend, every strike by a centre bend, releases balanced, strip on the master only, sustain never sticks; source 2 carries the config + the ripple replay (160 msgs) | `android_midi_log_play.csv` |
| USB-MIDI to this box | `amidi -l`, `midi_capture_alsa` | **YOURS on the Tab** — the phone stayed in adb mode (a USB-mode flip drops the adb link this session ran on) | — |
| strip (#75) | Phone default hidden; toggle shows | **PASS** — chromatic grid in Play mode comes up without the strip; the toggle shows Pitch / Mod / CC 23 / CC 24 / Sus and hides it again | `android_strip_hidden_default_phone.jpg`, `android_strip_shown_after_toggle.jpg` |
| sheet order (#77) | CANVAS first | **PASS** — `midi-sink`, CANVAS, the two paper-dip rows, then LAYOUT & LOOK | `android_settings_canvas_first.jpg` |

Found on the way (Android): drops alternate ink / paper colour by parity, so
any pixel-count check of a *second* drop must diff against the blank sheet;
`midi_log.csv` is flushed only when the settings sheet closes (read it
after a dismiss, not after a launch); the roll speed's unit makes portrait
phones roll fast; `input motionevent` works on Android 17 for the long press.

Follow-ups for the iOS owner: mirror CANVAS-first (#77) and check whether
the iPad re-sends its persisted ripple CCs on a cold start (#77's pattern).

**Tree ready** — evidence `docs/evidence/step33/` (Linux + Android sections
appended), decisions #73–#77. The author commits.

# Batch 12 — Canvas section first (DECISIONS_4 #73)

Desktop window, web panel and iOS sheet open with Canvas (paper dip, save print);
Android already did. Desktop builds, iOS compiles, web `node --check` ok.

# Docs — rounded web icons; the real gallery (DECISIONS_4 #74, #75)

`tools/gen_icons.py --only site`: favicon-32/180, logo 512, og.png 1200×630 on
cream, and `web/site/favicon-180.png` (the marble page now has an icon). Gallery:
five real performances with synth and "based on" fields, all five with videos
(0YMdfW700BY, CJXT1IwcI-A, wD_wSZv09-s, DWGBWi4C98o, 1HPt0arkAdE); Ali Paşa stays
the Jaffer tribute. Site build + check ok.
