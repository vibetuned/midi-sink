---
title: Support
description: How to get help with midi-sink, report a bug, and what to include so it can be fixed.
---

**Stable URL:** `https://midi-sink.vibetuned.com/support/` — referenced by the
store listings; it will not change.

## The apps

* **iPad:** [midi-sink on the App Store](https://apps.apple.com/us/app/midi-sink/id6810793641)
  (iPadOS and iOS 16 or later).
* **Android:** [midi-sink on Google Play](https://play.google.com/store/apps/details?id=com.vibetuned.midisink)
  (Android 10 or later, OpenGL ES 3).
* **macOS, Windows, Linux and the browser:** the [install page](../guide/install/).

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

* Play mode exists on the eight keyed layouts and the theremin; the circle
  of fifths and the piano rolls are Marble-only by design.
* On the Galaxy Tab the S Pen's palm rejection drops the fingers while the
  pen is near the glass, so the brass layouts' valves and slide cannot be
  worked by a finger while the pen plays — a platform limit the iPad does
  not have ([the instruments](../guide/instruments/)).
* The web canvas is Marble mode only — the instruments are overlays, a
  replay there is the picture alone — and needs WebGPU (Chrome, Edge,
  Safari 26, Firefox 141+) in a secure context — `https://` or `localhost`.
* Many MPE synths ignore polyphonic key pressure (0xA0); the Play surface's
  "pull back to stir" axis is primarily a visualizer dimension.
* Prints are what touches the water: the live ripple shimmer is not in them,
  on purpose.

The [changelog](../notes/changelog/) lists what each release fixed; the
[design notes](../notes/decisions/part-1/) explain why things are the way
they are.
