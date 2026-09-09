---
title: The desktop app
description: The macOS, Windows and Linux app — the settings window, the mouse gestures, MIDI inputs, prints, and the lab bench behind --dev.
---

The desktop app is a canvas window and a **settings window** that opens beside
it (close it any time; bring it back with ⌘ , on macOS or Ctrl , elsewhere).
It is Marble mode with MIDI in — there is no Play mode on desktop.

On macOS the canvas has **no title bar**: it runs edge to edge with the
traffic lights kept, drags by its top strip and resizes at its edges. On
Linux it is a borderless window — move it with the Super + arrow keys, size
it with `--window` or fullscreen. On Windows the canvas keeps its title bar:
a borderless window there can neither be dragged nor snapped (Windows Snap
refuses frameless windows), so the normal frame is the usable default. The
settings window keeps its frame everywhere.

## The settings window

| Section | What it holds |
|---|---|
| **Layout & look** | the eight [layouts](../layouts/), the three palettes (Sumi black, Indigo, Ochre), viscosity, ink feed, paper roughness, full-resolution simulation, tempo and roll speed for the piano rolls |
| **Expression routing** | Note bend → Glide / Ripple · Channel pressure → Ink feed / Swirl · CC 74 → Hue / Pinch · Pinch style → Saddle / Crossed tines · Vortex profile → Exponential / Rankine · Stylus wake → Inviscid doublet / Viscous stroke (with its spread) |
| **Ripple** | amount and wavelength (sent as CC 102 / 103 through the real control path), the frame angle, and a live/bake override |
| **CC map** | the routing table — any CC, any channel or "any", to any global dimension; defaults for the mod wheel, breath aliases and the Airwave; add, edit, clear |
| **MIDI inputs** | every connected port with its rescan status — hotplug is automatic |
| **Window** | fullscreen (⌃⌘F on macOS, F11 elsewhere) |
| **Canvas** | paper dip (fresh sheet) and save print as PNG |
| **About** | version (from the release tag), commit, engine version |

The iPad and Android sheets carry the same rows (see
[Marble mode → The same settings everywhere](../marble-mode/#the-same-settings-everywhere)).

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

## Command line

`--window <w>x<h>` opens the canvas at an exact size in screen points, for
reproducing a report at a given resolution (the default is 1280×720; the
window stays resizable):

```sh
# macOS (the installed bundle)
/Applications/midi-sink.app/Contents/MacOS/midi-sink --window 1920x1080
# Windows
"C:\Program Files\midi-sink\midi-sink.exe" --window 1366x768
# Linux
midi-sink --window 2560x1440
```

On a HiDPI display the framebuffer is that size times the scale factor; the
settings window's *Session* line and the `--dev` bench report the pixel size.

`--fullscreen` starts with the canvas filling the display it opens on. It sets
the *Window › Fullscreen* setting, so it persists like the checkbox; ⌃⌘F
(macOS) or F11 toggles fullscreen from the canvas at any time.

## The lab bench (`--dev`)

Without the flag the app accepts `--window`, `--fullscreen`, `--help` and
`--version`, and the keyboard does nothing but the settings chord and the
fullscreen toggle. With `--dev` you get the debug
keys (viscosity, feed, roughness, palette, layout, dip, BPM, profile, ripple
live/bake, pinch variant, pressure and bend routing, ripple angle and
amplitude/frequency, the crossed-tine prototype stamp and the swirl test voice),
the scripted operator tests, and `--field-dump <file>`, which writes the §4.6
cross-backend field dump that the release gates compare across Metal, D3D11,
OpenGL and WebGPU.
