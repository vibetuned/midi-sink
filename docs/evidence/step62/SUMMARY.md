# Step 62 — hostmpe: the fingering on the wire, the brass retune, the theremin surface, the small UX items — the Mac's evidence

`DECISIONS_8 #9–#12`. Spec: INSTRUMENT §2, §3, §5; SPEC §8; QOL §6 (the
roadmap's reference; the undone items are QOL §2). Platform-neutral (ctest);
machine: the author's Mac; the desktop `build/` (Debug).

## What changed in the tree

- `hostmpe/include/hostmpe.h`, `hostmpe/src/hostmpe.cpp`: the valve buttons
  (`hostmpe_strip_valve_press / _release / _valves`), the positional slider
  (`hostmpe_strip_slide_set / _value`), the announce of nine, the strip
  reset, the quick-switch subset, the brass retune (`hostmpe_voice_retune`,
  `hostmpe_tick`, `hostmpe_voice_pitch_offset`, `hostmpe_touch_begin_offset`),
  the theremin surface (`hostmpe_theremin_begin / _move`), mirroring
  (`hostmpe_set_mirror / _mirror`), the device profiles
  (`hostmpe_device_profile`); the bipolar Y factored into one helper.
- `core/src/voice_mapper.cpp`: CC 120 (All Sound Off) and CC 123 (All Notes
  Off) end the channel's held voice (the panic's voice flush); never a
  control.
- `desktop/src/settings_ui.cpp`: "Panic (all notes off)" in the MIDI section.
- `tests/hostmpe_tests.cpp` (five tests, the three traces),
  `tests/normalizer_tests.cpp` (`test_all_notes_off`),
  `tests/hostmpe_c_compile.c` (the eighteen new symbols);
  `tools/midi_asserts.py` (an attack within half a semitone of centre).
- `_work/DECISIONS_8.md` #9–#12; `docs/CHANGELOG.md`; `CLAUDE.md`.

## The goldens (`hostmpe_tests.txt`, `normalizer_tests.txt`, `ctest.txt`)

| suite | result |
|---|---|
| `hostmpe_tests` | 3 158 checks passed: the widgets (the valves' change-only CCs, the slide's seven positions and its continuity, the announce of nine on the master, the reset's three then nothing, the limiter classes, the quick-switch), the trumpet phrase, the trombone glissando, the theremin's stream, mirroring, the device profiles |
| `normalizer_tests` | OK, 41 610 checks: All Notes Off / All Sound Off in MPE (the channel's voice, silently), classic (nothing held), wind (the one brush) |
| `hostmpe_c_compile` | OK: the eighteen step-62 symbols from C11 |
| ctest | 10 of 10 |

## The byte traces (`traces/`, the MIDI chart's rows for step 67)

Written by the goldens under `HOSTMPE_EVIDENCE`; columns t,status,d1,d2,src
(1 the finger, 2 the session config, 3 the strip), the merge-point log's.
`tools/midi_asserts.py device <trace>` passes every assert on each
(`traces/midi_asserts.txt`).

| trace | what it holds |
|---|---|
| `trumpet_phrase.csv` (113 messages) | the MCM and the strip's announce of nine; B♭3 struck open on the 4th partial; valve 2 down (CC 111 on the master) and the voice a semitone down over eight 5 ms ticks, monotone, landing exactly at −1 st; the lip bend riding on the fingering; 1+2+3 (CC 110, 112) and the voice to −6; the hand home; the lift; the valves up |
| `trombone_glissando.csv` (294) | the slide at 0.7 st (CC 113 = 15) and the attack on 69 with the +0.3 fraction's bend; a hundred 10 ms steps out to the 7th position, a strip CC and a voice bend each, the pitch following 70 − s within 0.004 st (the 14-bit quantum is 0.0029) and monotone, landing at 64; no ramp ran |
| `theremin_stream.csv` (869) | C4 + 0.2 struck at its fraction; fifty semitones up in 500 steps of 4 ms while pushing away then pulling back — one re-anchor past 47 semitones, no retrigger inside, the pitch within 0.004 st of the hand and monotone; pressure then the swirl; the lift's three messages |

The analyser's centre-bend rule widened for the attack between semitones:
`pen_button_sustain_byte_log.csv` reports what it did before (no touch
strikes in it); the other four fixtures pass as before.

## The gates (`gates/`)

| gate | result |
|---|---|
| the field gate, Metal | GREEN: max 0.0, mean 0.0 — bitwise; the negative control red |
| the composite gate, Metal | GREEN: 0 of 1 048 576 channel samples differ; the negative control red |
| the marble's web gate (`web_gate.txt`) | PASS: max 9.8e-4, mean 3.9e-9 (59c's numbers) |

## The other builds and the devices

| build | result |
|---|---|
| iOS `build-ios`, xcodegen, xcodebuild, devicectl install + launch | builds; installed and launched on the iPad |
| Android `:app:assembleDebug`, adb install, launch | BUILD SUCCESSFUL; "sumi 1.5.0 ready", the session config sent |
| web `cmake --build build-web` | builds; the gate PASS |

The shells call none of the new functions yet (63/64); their re-announce
now carries the valves (CC 110–112 at 0) and truncates the slide's line at
their buffer of 8 — harmless until 63/64 widen it.

## For the author

- **The roadmap's DONE** names the three desktops' CI: the Mac's ctest is
  green here; the Linux and Windows boxes run the same suites on the push.
- **Flags** (`DECISIONS_8 #12`): the analyser's half-semitone attack
  window; the lip bend's scale by ear at 63; the per-device offer's table
  (the input mode only — the repo ships no per-device preset files); the
  shells' announce buffers.
- **Next, step 63 (iOS):** the strip's valves and slide, the theremin
  surface, panic, quick-switch, mirroring, the per-device offer, the
  trumpet arrangement by eye, and Suzu's bowed patch under the valves.
