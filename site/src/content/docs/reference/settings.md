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
| **Pitch layout** | the fourteen: Circle of fifths · Chromatic grid · Jankó · Piano roll (left, top, right, bottom) · Piano grid · Trumpet · Trombone · Wicki-Hayden · Strings · Theremin · Boehm flute (v3.0) · default *Circle of fifths* | Where a note lands on the sheet, and the lattice Play mode touches; the tablets mark the nine keyed ones *(playable)*. [Layouts →](../../guide/layouts/) · [The instruments →](../../guide/instruments/) | params `pitch_layout` |
| **Partials on an arc** | off · on · default off · *under the trumpet, the trombone and the Boehm flute (there "Registers on an arc")* | The brass partials — or the flute's three registers — on a ring (radius 0.30, centred right of the middle at 0.65 of the height) instead of a column. | params `trumpet_arc` |
| **Split surface (a layout per half)** | off · on · default off | v3.0 (step 77): two layouts side by side, a hand each — the pitch layout on the left half at half width, the layout below on the right; both must be playable layouts, or the split is off. [The split surface →](../../guide/instruments/#the-split-surface) | params `split_mode` |
| **Right half** | the fourteen · default *Chromatic grid* · *under the split* | The right half's layout. | params `split_layout` |
| **Split note** | 0–127 · default C4 (60) · *under the split* | The first note of the right half: the left plays the notes below it, the right the notes from it — a wire note's home, and the probe refuses a cell across the split. | params `split_note` |
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
| **Glow reading** | plasma · spark · default *plasma* · *Anod* | Plasma: every strained texel glows by its strain. Spark (v3.0): only the ridges of strain — a leader where strain is being added, an afterglow where it was, the bloom on the leader alone — and the strike's SHAPE follows it: the plasma strike is 2.0's blob episode, the spark strike that same episode with the [leader stroke](../../operators/leader/) struck beside it, a jagged tree torn from the key. [The spark reading →](../../operators/anod/) | params `anod_glow_mode` |
| **Spark afterglow** | 0.05 – 5 s · default 0.6 · *Anod, spark* | The strain buffer's time constant: how long a channel stays hot once nothing strains it. Under Kessho the same constant is the exposure Layer A reads. | params `spark_afterglow` |

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
| **Medium** | Sumi (ink on washi) · Anod (strain-glow) · Kessho (crystal) · default *Sumi*; one line per medium | The reading of the field; a switch is live and bitwise. [The medium →](../../guide/medium/) | params `medium` |
| **Exposure** (Kessho) | 200–8000, default 2000 (one stop) | Scales the mineral's retardation: at 2000 a unit of strain is worth the mineral's own; smaller climbs the Michel-Lévy orders sooner. | params `kessho_retard` |
| **Backlight** (Kessho) | 0–2, default 0.85 | The lamp behind the crystal; the birefringence layer multiplies over it. | params `kessho_backlight` |
| **Regime tint** (Kessho) | 0–1, default 0.25 | Layer B's tint by the trace discriminant: cool where the flow is elliptic, warm where it is hyperbolic. | params `kessho_tint` |
| **Caustic** (Kessho) | 0–2, default 0.5 | The fold of a bed under the water; the mod wheel (or a CC on the caustic depth) sets the depth. | params `kessho_caustic` |
| **Polarizer** (Kessho) | One analyzer · Per voice · default *One analyzer* | One angle for the whole sheet (the latest slide or a CC), or each held note's slide turning its own analyzer, the sheet blending the nearest four. | params `polarizer_mode` |
| **Braid vortices** (Kessho) | Pinned to the keys · Free (Kirchhoff) · default *Pinned* | Where a chord's vortices sit: at their keys, the ink braiding between them; or moving under each other's flow, consonant notes orbiting, dissonant ones travelling as a pair (the A/B item B-1). | params `braid_mode` |

### Mineral *(Kessho only; replaces the Palette section — v3.0, libsumi 1.7.0)*

| Setting | Values | What it does | Where it lands |
|---|---|---|---|
| **Mineral** | Quartz · Calcite · Muscovite · Barite · Fluorite · Selenite · Edited | The library: picking one sets both halves below; editing any field keeps your version as *Edited*. [The minerals →](../../operators/minerals/) | `sumi_set_mineral`; the INI's `mineral_*` keys; the session's `mineral` block |
| **Optic class** | Isotropic · Uniaxial · Biaxial (+ **2V**, 0–90°) | No birefringence; the Maltese cross; the cross split into two brushes 2V apart. | `optic_class`, `two_v` |
| **Retardation** | 100–200000 nm / strain | The mineral's retardation at unit strain (its Δn under the exposure). | `retardation` |
| **Sensitive tint** | Off · 1–2200 nm | The full-wave plate fixed to the sheet: the sign of a shear as blue or yellow. | `retardation_bias` |
| **Walk-off** | Off · 0–32 texels | Calcite's double image along the strain's axis, closed on the dark arms. | `walkoff` |
| **Dispersion** | Off · 0–1 | Three caustic depths: a rainbow on the fold. | `dispersion` |
| **Conical ring** | Off · 0–16 texels | Hamilton's hollow cone on the fold. | `conical_ring` |
| **Thin film** / **Film index** | Off · 0–0.95; 1–3 | Airy's interference colours over the ink rings (mica's pearl). | `film_r`, `film_n` |
| **Body** | a colour | The backlight through the crystal's body: the substrate's tone and the light's cast. | `body_tint` |
| **Bloom gain** | 0–4 | Only the fold and the regimes bloom, never the fringes; the Anod section's bloom strength scales it. | `bloom_gain` |
| **Chart** | Michel-Lévy (physical) · Custom palette, by order | Which table the retardation reads. | `lut_id` |
| **Glow scale** | *Anod* | The strain a texel needs to glow: smaller is hotter, sooner. | params `anod_glow` |
| **Grid lines** | lines per canvas height · 0 = glass · *Anod* | The water's deformed grid at rest. | params `anod_pitch` |
| **Strike charge** | 0.1 – 1 · default 0.57 · *Anod* | The spark's size as a fraction of the Sumi drop's radius. | params `anod_drop` |

