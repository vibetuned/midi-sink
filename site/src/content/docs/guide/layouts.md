---
title: Layouts
description: Where a pitch lands on the water — six layouts, three of them playable lattices, two of them scrolling timelines.
---

A **layout** is a pure function from a note to one to three canvas positions.
It decides where an instrument's notes fall in Marble mode and, on the three
playable lattices, what your fingers touch in Play mode. Switch it live in the
settings; the engine keeps one implementation of the geometry, and the tablet
shells draw and hit-test by asking it (the *layout probe*), so the circle you
see is the cell you touch.

## Circle of fifths *(default; Marble-only)*

Pitch class around a circle in fifths order, octave as radius — low notes
outer, high notes inner. Chords become constellations; a progression walks
around the dial. Not playable: adjacent wedges are a fifth apart, so a sideways
bend has no semitone meaning.

## Chromatic grid *(playable)*

Reading order, C1 at the top left to B7 at the bottom right: a row per octave,
a column per pitch class, cells inset so edge drops stay on the sheet. Notes
outside C1–B7 clamp to the nearest edge cell. Pitch is a function of x alone
within a row, so pen legato is continuous here.

## Jankó *(playable)*

The Jankó keyboard's six staggered rows of whole-tone columns, every second
row offset by half a column. **A note stamps on all three rows of its parity**
— the whole lattice stays live, which is the point of visualizing Jankó. Any of
the three rows plays the note; the loopback paints all three, with one ink band
and one hue so they read as the same note. The semitone axis is horizontal:
one semitone is half a column straight across, the same for every echo. The
stagger ends are dead zones.

## Piano grid *(playable)*

The chromatic grid's frame with each octave as a two-row keyboard: five
accidentals above at the white-key positions 1, 2, 4, 5, 6 — C♯/D♯ between
their naturals, F♯/G♯/A♯ between theirs — and seven naturals below, seven
octaves tall. The geometry is tuned for glissando: an accidental's cell is 0.6
of a white key wide and 0.6 of the octave pair tall; a natural owns the bottom
0.6 of the pair. The strip above the naturals, off any accidental, is the
**glissando corridor** — a dead zone that *sustains*, so a stroke through it
plays the black-key run while the natural band below plays the white-key run
without grazing an accidental. Tapping in the corridor plays nothing; a stroke
must start on a key. Because pitch here is not a function of x alone, pen
legato is quantized to real key steps — a piano glissando, by design.

## Horizontal piano roll *(Marble-only)*

Pitch → y (low at the bottom); every drop is born on a fixed **now-line** at
x = 0.12 and the whole sheet drifts to the right — a DAW timeline flowing away
from the playhead. Old ink slides off the far edge; fresh water enters behind
the now-line. [The scroll operator →](../../operators/scroll/)

## Vertical piano roll *(Marble-only)*

Synthesia-style: pitch → x (low at the left), now-line near the top, the sheet
falls. Same drift, same fresh-water ingress.

## Tempo and roll speed

The rolls scroll at (bpm / 60) × roll_speed canvas lengths per second.
`roll_speed` is canvas-lengths per beat, default 1/16: the sheet holds 16
beats — four bars of 4/4 — so at 120 BPM ink lives eight seconds. BPM is a
setting the host supplies (the desktop nudges it ±5 for syncing to a
metronome); midi-sink never guesses tempo from the MIDI stream.

## Glides on a lattice

On the three lattices a one-semitone bend moves a drop **exactly one cell**, so
in Play mode the drop travels under your finger. The circle and the rolls keep
a gentler rendering cap — there a bend is a gesture, not a position.
