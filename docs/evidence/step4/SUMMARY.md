# Step 4 DONE evidence — MIDI ingest, normalizer, classic mode

Date: 2026-08-31. Same rig. All GUI runs with `METAL_DEVICE_WRAPPER_TYPE=1`;
zero validation output. Real devices present: ROLI Piano (hardware),
GarageBand virtual output; scripted evidence via a virtual CoreMIDI source
(DECISIONS.md #26).

| DONE check | Result | Evidence |
|---|---|---|
| Keyboard painting: drops at pitch-mapped positions, velocity-scaled radii | **PASS** — sparse 8-note run (C/G/D/E pitch classes, octaves 1–7, velocities alternating 30/115): drops land on the circle-of-fifths arc with octave-scaled radii from center; vel-115 drops visibly ~2× the vel-30 drops. The user's physical ROLI Piano also painted drops live. | `notes_pitch_positions.png`, `run_notes_only.log` |
| `sumi_dropped_midi_count` = 0 during a dense 30 s performance | **PASS** — 10 142 messages in 30 s (200 Hz bend + CC1 streams + walking notes) through hotplug-opened virtual source: `dropped MIDI messages: 0`, 100.0 fps | `run_midi_30s.log`, `midi_performance.png` |
| Normalizer tests pass headlessly (no GPU) | **PASS** — 66 checks: SPSC overflow drop-oldest + counter, note on/off/vel-0, 14-bit bend assembly, RPN 0 bend range, NRPN isolation, running-status tolerance, SysEx/system ignore, §2.5 mode detection (classic/MPE/wind/MCM/override), both pitch layouts + aspect correction, classic lowering (sqrt radius, bend-delta shear, CC1 coalescing, CC64 rising edge) | `ctest` output; tests/normalizer_tests.cpp |

Also working live: CC64 → paper dip (UV reset — the canvas visibly wipes; one
run captured after the dip shows blank paper), MIDI hotplug (virtual source
opened ~1 s after appearing, pruned on disappearance), and the full-density
performance painting (`midi_performance.png` — drops swirled by mod-wheel
vortex and bend shear).

**Found & fixed on the way** (details in DECISIONS.md #25):
- libremidi filters **virtual** MIDI endpoints out by default (`track_virtual`)
  — that's why a test synth was invisible while the hardware ROLI worked.
- libremidi's CoreMIDI hotplug callbacks never fire in this app (verified
  against a raw-CoreMIDI control that does receive notifications); the
  harness now rescans the port list once per second instead.

Regressions: abi_c_compile + normalizer_tests green; `leaks --atExit`
unchanged OS baseline (287/18 720 B, no core frames); ~100 fps sustained.
