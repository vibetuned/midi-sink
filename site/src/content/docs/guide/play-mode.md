---
title: Play mode
description: The tablet as a 15-voice MPE instrument — a joystick under every finger on a pitch lattice, a legato pencil, a floating control strip, and a standard MPE stream to your DAW. iPad and Android only.
---

:::note[iPad and Android]
Play mode exists on the tablets. Desktop and the web canvas are Marble-only:
they have MIDI *in*, not a touch surface worth playing.
:::

Switch **Settings → Mode → Play** on the **chromatic grid**, **Jankó** or
**piano grid** layouts. (The circle of fifths and the piano rolls stay
Marble-only: adjacent wedges on the circle are a *fifth* apart, so angular bend
has no sane semitone scaling, and the rolls are timelines.) A faint lattice
appears over the water — each cell drawn as a circle, at exactly the size of
the joystick it will become. The marbling stays the star; the lattice is a
guide.

## Every finger is a joystick

* **Touch down** where the note is. The contact point becomes the joystick's
  centre: midi-sink sends a centre pitch bend and *then* the Note On, so the
  attack is in tune by construction. Velocity is a fixed 96 (glass has no
  force sensor; a touch-size modulation sits behind a setting).
* **Move sideways** to bend. Inside the circle a soft knee removes finger
  jitter; beyond it the bend tracks your finger's lattice position exactly —
  drag one cell over and you are exactly one semitone up, drag across five
  columns and the glissando stays in tune. A hairline circle and a thumb dot
  show the deadband and your deflection as the engine computes them.
* **Push away** (up) to feed ink — channel pressure grows your drop, without
  bound: hold and it floods.
* **Pull back** (down) to stir — polyphonic key pressure drives a Lamb–Oseen
  swirl centred on your drop. Both halves live at once with no mode flip;
  crossing the centre releases the departing half through zero.
* **Lift** and the drop sets. Pressure goes to zero, then Note Off; the
  channel returns to the allocator as most-recently-released.

Fingers never send CC 74 — timbre belongs to the [stylus](../stylus/).

## Fifteen voices, honestly allocated

The Play surface is a lower-zone MPE instrument: master channel 1, members
2–16. Voices are allocated least-recently-released round-robin (first-free
would hijack the release tail of the note that just freed a channel). Channels
holding a note from a *hardware* instrument plugged into the same tablet are
masked out. When all fifteen are busy the sixteenth touch is a silent drop and
a HUD blink — never a steal.

On Jankó, touching any of a note's three rows plays it, and the loopback
paints all three.

## Where the stream goes

Everything you play is consumed twice:

1. **Loopback** into the visualizer — full rate. The visualizer is just another
   MPE synth; the picture you see *is* the MIDI you sent.
2. **Outbound** to the transports you enable, each under its own rate policy
   (change-only filtering, then ≤ 100 Hz per voice per dimension on USB /
   virtual / network, a ~300 msg/s budget on BLE; Note On/Off, the centre bend,
   pressure-0-before-Note-Off and the pedal are never dropped):

| | iPad | Android |
|---|---|---|
| Wired | virtual CoreMIDI source "midi-sink Play Surface" — also sent explicitly to a USB-tethered Mac (IDAM) | **USB-MIDI gadget** (primary): set *Use USB for → MIDI* in the system USB preferences and any host sees a class-compliant device |
| On-device apps | the same virtual source | `MidiDeviceService` virtual device |
| Wireless | MIDI network session (rtpMIDI) over Wi-Fi; BLE to a paired Bluetooth MIDI destination | BLE-MIDI peripheral — the tablet advertises, your desktop DAW connects |

Entering Play mode, opening a sink, and **Re-sync DAW** all send the MPE
configuration first (MCM: lower zone, 15 members; then bend range 48 on every
member), so a DAW or synth configures itself. **Panic** releases every voice
and silences the zone on every pipe.

## The control strip

A compact palette floats at the top left: a Pitch spring wheel (±2 on the
master), a Mod latch wheel (CC 1 — the loopback routes it to the vortex, so the
mod wheel stirs the water while it modulates your synth), two assignable
wheels (CC 23 / 24 by default), and Sustain (CC 64), which the Pencil Pro's
squeeze and the S-Pen's button also drive. All of it on the master channel.
[The control strip →](../control-strip/)

## Recording it

A DAW records a valid MPE performance you can replay into midi-sink or any
MPE synth. Two things a recording does not carry, by design: the stylus
**wake** (physical, not musical) and — only with the CC 74 → pinch routing —
the pen's CC 74, which goes outbound but not into the loopback because the
shell pinches directly. The [MIDI implementation chart](../../reference/midi-chart/)
has every message.
