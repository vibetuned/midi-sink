---
title: Paper and prints
description: The washi paper under the ink — or the glass under the charge — fresh sheets, the prints ledger, and saving a print — what a print captures and what it deliberately does not.
---

## The paper, or the glass

Under **Sumi** the substrate is washi; under **Anod** it is black glass with
a phosphor grain, and the charge glows over it. The medium's side — the
switch, the substrate pages, the palettes — is the [medium guide's](../medium/).
This page is the paper and the prints.

Under the ink is procedural **washi** — mulberry-fibre noise and absorption
grain, with *paper roughness* setting the fibre strength. The paper is
**screen-locked**: fibres and grain are sampled in screen space, never through
the deformed field, because paper is the stationary substrate. Ink moves; the
sheet does not. (If fibres ever warp with a comb stroke or drift with a piano
roll, that is a bug, not a feature.)

Three built-in palettes per medium — **Sumi black**, **Indigo**, **Ochre**;
**Electric blue**, **Plasma orange**, **Phosphor green** — and a custom slot
with a library behind it, chosen on the *Palette* page; the per-drop hue
selector (CC 74 in its default routing, or the palette-morph CC) shifts
individual drops within the palette. The *Substrate* page sets the paper's
tint (cream, white, custom), its roughness and the fibre scale — or the
glass's darkness, grain and glow. The picture is rendered in linear light
and encoded to sRGB identically on every backend, so a print matches the
screen bit for bit in tone. [Palettes →](../../operators/palettes/)

## A fresh sheet: the paper dip

**Paper dip** resets the tray to plain water. It is a settings action on
every platform — the first control of the sheet on Android, *Canvas* on the
desktop and the iPad, *Paper dip* in the web panel — never a MIDI message:
the sustain pedal is a musical control in every mode and goes to your synth.
Dip as often as you like: every dip lands in the **prints ledger** with the
field it printed. The dip also re-bases the drop counter that drives hues,
so a long session never runs the half-float palette selector out of
precision. A [recording](../replay/) begins with a dip.

## Saving a print

Every dip of the session is in the **Prints** page — the ledger — with a
thumbnail, its time, and the field it printed, so any of them re-exports at
any size while the session lives: *Export size* (the screen's, or larger —
the field is resolution-independent), *Export PNG*, and under Anod *Anod
over alpha*, the glow and the grid over a transparent background. The
ledger is memory-capped, the oldest entries going first. **Save the newest
print** writes the last dip's print at the screen's size — on the desktop to
the *Print folder* (*Save last print as PNG*), on the iPad to Photos, on
Android to the gallery, on the web as a download. What is in a print:

* the ink exactly as the field holds it — at the simulation resolution for
  a saved print, at the chosen size for an export — with the washi paper (or
  the glass and the glow) composited under it;
* **not** the orbit trace's scope and **not** the fingering overlays: they
  are screen-locked drawings over the water, like the lattice;
* a PNG, always — 8 bits per channel, over alpha under Anod on request. A
  16-bit TIFF for print workflows is not built; it waits on a demand that
  has not come;
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
