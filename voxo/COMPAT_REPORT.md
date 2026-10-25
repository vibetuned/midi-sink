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
| `VOXO_NOTE_OTHER_FILTERS` | EQ and other filters (peak, notch, high-pass, band-pass): this preset uses them; it will play without them. |
| `VOXO_NOTE_UNKNOWN_EFFECT` | An effect this version does not know: it will play without it. |
| `VOXO_NOTE_MODULATORS` | Modulators (LFOs, envelopes, sequences): this preset uses them; it will play without them. |
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
