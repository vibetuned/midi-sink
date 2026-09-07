---
title: Settings reference
description: Every setting on every platform — what it drives in the engine, its range and default, which gesture or MIDI dimension it changes, and where it is stored.
---

One settings model, four surfaces: the desktop **settings window** (⌘ , or
Ctrl ,), the browser's **panel** (top-left), and the **sheet** behind the gear
on iPad and Android. Every row below exists on every platform unless the
*Platforms* column says otherwise. Ranges and defaults are the engine's; a
setting reaches the core either as a **params** field (applied on the render
thread), as a **CC** through the same MIDI path a controller uses, or as a
host action.

## Layout & look

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Pitch layout** | Circle of fifths · Chromatic grid · Jankó · Piano roll (horizontal) · Piano roll (vertical) · Piano grid · default *Circle of fifths* | Where a note lands on the sheet, and the lattice Play mode touches. The two piano rolls scroll with the tempo. [Layouts →](../../guide/layouts/) | params `pitch_layout` |
| **Palette** | Sumi black · Indigo · Ochre · default *Sumi black* | The ink's colour family. Per-note CC 74 (with *Slide = Hue*) moves a drop's hue inside it; the *Palette morph* CC dimension blends toward the next palette (sumi → indigo → ochre → sumi). | params `active_palette_id` |
| **Viscosity** | 0 – 1 · default 0.50 | Damping of continuous agitation: how quickly swirls, feeds and ripples settle. Also a CC dimension (Airwave Tilt R by default). | params `fluid_viscosity` |
| **Ink feed (pressure)** | 0.1 – 4 · default 1.00 | Scale of the pressure- and breath-driven drop growth: how much a held key or a breath expands its drop per second. | params `expansion_rate` |
| **Paper roughness** | 0 – 1 · default 0.50 | Strength of the washi fibre composite. Screen-locked: the paper never moves with the ink. Also a CC dimension (Airwave Flex L by default). | params `paper_roughness` |
| **Tempo (BPM)** | 20 – 300 · default 120 · *rolls only* | The piano rolls' scroll tempo. Host-supplied; the core never guesses tempo from MIDI. | params `bpm` |
| **Roll speed** | 0.02 – 0.25 · default 0.0625 · *rolls only* | Canvas lengths per beat. 1/16 keeps 16 beats, four bars of 4/4, on screen; 0.25 flushes the canvas every bar. | params `roll_speed` |
| **Full-resolution simulation** | on · off (0.75×) · default on for desktop and iPad-class GPUs · *desktop, web, iOS* | Simulation field size relative to the output. Off runs a 0.75× field for machines that run warm under dense MPE streams. Android has no toggle: its thermal listener owns the scale (0.75 ↔ 0.6). | params `sim_scale` |

## Expression routing

Each MIDI dimension has exactly one consumer at a time; these rows choose it.
[The Operators →](../../operators/)

| Setting | Choices · default | What it does | Reaches the core as |
|---|---|---|---|
| **Per-note bend** | Glide · Ripple · default *Glide* | Glide: a note's pitch bend drags its drop along the pitch axis. Ripple: the bend's distance from centre breathes the sine ripple's amplitude and the drop holds; each cycle bakes a faint comb into the ink, permanent like glide. Master bend keeps its shear tine either way. | params `bend_mode` (+ `ripple_bake` rides along) |
| **Channel pressure** | Ink feed · Swirl · default *Ink feed* | What hardware aftertouch (0xD0) plays: the drop's growth, or a Lamb–Oseen swirl at the note whose neighbours counter-rotate. Poly pressure (0xA0) always swirls, so the play surface's down-pull stirs regardless. | params `press_mode` |
| **Slide (CC 74)** | Hue · Pinch · default *Hue* | Per-note CC 74 modulates the drop's hue inside the palette, or its deltas fold the water at the note. | params `slide_mode` |
| **Pinch style** | Saddle · Crossed tines · default *Saddle* · shown when Slide = Pinch | Saddle: the Hamiltonian fold, area-preserving exactly. Crossed tines: two perpendicular opposing tine passes, softer and lumpier. Applies to the CC 74 route, the stylus pinch and the two-finger pinch alike. | params `pinch_variant` |
| **Vortex profile** | Exponential · Rankine · default *Exponential* | The CC-routed vortex (mod wheel, Airwave Raise L) and the Marble-mode gesture vortex (right drag, two-finger twist). Exponential: diffuse, breath-like. Rankine: a rigid core that turns as a disk. | params `vortex_profile` |
| **Stylus wake** | Inviscid doublet · Viscous stroke · default *Inviscid doublet* | The fluid the pen's stroke displaces. Doublet: the exact potential flow around a rigid tip. Viscous: the impulse of a tip in a Stokes layer (the 2-D Stokeslet), spread by viscosity. | params `wake_profile` |
| **Spread (l/a)** | 1.5 – 12 · default 3.0 · shown for the viscous stroke | How far the stroke's momentum has diffused, in tip radii: small is sharp and close, large is soft and far-reaching. | params `wake_spread` |

## Ripple

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Amount** | 0 – 127 · default 0 | The standing ripple's amplitude. Sent through the MIDI path as the CC routed to *Ripple amount* (CC 102 by default), the same route a controller uses. Disabled when nothing is routed there. | CC → `SUMI_CTL_RIPPLE_AMP` |
| **Wavelength** | 0 – 127 · default 32 | The ripple's wavenumber, likewise as the CC routed to *Ripple wavelength* (CC 103 by default). | CC → `SUMI_CTL_RIPPLE_FREQ` |
| **Angle** | 0° – 180° · default 0° | Rotation of the ripple's frame. | params `ripple_angle` (radians) |

