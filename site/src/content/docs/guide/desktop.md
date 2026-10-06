---
title: The desktop app
description: The macOS, Windows and Linux app — the settings window with its twenty sections, the mouse gestures, MIDI inputs, the sound engine, prints, presets, replay, and the lab bench behind --dev.
---

The desktop app is a canvas window and a **settings window** that opens beside
it (close it any time; bring it back with ⌘ , on macOS or Ctrl , elsewhere).
It is Marble mode with MIDI in — there is no Play mode on desktop.

The canvas is an ordinary window with its title bar on every platform: drag
it, snap it, resize it, and use **Fullscreen** (Settings › Window, or ⌃⌘F on
macOS and F11 elsewhere) for the display.

## The settings window

| Section | What it holds |
|---|---|
| **Canvas** | paper dip (fresh sheet), the print folder, save the last print as PNG |
| **Layout & look** | the thirteen [layouts](../layouts/) with *Partials on an arc* under the brass and *Tuning* under the strings, viscosity, ink feed, tempo and roll speed for the piano rolls, full-resolution simulation |
| **Prints** | the [ledger](../paper-and-prints/): every dip of the session, export size, Anod over alpha, export PNG |
| **Presets** | the session as a file: load, delete, save as, export, import — [presets](../presets/) |
| **Replay** | record, stop, the recordings, play, play file, stop replay — [replay](../replay/) |
| **Substrate** | the paper's tint, roughness and fibre scale; the glass's darkness, grain, bloom and reach |
| **Palette** | the active palette, the library, the custom slot's editor — [palettes](../../operators/palettes/) |
| **Medium** | Sumi or Anod, the glow scale, the grid lines, the strike charge — [the medium](../medium/) |
| **Expression routing** | the input dialect · Per-note bend → Glide / Ripple / Torsion wavelength / Spark frequency / Chladni stir · Channel pressure → Ink feed / Swirl / Torsion · Slide (CC 74) → Hue / Pinch / Spark frequency · the medium default for each · Pinch style · Vortex profile → Exponential / Rankine / Torsion · Stylus wake and its spread · Torsion sweep on note-on |
| **Ripple**, **Chladni**, **Burst**, **Spark**, **Chirikov** | each operator's knobs; the sliders that ride a CC (102/103, 106–109) go through the real control path |
| **CC map** | the routing table — any CC, any channel or "any", to any global dimension, the sound's bus effects included; defaults for the mod wheel, breath aliases and the Airwave; add, edit, restore |
| **MIDI inputs** | every connected port with its rescan status — hotplug is automatic — and the *Panic* button |
| **Sound** | the [sound engine](../sound/): internal sound, volume, source, the sampler's instrument or WAV, every knob of the current Suzu voice |
| **Suzu trace** | the [orbit trace's](../../suzu/orbit-trace/) routes, scale, segments, stroke, and the scope on the canvas |
| **Window** | fullscreen (⌃⌘F on macOS, F11 elsewhere) |
| **Lab bench (--dev)** | smoothing, the MIDI log, the debug keys |
| **About** | version (from the release tag), commit, engine version |

The iPad and Android sheets carry the same rows where the concept exists
(see [the settings reference](../../reference/settings/)).

Settings persist in the platform's config directory
(`~/Library/Application Support/midi-sink`, `%APPDATA%\midi-sink`,
`~/.config/midi-sink`).

## Mouse

Left click = drop (the spark under Anod) · left drag = tine · right drag =
vortex (profile from the settings) · Shift + left drag = pinch (distance =
strength delta, angle = fold axis) · middle drag = stylus wake (scroll wheel
sets the tip radius) · Shift + right drag = pressure.

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
the scripted operator tests, and `--field-dump <file>`, which writes the
cross-backend field dump that the release gates compare across Metal, D3D11,
OpenGL and WebGPU. The bench also holds the conservation soak of every
operator (`--soak <op>`), the layout shots (`--layout-shot`), the sound
profiles (`--voxo-profile`), the Voxo storm (`--voxo-storm`), and the replay
gate's recorder and player (`--record-demo`, `--replay`, `--replay-wav`);
`tools/README.md` in the tree lists them all.
