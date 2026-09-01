# midi-sink

A suminagashi (Japanese ink-marbling) visualizer driven by expressive MIDI.
The core engine (`libsumi`, C-ABI, sokol_gfx) is platform-portable by design;
this repo builds the macOS desktop harness. Full specification:
[PROJECT_SPEC.md](PROJECT_SPEC.md); implementation decisions:
[DECISIONS.md](DECISIONS.md).

## Build & run

```sh
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
./build/desktop/midi-sink
```

All connected MIDI inputs (hardware and virtual, hotplugged) are opened
automatically. Mouse: left click = ink drop, left drag = tine, right drag =
vortex.

## Input modes (auto-detected, override via `sumi_set_input_mode`)

- **MPE** (ROLI Seaboard/Piano, Expressive E Osmose): one voice per member
  channel — strike paints a drop, press grows it continuously, glide drags it
  along the pitch axis, slide (CC74) modulates its ink selector, lift leaves a
  faint clear ring.
- **Wind** (Aerophone, Travel Sax): a single wandering ink brush — breath
  (CC2 / CC11 / channel pressure) feeds the line's thickness, legato note
  changes migrate the brush with a wake.
- **Classic** (any keyboard): notes are drops on the circle of fifths
  (velocity → size), pitch bend shears the bath, sustain pedal dips the paper.

Layouts (key `L` cycles live): circle of fifths, chromatic grid (C1–B7),
Jankó (each note stamps all three rows of its parity), and two BPM-driven
piano rolls (horizontal / vertical) whose field scrolls at
`(bpm/60) × roll_speed` canvas-lengths per second (default roll_speed 0.0625:
16 beats — 4 bars of 4/4 — of history span the canvas) — key `B`/`Shift-B`
nudges BPM ±5 for syncing against a metronome.

## CC routing (global field controls)

Any CC can drive any global dimension at runtime via the C ABI:
`sumi_map_cc(inst, channel /*0xFF = any*/, cc, target)`;
`sumi_clear_cc_map(inst)` removes all routes (including these defaults).
A channel-specific route overrides an any-channel route. CC64 (paper dip) and
CC74 on MPE member channels (slide) are reserved and not routable.

Default bindings (Airwave dimensions are user-assigned on the device side —
match them to this table or remap):

| CC | Target (`sumi_ctl_t`) | Intended source |
|----|------------------------|-----------------|
| 1  | `SUMI_CTL_VORTEX_STRENGTH` | mod wheel |
| 2  | `SUMI_CTL_INK_FLOW` (breath) | wind instruments |
| 7  | `SUMI_CTL_INK_FLOW` (breath alias) | wind instruments (volume) |
| 11 | `SUMI_CTL_INK_FLOW` (breath alias) | wind instruments (expression) |
| 20 | `SUMI_CTL_VORTEX_STRENGTH` | Airwave left-hand Raise ("wind over the water") |
| 21 | `SUMI_CTL_VORTEX_X` | Airwave Glide (vortex center drift) |
| 22 | `SUMI_CTL_VORTEX_Y` | Airwave Glide (vertical) |
| 23 | `SUMI_CTL_VISCOSITY` | Airwave right-hand Tilt (damping) |
| 24 | `SUMI_CTL_PAPER_ROUGHNESS` | Airwave Flex |
| 25 | `SUMI_CTL_PALETTE_MORPH` | Airwave Flex (alternate) |

Vortex strength/center and viscosity act immediately; paper roughness and
palette morph are tracked and smoothed but only take visible effect once the
washi/palette composite lands (see DECISIONS.md).

Harness test flag: `--map-cc <cc>:<target>` applies one any-channel route at
startup (target = numeric `sumi_ctl_t`, e.g. `--map-cc 30:0` routes CC30 to
vortex strength).
