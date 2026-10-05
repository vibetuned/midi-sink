# Step 64 — The Android play surface: the instruments on the Tab — the Mac's evidence

`DECISIONS_8 #18`, the core fix `#19`, the author's report `#20`. Spec: as step 63 (INSTRUMENT §2–§5; QOL §2 the undone
items). Machine: the author's Mac, the Galaxy Tab S8 Ultra over adb. The
iPad's step 63 (#13–#17) read as the specification; the Tab's own
architecture kept (hostmpe on the AMidi poller thread, every UI call a posted
command, touch-down a sync hop, the params snapshot the UI thread's probe
truth). One core touch, a fix (#19): the default params were undefined for
the two Phase-9 fields — found in the Tab's session file, fixed under bug →
regression test → fix; libsumi stays 1.5.0.

## What changed in the tree

- `android/cpp/sumi_play.cpp`: the fingering mirror and the held cells on
  the MIDI thread, `fingering_changed` (the retune: the trumpet's cell
  re-probed under old and new state and ramped 30 ms, the trombone's slide
  delta at once), `hostmpe_tick` in the drain, the announce of nine,
  `nativeTouchBegin` with the attack offset and the cell,
  `nativeThereminBegin/Move`, `nativeStripValveDown/Up`,
  `nativeStripSlideSet`, `nativeStripQuickSet/Next`, `nativeSetMirror`,
  `nativeSetAspect`, `nativeDeviceProfile`, the panic's strip reset,
  `nativeStripState` of ten, the probe and the sweep under the state (the
  flags; the theremin's field as its slots).
- `android/cpp/sumi_jni.cpp`: `nativeVoxoSetSource` (the source and Suzu's
  patch, the iPad's table).
- `NativeBridge.kt`: the new entry points and the widened signatures.
- `FingeringPanelView.kt` (new): the Swift view's twin — the pads or the
  slide, vertical or horizontal, the rotate button, mirrored left-handed;
  the mirror exact by construction (the slide quantised as the engine
  sends it).
- `PlayOverlayView.kt`: the fingering handed to every probe, the brass
  cells' notes refreshed on a change, the mirror (a canvas flip, the
  touches' x), the theremin path, the trombone's attack offset, the aspect
  to the native side.
- `ControlStripView.kt`: the dynamic row (Next, Panic), `preferredWidthDp`.
- `MainActivity.kt`: the panel in the frame and `layoutPlaySurface` (the
  strip's corner, 62 dp short of the gear at the right; the panel at the
  side or along the bottom), the playable set of eight, the lattice key
  (layout, arc, tuning), left-handed, the panel's form, quick-switch, the
  offer alert, the native panic, the fingering demo, the lab extras.
- `SettingsSheet.kt`: thirteen layouts, the arc and the panel's form under
  the brass, the tuning under Strings, the Mode note, left-handed and the
  quick-switch toggles, the Instrument row naming what sounds, the Sound
  page's Source row and patch picker, the demo action.
- `Sound.kt`: the source and the patch (persisted), `activeName`, the reach
  the sampler's alone; `MidiInputs.kt`: `onSourceAppeared`, `forEachOpen`.
- `core/src/engine.cpp` (#19): `default_params` zeroes the struct and names
  `trumpet_arc` and `string_tuning`; `desktop/src/main.cpp`,
  `desktop/src/dev_tools.{h,cpp}`: the params captured at `sumi_create` and
  the bench's `--defaults-test`; `android/cpp/sumi_jni.cpp`: the session
  healed from the core's clamped params.
- `android/cpp/sumi_jni.cpp` (#20, the author's report): `nativeSessionPatch`
  updates the probe snapshot at once, so the lattice swept on a layout
  change is the new layout's; the sweep and the session change log a line.
- `_work/DECISIONS_8.md` #18–#20; `docs/CHANGELOG.md`; `CLAUDE.md`.

- `android/app/src/main/java/com/vibetuned/midisink/MainActivity.kt`
  (#21): the fingering panel in a window of its own (`syncPanelWindow`: a
  split-touch, non-modal sub-window over the frame, laid at the rect the
  frame gave it, present on the brass layouts in Play mode while the
  activity is started) so the fingers' input stream never meets the S
  Pen's; `FingeringPanelView.kt` releases its grabs through the engine on
  a mode or form change.

## The device runs (`tab/`)

Each: a fresh launch (`am start` with the extras) with the layout, Play
mode, `--es fingeringDemo 1` (the scripted phrase three seconds in,
through the real path) and, for the brass, `--es voxoSource suzu`; two
`screencap`s pulled from the Mac, the byte log and the latency log pulled
with `run-as`; the analyser (`tab/midi_asserts.txt`) run on every log —
every assert holds on all nine logged runs (the mirrored trumpet runs
without the demo: no log). The author's session file and preferences were
saved before the runs and put back after them, verified byte for byte.

| run | the log shows | the capture shows |
|---|---|---|
| `trumpet_column` (Suzu as the source) | the announce of nine on the master; B♭3 struck open; CC 111 = 127 then CC 110 and 112 on the master with the voice's bend stepping under them; the lift; the valves up; the open series | the eight-cell column, the valve panel at the left (three stacked pads under the rotate button), the strip at the top-right clear of the gear |
| `trumpet_arc` | the same phrase | the ring of eight small cells in the sheet's middle-right |
| `trombone` | the slide's 42 CCs (113) and the voice bends under one held partial | the vertical slide at the left with its seven numbered positions, the seven cells in a column |
| `trombone_arc` | the same slide phrase | the trombone's seven cells on the ring |
| `theremin` | one note on, one off, the bends and pressures of a two-octave sweep | the 61 semitone slots along the middle, the drop at C4 |
| `strings_guitar` | the scale, 32 messages | the 6 × 25 fretboard, every note lighting every string that reaches it |
| `wicki` | the scale, 32 messages | the hex field |
| `trumpet_mirror` (no demo) | — | left-handed: the valve panel at the right, the strip at the top-left |
| `trumpet_horizontal` | the same phrase as the column | the three pads side by side along the bottom edge, the rotate button at the panel's corner |
| `trombone_horizontal` | the slide's CCs and the voice bends as the vertical run | the slide along the bottom edge, the 1st position at the left |

## The author's report: "the previous layout persists" (#20)

Reproduced on the Tab through the intent path (the same session patch the
picker sends): the core logged `layout -> 10` on its render thread while
the lattice sweep logged `layout 5` — the shell's probe snapshot was
written only when the render thread applied the patch, a frame after the
sweep. The patch updates the snapshot at once now. Verified with four live
switches on the running app (the ring → 8 cells, the strings → 150, Jankó
→ 252, each the new layout's); the author's session restored byte for byte.

## The author's report: the S Pen and the fingers (#21, `tab/stylus/`)

"We cannot play the partials with the S Pen while playing the valves or
the slider." Measured with virtual devices through `/system/bin/uinput`
(`uinput_gen.py` registers a touchscreen and a pen with the Tab's own
classes and writes the sequences `*.json`; the trumpet in Play mode, the
panel horizontal as the author keeps it): the pen coming into HOVER range
cancels the finger's gesture (`ACTION_CANCEL`, device 0, source 0) and
the finger's lift never arrives; a finger press while the pen hovers or
touches never reaches the app; the cancelled finger never revives; a
window focus loss sends the same cancel (`before_fix_logcat.txt`). The
rule is the dispatcher's one-device-per-window, the stylus preferred, so
the panel is a window of its own now (a latch across the cancel was
tried first and rejected by the author). After the fix
(`after_fix_bytes.txt`, `after_fix_logcat.txt`, `pen_moving_bytes.txt`,
`panel_window_*.png`): the pen's note 75 under a held valve where 77
sounded before, the finger's lift arriving, a press under a held pen
note arriving and re-articulating the pen's note on its next move, a
press during the pen's hover arriving, no dispatcher cancel against the
app's windows (`after_fix_dispatcher.txt`). On the real glass
(`real_glass_kernel.txt`, `real_glass_logcat.txt`,
`real_glass_reader.txt`): the kernel reports the finger while the pen is
down, the panel's window receives it, and the Tab's InputReader cancels
it (`FLAG_CANCELED`) at the pen's hover enter and skips touch "while pen
is in use" — the reader's stylus-over-touch rule, below every window,
keyed on the real S Pen. The pen plays alone on this Tab; the brass with
the pen is the wire's fingering CCs from any controller.

## The latency gate (`latency/`)

"Touch latency unchanged from Phase 4 (the probe stayed pure — measured,
not assumed)": the in-app marks (touch-down → the first rendered frame,
touch-down → `sumi_push_midi`), 48 scripted `input tap`s on the chromatic
grid in Play mode, the SAME script on two builds the same evening — the
committed tree before this step (a scratch worktree of `f2916db`) and this
tree — beside the Phase 4 record (step 22's evidence).

| build | n | touch → rendered frame (ms): min / median / p90 / max | touch → push median |
|---|---|---|---|
| Phase 4, step 22 (the Linux box, 2025) | — | — / **3.73** / 8.19 / 11.55 | 0.06 ms |
| the tree before step 64 (`baseline_step63_tree.csv`) | 46 | 0.86 / **4.89** / 10.05 / 11.57 | 0.46 ms |
| step 64 (`step64.csv`) | 46 | 0.56 / **4.57** / 9.36 / 12.53 | 0.33 ms |

The two builds are within each other's noise under identical taps (the
max is a frame boundary at 120 Hz: 8.3 ms a frame); both sit beside the
Phase 4 figure. The probe is still instance-free and the touch-down hop
unchanged — the fingering state rides along as two extra arguments.

## The suites and the gates, after the core fix (`defaults_test.txt`, `normalizer_tests.txt`, `ctest.txt`, `gates/`)

| check | result |
|---|---|
| `midi-sink --dev --defaults-test` | the documented defaults, `trumpet_arc` 0 and `string_tuning` the guitar among them |
| `normalizer_tests` | OK, 43 630 checks |
| ctest | 10 of 10 |
| the field and composite gates, Metal | bitwise after the fix (the fixture is the piano grid) |

## The other builds

| build | result |
|---|---|
| Android `:app:assembleDebug`, adb install + the ten launches, then the fixed core installed and launched | BUILD SUCCESSFUL; every run launched, logged and captured |
| iOS `build-ios`, xcodegen, xcodebuild, devicectl install + launch | the fixed core on the iPad |
| web `cmake --build build-web` | builds |

## For the author

- **The Tab plays every instrument** the iPad does; the panel's form, the
  arc and the tuning are under the layouts in the settings; the Sound page
  has the Source row. Lab extras: `--ei layout N`, `--es trumpetArc 1`,
  `--ei stringTuning N`, `--es mirror 1`, `--es fingeringHorizontal 1`,
  `--es fingeringDemo 1`, `--es voxoSource suzu`, `--ei suzuPatch N`.
- **By hand:** the S-Pen plays the instruments too (the theremin takes
  the pen as a finger). **The valves and the slide with the pen (#21):**
  on this Tab the input reader cancels and skips every finger while the
  S Pen hovers or touches (your test, the reader's own log line), below
  any window — the pen plays alone; two hands of fingers play the brass,
  and the valves and the slide from a controller's fingering CCs (110–113)
  play under the pen. The panel's own window stays: it is the right
  structure, and a build with multi-device input on would need it.
- Step 65 is the session replay.
