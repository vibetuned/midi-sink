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

## Step 49 — Voice dispatch & pitch — "it glides" (macOS)

10. **4-point Hermite, and the pitch as a straight line across each block —
    the measurement.** The voice reads THE sample at the ratio
    2^((note − root + bend)/12) × sample_rate / device_rate; the ratio is
    recomputed at block start from the events just drained and reached by a
    LINEAR ramp across the block (SOUND §2's "per-block ratio with per-sample
    ramping", taken literally: a one-pole toward a stepped target, the
    skeleton's, left a ripple at the block rate that the spectral check
    showed as a flat floor); the read is 4-point Hermite (Catmull-Rom
    tangents), linear kept behind `voxo_set_interpolation` as the lab's
    comparison only. The check (`--dev --voxo-bounce`, then
    `uv run tools/voxo_glide_check.py`): six partials at 1/k rooted at A3,
    recorded at 12 kHz so the top partial sits at 0.22 of the sample's own
    Nyquist (where an acoustic sample keeps its strong partials — a 48 kHz
    test tone exercises no interpolator at all), read at 48 kHz through
    Voxo's real renderer: two static holds at ∓45 semitones and the full
    ±48 sweep over 8 s, each once per interpolation; 4096-point
    Blackman-Harris frames (a Hann window's sidelobes alone set a −37 dB
    floor), the partials' bins — widened by each frame's pitch excursion —
    as signal, the rest as artefact. Hermite: **−61.8 / −62.4 dB** on the
    holds, **−59.9 dB worst, −62.3 median** on the sweep; linear −47.0 /
    −48.1 / −47.0 — **12.8–14.8 dB worse**. The bar in the tool: Hermite at
    or under −55 dB and linear at least 10 dB behind, for all three cases.
    Recorded limits: at 0.44 of the sample's Nyquist (a 6 kHz recording of
    the same tone) the two kernels sit 7 dB apart (−42 vs −35): no 4-point
    read rescues content that close to its Nyquist — a library's zones
    (step 50) and multisampling are the answer there, not a bigger kernel;
    the sweep's corners splatter −40 dB for a frame whichever the read (the
    chirp's own spectrum), so the sweep is judged between its corners and
    the holds judge the endpoints; and the up-glide of a single sample
    aliases inherently once ratio × its top partial passes Nyquist — the
    bounce keeps 6 × 220 × 16 = 21.1 kHz under it by construction, so what
    it measures is the interpolation and nothing else. Pure numpy through
    `uv run` (the script's own header pins it); the author's call.

11. **One sample, published to the callback by swap; the desktop reads WAV
    itself for now.** `voxo_set_sample` copies the frames on the shell's
    thread into one padded block (two zero frames before, four after: the
    4-point read never bounds-checks), hands it to the callback through a
    pending atomic; the callback swaps it in at its next block start, ends
    every voice on the previous sample there, and parks the old one in a
    retired slot the SHELL frees at its next set/clear/destroy — the
    callback never frees. Stereo or mono, any rate (1 kHz – 384 kHz), root
    note fractional; the sample plays once (no loop until step 51), a
    retrigger of the same (channel, note) restarts it from the head, the
    end of the sample ends the voice; with no sample the sine stays (the
    tests' and the storm's sound, and the "no instrument" fallback). The
    desktop shell gained `wav_io` (PCM 16/24/32, float 32, mono/stereo — the
    lab's writer for the bounce and the reader for the "Sample (WAV)" row of
    the Sound section, with the root note beside it; `sound_sample` /
    `sound_root` in the INI); Voxo's own decoding is step 50's dr_wav/
    dr_flac and this reader retires when the format lands. Voxo is 0.3.0.

12. **Local Control, the strip's volume, the two switches — what each side
    owns.** CC 122 is tracked by Voxo (a CC in the stream or
    `voxo_set_local_control` sets it; `voxo_stats` reports it) and APPLIED
    by the shell: only the shell knows which bytes are its own play
    surface's, so Local Control OFF means the shell stops fanning those
    into `voxo_push_midi` while it keeps sending them out over MIDI and
    keeps feeding external input in — the tablets wire that in 53/54 (the
    desktop has no play surface). "Internal sound" and "outbound MIDI" are
    independent switches by construction: the desktop's Sound setting and
    the tablets' transports never touch each other. The strip's volume →
    `voxo_set_gain` is the tablets' step too; the desktop's Volume slider is
    the same call.

## Step 50 — Decent Sampler subset & the compat report (macOS, headless)

13. **The front end is shell-thread C++ with the STL inside; pugixml, miniz
    and dr_libs compile into the archive.** `voxo/src/ds_preset.{h,cpp}`
    parses the `.dspreset` XML into an instrument model (groups with the
    cascade `<groups>` → `<group>` → `<sample>`: ADSR, ampVelTrack, volume in
    dB, pan, tuning, seqMode / seqLength / seqPosition, trigger, tags; zones
    with lo/hi note and velocity, rootNote, tuning, start/end, the loop
    quartet; effects at group and instrument level with their parameters;
    `<midi>` sources — cc, note ranges, velocity — and `<modulators>`; the
    UI's controls for their bindings and starting values), reads the samples
    through a `Reader` (the preset's folder, or the `.dslibrary` zip whose
    first `.dspreset` is the preset and whose folder is the base), decodes
    them to interleaved float in memory (dr_wav, dr_flac, and a reader of
    our own for AIFF / AIFF-C PCM), deduplicated by path — the Bösendorfer's
    1 580 zones reuse 158 files. None of it is ever touched from the
    callback: the voice reads what `voxo.cpp` publishes. The libraries are
    pinned by FetchContent (pugixml v1.16 MIT, miniz 3.1.2 MIT, dr_libs at
    a master commit, public domain / MIT-0; the citations page of step 63)
    and compiled straight into `libvoxo.a` — pugixml without exceptions or
    XPath, miniz's four split sources without the writer or time, and a
    one-line `miniz_export.h` stub for its generated header — so no
    third-party build system enters the tree and the tablets compile the
    same sources (Gradle and the iOS CMake both rebuilt green).

14. **The compat report: its copy is the library's, and AIFF is in.**
    `voxo_load_preset` refuses only what is not a preset — not well-formed
    XML (pugixml's description and byte), no `<DecentSampler>` root, an
    empty or unopenable file, a zip without a preset — and loads everything
    else, with notes: ten `VOXO_NOTE_*` bits (missing samples, streaming,
    chorus, convolution, EQ and the other filters, an unknown effect, the
    modulators, note sequences, an unknown binding, the custom UI), each a
    canonical sentence from `voxo_note_copy` in the documented order after
    a summary line; `voxo/COMPAT_REPORT.md` is that copy, and
    `tests/voxo_preset_tests.cpp` asserts the file quotes every sentence
    verbatim — the docs' copy is the library's by test. Every number is
    clamped or defaulted (`badnumbers.dspreset`: negative roots, "abc",
    1e309, inverted ranges, a garbage translation table — it loads). The
    format's `[ITERATE: AIFF?]` is answered by the fixture library itself:
    Decent Sampler's stock Basic Piano ships eleven `.aif` files, so AIFF
    (and AIFF-C `NONE` / `sowt`, 8–32-bit PCM) is read; compressed AIFF-C
    and any other container are "could not be read" notes, never refusals.
    Binding types are read as Decent Sampler writes them: `amp`, `effect`,
    `general`, `control`, `modulator`, the UI-targeting `labeled_knob` /
    `knob` / `control` / `button` / `menu` at `level="ui"`, and
    `note_sequence` (the arpeggiator's — counted under the sequences note,
    since the sequences are what plays without); anything else is the
    unknown-binding note with its name. The mutation fuzzer
    (`tests/voxo_fuzz`: bit flips, truncation, chunks of other seeds, digit
    scrambling, duplication, over the presets, the sample files and the
    zip) runs 5 s under ctest and an hour as the step's evidence.

15. **Until the layers land, the zone under middle C is the sound.** Step 50
    is headless by charter, but a loaded instrument should not be silent:
    `voxo_load_preset` publishes the zone covering note 60 at velocity 100
    (attack-triggered; else the nearest) through the step-49 sample swap at
    its effective root (`rootNote − tuning`), so the author hears the
    library's middle register under the ROLI now; step 51 replaces the
    bridge with the real dispatch (layers, round robins, release samples,
    loops, the filter, the bindings). The load is synchronous on the
    caller's thread — the Bösendorfer's 139 MB on disk (277 MB as float)
    takes under a second, The Spellsinger's 928 MB four — and stalls the
    settings window meanwhile, until step 52's preload gate puts it behind
    progress and the advisory memory gate says what a library will cost.
    The desktop: an "Instrument (.dspreset / .dslibrary)" row in the Sound
    section (`sound_preset` in the INI; it wins over the sample row), the
    report shown once beneath it until the next load, and
    `--dev --voxo-load <preset>` printing the report headlessly (the six
    libraries on this Mac in the evidence). The DS MPE elements the author's
    own generator writes — `<mpePressure>` and `<mpeTimbre>` under
    `<modulators>`, with `scope`, the smoothing times and `<binding>`s — are
    parsed as supported sources; LFOs, envelopes and sequences under the
    same element are the modulators note.

## Step 51 — Layers, round robins, release samples, loops, filter, smoothing & bindings (macOS)

16. **The compiled instrument, and the voice as a stack of layers.** The
    parsed model is COMPILED on the shell's thread (`voxo/src/instrument.
    {h,cpp}`) into what the callback plays: flat arrays of zones (a pointer
    into the decoded frames re-laid with the read's padding, the ranges,
    the effective root, gain and pan, start/end, the loop quartet, the
    sequence position, the release flag) and groups (ADSR, ampVelTrack, the
    sequence mode, the low-pass, the MPE curves), plus a per-group RUNTIME
    block (the round-robin counter, a random seed, the live gain, cutoff,
    resonance and envelope times) that becomes the callback's after the
    swap — the step-49 swap protocol now carries whole instruments, and
    `voxo_set_sample` is a one-zone instrument. A PERFORMANCE voice —
    still one per (channel, note) — stacks up to eight LAYERS: at note-on
    every group's zones matching the note and velocity are candidates;
    `always` starts them all, `round_robin` the ones at the group's next
    sequence position (skipping positions no zone holds), `random` a
    position at random; a retrigger releases the old stack fast and starts
    the new. Velocity crossfades: where two zones of a group share notes and
    overlap in velocity, each fades across the overlap (equal power) — the
    Decent Sampler format has no crossfade attribute, and its libraries
    split hard, so a hard split stays hard and stacked zones (identical
    ranges) play in full. Release samples: a group's `trigger="release"`
    zones start on the note-off (or when the pedal lifts a pedalled note)
    in the same voice, at the note's velocity, with their own envelope; a
    panic starts none. Each layer: a linear attack (2 ms at least), a decay
    and a release as one-poles landing within 1% of their target in the
    preset's time (5 ms at least for the release), the sustain level; the
    read at the voice's pitch through Hermite (linear as the lab's) with the
    loop — past `loopEnd` the position wraps by the loop's length, and over
    the last `loopCrossfade` frames the sample a loop-length behind is mixed
    in at equal power, so a 3 s pad holds thirty seconds (`loop.dspreset`
    holds 30 s within 20% of its first second; `noloop` ends at 3 s); DS's
    `ampVelTrack` as a linear velocity track; pan at constant power.

17. **The filter, the MPE sources and the bindings — the defaults and the
    preset's.** Every layer runs a 2-pole state-variable low-pass (Simper's;
    its coefficient computed at the block's start and end and ramped per
    sample; bypassed while open above 19 kHz with no resonance) from the
    group's `lowpass*` effect, else the instrument's, else open — because
    CC 74 must move something (SOUND §3). The voice's sources: pressure
    (channel pressure), timbre (CC 74, per channel — a member's slide),
    swirl (poly pressure 0xA0, the `[ITERATE]` answered: offered as a
    source, no target by default, no Decent Sampler syntax binds it yet);
    each smoothed per block by a one-pole with the RISING or FALLING time
    of the group's `<mpePressure>` / `<mpeTimbre>` element (20 / 40 ms when
    the preset says nothing), then ramped across the block. Without a
    binding the defaults apply: pressure → expression as 0.35 + 0.65 p
    (the skeleton's curve), timbre → the live cutoff × 2^((t − 0.5) × 6)
    (the slide at its centre leaves the preset's filter; fully down closes
    it three octaves; a channel that never sent CC 74 sits at the centre).
    A preset's `<mpePressure>` binding on `AMP_VOLUME` or `<mpeTimbre>` on
    `FX_FILTER_FREQUENCY` REPLACES the default for its group (or every
    group at instrument level) — the binding's translation (linear, table,
    fixed) sampled into a 33-point curve the callback looks up; a
    `<velocity>` binding on the cutoff scales it by DS's modAmount; `<cc>`
    bindings on `AMP_VOLUME`, `FX_FILTER_FREQUENCY` / `RESONANCE` and
    `ENV_*` move the group's live state when the CC arrives (envelope times
    for the notes that follow); the UI controls' starting values set the
    same parameters once at compile time, and a `TAG_VOLUME` starting value
    scales the groups carrying the tag (the Spellsinger's "Wicked" layers).
    Pressure has no effect in the classic dialect (the level is the
    velocity's). Measured: `filter_default` brightness (the first
    difference's energy) open : centre : closed better than 2 : 1 each step
    under CC 74; `filter_bound` doubles its level from pressure 0 to 127
    through `AMP_VOLUME` 0.5..1 and darkens three-fold through its table.

18. **The contract under fifteen voices of stacked samples.** With
    `stack.dspreset` (six looping, filtered zones on every note) fifteen
    members hold 90 layers; two seconds of blocks under bends, pressure and
    CC 74 on every channel allocate nothing (the counting allocator in the
    preset suite), and a 128-frame block renders in **0.225 ms** on this
    Mac's debug build against a 2.667 ms period. Voxo is 0.5.0 (the
    `active_layers` stat). The layers cap (eight per voice) and the voice
    pool (sixteen) are the step's numbers; the combined stress of step 55
    revisits them on the tablets.

## Step 52 — Bus reverb & delay, preload gate (macOS)

19. **The bus: Freeverb and a feedback delay after the sum, real-time safe by
    construction.** `voxo/src/bus.{h,cpp}`: Jezar's public-domain Freeverb
    design (eight combs and four allpasses per channel, the right channel's
    lengths 23 samples longer, the 44.1 kHz lengths scaled to the device's
    rate) and a stereo feedback delay (two seconds of line at the rate). The
    buffers are allocated ONCE on the shell's thread when the rate is known
    (create, open, stop — never while the callback runs) and owned by the
    callback; the parameters are plain numbers in the compiled instrument's
    `bus` block — the preset's first `<effect type="reverb">` (roomSize,
    damping, wetLevel) and first `type="delay"` (delayTime in seconds,
    feedback, stereoOffset, wetLevel; `musical_time` has no tempo here and
    reads as seconds), the UI controls' starting values on `FX_REVERB_*` /
    `FX_DELAY_*` (the stock piano's "Reverb" knob at 50% through its
    `factor="0.01"`), and `<cc>` bindings on the same targets moving the
    live copy when the CC arrives. The bus runs post-sum, before the master
    gain and the knee, never per voice; its tail is dropped at an instrument
    swap; with no instrument there is no bus. Measured (`bus.dspreset`, a
    19 ms constant): the dry preset silent after the note, the reverb
    ringing at −30 dB, the delay's echoes at 0.25 and 0.5 s standing above
    the reverb between them; fifteen looping, filtered voices through both
    effects allocate nothing and render a 128-frame block in 0.06 ms; the
    desktop storm with `pad_bus.dspreset` (the looping pad, the low-pass,
    both effects) on the real device at 128 frames: the XRun count in the
    evidence.

20. **The advisory gate: an estimate off the headers, the shell's advice, a
    note — never a wall.** Before any decoding, `voxo_ds::load` reads the
    first 4 KB of every distinct sample (a folder's file or a zip entry
    inflated only that far) and sums the decoded size from the header — the
    WAV data chunk over its bytes per sample, FLAC's STREAMINFO total samples
    and channels, AIFF's COMM frame count, each × channels × 4 bytes; an
    unreadable header counts twice its file size. The SHELL sets the advice
    (`voxo_set_memory_budget`; 0 = no gate): the desktop refreshes it before
    each load as 60% of the free physical memory (macOS free + inactive
    pages, Linux MemAvailable, Windows available physical — `desktop/src/
    sys_info.cpp`), iOS will use `os_proc_available_memory` (53), Android the
    activity manager (54). An estimate over the advice adds
    `VOXO_NOTE_MEMORY` — "Memory: this library is larger than advised for
    this device; it is loaded anyway. (about N MB against M MB advised)" —
    first in the report, and the load proceeds (`minimal` over a 1 KB budget
    loads and plays; the Bösendorfer over a 100 MB budget in the evidence).
    The report carries `memory_estimate` and `memory_budget` beside
    `memory_bytes`. Preload-first stands (SOUND §3): everything to memory,
    streaming deferred to Voxo Dorean.

21. **Preload behind a worker on the desktop, and the demo instrument's
    slot.** The desktop loads a preset on a worker thread — the only shell
    thread that touches Voxo's load while it runs; the main thread keeps to
    gain and mode, the sample and instrument rows wait — polled each frame
    into the report ("Loading … " meanwhile), joined at teardown before
    Voxo goes; the tablets' steps do the same behind their own progress.
    The demo instrument's slot is `voxo/demo/` (`demo.dspreset` +
    `Samples/`): the desktop bundles it into `Resources/demo` (macOS) or
    beside the executable (Windows, Linux), the Sound section's "Demo
    instrument" button loads it, and `app_resource_dir()` finds it. What filled the slot first was a placeholder — a synthesised music
    box — replaced the same day by the author's pick, the Dan Tranh (#22);
    nothing else is ever bundled. The lab
    bench gained `--voxo-preset <path>` (the run's instrument, the setting
    untouched — the storm's material) and `--voxo-budget-mb <n>` (the
    gate's advice for the run, honoured by `--voxo-load` too). Voxo is
    0.6.0.

## Step 53 — iOS (macOS machine)

22. **The demo instrument is the Dan Tranh, CC0, from the Versilian Community
    Sample Library — the author's pick, and a test library besides.** SOUND
    §3's `[ITERATE: record it ourselves]` is answered with an existing
    public-domain recording: the VCSL's Dan Tranh (a Vietnamese zither,
    CC0 1.0, https://github.com/sgossner/VCSL), which the author named as
    the demo and as a good test instrument. `tools/fetch_dan_tranh.py`
    downloads its "Normal" articulation (48 WAVs, 16 notes × 3 velocity
    layers, 34 MB) into `~/Music/midi-sink/Dan Tranh (VCSL)/` with a
    `.dspreset` converted from the SFZ (regions → zones with lokey/hikey,
    pitch_keycenter, lovel/hivel, offset → start, volume in dB; the group's
    ampeg attack and release) — the three-layer test library, outside the
    tree — and writes the bundled demo into `voxo/demo/`: the f layer
    alone, sixteen samples at 32 kHz 16-bit mono, the offset applied,
    trimmed to three seconds with a fade, each normalised to −3 dBFS, the
    zones stretched across the whole keyboard (the instrument is pentatonic
    and its SFZ leaves gaps), a touch of the bus reverb — 2.9 MB, with
    `LICENSE.txt` naming the source and the conversion. The desktop bundle
    and the iOS app carry it (the iOS project references the folder whole;
    Android's step does its assets).

23. **The iPad's sound: the shell owns the session, the sound is foreground
    only, and the background-mode key stays for CoreMIDI.**
    `SoundController` (Swift) configures the AVAudioSession as playback
    with mixWithOthers, a preferred 48 kHz and a 128-frame IO buffer (step
    48's numbers), and starts Voxo's device only while the app is active
    and the setting is on: the scene phase drives it beside the display
    link — `.background` stops the device, `.active` starts it again — so
    the sound pauses with the visuals and returns with them (SOUND §4).
    Interruptions (a call, Siri) stop the device on `began` and restart it
    on `ended` when iOS says to resume; a route change (headphones in or
    out, a Bluetooth speaker) restarts it on the new route. SOUND §4's "no
    `UIBackgroundModes`" meets the shell as it is: the `audio` mode has
    been in the plist since Phase 4, because `MIDISourceCreate` returns
    kMIDINotPermitted without it (DECISIONS_3 #24); the key stays for the
    virtual MIDI source, and Voxo's device is stopped on backgrounding by
    the shell's own hand, so no audio runs in the background regardless —
    flagged here for the author, the spec's owner. First launch makes a
    sound: the setting defaults to ON with the demo instrument (the
    tablets are where "a controller without a sound" was the complaint),
    the volume 0.8; OFF is the 1.x app. Local Control is a toggle on the
    Sound page applied as #12 says: the canvas tags its own bytes (touch,
    pen, strip) and fans them into Voxo only while it is on; an external
    controller's bytes and the session's control CCs always go through.

24. **Instruments on the iPad: Documents/Instruments, the Files import,
    the gate on `os_proc_available_memory`.** Libraries live in the app's
    `Documents/Instruments` (visible in Files as On My iPad → midi-sink →
    Instruments, since the app already shares its documents): the Sound
    page lists every `.dspreset` and `.dslibrary` found there, the demo
    first, "a sine per voice" last; "Import an instrument…" takes a
    `.dslibrary` or the FOLDER holding a `.dspreset` and its samples
    (security-scoped, copied whole), and a swipe deletes. A pick loads on
    a background queue with a progress row, the compat report beneath it
    once loaded; the advice is 60% of `os_proc_available_memory()` (what
    the process may still take before Jetsam), refreshed before every load
    and shown under the report; the lab's launch arguments
    `--voxo-instrument <relative path | demo>` and `--voxo-budget-mb <n>`
    drive the evidence. The numbers of the demo on the iPad and the harp
    library over a 10 MB advice are in the step's evidence.

25. **MPE's zone in Voxo: the master channel's messages reach every member;
    every shell tells Voxo the dialect.** The author's first hands-on on the
    iPad: "the sound is ok but the sustain is not working, nor the pitch
    bend". Two causes. Voxo keyed sustain, pressure and the slide per
    channel, and applied bend per channel, while under MPE the pedal (the
    ROLI's, the strip's), the master's bend (the strip's wheel), the
    master's pressure and CC 74 are ZONE messages on the master channel
    that every member obeys (the core's mapper already treated the master's
    bend as a global bend). Voxo now reads the zone
    (`sumi_normalizer_zone`) and the resolved dialect at every block start;
    under MPE or wind a message on the master channel fans out to the
    zone's channels — the pedal holds every member's voices, the master's
    bend adds to each member's own (tested: +2 on the master over a
    member's +2 gives +4), pressure and CC 74 reach every member's voices;
    in the classic dialect there is no zone and the channels stay apart.
    And the iPad never told Voxo the dialect: the normalizer inside Voxo
    sat in the automatic mode, whose classic default gives a member
    channel's bend the ±2 range (±48 only once MPE is resolved) and gates
    the pressure off — so a ROLI slide moved a semitone at most. The rule
    for every shell, as the desktop already did: the input mode the
    session sets on the core is mirrored into `voxo_set_input_mode` (the
    iPad in `applyInputMode`; Android's step follows). Voxo stays 0.6.0
    (no ABI change).

26. **The iPad takes libraries from anywhere: the types are declared, the
    app is a handler, and Sound has its own row in the sheet.** The author,
    on the first build: "the plist is declaring the dslibrary so we can
    AirDrop or pick the libraries? Also we don't have any way to pick that
    in the app." It was not, and the picker had no type to filter on. The
    project now imports the two Decent Sampler types
    (`com.decentsamples.dslibrary`, a zip; `com.decentsamples.dspreset`,
    XML — imported declarations, since the format is Decent Sampler's) and
    registers the app as an Alternate handler for both, so a `.dslibrary`
    arrives by AirDrop, Mail, the Files app's share sheet or "Open in
    midi-sink"; `onOpenURL` copies it into Documents/Instruments, selects
    it, switches the sound on and opens the settings sheet on it. The Sound
    page's "Import an instrument…" filters on the declared types and on
    folders (a `.dspreset` with its `Samples/` beside it). The sheet gained a
    "Sound" section of its own right after "Canvas", its row naming the
    instrument in force, so the page is one tap from the top.

27. **The play surface shows the instrument's reach, and the CC map reaches
    the bus.** The author, with the Dan Tranh under the pen: "hide the cells
    that are not mapped in the playable instruments — we only need 38
    zones, not sure why you said 48" (48 is the preset's zone count: 16
    recorded notes × 3 velocity layers; 38 is the keys they span, B2 to
    C6), "and I would like to map reverb and delay to the CC controls".
    Two additions, Voxo 0.7.0 (additive). `voxo_covered_notes` returns the
    union of the loaded preset's ATTACK zones' note ranges as 128 bits
    (computed at compile time; release zones do not count; the newest
    instrument the shell handed over, swapped in or not; false with no
    preset — the sine and a raw sample answer every note): the iPad's play
    surface reads it after every load, unload and sound toggle, and a cell
    whose note the instrument cannot sound is neither drawn nor playable
    while the sound is on; with the sound off every cell shows again, since
    an external synth may answer any note. And the shell's one CC map gains
    six targets numbered from 1000 — reverb amount / room / damping, delay
    amount / time / feedback (`VOXO_CTL_*`; `presets/SCHEMA.md` records the
    numbering, a shell without Voxo ignores them) — routed through
    `voxo_map_cc` / `voxo_clear_cc_map` (a double-buffered table of 32
    routes the callback reads at block start) into the loaded instrument's
    live bus, switching the effect on if the preset had none; a route
    removed leaves its last value until the next load. The iPad's CC map
    editor and the desktop's list them beside the core's controls, the
    desktop applying them from `sound_apply`; Android's step follows.

## Step 54 — Android (macOS machine)

28. **The Tab's sound: foreground only under audio focus, the tuner paced by
    the status line, first launch makes a sound.** `Sound.kt` owns the
    settings (on by default with the demo, the volume 0.8, the instrument,
    Local Control — SharedPreferences, beside the session file), and the
    device: it runs only while the activity is resumed, the setting is on
    and audio focus is held — `AudioFocusRequest` (GAIN, usage GAME) taken
    when the sound should run and abandoned when it should not; a loss
    (a call, another player) stops the device and a regained focus starts
    it, as SOUND §4 asks (a foreground-only shell needs no service). The
    activity ticks `Sound` once a second: the status line
    (`nativeVoxoStatus` — rate, burst, buffer, low-latency, voices and
    layers, the render time, AAudio's underruns and Voxo's late callbacks)
    is also what paces the AAudio buffer tuner of #7 in the backend (each
    underrun AAudio counts since the last query buys one more burst up to
    the capacity), and a device that dropped (the spike stopped it, AAudio
    lost it) is restarted on the tick. The JNI's one producer gained the
    `local` flag: the play surface's loopback bytes reach Voxo only under
    Local Control (#12), the AMidi devices' and the session's control CCs
    always; the session's input mode and CC map reach Voxo as on the iPad
    (#25, #27: the bus targets from 1000 routed with `voxo_map_cc`, the
    editor's Dimension cycle running through both namespaces). The demo
    rides in the APK's assets (Gradle copies `voxo/demo` into a generated
    assets folder; a plain path, since the plugin refuses a lazy one) and
    is installed into `filesDir/demo` once per build (Voxo reads files);
    the reach (#27) hides the play surface's cells outside the loaded
    instrument while the sound is on.

29. **Instruments on the Tab: `filesDir/Instruments`, the Storage Access
    Framework, "Open with".** Libraries live in the app's external-files
    Instruments folder, visible to a file manager
    (`Android/data/com.vibetuned.midisink/files/Instruments`); the Sound
    page lists every `.dspreset` and `.dslibrary` there after the demo and
    before "a sine per voice", with a delete per row, the compat report
    and the memory advice beneath (60% of the activity manager's
    `availMem`, refreshed before every load; the lab's `--ei voxoBudgetMb`
    overrides, `--es voxoInstrument <relative | demo | ->` picks). "Import a
    .dslibrary…" opens the document picker (`OpenDocument`, any type — the
    name decides), "Import a preset's folder…" the tree picker
    (`OpenDocumentTree`, copied whole through DocumentsContract queries, no
    library added); and the manifest registers the activity for VIEW
    intents on zip / octet-stream content with a `.dslibrary` path, so a
    library opened from Files, Nearby Share or Mail is imported, selected,
    the sound switched on and the sheet opened — the iPad's #26, the
    Android way. A load runs on a worker with a "Loading…" row.

30. **One native frame for the canvas, the overlay and the strip; a two-way
    picker in the Tab's CC map.** The author, on the Tab: "the sustain panel
    is not multitouch — touching it you can no longer play the cells, and
    playing a cell you cannot use the panel; an older bug", and "the channel
    and dimension only move to the right". The first was Compose's interop:
    the surface, the play overlay and the strip were three `AndroidView`s
    in a `Box`, and Compose hands a whole multi-finger gesture to the
    interop view that took the first finger, so a second finger on another
    view never arrived — since the strip landed over the lattice in Phase
    4. They now sit in ONE `FrameLayout` hosted by a single `AndroidView`
    (the strip placed by layout params under the status bar), and a
    ViewGroup splits pointers between its children by default: a finger on
    the strip and fingers on the cells are separate streams. The second was
    the sheet's `Cycle` control (a tap = next); the Channel and Dimension
    rows are a `Pick` now — the value on the row, a tap opening the list of
    options beneath it (the iPad's picker, in the sheet's own idiom) —
    while the other rows keep cycling. Both verified to build, launch and
    take a tap on the strip and one on a cell from the Mac; the two-finger
    case is the author's thumbs.

31. **The bus routes survive every loader.** The author, on the Tab: "I
    mapped delay amount to CC 75 and reverb amount to CC 74 and put both on
    the panel; they do nothing, pitch and bend work." Three range checks
    written before Voxo's targets existed dropped them: the Tab's JNI
    skipped any target at or past `SUMI_CTL_COUNT` before the branch that
    mapped the bus (the routes never reached `voxo_map_cc`), the desktop's
    INI loader refused the same range on reload, and the iPad's session
    parsing accepted targets below 20 only. Each now lets 1000–1005 through
    beside the core's range (the Tab maps the bus before the core's check;
    the Kotlin session parsing had no clamp), so a route to the reverb or
    the delay applies when set and comes back after a relaunch on all
    three shells.
