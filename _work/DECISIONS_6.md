# DECISIONS_6 — Phase 7: Sound (Voxo, the sibling library)

Ambiguities resolved during Phase 7 (steps 47–55 of `_work/ROADMAP_5.md`).
Prior history: `docs/DECISIONS.md` (Parts I–V; references written as
`DECISIONS_5 #n` mean Part V). This file merges into that document as Part VI
when the phase ships. The specs are `specs/SOUND_SPEC.md` (`SOUND §n`),
`specs/INSTRUMENT_SPEC.md` and `specs/QUALITY_OF_LIFE_SPEC.md`; where an
entry here and a spec conflict, the entry is the record of what shipped — and
the conflict is flagged to the author, who owns the specs.

## Step 47 — Voxo skeleton & the desktop backend (macOS)

1. **Phase 7 opens on the tag; Android moves to the Mac.** `v2.0.0-alpha.1`
   is out (the author's tag, 2026-09-26) and the core is frozen for the
   phase (`libsumi` is never touched — SOUND §1; the field fixture stays
   bitwise trivially). The author's instruction with the opening: the Linux
   box is no longer needed for Android — the Tab S8 is plugged into the Mac
   and the Gradle / NDK / JDK toolchain was installed there for
   `DECISIONS_5 #88`'s build — so steps 48, 54 and 60 are relabelled
   "macOS machine" in `_work/ROADMAP_5.md` and the working rule reads
   "Android on the Mac too"; the Linux box keeps only the Linux desktop
   (step 55's verification fan-out).

2. **Voxo is a sibling library built from the core's normalizer source, with
   its own voice table.** `voxo/` beside `core/`: `voxo/include/voxo.h` is
   pure C (the `sumi_core.h` rules verbatim — a three-state `VOXO_API`,
   `voxo_version` = 0.1.0, no STL, no exceptions, no callbacks-into-C++
   across it; `module.modulemap` beside it for the Swift import of step 53);
   `voxo/src/` is C++20 with `-fno-exceptions -fno-rtti
   -fvisibility=hidden`, a STATIC archive. It compiles
   `core/src/midi_normalizer.cpp` from source (the include path reaches
   `core/include` for the log-fn typedef and the input-mode enum, and
   `core/src` for the internal header) and never links `libsumi`: one MPE
   decoder, two builds, zero runtime coupling — the decoder's ring IS the
   second SPSC of SOUND §1. What Voxo does NOT share is the core's voice
   mapper: SOUND §1's "voice-table logic" in the core is the §3.3 gesture
   vocabulary (drops, glides, presses), which has no meaning for a sampler;
   Voxo keeps its own pool of 16 voices (the fifteen MPE members and the
   master, or a classic keyboard's polyphony; `max_voices` up to 64) keyed
   by (channel, note), consuming the normalizer's device-agnostic events
   directly (`SUMI_MEV_NOTE_ON/OFF`, `BEND` in semitones with the RPN range
   already applied, `CC`, `CHANNEL_PRESSURE`, `POLY_PRESSURE`). Stealing:
   the oldest releasing voice, else the oldest. The normalizer's log hook
   stays NULL inside Voxo (its mode-change lines would fire on the callback
   thread); the shell reads the effective dialect from `voxo_stats`.

3. **The callback-thread contract, as implemented.** Written at the top of
   `voxo.h` and tested: on the audio thread Voxo allocates nothing, frees
   nothing, takes no lock, logs nothing and makes no blocking call; it reads
   MIDI only from the normalizer's wait-free ring, drained ONCE at block
   start (up to 512 events a block; the rest waits in the 4096-slot ring
   for the next block), and every voice transition happens there, in event
   order, before a sample is written; the shell's settings (master gain,
   input mode) arrive through atomics read at block start — the mode as a
   pending value the callback applies with `sumi_normalizer_set_mode` on
   its own thread, so the normalizer's non-atomic override is written by one
   thread only; the counters go back the same way. `voxo_render` is the
   callback's whole body and is callable with no device (the tests, offline
   bounces). The test (`tests/voxo_tests.cpp`) replaces every global
   `operator new` / `delete` form with counting versions, arms the counters
   after its own buffers exist, drives 4000 blocks of a fifteen-channel
   storm with periodic ring overflows and mode switches through
   `voxo_render`, and asserts zero — with a negative control proving the
   counter counts. The XRun proxy for the author's ear: a callback that
   arrives late by more than half its period, or whose render outran the
   period, counts one XRun; the first eight callbacks after a start are
   not judged (CoreAudio's warm-up burst). A C allocation on the callback
   would escape the counter; the render path holds none by construction
   (the normalizer's drain formats a mode-change line into a stack buffer
   before its silent hook — no heap).

4. **Block-size defaults, the table** (SOUND §1's `[ITERATE: per-platform
   defaults]`; `voxo_default_block_frames`):

   | platform | frames | at 48 kHz | why |
   |---|---|---|---|
   | macOS | 128 | 2.7 ms | CoreAudio honours the period as asked (measured here: the storm ran at exactly 128) |
   | iOS | 128 | 2.7 ms | AVAudioSession's preferred IO buffer; confirmed in step 48 |
   | Android | 192 | 4 ms | a multiple of the common AAudio burst (96/192 on the Tab's class); step 48's spike may move it |
   | Windows | 256 | 5.3 ms | WASAPI shared mode hands back its own engine period; step 55 measures what it took |
   | Linux | 256 | 5.3 ms | ALSA direct or through PulseAudio/PipeWire; step 55 measures |

   The device may run another period than asked; `voxo_stats` reports the
   one it took, and the storm's check requires 128 on the Mac. A shell
   passes 0 for the default; a `[ITERATE]` stays on the three unmeasured
   rows until 48 and 55 fill them.

5. **miniaudio 0.11.25, header-only, one implementation TU, no dlopen on
   Apple.** Pinned by tag through FetchContent (`SOURCE_SUBDIR resources` —
   a directory without a CMakeLists — so the checkout's own CMake project,
   a full `libminiaudio.a` with the vorbis/opus extras, is never added);
   the licence is the author's choice of public domain or MIT-0 (for the
   citations page of step 63). `voxo/src/backend_miniaudio.cpp` is the ONE
   translation unit defining `MINIAUDIO_IMPLEMENTATION`, with decoding,
   encoding, generation, the resource manager, the node graph and the
   engine compiled out (the sampler decodes through dr_wav/dr_flac in step
   50 on its own terms). On Apple `MA_NO_RUNTIME_LINKING` and the
   frameworks linked (CoreFoundation, CoreAudio, AudioToolbox): no dlopen
   at start, and iOS forbids it anyway; Linux and Windows keep miniaudio's
   runtime loading of ALSA / PulseAudio / WASAPI (no `libasound` at link
   time). The device: the platform's default playback, f32 stereo, the
   wanted rate and period, `ma_performance_profile_low_latency`; `stop` is
   synchronous (no callback after it returns). The library is desktop-only
   in the build until the mobile steps (48, 53, 54) add it to theirs — a
   step never touches another platform's build.

6. **The skeleton's sound and the desktop wiring.** A sine per voice:
   pitch 440·2^((note − 69 + bend)/12) recomputed at every bend and glided
   per sample by a 4 ms one-pole (step 49 brings the per-block ratio and
   Hermite interpolation); level = velocity^1.5 × (0.35 + 0.65 × pressure)
   in MPE and wind, velocity alone in classic (pressure is channel or poly
   pressure, smoothed 20 ms); a 3 ms attack and a 40 ms release, the
   sustain pedal holding the release until it lifts, CC 123 releasing and
   CC 120 cutting; one voice at 0.18 full scale, the sum through a soft
   knee (linear to 0.5, compressed toward 1.0) so sixteen in phase never
   wrap. The desktop: the harness gains a tap (`sumi_midi_harness_set_tap`)
   called under the producer mutex right after `sumi_push_midi` for device
   callbacks and injections alike — one producer, two rings, identical
   bytes; `main.cpp` creates Voxo beside the harness and starts the device
   only when the new "Sound" setting asks (`sound`, `sound_gain` in the
   INI; OFF is the 1.x app, nothing regresses), the settings window's
   Sound section carrying the checkbox, the volume and a status line
   (device, rate, period, voices, render time, XRuns, dropped). The
   author's DONE (fifteen sines under the ROLI) has an agent proxy:
   `--dev --voxo-storm <s>` drives fifteen member channels — a note every
   50 frames, a bend sweep, channel pressure and CC 74 every frame,
   ~4400 messages a second — through the harness's inject path with the
   real device open at 128 frames, and fails on any XRun, any dropped
   message, or a period other than 128. Measured here: 30 s, 0 XRuns,
   render max under 0.1 ms a block (see the step's evidence).

## Step 48 — The mobile latency spike (macOS machine: the Tab, then the iPad) — TIMEBOXED

7. **Android: miniaudio's AAudio path is confirmed; the buffer is two bursts;
   the bar is AAudio's own underrun count.** Voxo compiled into the JNI
   shell (`android/cpp`, linked beside `sumi_static`; the one producer
   `shell::push_midi` fans into Voxo under its mutex), started only by the
   evidence intent `--ei voxoSpike <s>` until step 54 wires the setting and
   the lifecycle. On the Galaxy Tab S8 Ultra (SM-X906B, Android 16,
   `aaudio.mmap_policy` 2): miniaudio opened AAudio with
   `AAUDIO_PERFORMANCE_MODE_LOW_LATENCY` granted, usage GAME, a **192-frame
   burst (4 ms at 48 kHz)** and, untouched, an eight-burst buffer (1536
   frames, 32 ms) — the platform's own numbers read through
   `AAudioStream_*` resolved from `libaaudio.so` in the backend (the calls
   miniaudio does not load). The backend now sets the buffer to **two
   bursts** after opening (Oboe's practice): the timestamp-derived output
   latency fell from 55.4 ms to **29.3 ms** (the HAL's own ~21 ms beyond the
   buffer) with **0 AAudio underruns** across the 40 s window that carried
   the visual storm (`--ei stormSeconds 12`) as load; Voxo's stricter proxy
   (a callback more than half a period late — 2 ms here) counted 34, which
   the platform's count says were absorbed by the second burst. The
   measured path on the one clock (`shell::now_s` = `voxo_now_seconds` =
   CLOCK_MONOTONIC): **push → callback 1.8 ms median (4.0 max — one
   burst)**, **touch-down → callback 3.1 ms median, 6.9 max** over twenty
   injected touches (`adb shell input swipe`, the mark at the Kotlin touch
   callback as the Phase-4 latency marks); touch-to-DAC on the Tab is
   therefore ~32 ms with the two-burst buffer (~58 ms with the default), the
   input pipeline before the callback excluded. The Mac's microphone beside
   the Tab heard the tones (the onset count in the evidence). Verdict:
   miniaudio stays; no escape hatch. Step 54 carries: a buffer tuner
   (start at two bursts, one burst more on each AAudio underrun — the
   `[ITERATE: XRun budget]` of SOUND §5 is **AAudio's count = 0**; the proxy
   is a diagnostic), audio focus, the foreground-only lifecycle. The
   block-size table (#4): Android's row is **AAudio's burst (192 on this
   device), asked as 192**, the buffer two bursts.

8. **iOS: miniaudio's CoreAudio path is confirmed; the shell owns the
   session.** Voxo compiled for iOS (`libvoxo.a` beside the core in
   `build-ios`; the miniaudio TU is Objective-C++ on Apple because the
   CoreAudio backend speaks to AVAudioSession through Objective-C headers;
   `module.modulemap` gives Swift `import Voxo`), created with the instance
   in `SumiCanvasView`, fed by the canvas's serial MIDI queue — the one
   producer — through a `push()` wrapper over every `sumi_push_midi` site,
   started only by the launch argument `--voxo-spike <s>` until step 53
   wires the setting. The backend opens its own `ma_context` with
   `sessionCategory = none`: the SHELL configures the AVAudioSession
   (SOUND §4 — playback, a preferred 48 kHz, a preferred IO buffer of
   128 frames, later the interruptions), miniaudio only activates it. On
   the iPad Air 11-inch (M4): the session granted **48 000 Hz and exactly
   128 frames (2.667 ms)**, reported **9.73 ms output latency**; Voxo ran
   17 561 callbacks at 128 frames with **0 XRuns (proxy)** and a render
   maximum of 0.039 ms while the canvas's own storm (10 synthetic voices,
   18 371 messages over 16 s) rode the queue as load; **push → callback
   1.27 ms median, 2.38 p90, 2.65 max (n=40)** on the one clock (`voxo_now_seconds` for both marks). No
   touch pairs: the Mac cannot inject touches on the iPad — the touch mark
   is wired (`playTouchBegin`) for the author's finger, and the Phase-4
   measurement (0.39 ms touch-down → drop) says the handler is not where
   the time goes. Touch-to-DAC on the iPad is therefore ~14 ms (the
   handler-to-callback of a block, the IO buffer, the session's output
   latency). The Mac's microphone beside the iPad heard the tones. Verdict:
   miniaudio stays; AVAudioEngine is not needed. Step 53 carries the
   session's interruption and route handling and the foreground-only pause.

9. **The block-size table, the mobile rows measured** (#4 revised):

   | platform | asked | granted here | output path | evidence |
   |---|---|---|---|---|
   | macOS | 128 | 128 (CoreAudio) | — | step 47 |
   | iOS | 128 | 128 (2.667 ms IO buffer) | 9.73 ms session output latency | step 48 |
   | Android | 192 | AAudio's burst, 192 (4 ms); buffer two bursts | 29.3 ms timestamp-derived (55.4 with the default buffer) | step 48 |
   | Windows | 256 | `[ITERATE]` step 55 | | |
   | Linux | 256 | `[ITERATE]` step 55 | | |

   `voxo_default_block_frames` keeps 128 / 128 / 192 / 256 / 256. The
   latency probe (`note_ons`, `last_note_on_seconds`, `voxo_now_seconds`)
   and the backend's fields (`frames_per_burst`, `buffer_frames`,
   `device_xruns`, `output_latency_ms`, `low_latency`) grew the ABI to
   Voxo 0.2.0 additively; they stay as the diagnostics step 55's combined
   stress reads.
