# Step 54 — Android (macOS machine) — DONE evidence

Decisions `DECISIONS_6 #28` (the Tab's sound: foreground only under audio
focus, the tuner paced by the status line, first launch makes a sound), `#29`
(Instruments on the Tab, the SAF imports, "Open with").

## What shipped

- **Android**: `Sound.kt` (the settings, audio focus, the lifecycle, the
  instrument library, the demo installed from the assets, the gate on the
  activity manager's available memory, the worker-thread load, the tick that
  paces the tuner and restarts a dropped device, the reach); the Sound page
  in `SettingsSheet.kt`; the bus targets in `CcMap` and the Dimension cycle;
  the reach in `PlayOverlayView.kt`; the JNI's product entry points and the
  `local` flag on the one producer; the session's input mode and CC map
  mirrored into Voxo; the manifest's VIEW filter for `.dslibrary`; the demo
  in the APK's assets (`build.gradle.kts`).
- **Voxo backend**: the AAudio buffer tuner (#7), one burst per underrun up
  to the capacity, ticked by the stats query.

## DONE checks

| check | result |
|---|---|
| as 53 on the Galaxy Tab: the demo at first launch | `launch_logcat.txt`: the instrument list read, the device "started: 48000 Hz, 192 frames per burst, buffer 384", "demo: 16 zones in 1 groups, 16 samples, 5.8 MB in memory" |
| a large library warns and loads | `gate_logcat.txt`: the harp `.dslibrary` (pushed into `files/Instruments`) over a 10 MB advice — "Memory: this library is larger than advised for this device; it is loaded anyway. (about 36 MB against 10 MB advised)", then its 36 zones in memory |
| touch-to-sound within the step-48 bar | `voxo_spike.csv` / `run.log`: the spike on the product build with the Dan Tranh demo playing (its layers and reverb on the bus) — touch-down → callback **2.75 ms median**, 4.4 p90, 5.8 max over 17 injected touches; push → callback 2.1 ms median; AAudio low latency granted, burst 192, buffer 384 (two bursts), output latency 28.7–30.6 ms timestamp-derived, **0 AAudio underruns**, render max 1.68 ms of a 4 ms period — touch-to-DAC ~32 ms, the step-48 bar |
| the ROLI / the Travel Sax over USB, the pen on the play surface, a call | the author's hands |
| the desktop and the iOS libraries still build; ctest | green |

## After the author's first hands-on (#30)

The strip and the cells could not be touched together (three Compose-hosted
views; a whole gesture to the first one): the canvas, the overlay and the
strip now share one native frame, where Android splits pointers. The CC
map's Channel and Dimension rows became two-way pickers.

## After the author's second hands-on (#31)

Routes to the reverb and the delay did nothing: three pre-Voxo range checks
(the Tab's JNI before the bus branch, the desktop's INI loader, the iPad's
session parsing) dropped targets from 1000. All three let the bus targets
through now; rebuilt and installed on the Tab and the iPad, ctest green.

## Notes for the record

- The lab's `--es voxoInstrument` with a name that carries spaces must be
  quoted for the device shell as well (`adb shell "am start ... --es
  voxoInstrument 'Arpa Chiquitana MPE.dslibrary'"`): the first gate run
  passed `Arpa` alone.
- Voxo's late-callback proxy counted 25 over the 30 s spike while AAudio
  counted 0 underruns — the two-burst buffer absorbing them, as in step 48;
  the tuner would have grown the buffer had AAudio counted one.
- The Tab was left with the demo as its saved instrument.
