# Step 55 — Phase 7 on the Windows desktop (WASAPI) — the Windows box's evidence

Decisions `DECISIONS_6 #39` (the Windows row of #9: WASAPI shared mode's
480-frame engine period against the 256 callback, and the XRun proxy's
lateness reference fixed to the device's period), `#40` (byte-exact fixtures
and `core.autocrlf` — the `.gitattributes`), `#41` (hotplug follows the
default output; the portable zip and the installer carry `demo/`).

Box: Windows 11 Pro (26220), MSVC 14.44 (VS 2022 Community), CMake + Ninja
through `build_win.bat`, Release. NVIDIA RTX 5090; the canvas ran on the
5120×2160 @ 165 Hz panel (125 % scaling). Sound: **WASAPI**, miniaudio
0.11.25; active render endpoints: the NVIDIA display audio ("LG ULTRAGEAR+",
the default), a Realtek USB digital output, a Samsung display audio — all
three with a fixed 480-frame shared-mode engine period at 48 kHz
(`wasapi/wasapi_periods.txt`). The ROLI Piano was not attached; the Airwave
Expression was (CC only), so the notes of the hands-on came from a loopMIDI
port fed by WinMM (`gui_sessions.log` shows both inputs open). The Bösendorfer
is not on this box: the heavy library is the VCSL Dan Tranh fetched with
`tools/fetch_dan_tranh.py --no-demo` (48 zones, 44.9 MB, left in
`~/Music/midi-sink/Dan Tranh (VCSL)/`).

## What changed in the tree

