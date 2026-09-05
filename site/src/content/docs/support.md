---
title: Support
description: How to get help with midi-sink, report a bug, and what to include so it can be fixed.
---

**Stable URL:** `https://midi-sink.vibetuned.com/support/` — referenced by the
store listings; it will not change.

## Where to ask

* **Bugs and feature requests:**
  [github.com/vibetuned/midi-sink/issues](https://github.com/vibetuned/midi-sink/issues).
  Search first — the issue tracker is also the beta wave's confusion log.
* **Email:** [info@vibetuned.com](mailto:info@vibetuned.com) for anything that
  should not be public (a device you cannot name, a recording you would
  rather not post).
* **Discussions, performances, "is this expected?":** open an issue with the
  `question` label, or send a link to a recording — the
  [gallery](../gallery/) grows from what people play.

## What to include in a bug report

1. **Platform and version.** The version is in Settings → About on every
   platform (it comes from the release tag, e.g. `1.0.0`). Say which app:
   macOS, Windows, Linux, iPad, Android, or the web canvas and its browser.
2. **The instrument** and how it is connected (USB, network session,
   Bluetooth), or "fingers/pencil" for the Play surface.
3. **What you did, what you saw, what you expected.** A short screen
   recording beats a paragraph.
4. **For MIDI problems on the tablets:** Settings → Evidence flushes the
   byte log (`midi_log.csv`) and the session log into the app's documents
   folder. Attach them — they contain only MIDI messages and timestamps, no
   personal data — and the analysers in `tools/` will tell us what went wrong
   in seconds. The [MIDI implementation chart](../reference/midi-chart/) is
   the contract they are checked against.
5. **For visual problems on desktop:** run with `--dev` and use
   `--field-dump` if asked; the cross-backend field regression usually
   localises a rendering difference to one backend in one run.

## Known limits, before you file

* Play mode exists on the chromatic grid, Jankó and the piano grid only; the
  circle of fifths and the piano rolls are Marble-only by design.
* The web canvas is Marble mode only and needs WebGPU (Chrome, Edge, Safari 26,
  Firefox 141+) in a secure context — `https://` or `localhost`.
* Many MPE synths ignore polyphonic key pressure (0xA0); the Play surface's
  "pull back to stir" axis is primarily a visualizer dimension.
* Prints are what touches the water: the live ripple shimmer is not in them,
  on purpose.

The [changelog](../notes/changelog/) lists what each release fixed; the
[design notes](../notes/decisions/part-1/) explain why things are the way
they are.
