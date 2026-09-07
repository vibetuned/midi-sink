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
