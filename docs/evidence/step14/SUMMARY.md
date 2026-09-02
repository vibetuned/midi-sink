# Step 14 — Android shell (Jetpack Compose): DONE evidence

Device: Samsung Galaxy Tab S8 Ultra (SM-X906B), Android 16 (API 36),
Adreno 730, OpenGL ES 3.2, 2960×1848 @ 120 Hz panel. Build: Gradle 8.14 /
AGP 8.10 / Kotlin 2.1.20 / NDK r27c (27.2.12479018), arm64-v8a,
`externalNativeBuild` → repo-root CMake (iOS-precedent wiring). EGL surface
capped at phone-class pixels per DECISIONS_2 #31 (panel 5.5M px → surface
1480×924; sim_scale 0.75 → 1110×693 field). All scripted runs driven by
`adb shell am start` Intent extras (`stressMinutes`, `fieldDump`); CSVs are
the render thread's per-second `t,fps,worst_frame_ms,thermal` log pulled
with `run-as`.

| DONE criterion | Result | Evidence |
|---|---|---|
| Osmose stress ≥ 55 fps for 10 minutes at sim_scale 0.75, graceful thermal degradation logged | **min 119.6 / avg 120.9 fps over the full 10-minute window (597 s samples), 0 seconds below 55** (vsync-limited at 120 Hz), worst frame 17.58 ms; **1,367,339 messages, dropped = 0**. Thermal step-down demonstrated live via `cmd thermalservice override-status 3` at t=421 s: `sim_scale -> 0.60 (thermal severe)`, sim targets 888×554, recovery to 0.75 at t=512 s — both as CSV events, drawing preserved across both (DECISIONS_2 #28/#31). PSS flat: 180.7 MB @ t=60 s → 180.6 MB @ t=633 s | `stress_10min.csv`, `stress_10min_logcat.log`, `stress_memory_series.log` |
| GLES3 passes the §4.6 cross-backend field regression | **PASS vs the committed Metal fixture** (max 1.51e-2, mean 3.76e-4) **and vs the step-12 desktop-GL dump** (max 1.71e-2, mean 3.81e-4) under the documented mobile-tier tolerance 2.5e-2 / 1e-3 (DECISIONS_2 #30); aux ≤ 4.9e-4; only 54/262,144 texels over the desktop-tier 1e-2, 99.9% ≤ 6.8e-3. Dump **bit-identical across runs** (deterministic Adreno fp16-filter rounding, not noise — unchanged bit-for-bit with GL_DITHER on or off). Zero new flip code: `flip_vert_y` applies to glsl300es exactly as to glsl410, screen composite unflipped, readbacks straight-copy | `field_512_gles3.bin`, `field_regression.log` |
| ×10 surface destroy/recreate mid-stress: zero EGL errors, zero native crashes, nothing leaked | **11 attaches / 10 releases** (10 home/relaunch cycles while the stress feeder saturated the render thread): **EGLERR sweep = 0** (every EGL call is checked, DECISIONS_2 #32), **crash buffer empty**, `sumi_create` count = 1 — the EGL context and the performance survived every cycle. PSS across the 10 cycles: 134.0 → 135.4 MB (flat). A prior mixed run (5 rotations + 5 relaunches) is included: rotations only fire surfaceChanged on this device (surface preserved under configChanges + fixed size), exercising #28's resize preservation instead; same zero-error result | `teardown10_run.csv`, `teardown10_logcat.log`, `teardown10_memory.log`, `teardown10_crash_sweep.log`, `teardown_run.csv`, `teardown_logcat.log`, `teardown_memory_series.log`, `crash_buffer_sweep.log` |
| Bluetooth MIDI from the ROLI works end-to-end | In-app BLE scan → `MidiManager.openBluetoothDevice` → AMidi: `BLE-MIDI opened 48:B6:20:23:06:13`, `MIDI input opened: ROLI Piano 49 B51D`, **`input mode -> MPE`**, dropped = 0; expressive playing painted pressure-fed rings/glide swirls on the canvas (user-confirmed live, screenshot attached). First test exposed a double-attach (BLE-open callback + hotplug callback both fired) — fixed with device-id dedup and re-verified: attach count = 1, either callback-order handled | `bt_midi_logcat.log`, `bt_midi_roli_canvas.png` |
| Desktop Linux build + ctest still green (shared swapchain TU) | Full rebuild clean, ctest 2/2 (8,934-check normalizer suite unchanged), and the desktop `--field-dump` remains **bit-identical to the committed step-12 fixture** after the SOKOL_GLES3 split, GL_DITHER disable and ES readback fallback | `desktop_unregressed.log` |

Teardown contract (§5.4): `surfaceDestroyed` blocks on a condvar until the
render thread finishes its in-flight frame, unbinds via
`eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)`, destroys the EGL
surface and releases the ANativeWindow (DECISIONS_2 #32). The context is
created once and lives for the app — the ×10 race above is the direct test.

Settings menu (iOS SettingsSheet sibling): the translucent gear opens a
dialog with the same five-layout picker in the same order (Circle of fifths /
Chromatic grid / Jankó / Piano roll H / Piano roll V) plus the Bluetooth-MIDI
pairing entry. Verified on device: selecting Piano roll (horizontal) moved
the radio and logged `layout -> 3` (the core switched live via the
render-thread command queue); the picker routes through `nativeSetLayout` →
`sumi_set_params`. Unlike iOS there is no manual sim_scale toggle — on Android
the PowerManager thermal listener owns sim_scale (0.75 ↔ 0.6), so exposing a
second control would fight it. Evidence: `settings_menu.png`,
`layout_roll_h_canvas.png`.

Not covered here: the on-device gesture set (tap/drag/twist) was exercised
manually alongside the ROLI session; it reuses the desktop/iOS constants
verbatim through the render-thread command queue.

New code: `android/` (Compose shell + `cpp/sumi_jni.cpp` JNI host),
`ANDROID` branches in root/core CMake (static archive only, GLESv3
interface link), `SOKOL_GLES3` split + ES3 checks/fallback + dither disable
in `core/src/swapchain_gl.cpp` (DECISIONS_2 #29–#34).
