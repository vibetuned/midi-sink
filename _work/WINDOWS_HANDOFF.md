# Handoff — Windows machine: Step 55 (Phase 7 on the Windows desktop)

Written on the macOS machine, 2026-09-26. You are the agent on the author's
Windows machine (MSVC + Ninja, the D3D11 backend, the ROLI and the Airwave
over WinMM, WASAPI for the sound). Do not touch the macOS, iOS, Linux,
Android or web lanes.

**Your step:** Phase 7 gave the app a sound. `voxo/` is a sibling library
beside the core (pure C `voxo/include/voxo.h`, C++20 behind it) that plays
Decent Sampler instruments from the same MIDI bytes the visuals get; the
desktop shell links it, feeds it through the harness's tap, and drives it
from the settings window's Sound section. Every line of it was written and
measured on the Mac (CoreAudio), the Tab (AAudio) and the iPad; **nothing has
run on WASAPI**. Build, run the suites, run the acceptance suite, look, fix
(bug → regression test → fix), record — and fill the Windows row of the
block-size table.

> Step 55 in `_work/ROADMAP_5.md` is authored on the macOS machine (device
> selection and hotplug in the settings window, the acceptance suite, the
> licensing page); the boxes verify. What is in `main` when you start is
> what you verify; if the Mac's device-selection work lands while you run,
> pull and re-run the hotplug item. **The phase end (tag `v2.0.0-alpha.2`,
> the fold of `DECISIONS_6`) is the author's — do not fold the log.**

## Read first, in this order

1. `CLAUDE.md` — **agents never commit, stage or push**; **agents never edit
   specs or roadmaps** (flag conflicts in a decision entry); temporary
   verification hooks are removed from the tree.
2. `_work/DECISIONS_6.md` — the Phase-7 record. New entries continue after
   the last one written (at the time of writing the next free number was
   **#33**; the Linux agent writes in parallel — take the next free number
   when you write, and say which box in the heading). Most relevant: #2–#6
   (the library, the callback contract, the block-size table, miniaudio),
   #9 (the table with the mobile rows measured — yours is `[ITERATE]`),
   #10–#12 (Hermite, the sample swap, Local Control), #13–#15 (the format
   and the compat report), #16–#18 (layers, the filter, the contract under
   fifteen stacked voices), #19–#21 (the bus, the memory gate, the
   worker-thread load, the demo slot), #22 (the Dan Tranh demo), #25 and
   #27 (MPE's zone, the reach, the bus routes in the CC map), #31 (the
   loaders that dropped the bus targets), #32 (the acceptance suite's pass
   rule and the XRun budget).
3. `docs/evidence/step47/` … `step54/` — what landed and how it measured
   (the storm and the spike numbers are the bar).
4. `docs/BUILD.md` — every harness test, gate and lab flag with its
   command line (the Voxo paragraphs are new).

## What is new for you

* **Third parties, all fetched by CMake at configure:** miniaudio 0.11.25
  (header, one implementation TU `voxo/src/backend_miniaudio.cpp` — plain
  C++ on Windows; Objective-C++ only on Apple), pugixml v1.16 (compiled in,
  no exceptions, no XPath), miniz 3.1.2 (four split sources, a generated
  `miniz_export.h` stub), dr_libs (dr_wav, dr_flac; a master commit). No
  new SDK or vcpkg package: WASAPI is loaded at runtime by miniaudio.
* **New ctest suites:** `voxo_tests` (a counting global allocator asserts
  the callback allocates nothing), `voxo_preset_tests` (the fixtures under
  `tests/fixtures/dspresets/`, the docs-copy assertion, the bus, the gate,
  the reach, the routes), `voxo_c_compile` (strict C11 — under MSVC's `/W4`
  for the first time), `voxo_fuzz --seconds 5` (a mutation fuzzer over the
  parser, the decoders and the zip reader). Nine suites in all.
* **The desktop shell:** the Sound section (Internal sound, Volume, the
  Sample (WAV) row, the Instrument (.dspreset / .dslibrary) row with its
  compat report and the memory advice, the Demo instrument button, the
  status line with voices / layers / render time / XRuns); the CC map's
  target list carries the bus (reverb and delay, from 1000); `settings.ini`
  gained `sound`, `sound_gain`, `sound_sample`, `sound_root`,
  `sound_preset`. The preset loads on a worker thread. `desktop/src/
  sys_info.cpp` answers the free memory (`GlobalMemoryStatusEx`) and the
  resource dir (`GetModuleFileNameW`); `desktop/src/wav_io.cpp` reads the
  sample row's WAV. The demo instrument is copied **beside the executable**
  as `demo/` by a post-build step (the release zip lane must carry it: check
  `.github/workflows/release.yml`'s Windows lane lists the folder; if not,
  add it and say so in your entry).
