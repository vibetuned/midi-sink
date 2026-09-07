---
title: Marble mode
description: The direct gestures — tap, drag, twist, pinch, pen — on every platform, with zero MIDI. The Airwave-like expression tool.
---

Marble mode is the tray of water under your hands. Nothing you do here
generates MIDI; every gesture is a deformation of the sheet, applied
immediately, exactly. It is the default on desktop and in the browser, and the
mode the tablets start in.

## The gestures

| Gesture | Tablet | Desktop / web mouse | Operator |
|---|---|---|---|
| Ink drop | tap | left click | [Drop](../../operators/drop/) |
| Comb stroke | one-finger drag | left drag | [Tine](../../operators/tine/) |
| Vortex | two-finger twist | right drag | [Vortex](../../operators/vortex/) — the profile is the settings' *Vortex profile*: exponential (diffuse) by default, or Rankine, where the disk between your fingers turns rigidly |
| Fold | two-finger pinch | Shift + left drag | [Pinch](../../operators/pinch/) — the fold axis is the finger line (or the drag angle) |
| Stylus wake | pen stroke | middle drag (scroll wheel sets the tip) | [Wake](../../operators/wake/) — pressure sets the tip radius; the fluid (inviscid doublet or viscous stroke) is a setting |
| Pressure | **long press** (hold 250 ms) | **Shift + right drag** | the press lays a drop and becomes Play mode's Y axis: hold or push up = [ink feed](../../operators/drop/) on that drop, pull back = [Lamb–Oseen swirl](../../operators/swirl/) with the drop as its core |

The vortex profile and the pinch variant come from the settings. On the
tablets a MIDI instrument plays *into* Marble mode exactly as it does on
desktop — the gestures and the instrument share the water.

## The same settings everywhere

Every platform carries the same settings: the six layouts and three palettes,
viscosity, ink feed and paper roughness, tempo and roll speed on the piano
rolls, the expression routing rows (per-note bend, channel pressure, CC 74 and
the pinch style, vortex profile, stylus wake and its spread), the ripple's
amount, wavelength and angle, and the CC map. Desktop has them in the
[settings window](../desktop/), the browser in its panel, the iPad and Android
apps in the sheet behind the gear. Two platform differences remain: the web
panel has no CC map editor (Web MIDI hands the browser its ports, and the
panel keeps the default map), and Android has no full-resolution toggle
because its thermal listener owns the simulation scale. Every row, its range
and what it drives: [Settings reference →](../../reference/settings/)

## While an instrument plays

Marble mode is where a second player belongs. One person plays the ROLI, the
other combs and stirs what the notes leave behind — or an Airwave does it
hands-free, its Raise, Tilt, Glide and Flex dimensions arriving as CCs that the
CC map routes to vortex strength and centre, viscosity, roughness and palette.
[Devices →](../devices/)

## Fresh sheets and prints

**Paper dip** clears the tray to plain water: a settings action on every
platform (and the sustain pedal on a classic keyboard, where it is not a
musical control). **Save print** writes what touched the water — the ripple
shimmer is surface motion and is not in a print.
[Paper and prints →](../paper-and-prints/)

## Layouts still matter

Marble mode has no cells to touch, but the pitch layout decides where an
instrument's notes land: around the circle of fifths, on the chromatic grid,
across the three Jankó rows, on the piano grid, or on a scrolling piano roll.
[Layouts →](../layouts/)
