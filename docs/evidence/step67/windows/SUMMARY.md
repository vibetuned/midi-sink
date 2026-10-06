# Step 67 — the v2 RC's Windows lane, on the Windows box

Decision `DECISIONS_9 #8`. Tree `d7d9126` (step 67 with #7's performance-counter
clock). Box: Windows 11 Pro (26220), MSVC 19.44 (the lane's runner has 19.51),
CMake + Ninja through `build_win.bat`, Release; NVIDIA RTX 5090, driver 616.64,
D3D11; WASAPI through the NVIDIA display audio (480-frame engine period,
DECISIONS_6 #39). The ROLI was not attached; notes came from a loopMIDI port.

## What failed and the fixes (none in `core/`)

| stop | fix |
|---|---|
| `desktop/src/app_settings.cpp(491): fatal error C1061: compiler limit: blocks nested too deeply` — the INI loader's one `else if` chain of 134 keys; MSVC counts each `else if` toward its 128 nested blocks | the chain restarts as a second `if` at the first `suzu_*` key; the keys are distinct, so two chains read as one |
| `web/suzu/suzu_web.c(101…132): C2143/C2091/C2059` — `SW_EXPORT` = `__attribute__((used, visibility("default")))` unconditionally | the attribute under `__GNUC__` / `__clang__`, empty elsewhere (the native reference is linked statically) |
| `tests/voxo_suzu_tests.cpp(164, 167, 795): C2065 'M_PI'` | `M_PI` defined when `<cmath>` does not provide it |

Warnings stay: `C4996` (strcpy / sscanf / gmtime deprecation notes in the
replay and preset tests and `sumi_replay.c`), two `C4127` in the ABI test,
libremidi's. Nothing builds with `/WX`.

## The lanes' steps

| item | result | evidence |
|---|---|---|
| `cmake --build build` (Release) | clean after the three fixes | `build_log_tail.txt` |
| `ctest` | **12/12** (ABI tests at 1.5.0). First pass: `voxo_tests` SegFault at 2.75 s while `build-rel` compiled beside it; 8 direct reruns, `--repeat until-fail:6` and a second full pass on the idle box all passed (0.12 s) — a one-off under load, not reproduced | `ctest.txt` |
| release configure (`-DBUILD_TESTING=OFF -DSUMI_APP_VERSION=2.0.0-rc.2`) + `--version` | 99 steps; `midi-sink 2.0.0-rc.2 (commit d7d9126, libsumi 1.5.0)` | `version_check.txt` |

## The D3D11 gates

| gate | result | evidence |
|---|---|---|
| §4.6 field gate (new fixture) | **GREEN at the reference tier** (1e-2 / 1e-4, stricter than the second tier the handoff names): dx 2.44e-4, dy 3.66e-4, ink 3.91e-3, aux 0, mean 2.26e-6 — step 55b's numbers to the digit; the negative control red | `gates/field_gate_d3d11.txt` |
| composite gate | max channel diff 1 (the D3D11 tier), 25 617 of 1 048 576 samples differ; the negative control red (65) | `gates/composite_gate_d3d11.txt`, `gates/composite.rgba` |
| replay gate | **GREEN**: the canonical demo (480 frames, 4.0 s, 794 events) recorded through `--record-demo` on this box replays **bitwise** (every channel 0); the 60 Hz wall-time re-bucketing diverges (ink max 1.41, mean 3.0e-3). DECISIONS_7 #7's variable dip did not reach it (recording and replay start from the same dip on the same box) | `replay/report.txt`, `replay/demo.sumireplay`, `replay/demo.sumireplay.field.bin` |
| storm (`--voxo-preset "Dan Tranh.dspreset" --voxo-storm 60`) | **ok**: 0 XRuns, 0 dropped, 164.6 fps (165 Hz panel), render max 0.463 ms, 480 frames per block, 11 470 callbacks | `storm/storm_dan_tranh.log` |
| `--record-live 5` / `--replay-live` | recorded 824 frames / 5.0 s of loopMIDI phrases; replayed with the banner | `replay/live_round.log`, `replay/live_recording.sumireplay` |
| the settings window's Replay section | **Record** and **Stop recording** pressed by hand: a 97.9 s recording (16 123 frames) saved and listed in the picker. The **Play** button and an iPad `.sumireplay` were not exercised: the author was working on the box and input injection stopped (the player ran through `--replay-live`); no iPad recording is on the box | `replay/settings_replay_section.png`, `replay/settings_after_stop_recording.png`, `replay/window_recording.sumireplay` |

The user's `%APPDATA%\midi-sink` was restored byte-for-byte from the
pre-session backup; the session's copy (with the two recordings) is
`midi-sink-s67-session`, the backup `midi-sink-backup-step67` — the author's
to delete.
