# Step 50 — Decent Sampler subset & the compat report (macOS, headless) — DONE evidence

Decisions `DECISIONS_6 #13` (the front end and its third parties), `#14` (the
compat report, its copy, AIFF), `#15` (the middle-C bridge, the desktop row).

## What shipped

- **Voxo 0.4.0**: `voxo_load_preset` / `voxo_unload_preset` / `voxo_note_copy`,
  `voxo_report_t`, the ten `VOXO_NOTE_*` bits; `voxo/src/ds_preset.{h,cpp}`
  (pugixml over the XML, the model, the folder and zip readers, the report),
  `voxo/src/decoders.cpp` (dr_wav, dr_flac, an AIFF/AIFF-C reader);
  pugixml v1.16, miniz 3.1.2 and dr_libs pinned by FetchContent and compiled
  into the archive; `voxo/COMPAT_REPORT.md` — the copy.
- **Fixtures** `tests/fixtures/dspresets/`: `minimal`, `features` (every
  feature of the subset and every note), `formats` (AIFF stereo, FLAC, 24-bit
  WAV, a missing file, a text file), `library.dslibrary`, `malformed/`
  (truncated, wrong root, empty, random bytes, bad numbers).
- **Tests**: `voxo_preset_tests` (33 checks through the C ABI, including the
  docs-copy assertion) and `voxo_fuzz` (5 s under ctest; the hour below).
- **Desktop**: the "Instrument (.dspreset / .dslibrary)" row and the report
  in the Sound section (`sound_preset` in the INI), `--dev --voxo-load`.

## DONE checks

| check | result |
|---|---|
| every fixture loads with the expected report | `voxo_preset_tests`: all ok — `voxo_preset_tests.log` |
| an hour of fuzzing the parser never crashes | `fuzz_hour.log`: **24 960 464 mutated loads in 3600 s** (9 298 957 loaded with notes, 15 661 507 refused with a reason; 7 486 555 over mutated sample files, 2 494 426 over mutated zips), exit 0, no crash |
| the report's copy is the docs' copy | asserted by test: every `voxo_note_copy` sentence appears verbatim in `voxo/COMPAT_REPORT.md` |
| ctest | 9 of 9 |
| the tablets still build | Gradle `assembleDebug` green; `build-ios` green |

## The real libraries on this Mac (`real_libraries.txt`)

| library | zones / groups / samples | memory | load | notes |
|---|---|---|---|---|
| Basic Piano (Decent Sampler's stock, AIFF) | 9 / 1 / 9 | 41.9 MB | 0.24 s | UI |
| Mandolin – Main | 60 / 1 / 60 | 78.2 MB | 0.35 s | other filters, note sequences (its `note_sequence` bindings counted there), UI |
| Solar Choir | 3 / 3 / 3 | 12.5 MB | 0.06 s | UI (its CC-to-knob `labeled_knob` bindings are read as such) |
| The Spellsinger | 46 / 6 / 46 | 927.8 MB | 4.26 s | UI |
| Bösendorfer 280VC | 1 580 / 1 / 158 | 276.8 MB | 0.86 s | UI |
| Arpa Chiquitana MPE (folder and `.dslibrary`) | 36 / 1 / 36 | 35.9 MB | 0.12 / 0.27 s | other filters (the body EQ), UI; the `<mpePressure>` / `<mpeTimbre>` bindings parsed as supported |

The Spellsinger's 928 MB is the cost of decoding 46 long stereo samples to
float: the advisory memory gate is step 52's, and nothing here is bundled.

## Left to the author

- Load a library from the Sound section and play its middle register under
  the ROLI (the bridge zone); the full dispatch is step 51.
