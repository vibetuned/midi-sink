# Step 13 — iOS shell (SwiftUI): DONE evidence

Device: iPad Air 11-inch (M4), iPadOS 26.6.1, 60 Hz display. App: `ios/`
(SwiftUI, xcodegen `project.yml`), linking the CMake-built `libsumi.a`
(`build-ios/`, arm64 iOS) through `core/include/module.modulemap` — Swift
`import SumiCore`, no Objective-C wrapper (§5.4). MIDI source: ROLI Piano
over Bluetooth MIDI, paired with the in-app CoreAudioKit picker.

**Session-length note:** the spec author shortened the DONE session from 10 to
5 minutes at run time (their call, given the flat early numbers); every other
criterion is unchanged.

| DONE criterion | Result | Evidence |
|---|---|---|
| ROLI over Bluetooth MIDI paints MPE drops at sustained 60 fps, no thermal collapse below 50 fps | 5.51-minute traced session on the Jankó layout: **327 straight seconds at 60.00 fps flat, zero seconds below 50, worst frame 0.89 ms**. Instruments Thermal State shows **one unbroken Nominal interval for the whole trace** — the device never even reached Fair. | `thermal_session.trace` (open in Instruments), `thermal_intervals.xml` (exported table), `session_log.csv` (in-app 1 Hz fps/worst/thermal log; traced window t=616–947) |
| Xcode Instruments thermal log attached | `xctrace record --instrument 'Thermal State'`, 330 s, deferred mode. | `thermal_session.trace` |
| Backgrounding/foregrounding never crashes | Repeated home-swipe and app-switcher cycles (plus rotations) — display link pauses on `.background`, resumes on `.active`; **zero crash logs on the device after the fixes** (the only three `.ips` files predate the `metal_ios` dialect fix, see DECISIONS_2 #24). Frame log shows clean 60 fps resumption after every pause gap. | `background_rotation_log.csv`, device crash-log listing (empty post-fix) |
| Touch marbling works | Tap = drop, one-finger drag = tine, two-finger twist = vortex, using the desktop harness's gesture constants. Required simultaneous gesture recognition (the pan claimed the first touch and blocked the rotation recognizer). User-verified feel. | `ios/Sources/SumiCanvas.swift` |
| Core static library bit-identical to the macOS one; no `#if TARGET_OS_IPHONE` in core outside the swapchain TU | Interpreted per DECISIONS_2 #27 (Mach-O platform load commands differ by definition): identical source list, flags, and **identical exported-symbol tables (283 symbols, `nm` diff empty)** between `build/core/libsumi.a` and `build-ios/core/libsumi.a`; **zero `TARGET_OS_*`/`__APPLE__` conditionals anywhere in `core/`** — even `swapchain_metal.mm` needs none. | DECISIONS_2 #27 |

Issues found and fixed during the step (all logged in DECISIONS_2):

- **#24 — `metal_ios` shader dialect**: with only `metal_macos` compiled in,
  the first `sg_make_shader` on a real iPad aborts (three pre-fix crash logs
  on the device). One string in `cmake/CompileShaders.cmake`.
- **#28 — resize erased the performance** (user-reported): iPad rotation —
  and iOS's app-switcher snapshot layout passes, which resize the view in
  BOTH orientations on every backgrounding — hit the recreate-and-identity-
  init path. The core now carries the drawing across any resize/sim_scale
  change with a passthrough resample (§4.2 payload is resolution-
  independent); a pristine field keeps the exact identity init, so the §4.6
  field dump stays **bit-identical to the committed Metal fixture**
  (verified with `cmp`). Desktop-verified with a chevron surviving two
  programmatic resizes (`--demo-chevron --resize-test`). Shell defers
  resizes while backgrounded.
- **Screen sleep during a performance** (user-requested): any MIDI byte or
  touch arms a 3-minute idle-timer hold, re-armed while playing, released on
  background or after inactivity — host-side only.
- Settings sheet: pitch-layout picker (all 5 layouts, live), sim_scale
  toggle (default 1.0 on Metal GPU family apple7+, else 0.75 — DECISIONS_2
  #26), Bluetooth MIDI pairing pushed inside the settings navigation stack,
  live fps/thermal status line.

Regression state at step end: `ctest` 2/2 green (8,934 checks), §4.6
three-way field regression PASS (Metal fixture `tests/fixtures/
field_512_metal.bin` vs step 11 D3D11 and step 12 GL dumps: max|Δ| 3.9e-3,
mean 6.8e-6, aux bit-identical), Metal dip/burst revalidated after the
step-11 readback-seam refactor (chevron print, double-buffer + WARN refusal).
