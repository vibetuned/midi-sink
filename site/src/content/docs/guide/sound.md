---
title: Sound
description: The sound engine inside the app — Voxo plays a Decent Sampler instrument, a WAV or the Suzu synth from the same MIDI bytes the visuals draw, on every native platform; where to switch it on, how to load an instrument, what the play surface's Local Control means, and the bus effects the CC map reaches.
---

midi-sink was a visualizer that needed a synth beside it; since Phase 7 it
carries one. **Voxo** is the sound engine: a library beside the core that
receives the identical MIDI bytes — the one producer feeds two rings — and
plays them on the platform's default output. It has three sources: the
**sampler**, which plays [Decent Sampler](../../reference/licensing/)
presets and libraries (or one WAV at a root note, or a plain sine), the
**Suzu** synth, and the two **layered**. The web has no sound core: in the
browser the picture is the whole of it, except in the
[Suzu lab](../../suzu/lab/), where the synth runs alone.

## Switching it on

* **Desktop** — Settings → *Sound* → *Internal sound (Voxo)*. *Volume*,
  *Source*, and under the sampler an *Instrument* path (a `.dspreset`, its
  folder holding the samples, or a `.dslibrary`) with *Load instrument*,
  *Unload* and *Demo instrument*; a *Sample (WAV)* with its *Root note*
  sounds when no instrument is loaded. Under Suzu, every knob of the
  current voice ([patches](../../suzu/patches/)).
* **iPad and Android** — the *Sound* page of the sheet: *Internal sound
  (Voxo)*, *Volume*, *Source* (Sampler, Suzu, Both layered), the *Suzu
  patch* (eleven: the bowed string, the bell and the three winds, and the
  six fitted to the Versilian Community Sample Library's instruments —
  [the patch table](../../suzu/patches/)), the *Synth trace* rows (the
  synth's orbits into the water, the scope on the canvas —
  [the orbit trace](../../suzu/orbit-trace/)), and the sampler's
  instrument: the **Dan Tranh** demo, or one you import — *Import a
  `.dslibrary`…*, or a preset's folder — copied into the app's own
  Instruments folder (Files → On My iPad → midi-sink → Instruments on the
  iPad, where you can also drop them yourself) and never sent anywhere.
  **Foreground only** on the iPad: the sound pauses with the app and
  returns with it.

The Dan Tranh, a Vietnamese zither from the Versilian Community Sample
Library (CC0), is the one instrument bundled, so a first launch makes a
sound; everything else is yours and travels with its own terms —
[the licensing page](../../reference/licensing/).

## Local Control

The play surface's own notes reach Voxo only under **Local Control** (CC
122, the MIDI meaning): *The play surface sounds here* on the Sound page.
Off, your fingers and the pen still go out over MIDI to your DAW and still
paint the water, but do not sound inside — the right setting when the DAW's
synth is the instrument and midi-sink the picture. External instruments
always sound.

## What a library asks for

A Decent Sampler library may ask for features this version does not play —
an effect, a scripted control, a sample format. midi-sink reads what it
reads and says so: the **compat report**, shown once, calmly, when a
library is loaded, lists what it asked for that plays without. The report's
contract is a test in the tree.

## What Voxo consumes

The same stream the visuals read, with its own consumers: velocity, the
per-note bend (the member range), channel pressure, CC 74 (the sampler's
cutoff where the preset maps it; Suzu's filter and the trumpet's
embouchure), CC 1 (the rotor's K), CC 2 and CC 11 (breath — the bow and
the winds' mouth), CC 64 (the sustain pedal, below), CC 120 and CC 123
(the panic), and CC 122 (Local Control). The **bus effects** — a reverb
(*wet*, *room*, *damping*), a delay (*wet*, *time*, *feedback*) and, since
3.0, a chorus (*amount*, *rate*, *depth*) — are targets in the
[CC map](../../reference/settings/#cc-map) from 1000 up, beside the engine's
dimensions, so a controller's knob can ride the room or the shimmer. A
preset's own chorus plays where the format puts it — on the bus, or inside a
group as one chorus per note — and its phaser plays on the bus; both take
the format's `mix`, `modDepth` and `modRate`, the phaser its
`centerFrequency` and `feedback` too.

**The sustain pedal** (3.0) is a position, not a switch. A note released
under the pedal is held and decays at a rate that runs from the preset's
release at the top of the pedal's travel to the group's `sustainDecay` at
the bottom — an attribute of Voxo's own that a preset may declare; without
it the pedal fully down holds the note as it always did, and a continuous
pedal half way doubles the release. Pressing the pedal over a tail that is
still sounding catches it where it is. The Decent Sampler pedal attributes
play as the format documents them: a sample with `onLoCC64`/`onHiCC64`
fires once when the pedal crosses into its range (the thump going down, the
damper lift coming up), `loCC64`/`hiCC64` admit a pedal-down resonance
group only while the pedal is down, and `silencedByTags` chokes an open
hi-hat when the closed one strikes — in five milliseconds by the format's
default, or through its release with `silencingMode="normal"`.

## Latency and load

The device opens at the platform's lowest stable block; the tablets'
latency gates measure touch-to-sound against the phase's record on every
core change. The acceptance test for load is the **storm**: fifteen
channels of dense MPE through a heavy library (and, layered, through the
Suzu chain or the hybrid string) for the length of a piece, with the
visuals at rate — zero XRuns, zero dropped messages, or the step does not
ship.

## Later: Voxo Dorean

Background playback (the iPad's sound is foreground-only, said plainly
above), disk streaming of large libraries (everything decodes to RAM
today, with an advisory gate), and a shell of the sound engine's own are
one later project, **Voxo Dorean** — not 2.0.

## Rendering offline

The desktop bench can render a [replay](../replay/) to a WAV through the
same engine, deviceless (`--replay-wav`): the seed of an offline bounce,
not yet a feature.
