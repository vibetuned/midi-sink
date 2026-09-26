# Step 51 — Layers, round robins, release samples, loops, filter, smoothing & bindings (macOS) — DONE evidence

Decisions `DECISIONS_6 #16` (the compiled instrument and the layer stack),
`#17` (the filter, the MPE sources, the bindings), `#18` (the contract under
fifteen stacked voices).

## What shipped

- **Voxo 0.5.0**: `voxo/src/instrument.{h,cpp}` compiles the parsed model
  into what the callback plays (zones, groups, the per-group runtime block,
  the bindings as 33-point curves); `voxo.cpp`'s voice stacks up to eight
  layers — velocity layers with equal-power crossfades over overlaps, round
  robins and random sequences, release samples on note-off — each with its
  ADSR, its loop with an equal-power crossfade, a 2-pole state-variable
  low-pass, the voice's pressure / timbre (CC 74) / swirl smoothed by the
  preset's rising/falling times and mapped through the preset's `<mpePressure>`
  / `<mpeTimbre>` bindings or the defaults; `<cc>` and `<velocity>` bindings,
  the UI controls' starting values, `TAG_VOLUME`. `active_layers` in the
  stats and the settings window.
- **Fixtures** (`tests/fixtures/dspresets/levels`, `loop`, `filter`, made by
  `make_fixture_waves.py`): layers, roundrobin, release, stack, loop / noloop,
  filter_default, filter_bound.

## DONE checks

| check | result |
|---|---|
| the fixture library plays with audible layer crossfades and round robins | `voxo_preset_tests`: velocity 40 → the 0.5 layer alone, 120 → the 0.25 layer, 70 → both at equal power; six strikes cycle three round-robin positions at 0.5 / 0.25 / 0.125 — `voxo_preset_tests.log`; the author's ear on the Bösendorfer (velocity layers) and the Mandolin (random round robins) |
| a pad holds thirty seconds | `loop.dspreset` (a 3 s pad, loop 8000–23000 with a 2000-frame crossfade): the level at 30 s within 20% of the first second and never under it in between; `noloop` ends at 3 s |
| the Osmose's slide moves the cutoff | CC 74 on `filter_default`: brightness open > centre > closed by more than 2 : 1 each; `filter_bound`'s table darkens three-fold — the author's hands on the Osmose |
| the callback contract still holds under fifteen voices of stacked samples | 15 voices × 6 layers = 90 layers, 2 s of bends / pressure / CC 74 storm: **0 allocations**, **0.225 ms** per 128-frame block (debug build; the period is 2.667 ms) |
| release samples | the release zone fires on note-off at 0.25 while the held 0.5 fades |
| the preset's bindings override the defaults | pressure 0 → 127 doubles the level through `AMP_VOLUME` 0.5..1 |
| ctest | 9 of 9; the desktop storm `--voxo-storm 10` glitch-free — `storm.log` |
| the real libraries still load through the compiler | `real_libraries.txt` |
| the tablets still build | Gradle and `build-ios` green |

## Left to the author

- The Bösendorfer under the ROLI (its eighteen velocity layers per note),
  the Mandolin's round robins, a Spellsinger drone held for a minute, the
  Osmose's slide on the harp's timbre table.
