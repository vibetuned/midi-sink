---
title: The instruments
description: The six instrument layouts — the trumpet and the trombone with their valves and slide, the Boehm flute with its nine keys and three registers (v3.0), the Wicki–Hayden button-field, the strings under three tunings, and the theremin — on the water, under a controller, and under your hands on the tablets.
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

## The Boehm flute

The **Boehm flute** (v3.0, step 77) is three cells and nine keys. The cells
are *registers* — the fundamental, the octave, the second octave — on the
brass geometry, a column or, with *Registers on an arc*, the same ring; a
cell sounds the fingering's note plus 0, 12 or 24 semitones. The fingering
is nine momentary keys, **MIDI** on the master channel like the valves:
the left hand's three fingers on the valves' own CCs 110, 111 and 112
(L1, L2, L3), then CC 114 to 119 — the thumb, the G♯ lever, R1, R2, R3 and
the D♯ key (≥ 64 is pressed). The chart is the idealised Boehm flute's
**vent rule**: the highest open hole sounds. The holes from the top are the
C♯ hole under L1, the B hole under the thumb, the B♭ hole — closed by L2
*or* R1, the instrument's two linkages, which is why the "1 and 1" B♭
works — then A (L2), G (L3), the G♯ hole (a closed-standing key: pressing
it opens it), F (R1), E (R2), D (R3), the D♯ hole (closed-standing too);
everything closed is D4. One cross fingering is kept because every flutist
uses it: R3 alone below an open F hole flattens G to F♯. Every one of the
512 key states sounds a note, D4 to C♯5 on the fundamental.

| Note | Keys |
|---|---|
| D4 | L1 T L2 L3 R1 R2 R3 |
| D♯4 | L1 T L2 L3 R1 R2 R3 D♯ |
| E4 | L1 T L2 L3 R1 R2 (D♯) |
| F4 | L1 T L2 L3 R1 (D♯) |
| F♯4 | L1 T L2 L3 R3 (D♯) |
| G4 | L1 T L2 L3 (D♯) |
| G♯4 | L1 T L2 L3 G♯ (D♯) |
| A4 | L1 T L2 (D♯) |
| B♭4 | L1 T R1 (D♯) |
| B4 | L1 T (D♯) |
| C5 | L1 (D♯) |
| C♯5 | (D♯) |

The D♯ key in parentheses is held as a flutist holds it; it opens a hole
below the vent and changes nothing. The chart is `sumi_layout_boehm_note`
in the ABI, a pure function the shells' key panels label with. A note from
the wire lands on the register the current fingering sounds it from, else
the nearest register. The embouchure bend is the brass's lip bend: a
semitone to the right is one cell radius. With every key up the fundamental
sounds C♯5, so the registers play on a tablet before its key panel lands
(the panels are the iPad's and the Tab's own sub-steps); a controller that
sends the nine CCs plays the layout in full. Under [Suzu](../../suzu/) the
toneholes of the acoustic bore (step 81) are what these keys will drive.

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

## The split surface

Two layouts side by side, a hand each (v3.0, step 77; the two-handed split
of §14.6). *Split surface* in the settings draws the pitch layout on the
left half at half width and a second layout — *Right half* — on the right;
the *split note* divides the notes between them: the left plays the notes
below it, the right the notes from it, so a note from the wire has one home
and a touch's note agrees with where the engine draws it. The probe answers
for the half under the touch and refuses a cell whose note belongs to the
other half; the cells of both halves are enumerated (a shell greys the
other half's); the pitch axis follows the note's half. Both must be
playable layouts — the grids, the instruments, the theremin; a piano roll
scrolls the whole sheet and the circle of fifths has no cells, so either on
a side switches the split off. The web's overlay draws the split as the
probe sees it; the tablets' surfaces follow in their own sub-steps.

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

Shipped since, at 3.0's step 77: the woodwind key system (the Boehm flute above) and the two-handed split (the split surface). Still carried: the microtonal and Scala mapping of the layouts (§14.6).

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
