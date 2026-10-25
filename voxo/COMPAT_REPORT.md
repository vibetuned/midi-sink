# The compat report — its copy

What Voxo says when a Decent Sampler preset asks for something this version
plays without (SOUND §3: "compatibility is a dialog, not a crash or a silent
wrong sound" — calm, and once per load). These sentences ARE the library's
(`voxo_note_copy`); `tests/voxo_preset_tests.cpp` asserts the two never drift,
and the documentation (step 63) quotes this file.

The first line of every report is the preset's summary:

    <name>: N zones in G groups, S samples, M MB in memory.

Then, in this order, only the lines that apply:

| note | sentence |
|---|---|
| `VOXO_NOTE_MEMORY` | Memory: this library is larger than advised for this device; it is loaded anyway. |
| `VOXO_NOTE_MISSING_SAMPLES` | Samples that could not be read: those notes stay silent. |
| `VOXO_NOTE_STREAMING` | Disk streaming: this preset asks for it; everything is loaded to memory instead. |
| `VOXO_NOTE_CONVOLUTION` | Convolution reverb: this preset uses it; it will play without it. |
| `VOXO_NOTE_UNKNOWN_EFFECT` | An effect this version does not know: it will play without it. |
| `VOXO_NOTE_SEQUENCES` | Note sequences: this preset uses them; it will play without them. |
| `VOXO_NOTE_UNKNOWN_BINDING` | Bindings this version does not know: they are ignored. |
| `VOXO_NOTE_UI` | Custom interface: not shown here; its controls' starting values apply. |

The memory line adds `(about N MB against M MB advised)` — the decoded size
read off the sample headers before decoding, against the shell's advice (the
desktop's free memory with headroom); the missing-samples line adds
`(n of N zones; first: <path> (<reason>))`; the unknown-effect and
unknown-binding lines add the names in parentheses.

When a file is not a preset at all the load is refused and the text says why:
`not well-formed XML (<pugixml's description> at byte <n>)`,
`not a Decent Sampler preset (no <DecentSampler> root)`, `the file is empty`,
`cannot open the file`, `cannot open the library archive`,
`the library archive holds no .dspreset`.

## The modulator matrix, legato and glide play since 3.0 (step 90, `DECISIONS_14 #4`)

The modulators sentence is retired: `VOXO_NOTE_MODULATORS` stays in the ABI,
never raised, its copy empty. `<lfo>` plays (`shape` sine | triangle | square |
saw, `frequency` in Hz or — under `frequencyFormat="musical_time"` — the guide's
subdivision index on the host's tempo, `delayTime`, `modAmount`, `scope` global |
voice, `modBehavior` set | add | multiply | modulate, `enabled`) and
`<envelope>` plays with its `delay` and `hold` stages beside the ADSR; every
binding under either is a target: `AMP_VOLUME`, `PAN`, `GROUP_TUNING` /
`GLOBAL_TUNING`, `FX_FILTER_FREQUENCY` / `FX_FILTER_Q`, a group effect's
parameters, the bus effects' parameters and the EQ stages' — eight modulators
of eight targets at most. `glideTime` and `glideMode` (legato, always, off) on
the groups and the samples, `polyphony` (1 = monophonic), `trigger="first"` /
`"legato"`, `previousNotes` and `legatoInterval` play as the guide documents
them. A modulator element this version does not know is named with the unknown
bindings. The table above is what remains: the housekeeping lines and the four
features genuinely out of scope — the custom interface, disk streaming,
convolution reverb and the note sequences.

## The filter family, the filter envelope, the EQ, the gain and the wave shaper play since 3.0 (step 89, `DECISIONS_14 #3`)

The "EQ and other filters" sentence is retired: `VOXO_NOTE_OTHER_FILTERS` stays
in the ABI, never raised, its copy empty. The voice's filter is any of
`lowpass` (`lowpass_4pl` its legacy name), `lowpass_1pl`, `highpass`,
`bandpass` and, at the group level, `notch` — the group's first enabled one,
else the instrument's — with the format's `frequency` and `resonance` (the Q;
`Q`/`q` on the notch). `<effect type="peak">`, `low_shelf`, `high_shelf` and an
instrument-level `notch` are the bus EQ, in their listed order, eight stages at
most, with `frequency`, `Q`, `gain` and `gainUnit`. `<effect type="gain">` plays
(`level` in dB by default, "-6dB" allowed, `levelUnit="linear"`; at the group
level folded into the group's gain). `<effect type="wave_shaper">` plays at the
bus and at the group level (`drive`, `driveBoost`, `outputLevel`; `highQuality`
is read and ignored). The bindings `FX_FILTER_FREQUENCY`, `FX_FILTER_Q` /
`FX_FILTER_RESONANCE` and `FX_FILTER_GAIN` resolve to the voice filter or to the
EQ stage they point at; `FX_DRIVE`, `FX_DRIVE_BOOST`, `FX_OUTPUT_LEVEL` to the
shaper. A `<modulators><envelope>` bound to a filter's frequency or Q plays as
the group's filter envelope (one a group, per voice: `attack`, `decay`,
`sustain`, `release`, `modAmount`, `modBehavior` set | add | multiply); the
modulators sentence names the LFOs and the sequences from here. A group-level
`peak` or shelf (outside the format) is passed over silently.

## The chorus and the phaser play since 3.0 (step 88, `DECISIONS_14 #2`)

The chorus sentence is retired: `VOXO_NOTE_CHORUS` stays in the ABI, never
raised, its copy empty. `<effect type="chorus">` plays at the instrument level
(the bus) and at the group level (one instance a note, as the format has it) with
the format's `mix`, `modDepth`, `modRate`; `<effect type="phaser">` plays on the
bus with `mix`, `modDepth`, `modRate`, `centerFrequency`, `feedback` (the format
admits no group-level phaser). A binding on `FX_MIX`, `FX_MOD_DEPTH`,
`FX_MOD_RATE`, `FX_CENTER_FREQUENCY` or `FX_FEEDBACK` resolves to the effect it
points at (`position` / `effectIndex`); `enabled="false"` bypasses any effect.
The shell's CC map gains `VOXO_CTL_CHORUS_MIX` 1006, `_RATE` 1007, `_DEPTH` 1008.

## What the pedal plays since 3.0 (step 87, `DECISIONS_14 #1`)

No sentence above covered the pedal's attributes — a pedal-noise sample was
silently a note zone, a choke tag was ignored — so none is retired. Since
Voxo 0.27.0 the sampler honours, as the Decent Sampler guide documents them:
`onLoCCN` / `onHiCCN` on a sample or group (the CC number in the attribute
name — a sample that fires when that CC enters its range, once per crossing,
as a voice of its own, no note; `pitchKeyTrack="0"` plays it at its root),
`loCCN` / `hiCCN` (a note-on plays the zone only with that CC's last value in
range — the pedal-down resonance), `silencedByTags` with `silencingMode` fast
(the default, a 5 ms fade) or normal (the release phase) and `silencingDecay`,
`pitchKeyTrack`; and Voxo's own `sustainDecay` (seconds: the decay of a note
released under the full pedal; absent, the pedal holds). CC 64 is a position:
between up and down the decay rate interpolates (the half-pedal), and the
pedal pressed over a sounding tail catches it (re-pedalling). Still not played:
`trigger="continuous"`, `previousNotes` / `legatoInterval` (step 90),
`releaseTriggerDecay`, tag polyphony limits.
