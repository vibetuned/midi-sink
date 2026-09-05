---
title: The control strip
description: The floating palette in Play mode — Pitch spring, Mod latch, two assignable wheels and Sustain, all on the MPE master channel, mirrored by the pen's pedal.
---

A compact palette floats at the **top left** of the lattice in Play mode
(hidden in Marble mode). It floats rather than docks on purpose: a docked band
displaced every drop from its touched cell by the strip's height, which kills
the instrument feel. Every widget is built from the same joystick primitive as
the notes — a touch anchors its origin and the soft knee shapes the travel —
and every message it sends goes out on the **master channel**. Member channels
are never touched: global controls and per-note voices stay disjoint, as MPE
intends.

## The widgets

| Widget | Message | Behaviour |
|---|---|---|
| **Pitch** — spring wheel | pitch bend, master, ±2 st | Deflection maps to value while held; on release a ~50 ms ramp back to centre ending in a guaranteed exact-centre message (a snap is a zipper). Unaffected by the note-bend routing. The strip never re-declares the master range: ±2 is the MPE default. |
| **Mod** — latch wheel | CC 1 | Relative deltas accumulate; there is no absolute entry point, so a regrasp cannot jump the value. The loopback routes CC 1 to the vortex: the mod wheel stirs the water while it modulates your synth. |
| **A**, **B** — assignable latch wheels | CC 23, CC 24 by default | Same latch behaviour. Long-press to reassign — natural homes for the ripple's CC 102 (amount) and CC 103 (wavelength). Protocol CCs (1, 6, 38, 64, 98–101, 120–127) are refused with the reason shown: a strip-assigned CC 6 on the master would corrupt the DAW's RPN state. On the loopback the defaults drive viscosity and paper roughness. |
| **Sus** — button | CC 64 | **Momentary by default** (press-and-hold pedal feel), toggle behind Settings → Control strip. A mode switch while the pedal is down emits the release. Driven equally by the pad, the Pencil Pro squeeze and the S-Pen button; whichever you use, the pad mirrors it. |

Values live in the engine, not the view: they persist across mode and layout
switches by construction.

## Rate policy

Wheels are continuous dimensions and are **policed** on the outbound pipe like
any voice dimension (≤ 100 Hz latest-wins, budgeted on BLE). Buttons are
**exempt** — a decimated sustain-off is a stuck pedal. After every MPE re-sync
the strip **re-announces** its latched state (bend, CC 1, both assignables,
CC 64) so the DAW and the strip never disagree.

## Panic

Settings → *Stop all notes* releases every held voice (pressure 0, then Note
Off), then sends CC 64 = 0 and CC 123 = 0 on the master and every member — on
the loopback and every transport, exempt from limiting. A BLE peripheral
cannot disconnect its central, so this, not a disconnect, is the meaningful
control; switching a single transport off silences that sink only.
