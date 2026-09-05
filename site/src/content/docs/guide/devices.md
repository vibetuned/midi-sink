---
title: Devices
description: Per-instrument setup — ROLI Piano and Seaboard, ROLI Airwave, Expressive E Osmose, Roland Aerophone Brisa and Odisei Travel Sax, and any classic keyboard.
---

midi-sink listens to three genuinely different MIDI dialects and recognises
which one it is hearing (override in the settings if you must):

* an MPE Configuration Message, or note-ons spread across channels 2+ with
  per-channel bend or pressure → **MPE mode**;
* notes only on one channel plus a dense breath (CC 2) stream → **wind mode**;
* otherwise → **classic mode**.

All connected inputs — hardware and virtual, hotplugged — open automatically
on every platform. The full message table is the
[MIDI implementation chart](../../reference/midi-chart/).

## ROLI Piano, Seaboard — MPE

Plug in (USB, or pair over Bluetooth from the settings) and play. Each note is
a voice on its own member channel:

| ROLI dimension | midi-sink |
|---|---|
| Strike (velocity) | drop size — area tracks velocity |
| Press (channel pressure) | sustained ink feed: the drop keeps growing while you press — or the Lamb–Oseen swirl with **Pressure → Swirl** |
| Glide (per-note bend, ±48) | drags the drop along the pitch axis — or shimmers the water with **Note bend → Ripple** |
| Slide (CC 74) | per-drop hue — or folds the water with **CC 74 → Pinch** |
| Lift | the drop sets |

Default bend range is ±48 on members (RPN 0 honoured), ±2 on the master. The
sustain pedal is a musical control in MPE mode and never wipes the canvas.

## ROLI Airwave — global gestures

The Airwave tracks your hands and sends its Air dimensions as ordinary CCs,
independent of notes. midi-sink treats them as **global field controls**
through the CC map. Assign these numbers on the Airwave side (or remap in the
desktop settings window):

| CC | Airwave dimension | Controls |
|---|---|---|
| 20 | Left-hand Raise | vortex strength — "wind over the water" |
| 21 / 22 | Glide | vortex centre X / Y |
| 23 | Right-hand Tilt | viscosity (damping) |
| 24 | Flex | paper roughness |
| 25 | Flex (alternate) | palette morph |

Any CC can drive any dimension; a channel-specific route beats an any-channel
one. The ripple's amount and wavelength dimensions ship unmapped (desktop
binds CC 102/103 to them; on the tablets the strip's assignable wheels do).

## Expressive E Osmose — MPE, aftertouch-dense

The Osmose is recognised as MPE like the ROLI and behaves the same — with one
thing to know: its continuous pressure stream is *dense*, and midi-sink's
press response is deliberately unbounded. Lean on a key and the drop floods
the sheet; that is the Osmose behaviour, and the reason the ingest queue and
smoothing are sized the way they are (one value per dimension per voice per
frame, ~30 ms smoothing).

## Roland Aerophone Brisa, Odisei Travel Sax — wind

A wind controller is a single voice that never stops moving, so midi-sink
plays it as a **wandering ink brush**: pitch sets where the brush is; a legato
note change *migrates* the brush there, drawing a trail, rather than spawning
a new drop; and breath (CC 2 — or CC 11, CC 7 and channel aftertouch, which
alias onto the same dimension) sets the brush's **width**, relaxing toward a
breath-proportional target. Bounded on purpose: a twenty-second phrase is a
calligraphic line, not a blob. Pitch bend shears the whole bath.

## Classic keyboards

Every note is a drop on the current layout, sized by velocity. Pitch bend (±2)
shears the bath, the mod wheel (CC 1) stirs the vortex, and the **sustain pedal
dips the paper** — a fresh sheet — in classic mode only.

## Tablets as instruments

On iPad and Android the surface itself is an MPE controller that can play any
of the above synths' hosts. [Play mode →](../play-mode/)