- `voxo/src/voxo.cpp`, `voxo/src/backend_miniaudio.cpp`, `voxo/src/
  voxo_internal.h` (#39): the backend passes the gap since the previous
  callback; the core judges lateness against `max(callback, device period)`
  instead of the callback alone. Where callback and period agree nothing
  changes.
- `tests/voxo_tests.cpp` (+ `tests/CMakeLists.txt` include dir) (#39): test
  9, the WASAPI burst pattern into the proxy — 0 XRuns with the fix, 200
  without (`wasapi/proxy_negative_control.txt`).
- `.gitattributes` (new, #40): `tests/fixtures/**` and `voxo/demo/**` are
  `-text`.
- `.github/workflows/release.yml` (Windows lane) and `packaging/windows/
  midi-sink.iss` (#41): the `demo/` folder beside the exe goes into the
  portable zip and the installer.

No temporary hooks; the probes (`wasapi/*.cpp`) are standalone programs kept
as evidence, not built by the tree. The user's `%APPDATA%\midi-sink` was
restored byte-for-byte from the pre-session backup; the session's state is
kept beside it as `midi-sink-s55-session` and the backup as
`midi-sink-backup-step55` for the author to delete.

## Checklist

| item | result | evidence |
|---|---|---|
| 1. build + ctest | Release, MSVC: 0 errors; 74 warnings, all libremidi/cmidi2's (none from Voxo, miniaudio or dr_libs). First ctest **9/10**: `voxo_preset_tests`' header-estimate check read 6 438 bytes for 6 436 — the 18-byte text fixture was CRLF on disk (#40). After `.gitattributes` and a re-checkout **10/10**; final tree (with the proxy fix and test 9) **10/10** | `ctest.txt` |
| 2. the device (Settings → Sound → Internal sound, ticked by hand) | `[voxo] LG ULTRAGEAR+ (NVIDIA High Definition Audio): 48000 Hz, 480 frames per block` — **WASAPI shared mode granted 480 for the 256 asked** (its engine period, IAudioClient3 reports min = max = 480 on every endpoint here); the callback itself stays 256 (188 callbacks/s). After ~a minute of the demo instrument under MPE phrases: **render 0.07 ms (max 0.17), 0 XRuns in 18 203 blocks, 0 dropped**. The period is far from 256 (WASAPI's 10 ms, as the handoff anticipated); the default was not changed (#39 lists the author's options) | `shots/sound_on.png`, `shots/playing_demo.png`, `shots/after_a_minute.png`, `wasapi/wasapi_periods.txt`, `gui_sessions.log` |
| 3. the acceptance suite ×3 (`--voxo-preset "Dan Tranh.dspreset" --voxo-storm 60`) | before the fix: **FAIL — 6 120 XRuns** by the proxy in a clean run (render max 0.383 ms, 164.6 fps): every 10 ms burst's first 256 callback read 4.67 ms late against its own 5.33 ms. With #39: **pass ×3 — 0 XRuns, 0 dropped, 164.6 / 164.9 / 164.8 fps, render max 0.297 / 0.304 / 0.357 ms, 11 470 callbacks, 480 frames per block** | `storm/before_fix_1.log`, `storm/accept_1..3.log` |
| 4. the demo, a library, a folder preset, the gate, the sample row | by hand in the settings window: **Demo instrument** → "demo: 16 zones in 1 groups, 16 samples, 5.8 MB", sounds under loopMIDI notes (5 voices in the status line); Instrument row with the Dan Tranh folder preset → 48 zones, and under `--dev --voxo-budget-mb 10` the memory line "larger than advised for this device; it is loaded anyway. (about 45 MB against 10 MB advised)"; the fixture `.dslibrary` → "minimal: 1 zones"; Unload, then the Sample row with `demo/Samples/B2.wav` → "Loaded 96000 frames (3.00 s), 1 ch, 32000 Hz, root 60", 4 voices playing it. Headless reports the same (`compat/`). Under the ROLI Piano: the author (not attached) | `shots/demo_instrument.png`, `shots/instrument_row_dan_tranh_gate.png`, `shots/instrument_row_dslibrary.png`, `shots/sample_row_wav.png`, `compat/*.txt` |
| 5. the CC map into the bus | added through the picker: **any / CC 30 → Reverb amount, any / CC 31 → Delay amount** (the Airwave's Flex pair, the dimensions #69 left free); turned from loopMIDI (CC 30 = 100, CC 31 = 90, the raw log shows them arriving) — the ear is the author's, as is the Airwave's own turn. Relaunch: both routes back (`settings.ini` `ccmap` carries `255:30:1000;255:31:1003`, `last_session.json` too) — #31 holds on Windows | `shots/ccmap_bus_routes_added.png`, `shots/relaunch_routes_survive_budget10.png`, `gui_sessions.log` |
| 6. hotplug | the default output switched NVIDIA → Realtek USB → Samsung → NVIDIA while the demo played: **the stream follows** (the app's session goes ACTIVE on the new endpoint within ~1 s), the sound moves, nothing stops, no restart; the device line keeps the opening name; 5 / +1 / +2 late callbacks per move, 0 dropped. A physical unplug: the author. The Mac's device-selection work was not in `main` (origin/main = HEAD) | `hotplug/hotplug.log`, `hotplug/status_lines.png`, `wasapi/set_default_render.cpp`, `wasapi/audio_sessions.cpp` |
| 7. the glide check | **PASS** — static down: Hermite worst −61.8 dB, linear 14.8 dB worse; static up: −62.4 dB, 14.3 dB worse; sweep: −59.9 dB, 12.8 dB worse (the Linux and Mac figures) | `glide/glide_check.txt`, `glide/glide.json` |
| release.yml's Windows lane and `demo/` | **was missing** from both the portable zip stage and the Inno Setup script; added (#41) | `git diff .github/workflows/release.yml packaging/windows/midi-sink.iss` |

## Notes for the author

- The fps rule passes trivially on a 165 Hz panel (164.6 fps); each run
  has one frame-time maximum of 64–81 ms (its source was not pinned), the
  average unmoved.
- `voxo_stats.block_frames` prints the device's 480 while the callback is
  256 — the same reading gap as Linux's #33; both figures in the stats is
  one field (#39 a).
- A Windows clone with `core.autocrlf=true` keeps its CRLF fixture copies
  until they are re-checked-out after `.gitattributes` lands (#40).
