# Hand-off — the Windows box: the v2 release candidate's Windows lane

**For:** the agent session on the Windows box (the author beside it).
**From:** the Mac session of step 67 (2026-10-06), `DECISIONS_9 #7`.
**Goal:** `cmake --build build` and `ctest` green on Windows for the tree at
the step-67 commit, so the next `v2.0.0-rc.N` tag's `release.yml` Windows
lane passes; then the Windows gates the step owes.

## What happened

1. The first v2 RC's Windows lane (`release.yml`, MSVC 14.51, Ninja, Release)
   failed at `voxo/src/backend_none.cpp(20)`: `clock_gettime` /
   `CLOCK_MONOTONIC` do not exist on MSVC. That file is the deviceless Voxo
   backend, compiled into `voxo_nofma` (the no-FMA reference the web gate
   compares against) on every platform. FIXED on the Mac: a `_WIN32`
   branch reading `QueryPerformanceCounter` / `QueryPerformanceFrequency`
   (the clock miniaudio's backend already uses there); the other platforms
   untouched. Not compiled on Windows by anyone yet.
2. The lane failed AGAIN on the next run. The Mac session has not seen that
   log; ninja stopped the first run at [88/152], so everything after
   `voxo_nofma` — `suzu_web_reference` (links it), the replay tests
   (`tests/replay_tests.c`, C11), the desktop's step-65 files
   (`replay_host.cpp`, `sys_info.cpp`, `orbit_trace.cpp`, `dev_tools.cpp`'s
   `t65_*`), `app_settings.cpp`, the presets' apply unit — was never
   compiled by MSVC. A scan from the Mac for the usual MSVC traps
   (`strtok_r`, `strncasecmp`, `localtime_r`, VLAs, `__attribute__`,
   `<stdatomic.h>`, `ssize_t`, `M_PI`, `<unistd.h>` outside a platform
   branch) found nothing; `sys_info.cpp` has its `_WIN32` branches. The
   next error is yours to read.

## Do this

From an **x64 Native Tools** prompt at the repo root (`git pull` first; the
step-67 commit carries the backend fix):

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build 2>&1 | tee build.log
```

Read the FIRST `error C` in `build.log`; fix it; rebuild; repeat until the
build is clean. Then the two lanes' other steps:

```
ctest --test-dir build --output-on-failure          # build.yml: the headless suites
cmake -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DSUMI_APP_VERSION=2.0.0-rc.2"
cmake --build build-rel                              # release.yml's configure, tests off
build-rel\desktop\midi-sink.exe --version            # must print the injected version (the lane's "About must read the tag")
```

Warnings are not the job: the `C4996` on `sscanf` / `gmtime` in the replay
library are MSVC's deprecation notes, not errors (`_CRT_SECURE_NO_WARNINGS`
on the `sumi_replay` target is acceptable if you want them quiet; nothing
builds with `/WX`).

## Then the gates the step owes on D3D11

* **The §4.6 field gate** at the second tier (2.5e-2 / 1e-3, `DECISIONS_3
  #30`): `build\desktop\midi-sink.exe --dev --field-dump field_d3d11.bin`,
  then `python tools\field_gate.py` against `tests\fixtures\field_512_metal.bin`
  (the tool's `--help` has the flags).
* **The composite gate**: `python tools\composite_gate.py --backend d3d11`.
* **The replay gate**: `python tools\replay_gate.py` (it records the
  canonical demo through the real recorder with `--record-demo`, replays it,
  and re-buckets by wall time, which must diverge). KNOWN: the D3D11 bench's
  paper dip takes a variable number of frames (`DECISIONS_7 #7`); if the
  replay comes back off the tier because of it, record that reading rather
  than bending the gate — it is the author's open item.
* **The storm**: `midi-sink --dev --voxo-preset <a heavy library> --voxo-storm 60`
  with WASAPI (`tools\windows_wasapi\` has the endpoint helpers) — 0 XRuns,
  0 dropped, the visuals at rate.
* A `--replay-live 5` / `--record-live` round in the window, a recording
  from the iPad replayed on the box (any `.sumireplay` the author has), and
  the settings window's Replay section exercised once by hand.

## The rules (CLAUDE.md, in short)

* Never commit or stage; the author commits. Leave the tree modified.
* The core (`core/`) is frozen: a change there is bug → regression test →
  fix, and never a backend branch in the deformation chain (orientation
  flips live only in the swapchain composite and the print readback).
  Voxo, hostmpe, the replay library, the desktop shell and the tests are
  open to the minimal MSVC fix — portable, not `#ifdef`-shaped where a
  standard form exists.
* Record what failed and the fix as `DECISIONS_9 #8` in
  `_work/DECISIONS_9.md` (Part IX in flight; #7 is the backend clock);
  evidence under `docs/evidence/step67/windows/`: the build log's tail,
  the ctest output, the gates' outputs, the version check. One paragraph
  in `docs/CHANGELOG.md`'s step-67 entry.
* Report the tree ready with a commit message; the author pushes
  (`build.yml` runs the Windows lane on the push to `main`) and then cuts
  the next RC. The failed RC tag is the author's to replace.
