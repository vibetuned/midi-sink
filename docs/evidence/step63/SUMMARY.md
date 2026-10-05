# Step 63 — The iOS play surface: the instruments on the iPad — the Mac's evidence

`DECISIONS_8 #13`, the author's fix rounds `#14`–`#17`. Spec: INSTRUMENT §2–§5; QOL §6 (the roadmap's reference; the undone
items are QOL §2). Machine: the author's Mac, the iPad Air 11-inch (M4)
over devicectl. The author's input — the trumpet arrangement by eye — is
served by the captures below; the author's own session file was saved
before every round of runs and put back after it.

## What changed in the tree

- `presets/src/sumi_preset.c`, `tests/preset_tests.c`: `trumpet_arc` and
  `string_tuning` join the field table (steps 60–61 left them out; every
  shell's session carries them now).
- `ios/Sources/SumiApp.swift`: thirteen layouts, the arc toggle (under both
  brass layouts since #17) and the tuning picker, the fingering panel's form toggle under the Trumpet and
  the Trombone ("Fingering panel horizontal (along the bottom)", #16 —
  visible in either mode), left-handed, the quick-switch subset, the demo
  button, the per-device offer alert (`DeviceOffers`), the transient
  `--play` / `--mirror` / `--fingering-horizontal` and the
  `--fingering-demo` / `--capture` hooks.
- `ios/Sources/SumiCanvas.swift`: the fingering mirror and its queue copy,
  the held voices' cells, `fingeringChanged` (the mirror re-read, the brass
  retune), the theremin path, `hostmpe_tick` on the frame drain, the
  fingering panel's placement (at the side at mid-height, or along the
  bottom edge) and the strip's corner (stopping short of the settings gear
  at the right, #17), `toggleFingeringOrientation` (the
  panel's button, persisted), mirroring, the panic's strip half, the offer
  wiring, the fingering demo; the announce buffer of twelve.
- `ios/Sources/PlayOverlayView.swift`: the state handed to every probe, the
  theremin's slots and touch path, the trombone's attack offset, mirroring.
