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
