---
title: The control strip
description: The floating palette in Play mode — Pitch spring, Mod latch, two assignable wheels, Sustain, Panic and Next on the MPE master channel, mirrored by the pen's pedal — and the fingering panel the brass layouts add.
---

A compact palette floats at the **top left** of the lattice in Play mode
(hidden in Marble mode; top right left-handed, and at the opposite corner
from the fingering panel on the brass layouts). It floats rather than docks on purpose: a docked band
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
| **Panic** — button | CC 120 / CC 123 | Releases every held voice (pressure 0, then Note Off), silences the zone on the master and every member, resets the wheels and the fingering, and re-announces. Exempt from limiting; the visualizer's own voices flush too. |
| **Next** — button | — | Steps through the *quick-switch* subset of layouts ticked in Settings → *Quick-switch (the Next pad)*, in that order; no pad when none is chosen. |

## The fingering panel

On the trumpet and the trombone a second, larger panel appears — the three
**valve pads** (CC 110, 111, 112: 127 down, 0 up, momentary) or the
**slide** (CC 113, positional: where you hold it is the position) — at the
side at mid-height, or horizontal along the bottom (the toggle sits under
the brass layouts in the settings and on the panel's rotate button). The
pads are buttons and **exempt** from rate limiting; the slide is a
continuous dimension and **policed** like a wheel. Every held voice retunes
on a change — ramped over 30 ms for the valves, at once for the slide — so
the fingering is recorded as pitch on each voice's member channel and the
engine's own copy of the fingering (the CCs on the master) keeps the water
in step. The announce after a re-sync restates the fingering with the rest.
[The instruments →](../instruments/)

Values live in the engine, not the view: they persist across mode and layout
switches by construction.

## Rate policy

Wheels are continuous dimensions and are **policed** on the outbound pipe like
any voice dimension (≤ 100 Hz latest-wins, budgeted on BLE). Buttons are
**exempt** — a decimated sustain-off is a stuck pedal. After every MPE re-sync
the strip **re-announces** its latched state — the bend, CC 1, both
assignables, CC 64, the three valves and the slide, nine messages — so the
DAW and the strip never disagree.

## Panic

The strip's **Panic** pad and Settings → *Stop all notes* release every held
voice (pressure 0, then Note Off), then send CC 64 = 0, CC 120 and CC 123 on
the master and every member — on the loopback and every transport, exempt
from limiting — and reset the strip. A BLE peripheral cannot disconnect its
central, so this, not a disconnect, is the meaningful control; switching a
single transport off silences that sink only. The desktop has a *Panic*
button beside its MIDI inputs for the same purpose on the visualizer.

## Left-handed

*Left-handed (mirror the surface and the strip)* flips the lattice, the
strip and the fingering panel; the mirror is handed to every probe, so what
you touch is what the water paints.
