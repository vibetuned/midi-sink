# Step 52 — Bus reverb & delay, preload gate (macOS) — DONE evidence

Decisions `DECISIONS_6 #19` (the bus), `#20` (the advisory gate), `#21` (the
worker-thread preload on the desktop, the demo instrument's slot).

## What shipped

- **Voxo 0.6.0**: `voxo/src/bus.{h,cpp}` — a Freeverb-class reverb and a
  stereo feedback delay, post-sum, buffers allocated once per rate on the
  shell's thread; the preset's effect parameters, the UI controls' starting
  values and `<cc>` bindings on `FX_REVERB_*` / `FX_DELAY_*`;
  `voxo_set_memory_budget` and the header-based size estimate before decoding
  (WAV, FLAC, AIFF; a zip entry inflated only that far), `VOXO_NOTE_MEMORY`
  first in the report, `memory_estimate` / `memory_budget` in `voxo_report_t`.
- **Desktop**: the preset loads on a worker thread ("Loading …" in the report
  meanwhile); the gate's advice is 60% of the free memory
  (`desktop/src/sys_info.cpp`), shown under the Instrument row; the "Demo
  instrument" button; `--voxo-preset`, `--voxo-budget-mb`.
- **The demo slot**: `voxo/demo/` (`README.md`, the placeholder music box from
  `tools/make_demo_instrument.py`), bundled into the macOS app's
  `Resources/demo` and beside the executable elsewhere.
- **Fixtures**: `bus/bus.dspreset`, `dry.dspreset`, `pad_bus.dspreset`.

## DONE checks

| check | result |
|---|---|
| zero XRuns at 128 frames with both effects on | `storm_bus.log`: `pad_bus.dspreset` under the 20 s storm on the MacBook's output — 7 892 callbacks, **0 XRuns**, render max 0.142 ms, 0 dropped |
| the gate warns on an oversized library and loads it anyway | `gate.log`: the Bösendorfer at a 100 MB budget — "Memory: this library is larger than advised for this device; it is loaded anyway. (about 277 MB against 100 MB advised)", loaded in 1.15 s, exit 0 |
| the reverb rings and the delay echoes | `voxo_preset_tests`: the dry preset silent after a 19 ms note, the reverb at −30 dB, echoes at 0.25 s and 0.5 s above the reverb between them; the bus never wraps |
| the contract with the bus on | fifteen looping, filtered voices through both effects: 0 allocations, 0.062 ms per 128-frame block |
| the gate's arithmetic | `minimal` over a 1 KB budget: the note, the estimate 1 600 bytes (400 frames × 4), and it plays; no budget, no note; AIFF, FLAC and 24-bit WAV headers summed (6 436 bytes with the text file at twice its size); a zip entry's head read |
| the demo slot | the bundled `demo.dspreset` loads (3 zones, 0.3 MB) |
| ctest, the tablets | 9 of 9; Gradle and `build-ios` green |

## Left to the author

- A first launch with the Sound section's "Demo instrument" button, and the
  real recording for the slot when it exists (replace the files, keep the
  names).
- The Basic Piano's reverb knob and the harp's under the ROLI.
