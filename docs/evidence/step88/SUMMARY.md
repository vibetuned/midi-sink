# Step 88 — The chorus and the phaser, and the CC map's chorus targets

2026-10-25 · Voxo 0.28.0 (additive: `VOXO_CTL_CHORUS_MIX` / `_RATE` / `_DEPTH`) · `DECISIONS_14 #2` · `TO_PROJECT_SPEC` §12.8's chorus and phaser as shipped · the core frozen.

## What shipped

* **The format, read first**: the chorus takes `mix`, `modDepth`, `modRate` and nothing else; the phaser those plus `centerFrequency` and `feedback`, no `stages`; group-level effects are one instance a note in the format and the phaser is bus-only; `enabled` on any effect; the generic bindings `FX_MIX` / `FX_MOD_DEPTH` / `FX_MOD_RATE` / `FX_CENTER_FREQUENCY` / `FX_FEEDBACK` resolve to the effect at the binding's position.
* **`voxo_bus::Chorus`**: a stereo modulated delay (7 ms ± 4 ms × depth), the LFO a magic-circle cell in quadrature, Hermite reads, fixed buffers — an instance in every voice and one on the bus, nothing allocated; a chorused voice's layers summed in a scratch block before its chorus.
* **`voxo_bus::Phaser`**: six first-order all-passes a channel swept ±2 octaves about the centre, the coefficient refreshed every sixteen samples, feedback within ±0.95; a true notch at the centre at mix ½.
* **Bit-identical across the native builds**: both compiled with fused multiply-add contraction off; the sweep's output hash is `66a88a01` in the Debug run and `66a88a01` in the Release run.
* The CC map's 1006–1008 on the desktop, the iPad and the Tab; the report's chorus sentence retired.

## The gates

| gate | result |
|---|---|
| the preset suite, blocks 21–25 (Release) | `suite_preset_release.txt`, 114 ok: mix 0 the dry bit for bit; depth 0 at mix 1 the dry delayed by exactly 336 samples, bit for bit; full depth at 2 Hz 209.2–231.0 Hz (21.8 Hz peak to peak, the formula's 22.1), the right channel −0.8 Hz where the left peaks at +11.0; the group chorus on one note = the bus chorus bit for bit, on two notes within −143.6 dB (the dry path's fused accumulation); the phaser's 440 Hz notch −132.2 dB with 220 Hz at −0.58 dB, mix 1 the chain alone at the dry's level; the sweep bounded (0.368 against 0.258), two fresh instances identical, 20.4 dB of movement over 50 ms windows; zero allocation |
| the sampler suite (Release) | `suite_voxo_release.txt`, 50 ok |
| the synth suite (Release) | `suite_suzu_release.txt`, 256 ok |
| the Debug ctest | `ctest_debug.txt`: 14 / 14 in 414 s |
| the fuzz seeded with the effects | `fuzz_60s_debug.txt`: 256 925 loads in 60 s, 100 383 loaded, no crash |
| the storm, a chorus on every voice and a phaser on the bus (the Dan Tranh library) | `storm.txt`: 88 009 messages, 7895 callbacks, 0 XRuns, render max 0.204 ms (0.198 without the effects at step 87), 12 voices at the end, 96.7 fps |
| the web and lab gates | `web_and_lab_gates.txt`: the web gate GREEN (every kind as at step 87), the lab gate GREEN, six pages |
| the tablets' compiles | `tablets.txt`: the iOS libraries and the Android debug APK build |

## Found on the way

* The Debug build fuses multiply-adds: clang's fmuladd intrinsic lowers to fmadd on arm64 at -O0, so the sampler's layer accumulation rounds once per layer and the mix of two voices is FMA-dependent. The two-note chorus gate is a −120 dB tolerance for that reason; the one-note case is bit for bit.
* A quarter-second window averaged the phaser's sweep away (0.4 dB of variation); 50 ms windows follow the notch's dwell (20.4 dB).

## Files

`voxo/src/bus.h/.cpp` (`Params`, `Chorus`, `Phaser`, the contraction pragma), `voxo/src/ds_preset.h/.cpp` (`EffectKind::Phaser`, the attributes, `enabled`, the chorus note retired), `voxo/src/instrument.h/.cpp` (`Target::Chorus*` / `Phaser*`, `BUS_GROUP`, `target_of(parameter, kind)`, `binding_kind`, the group chorus, the bus effects), `voxo/src/voxo.cpp` (`Voice::chorus`, `voice_scratch`, the chorused voice path, the bus order, the bindings, the routes; 0.28.0), `voxo/include/voxo.h` (the CTL targets, the history), `voxo/COMPAT_REPORT.md`, `tests/voxo_preset_tests.cpp` (2, the copy test, 21–25), `tests/voxo_fuzz.cpp`, `tests/voxo_c_compile.c`, `tests/fixtures/dspresets/fx/` (+ `make_fixture_waves.py`, `features.dspreset`), `desktop/src/app_settings.cpp`, `ios/Sources/Session.swift`, `android/app/src/main/java/com/vibetuned/midisink/MainActivity.kt`, `presets/SCHEMA.md`, `site/src/content/docs/guide/sound.md`, `site/src/content/docs/reference/settings.md`, `_work/DECISIONS_14.md`, `_work/TO_PROJECT_SPEC.md` §12.8, `docs/CHANGELOG.md`, `CLAUDE.md`.
