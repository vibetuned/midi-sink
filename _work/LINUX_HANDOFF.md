# Handoff — Linux box: Step 55 (Phase 7 on the Linux desktop)

Written on the macOS machine, 2026-09-26. You are the agent on the author's
Linux box (GCC or Clang + Ninja, the GL backend, the ROLI and the Airwave
over ALSA, ALSA / PulseAudio / PipeWire for the sound). Do not touch the
macOS, iOS, Windows, Android or web lanes — **Android moved to the Mac in
this phase (DECISIONS_6 #1); the Tab is no longer your business.**

**Your step:** Phase 7 gave the app a sound. `voxo/` is a sibling library
beside the core (pure C `voxo/include/voxo.h`, C++20 behind it) that plays
Decent Sampler instruments from the same MIDI bytes the visuals get; the
desktop shell links it, feeds it through the harness's tap, and drives it
from the settings window's Sound section. Every line of it was written and
measured on the Mac (CoreAudio), the Tab (AAudio) and the iPad; **nothing has
run on ALSA or PulseAudio**. Build, run the suites, run the acceptance suite,
look, fix (bug → regression test → fix), record — and fill the Linux row of
the block-size table.

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
   **#33**; the Windows agent writes in parallel — take the next free
   number when you write, and say which box in the heading). Most relevant:
   #2–#6 (the library, the callback contract, the block-size table,
   miniaudio), #9 (the table with the mobile rows measured — yours is
   `[ITERATE]`), #10–#12 (Hermite, the sample swap, Local Control),
   #13–#15 (the format and the compat report), #16–#18 (layers, the filter,
   the contract under fifteen stacked voices), #19–#21 (the bus, the memory
   gate, the worker-thread load, the demo slot), #22 (the Dan Tranh demo),
   #25 and #27 (MPE's zone, the reach, the bus routes in the CC map), #31
   (the loaders that dropped the bus targets), #32 (the acceptance suite's
   pass rule and the XRun budget).
3. `docs/evidence/step47/` … `step54/` — what landed and how it measured
   (the storm and the spike numbers are the bar).
4. `docs/BUILD.md` — every harness test, gate and lab flag with its
   command line (the Voxo paragraphs are new).

## What is new for you

* **Third parties, all fetched by CMake at configure (network needed
   once):** miniaudio 0.11.25 (header, one implementation TU; ALSA and
   PulseAudio are `dlopen`ed at runtime — **no new apt package**; the
   README's `libasound2-dev` is the MIDI side's, as before), pugixml v1.16,
   miniz 3.1.2, dr_libs (dr_wav, dr_flac). `voxo/CMakeLists.txt` links
   `Threads`, `dl` and `m` on Linux.
* **New ctest suites:** `voxo_tests` (a counting global allocator asserts
  the callback allocates nothing), `voxo_preset_tests` (the fixtures under
  `tests/fixtures/dspresets/`, the docs-copy assertion, the bus, the gate,
  the reach, the routes), `voxo_c_compile` (strict C11), `voxo_fuzz
  --seconds 5` (a mutation fuzzer over the parser, the decoders and the
  zip reader). Nine suites in all. `build.yml` already runs them on
  ubuntu-24.04 — a failure there is yours first.
* **The desktop shell:** the Sound section (Internal sound, Volume, the
  Sample (WAV) row, the Instrument (.dspreset / .dslibrary) row with its
  compat report and the memory advice, the Demo instrument button, the
  status line with voices / layers / render time / XRuns); the CC map's
  target list carries the bus (reverb and delay, from 1000); `settings.ini`
  gained `sound`, `sound_gain`, `sound_sample`, `sound_root`,
  `sound_preset`. The preset loads on a worker thread. `desktop/src/
  sys_info.cpp` answers the free memory (`/proc/meminfo` MemAvailable) and
  the resource dir (`/proc/self/exe`; an installed binary looks under
  `<prefix>/share/midi-sink`); `desktop/src/wav_io.cpp` reads the sample
  row's WAV. The demo instrument is copied **beside the executable** as
  `demo/` in the build tree and installed to `share/midi-sink/demo` by the
  `desktop-integration` component (so the .deb carries it — verify with
  `cmake --install build --component desktop-integration --prefix
  ~/.local` and a launch from `~/.local/bin`; the Demo button must be
  there).