The desktop and the browser show a *live / bake* override under `--dev`; in
normal use the bake follows *Per-note bend*.

## CC map

*Desktop, iOS, Android.* The routing table: any CC number, on one channel or
**any**, to one of the nine global dimensions. Add, remove, restore the
default map. The browser keeps the default map (Web MIDI hands it its ports; no
editor).

| Dimension | Default route | Consumer |
|---|---|---|
| **Vortex strength** | CC 1 mod wheel · CC 26 Airwave Raise L | the CC vortex's circulation, profile from *Vortex profile* |
| **Vortex center X** | CC 24 Airwave Glide L | where that vortex sits |
| **Vortex center Y** | CC 22 Airwave Slide L | " |
| **Viscosity** | CC 29 Airwave Tilt R | live *Viscosity* |
| **Paper roughness** | CC 30 Airwave Flex L | live *Paper roughness* |
| **Palette morph** | CC 31 Airwave Flex R | blend toward the next palette |
| **Ink flow (breath)** | CC 2 breath · CC 7 volume · CC 11 expression | the breath-driven drop feed (wind mode's breath aliases here) |
| **Ripple amount** | CC 27 Airwave Raise R · CC 102 | the ripple's amplitude (the *Amount* slider rides CC 102) |
| **Ripple wavelength** | CC 28 Airwave Tilt L · CC 103 | the ripple's wavenumber (the *Wavelength* slider rides CC 103) |

Every value arriving on a routed CC is smoothed with the engine's smoothing
time constant (30 ms; the desktop's `--dev` bench exposes it as *Smoothing*)
and consumed as a rate where the operator is a feed. [MIDI chart →](../midi-chart/)

## MIDI inputs

*Desktop, iOS, Android.* Every connected input by name, opened automatically;
a live message counter with the last message; **Rescan now**. Desktop shows the
seconds since the last 1 Hz rescan, iOS shows how many CoreMIDI sources were
skipped, Android offers **Pair Bluetooth MIDI instrument…** here (iOS too).
The browser lists its Web MIDI inputs under *About*.

## Mode and Play mode

*iOS and Android only* (desktop and web are Marble mode with MIDI in).

| Setting | Choices · default | What it does |
|---|---|---|
| **Mode** | Marble · Play · default *Marble* | Marble: tap = drop, drag = tine, twist = vortex, pinch = fold, pen = wake, long press = pressure. Play: each touch is an MPE joystick on the lattice (Chromatic grid, Jankó and Piano grid only). |
| **Velocity from touch size** | off · on · default off | Glass has no force sensor: finger velocity is 96 fixed, or coarsely modulated by the touch's radius. The pen's pressure is real. |
| **Sustain button latches (toggle)** | off · on · default off (momentary) | The control strip's sustain pad: press-and-hold pedal feel, or a latch. |
| **Outbound MIDI** | iOS: Virtual source · Network session · Bluetooth. Android: USB-MIDI to the host · Virtual device · Bluetooth advertise | Which sinks carry the Play surface's MPE stream. Every transport carries the identical stream under its own rate policy. |
| **Re-sync DAW** | action | Re-sends the MPE configuration (MCM + bend range) and the strip's announce. |
| **Stop all notes (panic)** | action | Releases every held voice and silences every pipe. |
| **Storm test / on-device suites** | actions | Evidence tools: a 60 s ten-voice storm, and the headless hostmpe + normalizer suites on the device. |

## Canvas

| Setting | Platforms | What it does |
|---|---|---|
| **Paper dip (fresh sheet)** | desktop, web | Freeze, snapshot the print, reset the water to identity. |
| **Paper dip — save the print** / **— discard** | iOS, Android | The same dip, keeping the print (Photos / Pictures/midi-sink) or letting it go. |
| **Save last print as PNG** | desktop, web | Writes the last dipped print. The ripple shimmer is surface motion and is not in a print. |

## About, Session, Evidence

**About** shows the app version from the release tag, the git describe and the
engine's ABI version, the same three on every platform. The tablets' **Session**
line reports frame rate, worst frame, thermal state, dropped loopback
messages, outbound counts and echo drops once per second. **Evidence** (iOS)
captures timed screens and flushes the byte, latency and session logs to the
app's Documents folder.

## The lab bench (desktop `--dev`)

**Smoothing (ms)** (1 – 200, default 30: the expressive dimensions' smoothing
time constant), **Log every MIDI message to stderr**, the debug keys, the
scripted operator tests and `--field-dump`. [Desktop →](../../guide/desktop/)

## Where settings live

| Platform | Store |
|---|---|
| macOS | `~/Library/Application Support/midi-sink/settings.ini` |
| Windows | `%APPDATA%\midi-sink\settings.ini` |
| Linux | `~/.config/midi-sink/settings.ini` |
| Web | the browser's `localStorage` (`sumi-web-settings`), per origin |
| iOS | `UserDefaults` (the app's preferences; deleted with the app) |
| Android | `SharedPreferences` `sumi` (cleared with the app's data) |

The CC map persists as `channel:cc:dimension;…` with an empty value meaning
the default map; the ripple sliders persist as their last CC values and are
re-sent when the engine starts.