- `ios/Sources/ControlStripView.swift`: the dynamic row (Next, Panic), the
  pads; `ios/Sources/FingeringPanelView.swift` (new, the author's fixes):
  the valves as three large pads and the slide as a track with the seven
  positions numbered — stacked / vertical at the side (#14), or side by
  side / horizontal along the bottom, 1 under the index finger and the 1st
  position at the hand's near side (#15), both mirrored left-handed; the
  rotate button at the corner towards the sheet (#16).
- `core/src/layouts.h` (the author's fix, #14): `SUMI_MAX_ECHOES` 3 → 12 —
  a string note on every string that reaches it. `core/src/layouts.cpp`,
  `core/include/sumi_core.h` (#15–#17): the brass arc re-cut — a ring of
  radius 0.30 canvas heights (0.42 before) centred 0.08 right of the middle
  at 0.65 of the height, cells of 0.055 (half the chord, 0.089, before),
  the narrow-sheet clamp on the right end — and the trombone's seven on it
  too under the one `trumpet_arc` flag (`desktop/src/settings_ui.cpp`: the
  checkbox under both brass layouts). `tests/normalizer_tests.cpp`: the
  strings golden expects every site, the arc golden takes the cell count,
  the trombone block gains the ring (d2), the empty-centre probe at the
  ring's centre.
- `ios/Sources/SoundController.swift`, `SettingsPages.swift`: the source row
  and the five Suzu patches, the active sound's name ("Suzu: Trumpet");
  `MidiSource.swift`: the appearance callback.
- `ios/Sources/Session.swift`: `--layout` (clearing the arc flag, #16),
  `--trumpet-arc`, `--string-tuning`.
- `_work/DECISIONS_8.md` #13–#17; `docs/CHANGELOG.md`; `CLAUDE.md`;
  `trombone_arc_desktop.png` (the bench's `--layout-shot` of the trombone's
  ring at the desktop aspect).

## The device runs (`ipad/`)

Each: a fresh launch with the layout, `--play`, `--fingering-demo` (the
scripted phrase through the real path, three seconds in) and `--capture`;
the byte log and a capture pulled from the app's container; the analyser
(`ipad/midi_asserts.txt`) run on the log — every assert holds on all nine
logged runs (the mirrored trumpet runs without the demo). The captures are
the final form where a round touched them: the trumpet column and the
trombone ring after #17 (the strip clear of the gear, the panel in the
author's stored horizontal form), the trumpet ring and the horizontal forms
after #16, the trombone column after #15, the theremin, the strings, the
Wicki–Hayden and the mirrored trumpet after #14.

| run | the log shows | the capture shows |
|---|---|---|
| `trumpet_column` (Suzu as the source) | the announce of nine on the master; B♭3 struck open; CC 111 = 127 on the master and the voice's bend stepping to −1 st over two frames; CC 110 and 112 and the bend to −6 (8192 − 1024); the lift; the valves up; the open series | the eight-cell column, the strip at the top-right stopping short of the settings gear, the valve panel (the author's stored horizontal form, along the bottom), the drops with the retune's streaks |
| `trumpet_arc` | the same phrase | the ring of eight small cells in the sheet's middle-right, the lowest partial at the left, the pads at the left |
| `trombone` | the slide's 42 CCs (113) and 41 voice bends under one held partial | the vertical slide at the left with its seven numbered positions and the rotate button beside its label, the seven cells in a column |
| `trombone_arc` | the same slide phrase | the trombone's seven cells on the ring (#17), the slide panel along the bottom, the strip clear of the gear |
| `theremin` | one note on, one off, 77 bends, 81 pressures over a two-octave sweep | the 61 semitone slots along the middle, the drop at C4 |
| `strings_guitar` | the scale, 32 messages | the 6 × 25 fretboard, every note lighting every string that reaches it (the echo cap raised) |
| `wicki` | the scale, 32 messages | the hex field |
| `trumpet_mirror` (no demo) | — | left-handed: the valve panel at the right, the strip at the top-left |
| `trumpet_horizontal` | the same phrase as the column | the three pads side by side along the bottom edge at the left, 1 nearest the corner, the rotate button at the panel's top-right |
| `trombone_horizontal` | the slide's CCs and the voice bends as the vertical run | the slide along the bottom edge, the 1st position at the left, the 7th to the right |

## The suites (`ctest.txt`, `normalizer_tests.txt`, `preset_tests.txt`)

| suite | result |
|---|---|
| `preset_tests` | 34 of 34: the two new fields round-trip |
| `normalizer_tests` | OK, 43 630 checks: the strings golden expects every site, the arc golden the ring, the trombone on it too |
| ctest | 10 of 10 |
| the field and composite gates, Metal (`gates/`) | bitwise after the echo cap change and after each arc re-cut (the fixture is the piano grid) |

## The other builds and the devices

| build | result |
|---|---|
| iOS `build-ios`, xcodegen, xcodebuild, devicectl install + the launches | builds; every run launched, logged and captured; the author's session put back and the app relaunched, the session read back to confirm |
| Android `:app:assembleDebug`, adb install + launch | BUILD SUCCESSFUL; launched on the Tab with the final core (the shared serializer change and the brass ring; the Tab's own surface is step 64) |
| web `cmake --build build-web` | builds |

## For the author

- **Column or arc:** both captured on the iPad for both brass layouts
  (`trumpet_column_capture_4.png`, `trumpet_arc_capture_4.png`,
  `trombone_capture_4.png`, `trombone_arc_capture_4.png`); the one toggle
  sits under the Trumpet and the Trombone — the arrangement is the brass's,
  not each instrument's (a separate flag is a small addition if wanted).
- **The panel's form:** vertical at the side (the default) or horizontal
  along the bottom — the toggle under the Trumpet and the Trombone, or the
  panel's own rotate button while playing; both captured
  (`trumpet_horizontal_capture_4.png`, `trombone_horizontal_capture_3.png`).
- **By hand:** a finger on a partial while a thumb works the valves — the
  demo scripts it; GarageBand's replay of such a phrase is yours.
- **By ear:** the bowed string under the valves (Sound → Source → Suzu, the
  default patch), the lip bend's scale — on the ring the bend's one
  semitone spans the small cell's radius.
- Step 64 ports the same mechanisms, the panel in both forms with its
  button, to the Tab.
