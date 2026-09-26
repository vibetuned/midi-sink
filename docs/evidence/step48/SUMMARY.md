# Step 48 — The mobile latency spike (macOS machine: the Tab, then the iPad) — DONE evidence

Timeboxed, one session per platform, both in one day: the go/no-go on
miniaudio for Android (SOUND §1's `[ITERATE]`) and the iOS session path.
Decisions `DECISIONS_6 #7` (Android), `#8` (iOS), `#9` (the block-size table
with the mobile rows measured).

## What shipped

- **Voxo 0.2.0** (additive): the latency probe (`note_ons`,
  `last_note_on_seconds` stamped with the start of the consuming block,
  `voxo_now_seconds` as the one clock) and the backend's own numbers
  (`frames_per_burst`, `buffer_frames`, `device_xruns`, `output_latency_ms`,
  `low_latency`). The backend owns its `ma_context` (the iOS session category
  left to the shell), sets AAudio's buffer to two bursts, reads the AAudio
  calls miniaudio does not load from `libaaudio.so`; the TU compiles as
  Objective-C++ on Apple.
- **Android**: Voxo linked into `sumi-shell`, the fan-out in
  `shell::push_midi`, the spike intent `--ei voxoSpike <s>` writing
  `files/voxo_spike.csv`; `run_spike_android.sh` drove install, launch,
  twenty injected touches, the visual storm as load, the Mac's microphone.
- **iOS**: `libvoxo.a` in `build-ios`, the app links it (`project.yml`), the
  canvas creates Voxo with the instance and fans its one producer through
  `push()`, `VoxoSpike.swift` runs the same probe under `--voxo-spike <s>`
  with the AVAudioSession configured by the shell and the canvas storm as
  load; `run_spike_ios.sh` drove it.

## The numbers (the one clock on each device; pairs in the CSVs)

| | Tab S8 Ultra (SM-X906B, Android 16) | iPad Air 11" (M4) |
|---|---|---|
| backend | miniaudio → AAudio, `LOW_LATENCY` granted, usage GAME | miniaudio → CoreAudio, session playback |
| period | burst 192 frames (4 ms) | 128 frames (2.667 ms), as asked |
| buffer | default 1536 (8 bursts) → set to 384 (2 bursts) | 384 (miniaudio's 3 periods) |
| output latency | 55.4 ms (default buffer) → **29.3 ms** (two bursts), timestamp-derived | **9.73 ms** (session) |
| push → callback | 1.94 / 1.82 ms median, 4.07 / 4.04 max (runs 1 / 2) | 1.27 ms median, 2.65 max |
| touch-down → callback | 3.94 / **3.10 ms median**, 5.41 / 6.85 max (20 injected touches) | not injectable from the Mac (mark wired for the author) |
| platform underruns under load | **0** (AAudio's count, 12 s storm) | — |
| Voxo's proxy XRuns | 1 (8 bursts) / 34 (2 bursts: callbacks > 2 ms late, absorbed) | **0** (16 s storm) |
| render max | 1.04 ms of a 4 ms period | 0.039 ms of a 2.67 ms period |
| touch-to-DAC estimate | ~32 ms (two bursts) | ~14 ms |
| sound heard by the Mac's mic | yes (`mic_onsets.txt`) | yes |

## Verdicts

- **Android: miniaudio confirmed**; no escape hatch. Step 54 carries the
  buffer tuner (two bursts, one more per AAudio underrun), audio focus and
  the lifecycle. The XRun budget is AAudio's own count (0); Voxo's proxy is
  a diagnostic.
- **iOS: miniaudio confirmed**; AVAudioEngine not needed. Step 53 carries
  interruptions, route changes and the foreground-only pause.

## Checks

- ctest 7/7 on the desktop after every backend change; the desktop storm
  still glitch-free at 128 (`--voxo-storm 10`: 0 XRuns).
- Android: `./gradlew :app:assembleDebug` green, installed on the Tab.
- iOS: `cmake --build build-ios` + `xcodegen` + `xcodebuild` green, installed
  on the iPad.

## Left to the author

- A finger on the iPad's play surface during `--voxo-spike` fills the
  `touch` rows (the mark is wired); a microphone against the screen tap on
  either device is the physical cross-check of the estimates above.