* **Lab flags:** `--dev --voxo-storm <s>` (the acceptance suite's engine:
  fifteen member channels of notes, bends, pressure and CC 74 through the
  harness while the device runs; prints its verdict), `--voxo-preset
  <path>` (the run's instrument), `--voxo-budget-mb <n>` (the gate's
  advice), `--voxo-load <path>` (a compat report, headless), `--voxo-bounce
  <dir>` + `uv run tools/voxo_glide_check.py` (the Hermite check; needs
  `uv`, or numpy).

## Checklist

1. **Build + ctest** (MSVC, `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=
   Release && cmake --build build && ctest --test-dir build`): nine suites.
   Expect warnings from miniaudio / dr_libs under `/W4`; a warning is not a
   failure, an error is yours to fix (record the fix).
2. **The device:** launch, Settings → Sound → Internal sound. The console
   prints `[voxo] <device>: <rate> Hz, <n> frames per block`. Record what
   WASAPI shared mode granted for the 256 asked (`voxo_default_block_
   frames` is 256 off Apple and Android) — that is the **Windows row of
   DECISIONS_6 #9**, with the render time and the XRun count from the
   status line after a minute of playing. If the period is far from 256
   (WASAPI's 10 ms engine period is 480 at 48 kHz), say so; do not change
   the default without the author.
3. **The acceptance suite** (DECISIONS_6 #32): `midi-sink --dev
   --voxo-preset "<a heavy library's .dspreset>" --voxo-storm 60`. Pass =
   the printed `ok  : voxo storm - glitch-free, the visuals at rate` (0
   XRuns, 0 dropped, the visual loop at or above 58 fps). A heavy library:
   the author's Bösendorfer if it is on the box (139 MB on disk, 1 580
   zones), else the VCSL Dan Tranh — `python3 tools/fetch_dan_tranh.py`
   downloads it (34 MB, three velocity layers; needs ffmpeg only for the
   demo half: pass `--no-demo`). Run it three times; record the fps and the
   XRun line of each in your evidence. If it fails on XRuns, the period is
   the first suspect (item 2) — a Voxo change is a bug → test → fix.
4. **The demo and a library by hand:** the Demo instrument button sounds
   under the ROLI (the demo's reach is the whole keyboard); load a library
   through the Instrument row (a `.dslibrary` and a folder's `.dspreset`
   both), read its report; a `--voxo-budget-mb 10` run shows the memory
   line and loads anyway; the sample row with any WAV.
5. **The CC map into the bus:** route a CC to Reverb amount and one to
   Delay amount, turn them from the Airwave or the ROLI; relaunch and see
   the routes survive (#31 fixed the INI loader — your box proves it).
6. **Hotplug on WASAPI** (the DONE line): with the sound on, plug or unplug
   a USB audio device / switch the default output in Windows. miniaudio's
   WASAPI backend follows the default device by itself; record what
   happens (the sound moves, the sound stops, a restart is needed). If the
   Mac's device-selection work has landed in `main` by then, pull and
   re-run with the selection row.
7. **The glide check:** `midi-sink --dev --voxo-bounce <dir>` then
   `uv run tools/voxo_glide_check.py --dir <dir>` — PASS is expected
   (the arithmetic is the same on every box); record the three lines.
8. **The mobile half is not yours**; neither is the licensing page (the Mac
   drafts it).

## Evidence and hand-back

`docs/evidence/step55/windows/SUMMARY.md` (the table: the period granted,
the three storm runs, the hotplug outcome, the glide lines, ctest), the
storm logs and the check's output beside it. Decision entries in
`_work/DECISIONS_6.md` for anything you resolved (the Windows row of #9 is
one; a WASAPI finding is another). Leave the tree uncommitted; report the
evidence path for the author's commit message.

## Open items that are the author's (don't act on them)

* The tag `v2.0.0-alpha.2` and the fold of `DECISIONS_6` into
  `docs/DECISIONS.md` Part VI.
* The Windows release zip carrying `demo/` — say what you found; the Phase-9
  spine is where it gets exercised.
* The demo instrument's recording (the Dan Tranh fills the slot for now).
