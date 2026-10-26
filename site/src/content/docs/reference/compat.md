---
title: Decent Sampler compatibility
description: What Voxo plays of the Decent Sampler format at 3.0 and what it says when a preset asks for something it plays without — the compat report's eight sentences, the features that landed with Voxo DS 2.0, and the four genuinely out of scope.
---

Voxo plays Decent Sampler presets and libraries — the `.dspreset` XML and
the `.dslibrary` archive — from the same MIDI bytes the visuals draw. When a
preset asks for something this version plays without, the load does not fail
and nothing plays wrong in silence: the shell shows **the compat report**,
once per load, calm, in the sentences below. They are the library's own
(`voxo_note_copy`); `voxo/COMPAT_REPORT.md` is their copy in the tree and the
headless suite asserts the two never drift.

The first line of every report is the preset's summary:

```
<name>: N zones in G groups, S samples, M MB in memory.
```

Then, in this order, only the lines that apply:

| the note | the sentence |
|---|---|
| memory | Memory: this library is larger than advised for this device; it is loaded anyway. |
| missing samples | Samples that could not be read: those notes stay silent. |
| streaming | Disk streaming: this preset asks for it; everything is loaded to memory instead. |
| convolution | Convolution reverb: this preset uses it; it will play without it. |
| an unknown effect | An effect this version does not know: it will play without it. |
| sequences | Note sequences: this preset uses them; it will play without them. |
| unknown bindings | Bindings this version does not know: they are ignored. |
| the custom UI | Custom interface: not shown here; its controls' starting values apply. |

The memory line adds the decoded size against the shell's advice (the
desktop's free memory with headroom); the missing-samples line adds the
count and the first path with its reason; the unknown-effect and
unknown-binding lines add the names in parentheses. A file that is not a
preset at all is refused, and the text says why: not well-formed XML (with
pugixml's description and the byte), no `<DecentSampler>` root, an empty
file, a file or an archive that cannot be opened, an archive holding no
preset.

## What plays at 3.0

Voxo DS 2.0 — the format coverage's second generation, Phase 15 of the v3.0
arc — read the format's own developer guide before each mechanism and
retired three of the report's sentences; the eight above are what remains:
the housekeeping lines and the four features genuinely out of scope.

**The sustain pedal** (Voxo 0.27.0). CC 64 is a position: between up and down
the release rate interpolates (the half-pedal), a note released under the
full pedal decays at Voxo's own `sustainDecay` (absent, the pedal holds, bit
for bit the first release's pedal), and the pedal pressed over a sounding
tail catches it at its level (re-pedalling). Pedal noises fire once per
crossing as voices of their own — `onLoCCN` / `onHiCCN` on a sample or group,
the CC number in the attribute name, `pitchKeyTrack="0"` playing them at the
root; `loCCN` / `hiCCN` plays a zone only with that CC's last value in range
(the pedal-down resonance); `silencedByTags` with `silencingMode` fast (the
default, a 5 ms fade) or normal (the release phase) and `silencingDecay` is the
choke, run before the new layers start.

**The chorus and the phaser** (0.28.0). `<effect type="chorus">` plays at the
instrument level and at the group level (one instance a note, as the format
has it) with `mix`, `modDepth`, `modRate`; `<effect type="phaser">` on the bus
with `mix`, `modDepth`, `modRate`, `centerFrequency`, `feedback`. A binding on
`FX_MIX`, `FX_MOD_DEPTH`, `FX_MOD_RATE`, `FX_CENTER_FREQUENCY` or `FX_FEEDBACK`
resolves to the effect it points at; `enabled="false"` bypasses any effect.
The shell's CC map gains the chorus's mix, rate and depth (targets 1006–1008).

**The filter family, the filter envelope, the EQ, the gain and the wave
shaper** (0.29.0). The voice's filter is any of `lowpass` (`lowpass_4pl` its
legacy name), `lowpass_1pl`, `highpass`, `bandpass` and, at the group level,
`notch` — with the format's `frequency` and `resonance`, which **is the Q**
(the first release read it as an amount: every lowpass preset sits 3 dB
brighter at its cutoff since). `peak`, `low_shelf`, `high_shelf` and an
instrument-level `notch` are the bus EQ, in their listed order, eight stages
at most; `gain` plays in dB (`"-6dB"` allowed, `levelUnit="linear"`);
`wave_shaper` plays at the bus and the group level with `drive`, `driveBoost`,
`outputLevel`. An `<envelope>` bound to a filter's frequency or Q is the
group's filter envelope.

**The modulator matrix, legato and glide** (0.30.0). `<lfo>` plays — `shape`
sine, triangle, square or saw; `frequency` in hertz or, under
`frequencyFormat="musical_time"`, the guide's subdivision index on the
session's tempo; `delayTime`, `modAmount`, `scope` global or voice,
`modBehavior` set, add, multiply or modulate — and `<envelope>` with its
`delay` and `hold` stages beside the ADSR. Every binding under either is a
target: the volume, the pan, the group and global tuning, the voice filter's
frequency and Q, a group effect's parameters, the bus effects' and the EQ
stages' — eight modulators of eight targets at most. `glideTime` and
`glideMode` (legato, always, off), `polyphony`, `trigger="first"` /
`"legato"`, `previousNotes` and `legatoInterval` play as the guide documents
them.

**The instrument slot** (0.31.0). A session preset and a recording's header
carry the library the sound played — by name and by the hash of its preset
XML, with the path as a hint, never embedded; each shell resolves it against
its own libraries and says when none is there. **The offline bounce** renders
a recording to a 32-bit float WAV through the same engine, deterministically;
**the output device** is chosen in the desktop's Sound settings.

## Out of scope

Still not played, and said so in the report: disk streaming (everything is
loaded to memory), convolution reverb, the note sequences and the custom
interface. Also not played, silently: `trigger="continuous"`,
`releaseTriggerDecay`, tag polyphony limits, and a group-level `peak` or shelf
(outside the format). The record of each decision is the decision log's
Part XIV (`DECISIONS_14 #1`–`#5`); the Sound guide says how to load a
library on each platform.