## Drop & tine shape

The shape of a strike and of a stroke, for every medium — the drop and the tine are every medium's operators (the two rows sat under Kessho's Mineral panel until 2026-10-18; a mineral preset still pairs a habit and a profile with its optics, and editing either marks the mineral *Edited*).

| Setting | Values | What it does | Stored as |
|---|---|---|---|
| **Drop habit** | Circle · Rhomb · Hexagon · Rosette (+ **Roundness** 0–1, **Blades** 2–12) | The shape a strike takes — exact for any shape at any size; the rings inside homothetic, a held note growing it as the same shape; 0 roundness creases the corners. | params `drop_habit`, `habit_roundness`, `habit_blades` |
| **Tine profile** | Smooth (Jaffer) · Twin kink (Baumhauer) · Terrace (slip) | How a stroke shears the sheet; every profile a bump, zero beyond four alpha; the glide and the master bend inherit it. | params `tine_profile` |

## Expression routing

Each MIDI dimension has exactly one consumer at a time; these rows choose it.
*Medium default* follows the medium's table (Sumi: glide, ink feed, hue;
Anod: the Chladni stir, torsion, the spark's frequency).
[The Operators →](../../operators/)

| Setting | Choices · default | What it does | Reaches the core as |
|---|---|---|---|
| **Input** | MPE · Classic keyboard · Wind · default *MPE* | The MIDI dialect. MPE: per-note expression on the member channels. Classic: per-note voices on any channel, bend = global shear. Wind: one voice played as MPE plus a wake dragging the sounding drop to the next note on legato. A setting, never a detection. [Devices →](../../guide/devices/) | `sumi_set_input_mode` |
| **Per-note bend** | Medium default · Glide (drag the drop) · Ripple amplitude · Torsion wavelength · Spark frequency · Chladni stir · Braid consonance · default *Medium default* | What a note's pitch bend plays: the drop along the pitch axis; the sine ripple's amplitude; the torsion's $k$; the spark's $k$; the Chladni stir (distance the rate, sign the sense); (v3.0) the braid's consonance — the drop holds, the bend moves the interval against the bass and so the note's circulation; Kessho's default. Master bend keeps its shear tine either way. [The braid →](../../operators/braid/) | params `bend_mode` (0–5; `SUMI_MODE_MEDIUM_DEFAULT`) |
| **Channel pressure** | Medium default · Ink feed · Lamb-Oseen swirl · Torsion sweep feed · Arnol'd tongue lock · default *Medium default* | What hardware aftertouch (0xD0) plays: the drop's growth, a swirl at the note, torsion spent around the note as a rate, or (v3.0) the Arnol'd lock — the press is the kick, the interval against the lowest held note the twist; Kessho's default. Poly pressure (0xA0) always swirls under Sumi; under Kessho it is the note's chord wave, baked as deltas. [The lock →](../../operators/arnold/), [the chord wave →](../../operators/chord-wave/) | params `press_mode` |
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
| **Suzu patch** | the table's seventeen: Bowed string · Bell · Flute · Saxophone · Trumpet · Dan Tranh, Glockenspiel, Tubular bells, Concert harp, Tenor sax, Baroque recorder (VCSL) · Drum · Gong · Harp on a soundboard · Bowed string (stick-slip) · Bowed chain · Unison pair; **Import a Suzu patch (.json)** (v3.0 step 82: a preset file's Suzu block, the editable table inside, over the picked patch; *tablets*) | The synth's patch — the whole setting on the tablets; *Load patch* puts it into the desktop's knobs. [The patch table →](../../suzu/patches/) |
| **Synth trace** · *Draw the orbits into the water* · *Scope on the canvas* · *Trace scale* | off · on; Off · Over the water · Alone; 0.05 – 2 · defaults off, off, 0.25 · *tablets, when the synth is the source* | The orbit trace's two routes and its scale in canvas heights per unit orbit amplitude (0.25 suits the rotor; the modal voices and the winds want more) — [the orbit trace](../../suzu/orbit-trace/). |
| **Suzu voice** and its knobs | *desktop* | Every parameter of the current voice kind, *Suzu attack (s)* among them — [patches](../../suzu/patches/). |
| **Suzu oversample** | ×2 (two-tap average) · ×4 (half-band) · default ×2 · *desktop, the cell and the lattice* | v3.0 (step 78): how many times the cell family steps for each output sample. ×2 is the shipped section, bit for bit; ×4 runs the cells, the shears and the filter at four times the rate and decimates through two half-band stages (64 dB of stopband) — the fold's corners without their aliases, at twice the cost. The chains, the bores and the chaos voices keep ×2. | suzu `oversample` |
| **Suzu drum size** · **drum stiffness** · **gong swell** · **strike cap** | 4..16 · 0..1 · 0..4 · 0.25..4 · defaults 16, 0.02, 0, 1 · *desktop, the membrane* | v3.0 (step 79): the membrane's grid (nodes a side at most; cut per note under the CFL gate), the biharmonic share of the fundamental's stiffness (0 a membrane, 1 a plate), the von Kármán swell at the cap's stretching, and the strike-energy cap in full strikes; *Suzu decay (s)*, *decay brightness*, *pluck position* and *pickup* are shared with the lattice and the strings. | suzu `drum_size`, `drum_kappa`, `drum_alpha`, `drum_cap` |
| **Suzu bus plate** · **plate size** · **plate pitch (Hz)** · **plate stiffness** · **plate decay (s)** · **plate brightness** · **plate gain** | off/on · 16, 32, 64 · 20..2000 · 0..1 · 0..30 · 0..1 · 0..4 · defaults off, 64, 110, 0.003, 2, 0.02, 1 · *desktop; the tablets through a patch* | v3.0 (step 79): one membrane on the bus at the output rate — every Suzu voice feeds its sample into the plate at its point (a play surface's layout cell; the desktop's default map) and the plate's two pickups join the stereo sum before the reverb. The pitch and the stiffness are clamped under the CFL gate for the size (the log line says what it took); the tablets cap the size at 32. | suzu `plate_on`, `plate_size`, `plate_hz`, `plate_kappa`, `plate_decay_s`, `plate_bright`, `plate_gain` |
| **Suzu bow** · **bow force** · **bow speed** | energy servo · stick-slip (Helmholtz) · 0..4 · 0.25..4 · defaults servo, 0.5, 1 · *desktop, the lattice and the strings* | v3.0 (step 80): the bow's mode — the energy servo of 2.0, or the stick-slip friction curve on the string's bowing point (*Suzu bow position*): the mouth is the bow's speed, the press and the velocity its force. Under the playable range the string whistles, over it the pitch pulls flat and the tone growls. | suzu `bow_mode`, `bow_force`, `bow_speed` |
| **Suzu toneholes** · **hole size** · **hole loss** · **reed inertance** · **bore leak** · **brass shock** · **lip shear** · **lip shear ratio** | bore cut per note · the holes (fingering keys); 0.1..4; 0..2; 0..30; 0..1; 0..4; 0..1; 0.25..4 · defaults off, 1, 0.3, 0, 0.4, 1, 0, 1.5 · *desktop, the winds* | v3.0 (step 81): the toneholes on the flute and the sax (the bore cut for the register's lowest note, the Boehm layout's nine keys on CC 110–112 and 114–119 opening the holes that sound the note; each hole tuned when the patch loads — a moment's pause, the sax's blown), the hole's size against a flute's and its radiation loss; the reed channel's air mass (the sax); the resistance on the sax's bell's slow flow that takes the quasi-periodic A3 (0 is the red); the trumpet's convective steepening with its flux limiter (0 the 2.0 bore); the lips' second, shearing mode and its resonance ratio (0 the 2.0 lips). [The winds →](../../suzu/winds/) |
| **Suzu preset** = **table (editable)** · the sixteen rows (ratio · T60 · weight) · **Start from the preset's table** · **Coupling compensated** | the five formulas or the table · ratio > 0, seconds, weight (0 = the formula's) · on/off · default on · *desktop, the lattice* | v3.0 (step 82): the lattice's ratios, decays and strike weights as you state them — a fitted instrument's measurement instead of the nearest formula; the table rides in the preset file (schema 2). Off, the compensation leaves the raw lattice: close pairs split and beat (the Unison pair), and the swirl adds nothing to the coupling. [The modal voice →](../../suzu/modal-voice/) |
| **Suzu Toda nodes** · **Toda nonlinearity** · **Toda cap** · **Toda pulse width** | 4..64 · 0..1 · 1..12 · 0.5..4 · defaults 16, 0.6, 6, 1.5 · *desktop, the Toda ring* | v3.0 (step 83): the [Toda ring](../../suzu/toda-ring/)'s masses (fewer at high notes under the leapfrog's bound), the exponential's steepness at full press (the press and CC 74 above centre set it: 0 the linear ring, 1 the sharpest train), the compression's soft ceiling (ln of the exponential's; held under the stiffness bound at the note), and the strike's pulse width in nodes. *Suzu decay (s)* and *pickup* are shared. The ring's rate and the nonlinearity's ceiling for the note follow the resolution rule on their own. | suzu `toda_nodes`, `toda_b`, `toda_cap`, `toda_width` |
| **Suzu field at full poly pressure (Hz)** · **field at rest (Hz)** | 0..50 · 0..50 · defaults 24, 2 · *desktop, the Boris gyroscope* | v3.0 (step 84): the [gyroscope](../../suzu/gyroscope/)'s magnetic field — the cyclotron frequency the poly pressure reaches, and the field with no pressure (0 is a mono line until the finger presses sideways). The field is the Zeeman pair's split and the rosette's precession rate. *Suzu decay (s)* is shared; *Suzu pluck position* is this voice's pan (0.5 the diagonal, mono). | suzu `boris_b`, `boris_b_rest` |
| **Suzu network masses** · **bridge (Hz)** · **bridge mass** · **bridge decay (s)** · **rattle stop** · **rattle loss** | 2..16 · 50..5000 · 0.05..10 · 0..2 · 0..10 · 0..1 · defaults 8, 700, 0.5, 0.5, 2, 0.3 · *desktop, the Dirac network* | v3.0 (step 85): the [prepared network](../../suzu/dirac-network/)'s chain, its bridge's own mode (a body in the range, where the bow sings), mass and loss (its radiation), the rattle stop's distance under the bridge in the pluck's static deflection (0 removes it) and the share of the bridge's momentum it takes at each hit. *Suzu bow*, *bow force*, *bow speed* and *bow position* drive it as a port; *Suzu decay (s)* and *pluck position* are shared. | suzu `dirac_masses`, `dirac_bridge_hz`, `dirac_bridge_mass`, `dirac_bridge_decay`, `dirac_stop`, `dirac_stop_loss` |
| **Suzu billiard drive** · **billiard aspect** · **stadium length** · **stadium length at rest** | 0.1..4 · 0.5..2 · 0..4 · 0..4 · defaults 1.4, 1, 2, 0 · *desktop, the billiard* | v3.0 (step 85): the [billiard](../../suzu/billiard/)'s kick in units of the wall (times the velocity gain: past 1 the fold, which raises the bounce), the end ellipses' aspect at CC 74 centre (CC 74 scales it), and the straight walls at full mod wheel and with the wheel down (0 the circle). *Suzu decay (s)* is shared; *pluck position* pans the kick. | suzu `bill_drive`, `bill_aspect`, `bill_length`, `bill_length_rest` |
| **Suzu mod matrix** — **Slot 1–4 source** · **target** · **depth**; **Suzu modulator rate** | source: none, chaotic modulator, press, swirl, slide (CC 74), mod wheel, breath, velocity · target: none, cutoff (±3 oct), coupling (+0.5), rotor K (+1), drive (×), level (×), pitch (±1 semitone) · depth −1..1 · rate 0.1..4 · defaults none, 1 · *desktop* | v3.0 (step 86): four routings summed per target every block; the chaotic modulator is a double pendulum per voice, its energy the voice's strike. The pre-3.0 *Suzu modulator* row became slot 1. [The modulator →](../../suzu/strings-and-chaos/#the-chaotic-modulator-and-the-mod-matrix) | suzu `mod_src`, `mod_dst`, `mod_amt` (arrays of four), `mod_rate`; the legacy `mod_target`, `mod_depth` |
| **Tuning (.scl)** · **Keyboard mapping (.kbm)** · **Load tuning** · **Equal temperament**; the tablets' **Import a Scala scale (.scl)** | a Scala scale file and its optional mapping · default none · *desktop; tablets through the picker* | v3.0 (step 82): retunes every note Voxo sounds — the synth and the sampler — at the one place a note becomes a frequency, the bend an equal-tempered offset on it; without a mapping the scale's first degree is MIDI 60 and MIDI 69 stays 440 Hz. The play surface's geometry is unchanged. A malformed file is refused with a reason in the log. |

## Suzu trace

*Desktop.* The [orbit trace](../../suzu/orbit-trace/): *ink (the gesture
route)*, *scope*, *this voice kind*, *scale*, *segments* (4 – 8), *stroke*
(tine, wake), *on the canvas* (off, over the water, the scope alone), the
*mode* (continuous; the Poincaré strobe on the bass phasor; the strobe on
each voice's own drive clock — v3.0), and the miniature scope.

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
| **Outbound MIDI** | iOS: Virtual source · Network session · Bluetooth. Android: USB-MIDI to the host · Virtual device · Bluetooth advertise · Network session (RTP-MIDI) over Wi-Fi | Which sinks carry the Play surface's MPE stream, each under its own rate policy. The Android network session is the app's own RTP-MIDI endpoint, advertised over mDNS for Audio MIDI Setup and rtpMIDI to invite; a status line names the host connected. |
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
