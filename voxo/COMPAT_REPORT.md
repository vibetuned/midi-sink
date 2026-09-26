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
| `VOXO_NOTE_MISSING_SAMPLES` | Samples that could not be read: those notes stay silent. |
| `VOXO_NOTE_STREAMING` | Disk streaming: this preset asks for it; everything is loaded to memory instead. |
| `VOXO_NOTE_CHORUS` | Chorus: this preset uses it; it will play without it. |
| `VOXO_NOTE_CONVOLUTION` | Convolution reverb: this preset uses it; it will play without it. |
| `VOXO_NOTE_OTHER_FILTERS` | EQ and other filters (peak, notch, high-pass, band-pass): this preset uses them; it will play without them. |
| `VOXO_NOTE_UNKNOWN_EFFECT` | An effect this version does not know: it will play without it. |
| `VOXO_NOTE_MODULATORS` | Modulators (LFOs, envelopes, sequences): this preset uses them; it will play without them. |
| `VOXO_NOTE_SEQUENCES` | Note sequences: this preset uses them; it will play without them. |
| `VOXO_NOTE_UNKNOWN_BINDING` | Bindings this version does not know: they are ignored. |
| `VOXO_NOTE_UI` | Custom interface: not shown here; its controls' starting values apply. |

The missing-samples line adds `(n of N zones; first: <path> (<reason>))`; the
unknown-effect and unknown-binding lines add the names in parentheses.

When a file is not a preset at all the load is refused and the text says why:
`not well-formed XML (<pugixml's description> at byte <n>)`,
`not a Decent Sampler preset (no <DecentSampler> root)`, `the file is empty`,
`cannot open the file`, `cannot open the library archive`,
`the library archive holds no .dspreset`.
