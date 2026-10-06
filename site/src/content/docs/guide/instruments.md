---
title: The instruments
description: The five layouts of Phase 9 — the trumpet and the trombone with their valves and slide, the Wicki–Hayden button-field, the strings under three tunings, and the theremin — on the water, under a controller, and under your hands on the tablets.
---

Eight layouts came with 1.0; five more make the sheet an instrument's
geometry rather than a keyboard's. All thirteen are in the picker on every
platform, and the eight keyed ones are playable on the tablets. Two of the
five are **stateful**: where a note lands depends on a fingering the
engine holds — the valves or the slide — so the same MIDI note is a
different cell under a different fingering. The other three are pure
functions of the note, like the first eight.

## The trumpet and the trombone

The **trumpet** is eight cells, one per partial of a B♭ instrument's
harmonic series — the pedal B♭1 at the bottom, then B♭2, F3, B♭3, D4, F4,
A♭4, B♭4 — and each cell *sounds its partial minus the valves' offset*:
valve 1 lowers by a whole tone, valve 2 by a semitone, valve 3 by a minor
third, and they add. The **trombone** is seven partial cells and a slide
that lowers continuously, up to six semitones. The fingering is **MIDI**,
global, on the master channel: valves CC 110, 111 and 112 (≥ 64 is
pressed), the slide CC 113 (0 is closed, 127 is the seventh position),
smoothed over 10 ms; a controller that sends them plays the layouts
exactly as the tablets do. A note arriving from the wire lands in the
partial the current fingering selects; if none does, the standard
fingering's; else the nearest partial.

Each cell's **lip bend** axis runs to the right: a bend of one semitone
moves the drop one cell radius, the brass player's lipping. The partials
sit in a **column** by default or, with *Partials on an arc*, on a ring —
radius 0.30 canvas heights, centred a little right of the middle at
0.65 of the height, cells of 0.055 — cut by the author's eye on the iPad;
the trombone's seven sit on the same ring under the one flag. Both
geometries are in the gates bitwise.

On a tablet the **fingering panel** holds the three valve pads (or the
slide) large, at the side at mid-height, or horizontal along the bottom —
the toggle lives under the brass layouts in the settings and on the
panel's rotate button — with the [control strip](../control-strip/) at
the opposite corner, clear of the settings gear. Press a valve and every
held voice **retunes** — ramped over 30 ms for the valves, at once for the
slide — through a bend on its member channel, so a DAW records the lipping
and the fingering as one MPE voice; a touch that lands while the valves
change begins between semitones and resolves on its first retune. The
engine's own fingering copy is what the panel mirrors: the probe is asked
with the state the shell sent, so the overlay and the hit-test agree with
the water.

Under [Suzu](../../suzu/) the *Trumpet* patch sounds the fingering it is
given and CC 74 is the embouchure; the *Bowed string* holds under the
valves as a brass note holds under the lips.

## Wicki–Hayden

A hex button-field six buttons wide and fifteen rows tall, on which every
note has exactly one button: a step right is a whole tone, up-right a
fifth, up-left a fourth, the octave two rows straight up — the Hayden
duet concertina's field. The rows alternate the two whole-tone scales and
sit half a button apart. Pitch is a plane over the sheet, so the bend axis
is that plane's gradient: a bend moves the drop up the field toward the
higher notes, whatever the row.

## Strings

String rows across chromatic frets under one of three fixed tunings —
*Standard guitar* (E2 A2 D3 G3 B3 E4), *Whole-tone tap* (twelve strings a
whole tone apart, E2 to D4, the tapping grid's isomorphism) and *All
fourths* — chosen with *Tuning* under the layout. A note is stamped at its
lowest-fret site on **every string that reaches it** (up to twelve echoes,
one ink band, one hue), so a chord shape reads across the neck; the bend
axis runs along the string. Touching any of a note's sites plays it.

## The theremin

No cells: the field is the cell. Five octaves run across the width — C2 at
the left edge to C7 at the right, 61 semitone slots — and the note is the
nearest semitone under the finger, its centre that slot's x at the middle
height. Sideways motion is continuous pitch: on a tablet a touch begins at
the note under it and moves as a bend, up to the member range, re-anchoring
on a new note past 47 semitones so a sweep across the whole width stays in
tune; pushing away and pulling back are the press axis as everywhere. Under
a controller the theremin places each note on its x and glides the drop
along the width.

## On the tablets: the rest of the surface

* **Panic** and **Next** are on the strip. Panic releases every voice and
  silences every pipe — the visualizer's voices too, since the mapper
  honours CC 120 and CC 123 — and resets the strip's wheels and the
  fingering. **Next** steps through the *quick-switch* subset of layouts
  you tick in the settings, for a change of instrument mid-piece without
  opening the sheet.
* **Left-handed mirroring** flips the lattice, the panel and the strip;
  the mirror is handed to every probe, so the water agrees with the hand.
* **The per-device offer.** The first time the app runs on a new device it
  offers a profile fitted to its size — the strip's and the panel's places,
  the horizontal fingering on a phone — in an alert you accept or dismiss;
  it is never applied on its own.
* **The S Pen's limit (Galaxy Tab).** Android's stylus palm rejection
  cancels every finger gesture the moment the S Pen comes within hover
  range of the glass, and drops the fingers until it leaves. The panel's
  valves latch across that cancel and release on the next touch of the
  panel, but a fingering change needs the pen away from the glass; the pen
  plays the partials alone, and a controller's fingering CCs play the
  valves beside it. The iPad has no such limit.

## Not in 2.0

Microtonal and Scala tunings — cheap on the theremin and the strings, and
asked about — are not built; every layout is twelve-tone equal temperament.
Woodwind key systems (the Boehm charts, a much larger state space) and
two-handed split layouts stay deferred with them.

## For a controller

Any controller plays the thirteen layouts through MIDI alone; the fingering
CCs 110–113 are the only new messages, listed in the
[implementation chart](../../reference/midi-chart/). The desktop and the
web draw the brass with a HUD of the fingering the engine holds, the web
as an overlay over the water ([the web canvas](../web/)).
