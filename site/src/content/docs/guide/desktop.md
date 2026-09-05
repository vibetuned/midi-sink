---
title: The desktop app
description: The macOS, Windows and Linux app — the settings window, the mouse gestures, MIDI inputs, prints, and the lab bench behind --dev.
---

The desktop app is a canvas window and a **settings window** that opens beside
it (close it any time; bring it back with ⌘ , on macOS or Ctrl , elsewhere).
It is Marble mode with MIDI in — there is no Play mode on desktop.

## The settings window

| Section | What it holds |
|---|---|
| **Layout & look** | the six [layouts](../layouts/), the three palettes (Sumi black, Indigo, Ochre), viscosity, ink feed, paper roughness, full-resolution simulation, tempo and roll speed for the piano rolls |
| **Expression routing** | Note bend → Glide / Ripple · Channel pressure → Ink feed / Swirl · CC 74 → Hue / Pinch · Pinch style → Saddle / Crossed tines · Vortex profile → Exponential / Rankine |
| **Ripple** | amount and wavelength (sent as CC 102 / 103 through the real control path), the frame angle, and a live/bake override |
| **CC map** | the routing table — any CC, any channel or "any", to any global dimension; defaults for the mod wheel, breath aliases and the Airwave; add, edit, clear |
| **MIDI inputs** | every connected port with its rescan status — hotplug is automatic |
| **Canvas** | paper dip (fresh sheet) and save print as PNG |
| **About** | version (from the release tag), commit, engine version |

Settings persist in the platform's config directory
(`~/Library/Application Support/midi-sink`, `%APPDATA%\midi-sink`,
`~/.config/midi-sink`).

## Mouse

Left click = drop · left drag = tine · right drag = vortex (profile from the
settings) · Shift + left drag = pinch (distance = strength delta, angle = fold
axis) · middle drag = stylus wake (scroll wheel sets the tip radius).

## MIDI

All inputs open automatically: CoreMIDI on macOS, WinMM on Windows, ALSA on
Linux. For a virtual source on Windows, create a loopMIDI port; on Linux the
harness's 1 Hz rescan picks up any `snd_seq` port as it appears.

## The lab bench (`--dev`)

Without the flag the app accepts only `--help` and `--version` and the
keyboard does nothing but the settings chord. With `--dev` you get the debug
keys (viscosity, feed, roughness, palette, layout, dip, BPM, profile, ripple
live/bake, pinch variant, pressure and bend routing, ripple angle and
amplitude/frequency, the crossed-tine prototype stamp and the swirl test voice),
the scripted operator tests, and `--field-dump <file>`, which writes the §4.6
cross-backend field dump that the release gates compare across Metal, D3D11,
OpenGL and WebGPU.
