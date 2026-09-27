# Step 55 — Phase 7 on the Linux desktop — the Linux box's evidence

Decisions `DECISIONS_6 #33` (the Linux row of #9: the PulseAudio path on
PipeWire, what the period figure means there, the proxy against PipeWire's
own counter, hotplug), `#34` (the acceptance suite's first verdict: sound
clean, the visual rule failed by the GL loop's pacing; the settings window's
second swap fixed), `#35` (the audio worker asks for real time on Linux),
`#36` (GCC 15 elided the test's negative control; FlixDrums' malformed
attribute pair), `#37` (the display pacer — the author's call — after which
the suite passes three times), `#38` (the gate's estimate divided by zero on
a fuzzed 6-bit WAV header).

Box: Ubuntu, kernel 6.17, GNOME on **Wayland**, GCC 15.2 (no Clang, no
system Ninja — the Android SDK's Ninja drove the build), NVIDIA RTX 5090
(driver 610.43) on a 3840×2160@60 HDMI panel plus a 5120×2160@165 DP panel
on the Intel iGPU. Sound: **PulseAudio API on PipeWire 1.4.7** (`pactl info`:
"PulseAudio (on PipeWire 1.4.7)"), graph quantum 128 at 48 kHz; sinks: a USB
"Generic USB Audio" IEC958 output and two HDMI outputs. The user is in the
`audio` group (rtprio 95). **Neither the ROLI nor the Airwave was plugged
in** (`amidi -l` empty) — the hands-on items are the author's.

## What changed in the tree

- `desktop/src/main.cpp` (#37, Linux only): `pace_to_display` after the
  canvas's swap — sleeps the surplus of the display's period when the swap
  returned early, inert where vsync works, resyncs after a stall.
- `desktop/src/settings_ui.cpp` (#34, Linux only): the settings window's
  swap interval is 0 on the GL host — its vsync'd swap after the canvas's
  cost a whole vblank per frame: two windows open = 25–30 fps.
- `voxo/src/backend_miniaudio.cpp` (#35, Linux only): the miniaudio context
  asks `ma_thread_priority_realtime` for its worker, capped by
  `MA_PTHREAD_REALTIME_THREAD_PRIORITY 70` (miniaudio's own 99 is refused
  by the `audio` group's 95 and falls back to a normal thread silently).
  Measured: no effect on the underruns of #34 — kept for the thread-class
  parity with CoreAudio, AAudio and WASAPI and for the ALSA-direct path.
- `voxo/src/ds_preset.cpp` (#38): the gate's WAV header estimate treats
  bits under 8 as unreadable instead of dividing by `bits / 8 == 0`.
- `tests/voxo_preset_tests.cpp` + `tests/fixtures/dspresets/malformed/
  bits6.{dspreset,Samples/bits6.wav}` (#38): the regression check.
- `tests/voxo_tests.cpp` (#36): the counting allocator's negative control
  is published through a volatile sink — GCC at `-O2` elided the
  `new`/`delete` pair and the check failed in Release.

No temporary hooks were added; the experiments used launch flags and env
variables only (`PULSE_LATENCY_MSEC`, `nice`, `__GL_YIELD`, `--fullscreen`,
`--sim-scale`).

## Checklist

| item | result | evidence |
|---|---|---|
| 1. build + ctest | Release, GCC 15.2: clean (three benign warnings: `-fno-rtti` on C TUs, an unused `system()` result in `voxo_fuzz`, a `-Wmisleading-indentation` in `sumi_preset.c`); ctest 8/9 twice before the fixes (`voxo_tests` #36, then `voxo_fuzz`'s SIGFPE #38), **9/9** after | `ctest.txt`, `fuzz/` |
| 2. the device | `[voxo] USB Audio Digital Stereo (IEC958): 48000 Hz, 384 frames per block` (later runs the HDMI sink — the default sink changed during the session). Backend: **PulseAudio on PipeWire's shim** (`pactl list sink-inputs`: `media.name = "miniaudio:0"`, `node.latency = "128/48000"`, s32le 2ch 48 kHz). The callback is **256 frames** (11 449 callbacks per 61 s = 187.7/s); the printed 384 is miniaudio's `internalPeriodSizeInFrames` derived from the negotiated buffer attributes, not the callback size (#33). Render max 0.21–0.35 ms over a minute with the 1.4 GB library; the audio worker runs `FF 70` (#35) | `storm/pace_accept_*.log`, `storm/samples.txt` |
| 3. the acceptance suite ×3 (Tenor Saxophone, 421 zones / 1443 MB — the heaviest library on the box; no Bösendorfer here), the author's persisted state (settings window open, fullscreen on the 60 Hz panel) | **pass ×3** with #37: 60.0 / 60.0 / 60.0 fps, 0 XRuns (proxy), **0 underruns (PipeWire's ERR on the stream node, sampled through each run)**, 0 dropped, render max 0.293 / 0.214 / 0.209 ms, 11 449 callbacks each. Before #37 the same three runs were audio-clean but 51.0 / 54.4 / 55.5 fps (#34) | `storm/pace_accept_1..3.log`; before: `storm/accept_1..3.log` |
| 3b. the same with the settings window closed (the playing state) | with #37: 59.9 fps, 0 XRuns, 0 underruns, "ok"; idle with no sound **59.9 fps at 5.7 % of a core**. Before #37 the loop free-ran (103–121 fps, 74–92 % of a core — idle too) and PipeWire counted underruns (ERR +6…+13 per 15 s; the proxy 32–89), unmoved by FIFO 70 on the audio worker, `PULSE_LATENCY_MSEC=20/40`, `__GL_YIELD=USLEEP`, `--fullscreen`, `--sim-scale 0.5`, and gone with `nice -n 10` (ERR 0) — the app's time-sharing load contending with `pipewire-pulse`'s protocol thread | `storm/pace_closed.log`, `storm/pace_idle_closed.log`; before: `storm/rt_closed*.log`, `storm/idle_closed.log`, `storm/samples.txt` |
| 4. the demo, a library by hand, the gate, the sample row | headless: the demo loads (16 zones, 5.8 MB); five Decent Sampler libraries from the author's `~/.config/DecentSampler` load with their reports (Basic Piano 9 zones, Violin 22, Strayer Guitar 67 ×2 — "Convolution reverb… it will play without it", "An effect this version does not know… (wave_shaper)" — Tenor Saxophone 421 in 4 groups, 1443 MB); **FlixDrums refused**: `not well-formed XML (Error parsing element attribute at byte 22428)` — `silencingMode="fast"pan="0"`, two attributes with no space (#36); `--voxo-budget-mb 10`: "Memory: this library is larger than advised for this device; it is loaded anyway. (about 1443 MB against 10 MB advised)". The rows by hand and the ROLI: the author (the Instrument row is a path field + "Load instrument", greyed until "Internal sound (Voxo)" is ticked — and only in this build, not the Sep 2 binary the app-grid launcher runs) | `compat/*.txt`, `compat/gate_10mb.txt` |
| 5. the CC map into the bus | the loader half proven: `[255,110,1000]` (Reverb amount) and `[255,111,1003]` (Delay amount) added to `last_session.json`'s `cc_map`, relaunched, both back in the session **and** written into `settings.ini`'s `ccmap` (#31 holds on the desktop; the session JSON is the map's truth — an INI-only edit is overwritten). Turning them from the Airwave: the author | `storm/roundtrip2.log` |
| 6. hotplug | the default sink switched HDMI → USB → HDMI while the storm played: the stream **follows** (sink 54 → 55 → 54), the sound moves, nothing stops, no restart; the app's device line keeps the opening name; two late callbacks by the proxy per move. A physical unplug: the author | `storm/hotplug.log` |
| 7. the glide check | **PASS** — static down: Hermite worst −61.8 dB, linear 14.8 dB worse; static up: −62.4 dB, 14.3 dB worse; sweep: −59.9 dB, 12.8 dB worse | `glide/glide_check.txt`, `glide/glide.json` |
| 8. the packaging | `cmake --install --component desktop-integration --prefix <scratch>` lands `bin/midi-sink` and `share/midi-sink/demo/{demo.dspreset,LICENSE.txt,Samples/}`; `cpack -G DEB` → `midi-sink_2.0.0-alpha.1-9-gfe92eb0_amd64.deb` carrying `/usr/share/midi-sink/demo/` | `deb_contents.txt` |
| 9. the fuzz | ctest's 5 s fuzz hit SIGFPE in the gate's header estimate (#38); fixed, the input replays clean, 60 s: 1 815 091 loads, no crash | `fuzz/README.md`, `fuzz/crash.dspreset`, `fuzz/crash_tone.wav` |

The VCSL Dan Tranh was fetched to `~/Music/midi-sink/Dan Tranh (VCSL)/`
(`tools/fetch_dan_tranh.py --no-demo`) and left there. The author's
`settings.ini` and `last_session.json` were backed up and restored around
every experiment (`cmp` clean each time), and once put back to their 16:07
copies (before the long storms): between those and then, `sim_scale` had
become 0.5 (the `--sim-scale 0.5` lab run persisted it — a bench flag that
writes the session), `vortex_profile` 0 → 1, `pinch_variant` 0 → 1,
`ripple_angle` 0 → 15° and the ripple-wavelength control 32 → 24 — not the
storm's doing (it sends CC 74 only, unmapped here); most likely keys pressed
on the fullscreen bench window while it held the screen. After the last
experiment the author's own hand-testing in the new binary (Internal sound
on, gain 1.5, root 11, the Tenor Saxophone loaded, the medium Anod,
`burst_order` 6, fullscreen off, CC 74 → Reverb amount and CC 75 → Delay
amount) was briefly overwritten by a restore and rebuilt from the diff into
both files — the author should glance at them. The default sink is back
where it was found.

## Left for the author

1. **The storm's period and XRun readouts on the Pulse path** (#33): the
   printed period is the buffer-derived figure and the lateness proxy
   counts Pulse's bursty requests as XRuns (1496 at a 40 ms buffer with
   nothing audible). PipeWire's stream counter is the truth; Voxo could read
   libpulse's underflow callback into `device_xruns` the way it reads
   AAudio's, and report the callback's frames as the running period.
2. **FlixDrums** (#36): lenience for attributes without whitespace, or not.
3. **The hands-on items**: the ROLI, the Airwave on the bus routes, a
   physical unplug, the heard latency against the Mac's ~3 ms.
