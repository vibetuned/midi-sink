# Step 89 — The filter family, the filter envelope, the bus EQ, the gain and the wave shaper

2026-10-25 · Voxo 0.29.0 (no new entry point) · `DECISIONS_14 #3` · `TO_PROJECT_SPEC` §12.9 as shipped (and the tail of §12.8) · the core frozen.

## What shipped

* **The format, read first**: the voice filters are `lowpass` (`lowpass_4pl`), `lowpass_1pl`, `highpass`, `bandpass` with `frequency` and `resonance` — **the resonance IS the Q** (step 51 read it as an amount above 0.7: every lowpass preset sits 3 dB brighter at its cutoff now); `notch` with `Q`; there is no `<effect type="eq">` — the EQ is `peak`, `low_shelf`, `high_shelf` with `gain` / `gainUnit`; `gain` takes `level` (dB, `"-6dB"`, or `levelUnit="linear"`); `wave_shaper` takes `drive`, `driveBoost`, `outputLevel`; `<envelope>` takes the ADSR, `modAmount`, `modBehavior`.
* **The family on the one SVF** (`Group::filter_mode`): the Zavalishin loop's low, high, band (unity) and notch outputs by mode, the one-pole on the same state; the group's first enabled voice filter, else the instrument's; a lowpass at the top with the default Q is the dry, bit for bit.
* **The filter envelope** (`Layer::fenv`, `fenv_advance`, `fenv_apply`): the first `<envelope>` bound to the voice filter's frequency or Q, per voice whatever its scope, stepped once a block into the cutoff ramp; set | add | multiply.
* **The bus EQ** (`voxo_bus::Biquad`, `EqChain`): RBJ biquads for every enabled peak, shelf and instrument-level notch in listed order, eight at most, set every block at the device's rate; `FX_FILTER_*` bindings resolve to the stage they point at (`Target::Eq*`, `eq_stage_of`).
* **The gain** folded (the bus's into `Params::gain_lin`, a group's into its gain); **the wave shaper** (`shaper_tanh` — Eigen's (13,6) rational, within 3e-7 of libm; `shape` at the format's full scale) on the bus and per voice through the scratch path; `FX_DRIVE` / `FX_DRIVE_BOOST` / `FX_OUTPUT_LEVEL`. The bus order: shaper, EQ, gain, chorus, phaser, reverb, delay.
* **The report**: the "EQ and other filters" sentence retired (its bit silent, nine sentences); the modulators sentence names LFOs and sequences.

## The gates

| gate | result |
|---|---|
| the preset suite, blocks 26–32 (Release) | `suite_preset_release.txt`, 211 ok: every filter mode at 1 kHz (Q 0.7 and 2) and the one-pole within 0.001 dB of its analytic curve over thirty harmonics; `lowpass_4pl` at the instrument level = the group's lowpass bit for bit; a disabled filter the dry bit for bit; the lowpass at Q 0.7 −3.01 dB on 990 Hz; the loop point within 2.7e-5 (1.6e-2 before the fix); the peak, the shelves and the notch within 0.002 dB of RBJ's response, the four-effect chain within 0.000 dB of its three enabled stages' product, `FX_FILTER_GAIN` lifting 6.02 dB (RBJ 6.02); the gain × 0.5011872 within 2e-8 sample for sample (five spellings), linear 0.5 exactly; the bus shaper within 7e-8 of 0.16·tanh(50x), the group shaper bit for bit with it, the drive on CC 20 squaring the saw (−6.1 → −46.0 dB second harmonic), the level on CC 21 by 0.5039; the envelopes' brightness −33.8 / −34.5 / −37.5 dB after the attack, 10–12 dB lower in the sustain, 24–35 dB lower in the release; the Q envelope −8.8 → −22.1 dB; an envelope raises no note; zero allocation |
| the sampler suite (Release) | `suite_voxo_release.txt`, 50 ok |
| the synth suite (Release) | `suite_suzu_release.txt`, 256 ok |
| the Debug ctest | `ctest_debug.txt`: 14 / 14 in 427 s |
| the fuzz seeded with the new fixtures | `fuzz_60s_debug.txt`: 252 361 loads in 60 s, 98 703 loaded, no crash |
| the storm on the Dan Tranh library — a lowpass at Q 1.2, a shaper and a chorus on every voice, a filter envelope on every group, a shaper, three EQ stages, a gain, the phaser, the chorus and the reverb on the bus (`storm_preset.dspreset`) | `storm.txt`: 89 330 messages, 7881 callbacks, 0 XRuns, render max 0.196 ms (0.204 at step 88), 12 voices at the end, 97.9 fps |
| the web and lab gates | `web_and_lab_gates.txt`: the web gate GREEN (every kind as at step 88), the lab gate GREEN, six pages |
| both tablets under the spike | `tablets.txt`, `tablets/`: the iPad clean (the sampler 0.049 ms of 2.67, Suzu patch 0 0.199 ms, 0 XRuns); the Tab's sampler 0 XRuns at 1.37 / 1.22 ms of 4 ms over two runs, Suzu patch 0 the carried placement item (8 / 12 proxy XRuns) |

