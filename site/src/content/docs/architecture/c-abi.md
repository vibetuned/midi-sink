---
title: The C-ABI
description: "sumi_core.h — the pure-C contract every shell consumes: lifecycle, MIDI ingest, params, gestures, the CC map, the layout probe, versioning. And hostmpe.h, its host-side sibling."
---

The header
[`core/include/sumi_core.h`](https://github.com/vibetuned/midi-sink/blob/main/core/include/sumi_core.h)
is the whole contract between the engine and any host. It must compile under
C99, C11 and C++20 with no includes beyond `<stdint.h>`, `<stdbool.h>` and
`<stddef.h>` — enforced by a C11 compile test in the suite. Swift imports it
directly through a module map; Kotlin reaches it through a thin JNI file;
JavaScript calls its exports from the wasm module. When this page and the
header differ, the header wins.

## Shape of the API

* **Lifecycle:** `sumi_create` takes a config — the backend enum (Metal,
  D3D11, GL/GLES, WebGPU), the native surface handle for that backend, the
  initial size and pixel ratio — and returns an opaque instance;
  `sumi_destroy`, `sumi_resize`, `sumi_update(dt)`, `sumi_render`.
* **MIDI ingest:** `sumi_push_midi(status, d1, d2)` — one raw message, any
  dialect, from exactly one producer thread. Everything else about MIDI (zones,
  14-bit accumulators, RPNs, mode detection) happens inside.
* **Params:** one struct, passed by pointer, read and written whole — layout,
  palette, viscosity, ink feed, roughness, sim scale, bpm and roll speed, the
  three routing switches (`bend_mode`, `press_mode`, `slide_mode`), the pinch
  variant, the vortex profile, the ripple's bake flag and angle. The struct has
  no size field by design: `sumi_version()` gates compatibility, and a grown
  struct means hosts rebuild.
* **Gestures** (host-side data with no MIDI path): drop, tine, vortex (with a
  profile), wake, pinch — all in normalized [0, 1] positions with radii in
  canvas heights; `sumi_trigger_paper_dip`.
* **CC routing:** `sumi_map_cc(channel | 0xFF, cc, target)` to any of the nine
  global dimensions; `sumi_clear_cc_map`. `sumi_set_input_mode` overrides the
  dialect heuristic.
* **The layout probe:** `sumi_layout_probe(layout, params, aspect, x, y)` — a
  pure, instance-free function returning the cell under a point: nominal note,
  centre, radius, the local semitone axis and the true lattice step. The tablet
  shells never re-derive lattice geometry; they sweep the probe to draw the
  lattice and call it to hit-test. Callable from any thread.
* **Prints and diagnostics:** the print readback, the dropped-message counter,
  and — behind a debug header — the field dump used by the cross-backend
  regression.

## Units, once

Positions are normalized to the canvas, [0, 1] both axes, y-down. Every
distance — gesture radii, the probe's cell radius and semitone step, the
shells' touch deltas — is in **canvas heights**, the project's one distance
unit; direction vectors are in aspect-corrected space. Keeping a single unit
is what let the Play surface's joystick radius equal the drawn circle exactly.

## Versioning

`sumi_version()` packs major.minor.patch. 0.2.0 = the layout system;
0.3.0 = the layout probe; 0.4.0 = the four v0.4 operators, `sumi_add_vortex`
gaining a profile argument (breaking) and the routing params; 0.5.0 = the
WebGPU backend enum and surface struct (additive); 0.6.0 = the FEED drop
layer and the Lamb–Oseen vortex profile — the two pressure operators as
gestures — plus print-buffer recycling (additive); 0.7.0 = the params struct
grew `wake_profile` and `wake_spread` (the viscous Stokeslet stroke — hosts
rebuild). The app's own version is separate and comes from the release tag.

## `hostmpe.h`

The host-side sibling follows the same discipline (pure C, C11 compile test):
the voice allocator (`hostmpe_touch_begin/update/end`), the stylus engine
(`hostmpe_pen_begin/glide/slide/pressure`), the session configuration
(MCM + RPN 0), panic and zone silence, external-occupancy observation, the
echo guard (`hostmpe_echo_record/is_ours`), rate- and budget-class limiters,
and the control-strip engine. Every function fills a caller-provided array of
3-byte messages and returns the count — no allocation, no callbacks. It
produces MIDI and never touches the GPU; the core consumes MIDI and never
produces it. [hostmpe →](../hostmpe/)