* **Lab flags:** `--dev --voxo-storm <s>` (the acceptance suite's engine:
  fifteen member channels of notes, bends, pressure and CC 74 through the
  harness while the device runs; prints its verdict), `--voxo-preset
  <path>` (the run's instrument), `--voxo-budget-mb <n>` (the gate's
  advice), `--voxo-load <path>` (a compat report, headless), `--voxo-bounce
  <dir>` + `uv run tools/voxo_glide_check.py` (the Hermite check; needs
  `uv`, or numpy).

## Checklist

1. **Build + ctest** (`cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   && cmake --build build && ctest --test-dir build`): nine suites. Both
   GCC and Clang if the box has both; a warning is not a failure, an error
   is yours to fix (record the fix).
2. **The device:** launch, Settings → Sound → Internal sound. The console
   prints `[voxo] <device>: <rate> Hz, <n> frames per block`. Record what
   the backend granted for the 256 asked, and WHICH backend miniaudio chose
   (PulseAudio / PipeWire's Pulse shim / ALSA direct — the device name
   tells; `pw-top` or `pactl list sink-inputs` confirms) — that is the
   **Linux row of DECISIONS_6 #9**, with the render time and the XRun
   count from the status line after a minute of playing. If PulseAudio adds
   a deep buffer, note the latency you hear against the Mac's ~3 ms; do
   not change the default without the author.
3. **The acceptance suite** (DECISIONS_6 #32): `midi-sink --dev
   --voxo-preset "<a heavy library's .dspreset>" --voxo-storm 60`. Pass =
   the printed `ok  : voxo storm - glitch-free, the visuals at rate` (0
   XRuns, 0 dropped, the visual loop at or above 58 fps — on GL the host
   presents, so the loop's rate is the swap's). A heavy library: the
   author's Bösendorfer if it is on the box, else the VCSL Dan Tranh —
   `python3 tools/fetch_dan_tranh.py --no-demo` downloads it (34 MB, three
   velocity layers). Run it three times; record the fps and the XRun line
   of each. If it fails on XRuns, the period and the backend are the first
   suspects (item 2) — a Voxo change is a bug → test → fix.
4. **The demo and a library by hand:** the Demo instrument button sounds
   under the ROLI; load a library through the Instrument row (a
   `.dslibrary` and a folder's `.dspreset` both), read its report; a
   `--voxo-budget-mb 10` run shows the memory line and loads anyway; the
   sample row with any WAV.
5. **The CC map into the bus:** route a CC to Reverb amount and one to
   Delay amount, turn them from the Airwave or the ROLI; relaunch and see
   the routes survive (#31 fixed the INI loader — your box proves it).
6. **Hotplug on ALSA / Pulse** (the DONE line): with the sound on, plug or
   unplug a USB audio device / change the default sink. Record what
   happens (the sound moves, the sound stops, a restart is needed). If the
   Mac's device-selection work has landed in `main` by then, pull and
   re-run with the selection row.
7. **The glide check:** `midi-sink --dev --voxo-bounce <dir>` then
   `uv run tools/voxo_glide_check.py --dir <dir>` — PASS is expected;
   record the three lines.
8. **The packaging:** `cmake --install … --component desktop-integration`
   (item above) and, if the box builds the .deb, that `demo/` is inside
   (`dpkg -c`). The mobile half is not yours; neither is the licensing page.

## Evidence and hand-back

`docs/evidence/step55/linux/SUMMARY.md` (the table: the backend and period
granted, the three storm runs, the hotplug outcome, the glide lines, ctest,
the install check), the storm logs and the check's output beside it.
Decision entries in `_work/DECISIONS_6.md` for anything you resolved (the
Linux row of #9 is one; a Pulse / ALSA finding is another). Leave the tree
uncommitted; report the evidence path for the author's commit message.

## Open items that are the author's (don't act on them)

* The tag `v2.0.0-alpha.2` and the fold of `DECISIONS_6` into
  `docs/DECISIONS.md` Part VI.
* The demo instrument's recording (the Dan Tranh fills the slot for now).