## Found on the way

* **A loop without a crossfade clicked once a pass since step 51**: the 4-point read's outer taps at the loop's edges read the sample around the loop — the padding's zeros when the loop spans the whole sample — eighteen output samples at a sixfold upsampling. Caught by the notch gate (its second half-second 1.2 dB off). `read_frame_wrapped` and `Layer::looped` wrap the taps into the loop; gate 27 pins it. The first version indexed `i0 − 1` unsigned and crashed on the loop's first frame — the Release suite's run showed it (a truncated output file, no summary line), lldb named it; the index is signed.
* The (7,6) Padé tanh first used for the shaper was 1e-4 off near 4 (the gate caught it at 1.5e-5 on the output); Eigen's (13,6) rational is within 3e-7 over the range.
* The step-51 CC 74 gate's open/centre margin fell from above 2 to 1.49 with the Q correction (the centre brightened); the bar is 1.3, the ordering intact.
* The Tab's sampler spike reads 1.2–1.4 ms where step 87 read 0.80 ms on the same demo and storm; nothing on that path changed in cost (no filter engaged, no envelope, no bus stage) — recorded with the carried thread-placement item for the author.

## Files

`voxo/src/bus.h/.cpp` (`Params::eq[8]` / `gain_lin` / `shaper_*`, `Biquad`, `EqChain`, `shaper_tanh`, `shape`), `voxo/src/ds_preset.h/.cpp` (`EffectKind::LowShelf` / `HighShelf` / `WaveShaper`, the attributes, the envelope's fields, the notes), `voxo/src/instrument.h/.cpp` (`filter_mode`, the Q, the shaper, the filter envelope, `Target::Shaper*` / `Eq*`, `is_eq_kind`, `eq_stage_of`, `target_of_binding`, the bus stages), `voxo/src/voxo.cpp` (`Layer::fenv` / `looped`, `fenv_advance`, `fenv_apply`, `read_frame_wrapped`, the family in `render_layer`, the scratch path's shaper, `voxo_t::eq`, the bus order, the bindings; 0.29.0), `voxo/include/voxo.h` (the history, the note comments), `voxo/COMPAT_REPORT.md`, `tests/voxo_preset_tests.cpp` (2, 6, 11, 26–32), `tests/voxo_fuzz.cpp`, `tests/voxo_c_compile.c`, `tests/fixtures/dspresets/fx/` (twenty-seven presets), `site/src/content/docs/guide/sound.md`, `_work/DECISIONS_14.md`, `_work/TO_PROJECT_SPEC.md` §12.8–§12.9, `docs/CHANGELOG.md`, `CLAUDE.md`.
