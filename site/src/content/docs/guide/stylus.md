---
title: The stylus
description: Apple Pencil and S-Pen in Play mode — absolute-position play with per-cell legato, real-force velocity, a barrel-roll vibrato booster, CC 74 from the tip's travel, a pedal in the squeeze, and a wake in the water.
---

Fingers already bend continuously and semitone-exactly, so the pencil's job is
different: **real note changes**. The pen abandons the joystick — precision
earns absolute-position play. Pen-as-lead, fingers-as-chords is the expressive
story; palm rejection is the platform's, so the two never collide.

## Legato, per cell

Touch a key and it sounds. Slide the tip into the next cell and midi-sink
emits, on the *same* channel, **bend (to the cell offset) → Note On (new) →
Note Off (old)** — the classic legato overlap. Mono and MPE synths glide, DAWs
record real terminated notes, the attack is in tune because the bend precedes
it. A crossing commits only once the tip is 0.65 semitones past the current
note, so a vibrato wiggle at a cell edge bends and never machine-guns
retriggers.

* On the **chromatic grid** and **Jankó** the sounding pitch is continuous
  across a crossing — the offset flips sign as the reference cell changes,
  within cents.
* On the **piano grid** the in-cell bend spans half a semitone while a
  natural-to-natural step is a whole tone, so part of every crossing arrives as
  a step: a **quantized glissando**, the way a piano glissando is. The white-key
  run lives on the natural band; the black-key run runs through the corridor
  above it, which sustains the last pitch instead of playing.
* A stroke must start on a key. Dead zones make no call.

## Velocity from force

* **Apple Pencil:** real tip force. A baseline tap (force 1.0, an average
  finger) plays at the finger default 96; a hard stab reaches 127. Force builds
  after contact, so sub-baseline touch-down readings clamp *up*, never to a
  whisper.
* **S-Pen:** normalized digitizer pressure, the same curve shape, hand-calibrated.

Continuous tip force then drives **channel pressure** (the ink feed, or the
swirl with the pressure routing set to Swirl), change-only.

## Vibrato, and the booster

Wiggle inside a cell and it bends — vibrato without retriggering, live at ×1.
The bend routes like any per-note bend: glide drags the drop, or with the
Ripple routing shimmers the water instead.

**Roll the Pencil Pro's barrel** and the vibrato deepens, ×1 up to ×3 while
rolling, decaying back to ×1 over 0.4 s once you stop. It is a gesture, not a
knob: a still hand is a still value, a regrip never jumps it. Only the
*emitted* bend is multiplied; note tracking and the crossing hysteresis stay
on the raw geometry. On a pen without a roll axis (S-Pen, older Pencils) the
same booster is fed by a small **azimuth stir** of the planted tip — lean the
pen back and forth while the tip stays put. Both feeds ignore tilt changes (a
tilt is posture, not a gesture) and no orientation value ever drives a knob
or a vortex.

Known and accepted: with the booster engaged, a cell crossing is no longer
pitch-continuous — the boosted offset can carry the pitch past the note step.
Deep vibrato plus a simultaneous slide is an extreme gesture.

## Up and down: CC 74

Move the tip toward the top of the screen and CC 74 rises (centre 64 at
pen-down, up = brighter), shaped by the same soft knee as the joystick,
change-only. With the **CC 74 → Pinch** routing, those slide deltas fold the
water at the pen's position along the pen's azimuth — and the CC 74 then goes
outbound only, so the loopback does not pinch twice.

## The pedal

**Squeeze the Pencil Pro** (or hold the S-Pen's barrel button; Pencil 2:
double-tap latches) and the sustain pedal goes down — the same engine as the
control strip's Sustain pad, so the pad lights, the message rides the master
channel, momentary by default and toggle behind the strip setting. Play mode
only. Sustain is a musical control here: it never wipes the canvas.

## The wake

Every pen stroke, in both modes, trails a **dipolar wake** — the water ahead of
the tip bulges forward, the flanks stream back, with the tip radius following
your pressure. It is physical, not musical, and is deliberately absent from the
MIDI stream: a DAW recording of a pencil performance replays every note, bend,
CC 74 and pressure exactly, and no wakes. A DAW has no stylus in the water.
[The wake operator →](../../operators/wake/)

## Hover

Where the hardware reports hover (Pencil on M2 and later iPads, the S-Pen
always), a ghost cursor shows the cell under the tip before it lands.
