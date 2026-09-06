---
title: Paper and prints
description: The washi paper under the ink, fresh sheets, and saving a print — what a print captures and what it deliberately does not.
---

## The paper

Under the ink is procedural **washi** — mulberry-fibre noise and absorption
grain, with *paper roughness* setting the fibre strength. The paper is
**screen-locked**: fibres and grain are sampled in screen space, never through
the deformed field, because paper is the stationary substrate. Ink moves; the
sheet does not. (If fibres ever warp with a comb stroke or drift with a piano
roll, that is a bug, not a feature.)

Three palettes — **Sumi black**, **Indigo**, **Ochre** — chosen in the
settings; the per-drop hue selector (CC 74 in its default routing, or the
Airwave's palette-morph CC) shifts individual drops within the palette. The
picture is rendered in linear light and encoded to sRGB identically on every
backend, so a print matches the screen bit for bit in tone.

## A fresh sheet: the paper dip

**Paper dip** resets the tray to plain water. It is a settings action on every
platform — on the tablets two buttons, *save the print* (to Photos / the
gallery) and *discard*; on desktop and the web *Paper dip* plus a separate
*Save last print* — and on a **classic** keyboard the sustain pedal does it
too. Dip as often as you like: the engine keeps the last two prints and
recycles the older unread one. In MPE mode the pedal
is a musical control — it goes to your synth and leaves the canvas alone; the
dip is always a deliberate act there. The dip also re-bases the drop counter
that drives hues, so a long session never runs the half-float palette
selector out of precision.

## Saving a print

**Save print** writes a PNG of what touched the water. Desktop: Canvas → Save
print, to a file you choose. Tablets: to the photo library or a folder you
pick. Web: *Save last print as PNG* in the settings panel. What is in a print:

* the ink exactly as the field holds it, at the simulation resolution, with
  the washi paper composited under it;
* **not** the live ripple shimmer — with the ripple in *live* mode the sampling
  coordinate ripples on screen but the print samples the un-rippled field. The
  print is what touched the water; the shimmer is surface motion. In *bake*
  mode the ripple is real ink displacement and is in the print.

## Resolution and performance

The simulation resolution is decoupled from the screen: **Full-resolution
simulation** runs the field at the framebuffer's size; off, it runs at a
fraction (0.75 on phones and smaller tablets) and upsamples. Android drops to
0.6 under severe thermal pressure and recovers. Fields are RGBA16F everywhere
— the parity form of the ink phase is what lets half floats survive a
thousand drops without speckling.
