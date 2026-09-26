# Step 53 — iOS (macOS machine) — DONE evidence

Decisions `DECISIONS_6 #22` (the demo instrument: the VCSL Dan Tranh, CC0),
`#23` (the session, foreground only, the background-mode key), `#24`
(Documents/Instruments, the Files import, the gate on
`os_proc_available_memory`), `#25` (MPE's zone in Voxo — the master's pedal,
bend, pressure and slide reach the members; the dialect mirrored into Voxo —
after the author's first hands-on), `#26` (the Decent Sampler types declared,
the app a handler for AirDrop / "Open in", the Sound section in the sheet).

## What shipped

- **The demo instrument**: `tools/fetch_dan_tranh.py` → the three-layer test
  library outside the tree and the bundled demo in `voxo/demo/` (16 zones,
  32 kHz mono, 2.9 MB, `LICENSE.txt`); the macOS bundle and the iOS app carry
  it (`Resources/demo`, a folder reference).
- **iOS**: `ios/Sources/SoundController.swift` (the AVAudioSession, start/stop
  with the scene phase, interruptions, route changes, the instrument list and
  import, the loader on a background queue, the gate, the settings, the launch
  arguments), the Settings sheet's "Sound" page (`SettingsPages.swift`), the
  canvas's Local Control tagging (`SumiCanvas.swift`), the project's demo
  folder (`project.yml`).

## DONE checks

| check | result |
|---|---|
| the app builds with the demo in the bundle | `xcodebuild` (generic iOS destination): BUILD SUCCEEDED; `midi-sink.app/demo/Samples` holds the 16 files |
| on the iPad the ROLI plays the demo instrument | launched with `--voxo-instrument demo`: "demo: 16 zones in 1 groups, 16 samples, 5.8 MB in memory", the device "started: 48000 Hz, 128 frames per block" — `ios_demo_console.txt`; the ROLI under the author's hands |
| a call interrupts and the sound returns | the author's hands (the `interruptionNotification` path: stop on began, start on ended-with-resume) |
| a large library warns and loads | the harp `.dslibrary` pushed into Documents/Instruments and launched over a 10 MB advice: "Memory: this library is larger than advised for this device; it is loaded anyway. (about 36 MB against 10 MB advised)", then its 36 zones in memory — `ios_gate_console.txt` |
| the Play surface plays it with the pen | the author's hands |
| the desktop still builds with the new demo; ctest | green (the bundle's `Resources/demo` refreshed) |

## Device runs

The iPad stopped answering devicectl and Xcode mid-session (transport wired,
tunnel connected, but the readiness state stuck at "preparing"); the author
kickstarted the CoreDevice and remote-pairing daemons and it answered again.
`tools/coremidi_recover.sh` reported the CoreMIDI loop not live (four past
crashes, the newest days old) — a different fault from the setup-store bug.
Then: install, the demo launch, the harp copy, the gate launch — all in one
pass; the iPad was left with the demo as its saved instrument.
