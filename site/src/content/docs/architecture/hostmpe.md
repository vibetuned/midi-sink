---
title: hostmpe — the shared MPE library
description: Everything the tablets generate lives in one pure-C library consumed by Swift and Kotlin — allocator, joystick, stylus legato, rate limiters, strip engine, echo guard — so the two shells cannot drift.
---

When the tablets became instruments, the obvious design was two
implementations — one in Swift, one in Kotlin — of the same MPE generation.
The obvious design is a drift bug waiting to happen. Instead everything the
Play surface generates lives in **`hostmpe/`**, a C++20 library behind a
pure-C header with the same discipline as the core's, consumed by Swift
through a module map and by Kotlin through JNI. One implementation, ~1,600
headless checks, the same bytes on both platforms.

## What it owns

* **Voice allocator** — least-recently-released round-robin over the 15 member
  channels; external-occupancy masking (channels holding a hardware
  instrument's note are unavailable; cleared on its Note Off, on device
  disconnect, or by a 30 s stuck-note timeout refreshed by activity);
  saturation is a silent drop, never a steal.
* **Joystick engine** — the soft knee (a deadband, not a travel limit), the
  absolute floor, the semitone-scaled 14-bit bend along the cell's local pitch
  axis, the bipolar Y (0xD0 up, 0xA0 down), the emit order (centre bend before
  Note On, pressure 0 before Note Off).
* **Stylus engine** — per-cell legato retriggers with ±0.65 st hysteresis, the
  in-cell bend with the booster scale applied only to the emitted value, CC 74
  from Y, pressure from force.
* **Session configuration** — the MCM and RPN 0 = 48 on every member, in wire
  order; panic and zone silence.
* **Rate limiters** — a *rate class* (per-voice, per-dimension latest-wins to
  ≤ 100 Hz) for USB, virtual, network and MidiDeviceService sinks; a *budget
  class* (~300 msg/s, latest-wins with round-robin fairness) for BLE; a
  never-dropped class for Note On/Off, the centre bend,
  pressure-0-before-Note-Off, CC 64 and button CCs — and any batch containing a
  Note On ships whole, so a legato crossing arrives intact.
* **Control-strip engine** — spring wheel with the exact-centre final message,
  latch wheels with relative deltas, sustain momentary/toggle, assignment
  validation (protocol CCs refused), re-announce after a re-sync.
* **Echo guard** — every byte that actually leaves the device is recorded at
  the single transport emit point; device input matching a record inside
  300 ms is consumed before the occupancy mask, the log and the loopback.
  Measured on an iPad, 99.5 % of "external" input was our own output mirrored
  back by a transport; without the guard it marked our channels as externally
  held and painted every note twice.

## The shells' part

The shells own what is genuinely platform-bound: touch and stylus event
delivery, the overlay drawing (a probe sweep, two-tone so cells stay legible
over ink), the transports (CoreMIDI virtual source + IDAM + network session +
BLE on iOS; the USB-MIDI gadget, a `MidiDeviceService` and a hand-rolled BLE-MIDI
GATT peripheral on Android — Android's `MidiManager` is central-only), the
settings sheet, and the merge point through which every byte passes.

## Verified from the bytes

The byte log at each shell's merge point tags every message by source —
device, finger, session config, strip, stylus — so one analyser set serves both
platforms: handshake order, centre-bend-before-strike,
pressure-0-before-Note-Off, no finger CC 74, no allocation on externally held
channels, strip on the master only, sustain never stuck, the rate policies;
and per-stroke legato reconstruction for the pen. Every assert has a negative
control, because two false greens were caught during review. The same logs
verify the [MIDI implementation chart](../../reference/midi-chart/).
