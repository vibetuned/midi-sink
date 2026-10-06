---
title: Presets
description: A preset is the whole session but the ink — every parameter, the input dialect, the custom palette, the CC map, the strip's assignments, the fingering defaults and the synth's patch — in one JSON file that loads on the desktop, the tablets and the web alike.
---

A **preset** is the session: every engine parameter (the layout, the
medium, the look, the routing rows, the operators' knobs), the input
dialect, the custom palette, the CC map, the control values a shell sends
on its routed CCs (the ripple's, the Chladni's, the strip's wheels), the
strip's two assignable CCs, the fingering defaults of the stateful layouts,
and [Suzu's patch](../../suzu/patches/). It is not the ink: loading a
preset replaces the session's settings and leaves the sheet as it is.

## Where

* **Desktop** — Settings → *Presets*: the list, *Load*, *Delete*, *Save
  as* (a name), *Export* and *Import* (a file you choose). Named presets
  live as `<config>/presets/<name>.json`; the last session is written as
  `<config>/last_session.json` and restored at launch.
* **iPad** — *Presets* on the sheet: *Save* (named), the *Saved* list,
  *Export this session…*, *Import a preset…*; the files are in Files → On
  My iPad → midi-sink → Presets, and an imported one is applied and added
  to the list.
* **Android** — the same page, the files in the app's documents folder
  with the system file picker for export and import.
* **Web** — the panel's presets, kept in the browser's `localStorage`
  with a download for export.

All of them are the same file; a preset exported from the Tab loads on the
desktop. The web page's own form round-trips byte for byte; a desktop
session's file carries a `suzu` block the page has no engine for, which it
ignores.

## The file

JSON, one object, documented in the tree as `presets/SCHEMA.md`. The rule
that keeps it safe across versions: a reader **ignores keys it does not
know and keeps its defaults for keys that are missing**, and the engine
clamps everything on apply, so a hand-edited file cannot wound it. A
preset from a version that knew fewer parameters loads with the defaults
for the rest; one from a newer version loads what this one understands.
The file names its schema (1) and the engine version that wrote it.

```json
{
  "midi_sink_preset": 1,
  "sumi_version": [1, 5, 0],
  "name": "Indigo evening",
  "input_mode": 1,
  "params": { "fluid_viscosity": 0.5, "active_palette_id": 1, "medium": 0 },
  "palette": { "stops": [[0.6, 0.05, 0.02, 0], [0.01, 0.01, 0.01, 1]], "hue_drift": 0.3 },
  "cc_map": [[255, 1, 5], [255, 74, 8]],
  "controls": [[9, 32]],
  "strip": { "assign_a": 23, "assign_b": 24 },
  "layout_state": { "buttons": 0, "slider": 0 },
  "suzu": { "source": 1, "voice": 1, "level": 0.5 }
}
```

A [replay](../replay/) begins with the session in exactly this form, so a
recording also carries the look it was played with.

**Not built:** a preset-next pad on the control strip (which presets would
cycle is a performance question still open); the strip's *Next* cycles
layouts, not presets.
