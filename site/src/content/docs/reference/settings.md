---
title: Settings reference
description: Every setting on every platform — what it drives in the engine, its range and default, which gesture or MIDI dimension it changes, and where it is stored. The desktop window's twenty sections, the tablets' sheet, the web panel.
---

One settings model, four surfaces: the desktop **settings window** (⌘ , or
Ctrl ,), the browser's **panel** (top-left), and the **sheet** behind the gear
on iPad and Android. Every row below exists on every platform unless the
*Platforms* column says otherwise; the sections follow the desktop window's
order, and the tablets fold them into pages (*Sound*, *Medium & look* with
its *Palette*, *Substrate*, *Presets* and *Operators* pages, *Layout*,
*Mode*, *Input*, *Expression routing*, *Ripple*, *Stylus wake*, *CC map*,
*MIDI*, *Outbound MIDI*, *Prints*, *Replay*, *About*, *Session*). Ranges and
defaults are the engine's; a setting reaches the core either as a **params**
field (applied on the render thread), as a **CC** through the same MIDI path
a controller uses, or as a host action.

## Canvas

On Android this section leads the sheet — the paper dip is the most-used
control; the desktop window and iOS keep it first too since Phase 6.

| Setting | Platforms | What it does |
|---|---|---|
| **Paper dip (fresh sheet)** | all | Freeze, print the sheet into the [ledger](#prints), reset the water to identity. Never a MIDI message. |
| **Print folder** | desktop | Where *Save last print as PNG* writes. |
| **Save last print as PNG** / **Save the newest print** | all | The newest dip's print at the screen's size — to the folder, to Photos (iOS), to the gallery (Android), as a download (web). |

## Layout & look

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Pitch layout** | the thirteen: Circle of fifths · Chromatic grid · Jankó · Piano roll (left, top, right, bottom) · Piano grid · Trumpet · Trombone · Wicki-Hayden · Strings · Theremin · default *Circle of fifths* | Where a note lands on the sheet, and the lattice Play mode touches; the tablets mark the eight keyed ones *(playable)*. [Layouts →](../../guide/layouts/) · [The instruments →](../../guide/instruments/) | params `pitch_layout` |
| **Partials on an arc** | off · on · default off · *under the trumpet and the trombone* | The brass partials on a ring (radius 0.30, centred right of the middle at 0.65 of the height) instead of a column. | params `trumpet_arc` |
| **Tuning** | Standard guitar · Whole-tone tap grid · All fourths · default *Standard guitar* · *under Strings* | The strings' tuning preset. | params `string_tuning` |
| **Viscosity** | 0 – 1 · default 0.50 | Damping of continuous agitation: how quickly swirls, feeds and ripples settle. Also a CC dimension. | params `fluid_viscosity` |
| **Ink feed (pressure)** | 0.1 – 4 · default 1.00 | Scale of the pressure- and breath-driven drop growth. | params `expansion_rate` |
| **Tempo (BPM)** | 20 – 300 · default 120 · *rolls only* | The piano rolls' scroll tempo. Host-supplied; the core never guesses tempo from MIDI. | params `bpm` |
| **Roll speed** | 0.02 – 0.25 · default 0.0625 · *rolls only* | Canvas lengths per beat. 1/16 keeps 16 beats, four bars of 4/4, on screen. | params `roll_speed` |
| **Full-resolution simulation** | on · off (0.75×) · default on for desktop and iPad-class GPUs · *desktop, web, iOS* | Simulation field size relative to the output. Android has no toggle: its thermal listener owns the scale (0.75 ↔ 0.6). | params `sim_scale` |

## Prints

The **ledger**: every dip of the session, newest first, with a thumbnail,
its time and the field it printed. [Paper and prints →](../../guide/paper-and-prints/)

| Setting | Range · default | What it does |
|---|---|---|
| **This session** | the list | Pick a dip to export. |
| **Export size** | Screen · 2K wide · 4K wide · 8K wide · default *Screen* | The export's width; the field is resolution-independent, so any dip re-renders at any size while the session lives. |
| **Anod over alpha** | off · on · default off · *Anod* | The glow and the grid over a transparent background. |
| **Export PNG** | action | Writes the chosen dip at the chosen size, with the look it was printed with. |

## Presets

The session as a file — everything but the ink. [Presets →](../../guide/presets/)

| Setting | Platforms | What it does |
|---|---|---|
| **The list** · **Load** · **Delete** | all | Named presets; loading replaces the session's settings and leaves the sheet. |
| **Save as** (a name) | all | Writes `<name>.json` — `<config>/presets/` on the desktop, the documents folder on the tablets, `localStorage` on the web. |
| **Export** · **Import** | all | A file you choose (Files on the iPad, the system picker on Android, a download on the web). |

## Replay

Record a session; play one back. [Replay →](../../guide/replay/)

| Setting | Platforms | What it does |
|---|---|---|
| **Record** · **Stop recording** | desktop, iOS, Android | Starts with the sheet dipped and the session re-announced; writes `<stamp>.sumireplay` under `<config>/replays/` (the desktop) or the app's Replays folder (the tablets). The row shows the frames and seconds while it runs. |
| **The recordings** · **Play** · **Delete** | desktop, iOS, Android | The list; playing mutes live input and applies only the palette until the end. |
| **Play file** / **Import a recording…** | desktop · tablets | One from another device. |
| **Replay a recording…** | web | A local `.sumireplay`; `?replay=<url>` from a link. The picture alone. |
| **Stop replay** | all | Ends playback; the session is re-applied, the sheet left. |

## Substrate

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Paper tint** | Cream (washi) · White · Toned · Custom · default *Cream* · *Sumi* | The washi's base tone; the mottle, grain and fibres ride on it. | params `paper_tint` |
| **Paper** | Smooth · Washi · Coarse · Custom · default *Washi* · *Sumi* | A preset of roughness and fibre scale. | params `paper_roughness`, `fiber_scale` |
| **Roughness** | 0 – 1 · default 0.50 · *Sumi* | Strength of the mottle, grain and fibre strands. Screen-locked: the paper never moves with the ink. Also a CC dimension. | params `paper_roughness` |
| **Fiber scale** | *Sumi* | The fibres' size. | params `fiber_scale` |
| **Glass darkness** | *Anod* | The near-black under the charge. | params `anod_dark` |
| **Phosphor grain** | *Anod* | The speckle in the glass, screen-locked. | params `anod_grain` |
| **Glow bloom** · **Glow reach** | *Anod* | The lens's blur added back over the reach's octaves; the brightest filaments clip toward white. Prints and exports bloom the same. | params `anod_bloom`, `anod_bloom_levels` |

## Palette

The active palette, the library, the custom slot. [Palettes →](../../operators/palettes/)

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Active** | the medium's three built-ins · Custom · default the first (Sumi black, Electric blue) | The ink's or the charge's colour family; the *Palette morph* CC dimension travels the ring from it. | params `active_palette_id` |
| **Library** · **Load into custom** | the built-ins and the curated presets: Cobalt & amber (ink, charge), Viridis, Cividis | Fills the custom slot as a starting point. | `sumi_palette_preset` |
| **The stops** (+ stop, − stop, each at a position with a colour) | 2 – 8 | The gradient along the ink's depth or the charge's glow. | `sumi_set_palette` |
| **Depth curve** · **Depth floor** | | How the depth walks the ramp (below 1 the ink pools early), and where it starts. | " |
| **Hue drift** · **Drift toward** | | Each drop leans by the drift toward the accent — the halo under Anod. | " |
| **Clear water** | | The tone between the rings. | " |

## Medium

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Medium** | Sumi (ink on washi) · Anod (strain-glow) · default *Sumi* | The reading of the field; a switch is live and bitwise. [The medium →](../../guide/medium/) | params `medium` |
| **Glow scale** | *Anod* | The strain a texel needs to glow: smaller is hotter, sooner. | params `anod_glow` |
| **Grid lines** | lines per canvas height · 0 = glass · *Anod* | The water's deformed grid at rest. | params `anod_pitch` |
| **Strike charge** | 0.1 – 1 · default 0.57 · *Anod* | The spark's size as a fraction of the Sumi drop's radius. | params `anod_drop` |

## Expression routing

Each MIDI dimension has exactly one consumer at a time; these rows choose it.
*Medium default* follows the medium's table (Sumi: glide, ink feed, hue;
Anod: the Chladni stir, torsion, the spark's frequency).
[The Operators →](../../operators/)

| Setting | Choices · default | What it does | Reaches the core as |
|---|---|---|---|
| **Input** | MPE · Classic keyboard · Wind · default *MPE* | The MIDI dialect. MPE: per-note expression on the member channels. Classic: per-note voices on any channel, bend = global shear. Wind: one voice played as MPE plus a wake dragging the sounding drop to the next note on legato. A setting, never a detection. [Devices →](../../guide/devices/) | `sumi_set_input_mode` |
| **Per-note bend** | Medium default · Glide (drag the drop) · Ripple amplitude · Torsion wavelength · Spark frequency · Chladni stir · default *Medium default* | What a note's pitch bend plays: the drop along the pitch axis; the sine ripple's amplitude; the torsion's $k$; the spark's $k$; the Chladni stir (distance the rate, sign the sense). Master bend keeps its shear tine either way. | params `bend_mode` (0–4; `SUMI_MODE_MEDIUM_DEFAULT`) |
| **Channel pressure** | Medium default · Ink feed · Lamb-Oseen swirl · Torsion sweep feed · default *Medium default* | What hardware aftertouch (0xD0) plays: the drop's growth, a swirl at the note, or torsion spent around the note as a rate. Poly pressure (0xA0) always swirls under Sumi. | params `press_mode` |
| **Slide (CC 74)** | Medium default · Hue · Pinch · Spark frequency · default *Medium default* | Per-note CC 74: the drop's hue inside the palette, a fold at the note, or the spark's streamer frequency. | params `slide_mode` |
| **Pinch style** | Saddle · Crossed tines · default *Saddle* · shown when Slide = Pinch | The Hamiltonian fold, or two perpendicular opposing tine passes. Applies to the CC 74 route, the stylus pinch and the two-finger pinch alike. | params `pinch_variant` |
| **Vortex profile** | Exponential · Rankine · Torsion · default *Exponential* | The CC-routed vortex (mod wheel, Airwave Raise L) and the Marble-mode gesture vortex: diffuse; a rigid core; rings of alternating shear. | params `vortex_profile` |
| **Stylus wake** | Inviscid doublet · Viscous stroke · default *Inviscid doublet* | The fluid the pen's stroke displaces. | params `wake_profile` |
| **Spread (l/a)** | 1.5 – 12 · default 3.0 · shown for the viscous stroke | How far the stroke's momentum has diffused, in tip radii. | params `wake_spread` |
| **Torsion sweep on note-on** | off · on · default off | Every strike also fires an outward wave of torsion around its drop, fading over two seconds. | params `torsion_sweep` |

## Ripple, Chladni, Burst, Spark, Chirikov

The operators' knobs. On the tablets they share the *Operators* page. The
sliders that ride a CC go through the real control path and are disabled
until a route exists in the CC map (the desktop's stock map routes 102/103
and 104–109).

| Setting | Range · default | What it does | Reaches the core as |
|---|---|---|---|
| **Ripple · Amount** · **Wavelength** | 0 – 127 · default 0 · 32 | The standing ripple's amplitude and wavenumber. | CC → *Ripple amount* (102), *Ripple wavelength* (103) |
| **Ripple · Angle** | 0° – 180° · default 0° | Rotation of the ripple's frame. | params `ripple_angle` |
| **Chladni · Stir (A)** · **Balance (B)** | 0 – 127 | The stirring rate; the odd keys' sense from opposite through still to the same. | CC → *Chladni stir* (106), *Chladni balance* (107) |
| **Chladni · Cell size** · **Mode** | 0.5 – 1.5 · Discs (exact) · Inverse Chladni (blended field) | The eddy's disc as a fraction of the key (capped at 1 in the exact mode); exact discs or the summed flow. | params `chladni_cell`, `chladni_mode` |
| **Burst · Age** · **Life** · **Order** | 1.5 – 12 · 0 – s · 2 – 8 · defaults 4 · 0.8 · 2 | The final diffusion length over the core; the release in seconds; the multipole order (the Anod strike's burst takes it). | params `burst_age`, `burst_life`, `burst_order` |
| **Spark · Shear** · **Decay** · **Octaves** · **profile** | · · 1 – 4 · triangle / noise · defaults 0.6 · 0.25 · 3 · triangle | The shear kick as a fraction of the radius; its decay; the stack's depth; the profile. | params `spark_shear`, `spark_tau`, `spark_stack`, `spark_profile` |
| **Spark · Frequency (k)** | 0 – 127 | The streamer frequency. | CC → *Spark frequency* (108; CC 74 under *Slide → Spark frequency*) |
| **Chirikov · K max** · **Periods** · **Drift** | | $K$ of a full one-frame throw; the kick's waves per canvas height; the drift's scale. The per-step $K$ is capped at 1.25 in the core. | params `chirikov_kmax`, `chirikov_periods`, `chirikov_eps` |
| **Chirikov · Throw** | 0 – 127 | The delta-driven throw. | CC → *Chirikov throw* (109) |

## CC map

*Desktop, iOS, Android.* The routing table: any CC number, on one channel or
**any**, to one of the twenty global dimensions or the sound's six bus
targets. Add, remove, restore the default map. The browser keeps the default
map (Web MIDI hands it its ports; no editor).

| Dimension | Default route | Consumer |
|---|---|---|
| **Vortex strength** · **center X** · **center Y** | CC 1 mod wheel · CC 26 Airwave Raise L; CC 24 Glide L; CC 22 Slide L | the CC vortex, profile from *Vortex profile*; its centre (Y reversed: CC up = up on screen) |
| **Swirl strength** · **center X** · **center Y** | CC 27 Airwave Raise R; CC 25 Glide R; CC 23 Slide R | the Lamb–Oseen stir and its centre |
| **Pinch (saddle)** · **Pinch (crossed tines)** | CC 20 Airwave Grasp L; CC 21 Grasp R | delta-driven folds at the vortex and the swirl centres |
| **Viscosity** · **Paper roughness** · **Palette morph** | — (route one) | the live sliders; the ring from the active palette |
| **Ink flow (breath)** | CC 2 breath · CC 7 volume · CC 11 expression | the breath-driven drop feed (wind mode's breath aliases here) |
| **Ripple amount** · **Ripple wavelength** | CC 29 Airwave Tilt R · CC 102; CC 28 Tilt L · CC 103 | the ripple |
| **Torsion wavenumber** · **Torsion phase** | CC 104 · CC 105 (desktop) | the wave torsion's $k$ and $\varphi$ |
| **Chladni stir** · **Chladni balance** | CC 106 · CC 107 (desktop) | the eddies' rate and sense |
| **Spark frequency** | CC 108 (desktop) | the spark's $k$ |
| **Chirikov throw** | CC 109 (desktop) | the standard map's delta-driven step |
| **Reverb wet** · **room** · **damping** · **Delay wet** · **time** · **feedback** | — (route one) | the sound engine's bus effects (targets 1000–1005) |

Every value arriving on a routed CC is smoothed with the engine's smoothing
time constant (30 ms) and consumed as a rate where the operator is a feed.
[MIDI chart →](../midi-chart/)

## MIDI inputs

*Desktop, iOS, Android.* Every connected input by name, opened automatically;
a live message counter with the last message; **Rescan now**; **Panic (all
notes off)** on the desktop. iOS and Android offer **Pair Bluetooth MIDI
instrument…** here. The browser lists its Web MIDI inputs under *About*.

## Sound

The [sound engine](../../guide/sound/). *Desktop, iOS, Android.*

| Setting | Range · default | What it does |
|---|---|---|
| **Internal sound (Voxo)** | off · on · default on for the tablets | Starts the output device. |
| **Volume** | 0 – 1 | The master level. |
| **Source** | Sampler · Suzu · Both, layered · default *Sampler* | What a note strikes; switching ends every voice. |
| **Instrument** (`.dspreset` / `.dslibrary`) · **Load** · **Unload** · **Demo instrument** | desktop | The sampler's instrument, loaded to memory; the Dan Tranh demo. |
| **Sample (WAV)** · **Root note** | desktop | One sample at a root note when no instrument is loaded. |
| **Sampler instrument** · **Import a `.dslibrary`…** · **Import a preset's folder…** | tablets | Copied into the app's Instruments folder; the Dan Tranh demo is the default. |
| **The play surface sounds here (Local Control)** | on · off · default on · *tablets* | Whether the surface's own notes sound inside (CC 122). |
| **Foreground only** | *iOS* | The sound pauses with the app. |
| **Suzu patch** | the table's eleven: Bowed string · Bell · Flute · Saxophone · Trumpet · Dan Tranh, Glockenspiel, Tubular bells, Concert harp, Tenor sax, Baroque recorder (VCSL) | The synth's patch — the whole setting on the tablets; *Load patch* puts it into the desktop's knobs. [The patch table →](../../suzu/patches/) |
| **Synth trace** · *Draw the orbits into the water* · *Scope on the canvas* · *Trace scale* | off · on; Off · Over the water · Alone; 0.05 – 2 · defaults off, off, 0.25 · *tablets, when the synth is the source* | The orbit trace's two routes and its scale in canvas heights per unit orbit amplitude (0.25 suits the rotor; the modal voices and the winds want more) — [the orbit trace](../../suzu/orbit-trace/). |
| **Suzu voice** and its knobs | *desktop* | Every parameter of the current voice kind, *Suzu attack (s)* among them — [patches](../../suzu/patches/). |

## Suzu trace

*Desktop.* The [orbit trace](../../suzu/orbit-trace/): *ink (the gesture
route)*, *scope*, *this voice kind*, *scale*, *segments* (4 – 8), *stroke*
(tine, wake), *on the canvas* (off, over the water, the scope alone), and
the miniature scope.

## Mode and Play mode

*iOS and Android only* (desktop and web are Marble mode with MIDI in).

| Setting | Choices · default | What it does |
|---|---|---|
| **Mode** | Marble · Play · default *Marble* | Marble: tap = drop, drag = tine, twist = vortex, pinch = fold, pen = wake, long press = pressure. Play: each touch is an MPE joystick on the lattice (the eight keyed layouts and the theremin). |
| **Velocity from touch size** | off · on · default off | Finger velocity is 96 fixed, or coarsely modulated by the touch's radius. The pen's pressure is real. |
| **Show the control strip** | Android · on / off · default on for tablets, **off on phones** | The floating strip; hidden, the S-Pen button still holds the pedal and the wheels keep their last CC values. |
| **Sustain button latches (toggle)** | off · on · default off (momentary) | The strip's sustain pad: press-and-hold, or a latch. |
| **Fingering panel horizontal** | off · on · *under the brass layouts* | The valves and the slide along the bottom instead of at the side. |
| **Left-handed (mirror the surface and the strip)** | off · on · default off | Flips the lattice, the strip and the panel; the mirror is handed to every probe. |
| **Quick-switch (the Next pad)** | a subset of the layouts, in order · default none | What the strip's Next pad cycles through; no pad when none is chosen. |
| **Outbound MIDI** | iOS: Virtual source · Network session · Bluetooth. Android: USB-MIDI to the host · Virtual device · Bluetooth advertise | Which sinks carry the Play surface's MPE stream, each under its own rate policy. |
| **Re-sync DAW** | action | Re-sends the MPE configuration (MCM + bend range) and the strip's announce of nine. |
| **Stop all notes (panic)** | action | Releases every held voice, silences every pipe, resets the strip and the fingering. |
| **Storm test / on-device suites** | actions | Evidence tools: a 60 s ten-voice storm, and the headless hostmpe + normalizer suites on the device. |

## Window

*Desktop only.* **Fullscreen** (default off): the canvas fills the display it
is mostly on; ⌃⌘F on macOS or F11 elsewhere toggles it from the canvas.
`--fullscreen` sets the setting for the session and onward.
[Desktop →](../../guide/desktop/#command-line)

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
scripted operator tests, the soaks, the profiles, the storm and the replay
gate's recorder. [Desktop →](../../guide/desktop/)

## Where settings live

| Platform | Store |
|---|---|
| macOS | `~/Library/Application Support/midi-sink/settings.ini`, with `last_session.json`, `presets/` and `replays/` beside it |
| Windows | `%APPDATA%\midi-sink\settings.ini` |
| Linux | `~/.config/midi-sink/settings.ini` |
| Web | the browser's `localStorage` (`sumi-web-settings`, the presets), per origin |
| iOS | `UserDefaults` (the app's preferences); presets, replays and instruments in the app's Documents, visible in Files |
| Android | `SharedPreferences` `sumi`; presets, replays and instruments in the app's files |

The CC map persists as `channel:cc:dimension;…` with an empty value meaning
the default map; the CC-routed sliders persist as their last CC values and
are re-sent when the engine starts; the last session is restored at launch.
