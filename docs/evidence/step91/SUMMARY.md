# Step 91 — The instrument slot, the offline bounce and the output device

2026-10-26 · Voxo 0.31.0 (additive: `voxo_set_device` / `voxo_get_device` / `voxo_device_count` / `voxo_device_name`, `voxo_preset_hash` / `voxo_preset_name` / `voxo_preset_file_hash`) · the preset schema 3 · `DECISIONS_14 #5` · `TO_PROJECT_SPEC` §12.12 as shipped · the core frozen · Phase 15's last step.

## What shipped

* **The instrument slot**: a session preset (and a recording's header) carries `library` — the library the sound played by name and by the FNV-1a 64 of its preset XML, with the path as a hint, never embedded (`presets/SCHEMA.md`, `sumi_preset_t::library_*`). The identity is kept on the Voxo instance at load (`voxo_preset_hash` / `voxo_preset_name`) and read off a file without a sample (`voxo_preset_file_hash`). Each shell resolves a loaded preset's slot against its own libraries by name, then by hash — the desktop over the hint's folder, the current instrument's folder and `~/Music/midi-sink`; the tablets over their Instruments folder — and shows the sentence when none is there: "Library '…' is not on this computer: the preset plays without it."
* **The offline bounce**: `sumi_replay_bounce` in the replay library walks a recording's frames — each frame's bytes pushed, its samples rendered in 128-frame calls, a 32-bit float stereo WAV at 48 kHz — with no engine dependency; every shell binds it to a deviceless Voxo configured as its live one (the desktop's `app_sound_configure`, the one body `sound_apply` takes too; the iPad's controller; the Tab's JNI record). *Bounce to WAV* on the desktop, the waveform button on the iPad, *bounce* on the Tab; the bench's `--replay-wav` shares the walker.
* **The output device**: miniaudio's enumeration, the desktop's *Output device* row ("System default" first) and `sound_device` in the INI; a running instance moves to the chosen device without a stop; a name no longer present falls back to the default.
* **The gate as read**: the live device's output is not byte-alignable to the frame clock, so the bounce IS the deterministic rendering — the same recording and session give the same bytes, measured twice through the host's own path.

## The gates

| gate | result |
|---|---|
| the bounce gate (`--bounce-test`, the bench's demo recording through the Dan Tranh library) | `bounce_test.txt`: two bounces through the host path — 192 000 frames (4.00 s, 597 events, peak 0.036), 1 536 044 bytes identical, hash `164241f1`; the WAV a 32-bit float stereo RIFF of exactly 44 + 8 × frames bytes |
| the device gate (`--device-test`) | `device_test.txt`: four playback devices enumerated; started on the speakers, moved to "LG ULTRAGEAR+" while running (running, the stats naming it, 188 callbacks in the half second after), a name that does not exist falling back to the speakers, back to the default — 188 callbacks each |
| the preset suite (Release) | `suite_voxo_preset_tests_release.txt`: all ok — block 42: the identity off a `.dspreset` and a `.dslibrary` without loading equals the loaded instrument's, a missing file false |
| the sampler suite (Release) | `suite_voxo_tests_release.txt`: all ok — the device list, the name kept for the next start, no preset = hash 0 |
| the synth suite (Release) | `suite_voxo_suzu_tests_release.txt`: all gates green |
| the replay suite (Release) | `suite_replay_tests_release.txt`: 0 failures — the bounce over a synthetic engine: 1600 frames for four frames of 1/120 s in sixteen render calls, the WAV 44 + 8 × 1600 bytes, two bounces byte for byte, the header a float RIFF |
| the preset module's suite (Release) | `suite_preset_tests_release.txt`: 43 / 43 — the slot's round trip, a bad hash reading as 0, the schema 3 |
| the Debug ctest | `ctest_debug.txt`: 14 / 14 in 429 s |
| the fuzz | `fuzz_60s_debug.txt`: 233 995 loads in 60 s, 91 437 loaded, no crash |
| the storm on the Dan Tranh library (step 90's preset variant) | `storm.txt`: 109 303 messages, 7892 callbacks, 0 XRuns, render max 0.163 ms, 13 voices at the end, 119.8 fps |
| the web and lab gates | `web_and_lab_gates.txt`: the web gate GREEN, the lab gate GREEN, six pages (the wasm's `none` backend enumerates no device) |
| both tablets under the spike | `tablets.txt`, `tablets/`: the iPad clean on 0.31.0 (the sampler 0.049 ms, Suzu patch 0 0.206 ms, 0 XRuns); the Tab's sampler 0 XRuns at 0.90 ms of 4 ms, Suzu patch 0 the carried placement item (11 proxy XRuns) |

## Found on the way

* `voxo_preset_hash` first read the current instrument's model and answered for the previous one until the callback's swap (the preset suite's block 42: an empty name, the old hash). The identity is kept on the instance at load, on the shell thread.
* A bench gate that renders no frame hangs the lab at exit: `sg_shutdown` waits on Metal's frame semaphore that no frame ever signalled. The first `--bounce-test` sat twelve minutes at 0 % CPU; `sample` named the wait. The two new gates render one frame first.
* The Tab's JNI helper `jstr` is defined below the Voxo natives: a forward declaration.

## Files

`voxo/src/ds_preset.h/.cpp` (`xml_hash`, `read_preset_xml`, `hash_file`, `fnv1a64`), `voxo/src/voxo_internal.h`, `voxo/src/backend_miniaudio.cpp` (`voxo_backend_enumerate`, the named device), `voxo/src/backend_none.cpp`, `voxo/src/voxo.cpp` (`device_want`, `preset_hash` / `preset_name`, the eight entry points; 0.31.0), `voxo/include/voxo.h`, `replay/include/sumi_replay.h` + `replay/src/sumi_replay_bounce.c` + `replay/CMakeLists.txt`, `presets/include/sumi_preset.h` + `presets/src/sumi_preset.c` (the `library` block, schema 3), `presets/SCHEMA.md`, `desktop/src/app_settings.h/.cpp` (`sound_device`, `sound_library_note`, `app_settings_suzu_params`, `app_sound_configure`, `app_sound_library_set/get`, `app_resolve_library`), `desktop/src/main.cpp`, `desktop/src/settings_ui.cpp`, `desktop/src/replay_host.h/.cpp` (`bounce`), `desktop/src/dev_tools.h/.cpp` (`--replay-wav` through the walker, `--bounce-test`, `--device-test`), `ios/Sources/SoundController.swift`, `ios/Sources/Session.swift`, `ios/Sources/Replay.swift`, `ios/Sources/SumiCanvas.swift`, `android/cpp/sumi_jni.cpp`, `android/app/src/main/java/com/vibetuned/midisink/{NativeBridge,Sound,SettingsSheet,MainActivity}.kt`, `tests/replay_tests.c`, `tests/preset_tests.c`, `tests/voxo_tests.cpp`, `tests/voxo_preset_tests.cpp` (42), `tests/voxo_c_compile.c`, `site/src/content/docs/guide/sound.md`, `site/src/content/docs/reference/settings.md`, `_work/DECISIONS_14.md`, `_work/TO_PROJECT_SPEC.md` §12.12, `docs/CHANGELOG.md`, `CLAUDE.md`.
