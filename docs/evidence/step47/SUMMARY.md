# Step 47 — Voxo skeleton & the desktop backend (macOS) — DONE evidence

Phase 7 (Sound) opens on `v2.0.0-alpha.1`. This step ships `voxo/`, the
sibling sound library, its desktop backend and the desktop wiring; decisions
`DECISIONS_6 #1–#6` (`_work/DECISIONS_6.md`, opened here).

## What shipped

- `voxo/include/voxo.h` — the pure C ABI with **the callback-thread contract
  written at its top**: `voxo_version` (0.1.0), `voxo_default_block_frames`,
  create/destroy, start/stop/running, `voxo_push_midi` (the shell's one
  producer), `voxo_set_input_mode` / `voxo_set_gain` (atomics applied at
  block start), `voxo_render` (the callback's whole body, callable without a
  device), `voxo_stats`. `voxo/include/module.modulemap` for step 53.
- `voxo/src/voxo.cpp` — the core: `core/src/midi_normalizer.cpp` compiled
  from source is the second SPSC ring; a 16-voice pool keyed by
  (channel, note); a sine per voice following note + bend, velocity and
  pressure to level, sustain and the panic CCs in the release logic, a soft
  knee on the sum. `voxo/src/backend_miniaudio.cpp` — miniaudio 0.11.25
  (pinned in the root CMake, header only, its own CMake project skipped),
  the default output at f32 stereo, low-latency profile, an XRun proxy from
  callback timing. `voxo/CMakeLists.txt` — static, `-fno-exceptions
  -fno-rtti -fvisibility=hidden`; desktop-only in the build for now.
- Tests: `tests/voxo_tests.cpp` (the ring, fifteen sines, bend, sustain,
  the panic, gain, the mode, **zero allocations across 4000 storm blocks
  under a counting global allocator**, the knee) and `tests/voxo_c_compile.c`
  (strict C11). `build.yml`'s ctest step names them.
- Desktop: `sumi_midi_harness_set_tap` fans the harness's bytes into Voxo
  under the producer mutex; `main.cpp` creates Voxo beside the harness and
  starts the device on the "Sound" setting (`sound`, `sound_gain` in the
  INI; OFF = the 1.x app); the settings window's Sound section (checkbox,
  volume, status line); `--dev --voxo-storm <s>`, the ROLI proxy.
- Docs: `docs/BUILD.md` (Voxo, the storm flag), `CLAUDE.md` (Phase 7 open;
  Android on the Mac), `_work/ROADMAP_5.md` machine labels for 48/54/60 at
  the author's instruction.

## DONE checks

| check | result |
|---|---|
| `ctest` (7 suites: ABI/C11, normalizer, hostmpe ×2, presets, **voxo_tests**, **voxo_c_compile**) | 7/7 passed — `ctest.log`, `voxo_tests.log` |
| zero allocations in `voxo_render` (counting `operator new`/`delete`, negative control) | 0 news, 0 deletes over 4000 storm blocks |
| the contract at the top of `voxo.h` | written (allocation, locks, logging, blocking, the drain-once rule, the atomics) |
| fifteen sines under MPE, glitch-free at 128 frames — the agent's proxy | `--voxo-storm 30`: 133 106 messages, 11 645 callbacks on "Haut-parleurs MacBook Pro" at 48 000 Hz / **128 frames**, **0 XRuns**, render max 0.065 ms a block, 0 dropped — `storm.log`, exit 0 |
| sines under a live MPE source (the author's hands-on) | confirmed by the author ("really nice") with `sound2midi MPE` as the input: the device opened at 48 000 Hz / 128 frames, the port hot-plugged out and back mid-session without incident — `hands_on.log` |
| the phase invariant | `--field-dump` vs `tests/fixtures/field_512_metal.bin`: max 0, mean 0 (bitwise); `composite_gate.py --backend metal`: bitwise, negative control red |
| other trees untouched | `build-ios` and `build-web` reconfigure with no voxo/miniaudio target |

## How to run

```
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
build/desktop/midi-sink.app/Contents/MacOS/midi-sink --dev --voxo-storm 30   # plays through the speakers
```

## Notes for the record

- The storm and the ROLI share the same path: device bytes and injections
  both pass the harness's producer mutex and its tap; a message reaches
  libsumi and Voxo in the same order.
- The block-size table (`DECISIONS_6 #4`) holds three unmeasured rows
  (Android, Windows, Linux) until steps 48 and 55.
- The mobile builds do not compile Voxo yet (a step never touches another
  platform's build); steps 48/53/54 add it.
