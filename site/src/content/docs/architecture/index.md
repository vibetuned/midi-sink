---
title: Architecture
description: The Option-2 pattern — a frozen headless C++ core behind a pure-C ABI, six thin host shells, and a shared host-side MPE library. For people building a host.
---

midi-sink is partitioned so that one engine serves six platforms without a
line of graphics or simulation code being rewritten. The pattern (the spec
calls it *Option 2*) is worth stating plainly, because every other page in
this book is a consequence of it.

## The parts

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        HOST LAYER (OS specific)                          │
│  Desktop (GLFW)   │  iOS (SwiftUI)  │ Android (Compose) │  Web (JS)      │
│  window + surface │  CAMetalLayer   │ ANativeWindow+EGL │  canvas        │
│  libremidi        │  CoreMIDI       │ AMidi             │  Web MIDI      │
│          Play surface + control strip → hostmpe/ (Swift, Kotlin)         │
└──────────────┬───────────────────────────────────────────────────────────┘
               │  native surface handle + raw MIDI bytes   (pure-C ABI)
               ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                     libsumi CORE (C++20, sokol_gfx)                       │
│  MIDI byte queue ─► normalizer ─► voice events ─► marbling simulator     │
│  (lock-free SPSC)   (MPE/wind/classic)  (§3 vocabulary)  (closed-form)   │
│                       ping-pong RGBA16F fields ─► composite + washi       │
└──────────────────────────────────────────────────────────────────────────┘
```

1. **`core/` — `libsumi`.** A standalone, headless C++20 library. Zero UI,
   zero windowing, zero event loops, zero MIDI device I/O. It exposes a
   100 % C-compatible ABI, owns the GPU device and swapchain for the surface
   handle it is given, and ingests **raw MIDI bytes** of any dialect through a
   thread-safe queue, normalizing them internally. It never generates MIDI.
2. **Host shells.** Each creates a window or view, prepares the native surface,
   hands the pointer to the core, forwards MIDI bytes one message at a time
   *unparsed*, and drives update/render. The desktop harness is the reference
   consumer that proves the contract before iOS and Android use it.
3. **`hostmpe/`** — the shared host-side MPE library behind its own pure-C
   header: voice allocator, joystick and stylus engines, per-transport rate
   limiters, control-strip and fingering engines, echo guard. Consumed by
   Swift through a module map and by Kotlin through JNI: one implementation,
   unit-tested headlessly, the same bytes on both tablets. It never touches
   the GPU. [hostmpe →](hostmpe/)
4. **`voxo/`** — the sound engine, pure C at its header, the Decent
   Sampler player and the [Suzu](../suzu/) synth inside it, fed the
   identical bytes by the shell's one producer. It never touches the core.
5. **`presets/`** and **`replay/`** — two small pure-C libraries every
   shell links: the one serializer of the session, and the recorder and
   player that write and drive a session frame for frame.
   [Replay →](replay/)

## The hard rule

**The simulator never sees MIDI.** It consumes only the normalized event
vocabulary — VoiceBegin, VoiceGlide, VoicePress, VoiceSlide, VoiceSwirl,
VoiceMigrate, VoiceEnd, GlobalCtl, PaperDip. That is what lets one engine serve
MPE controllers, Airwave gesture CCs, wind instruments and the tablets' own
play surface — whose generated MPE re-enters through the same byte queue like
any device — without a special case leaking into the graphics code.

## Why it works across six backends

* **One shader source**, cross-compiled by sokol-shdc to MSL, HLSL, GLSL 4.10,
  GLES 3 and WGSL. Every sokol call lives behind two files — the renderer and
  the per-backend swapchain — and nothing above them includes a sokol header.
* **One coordinate space.** The deformation chain is written once in
  texture space, y-down; any flip a backend needs happens only in the final
  swapchain composite and the print readback. A cross-backend regression
  (identical deformation script → identical field readback, bitwise or within a
  documented tolerance) guards it on every tagged release.
  [Orientation →](orientation/)
* **Per-backend ownership** is spelled out, including GL's necessary
  asymmetry. [Swapchains →](swapchains/)
* **A threading contract** with exactly one MIDI producer.
  [Threading →](threading/)
* **The header is the contract.** No exceptions, no STL types, no callbacks
  into C++ across it; a C11 compile test enforces purity for both headers.
  [The C-ABI →](c-abi/)

## Frozen on purpose

Between phases the core is frozen; a phase that needs it reopens it under
the bug → regression test → fix pattern and closes it again. Phase 6 gave
it the second medium and five operators, Phase 8 the scope's live composite,
Phase 9 the five instrument layouts and their fingering state — each behind
an additive version of the ABI, the cross-backend fixture bitwise on Metal
throughout. Host features — touch, stylus, transports, settings, sound,
presets, replay, the web page's panel — are host code.
The [design notes](../notes/decisions/part-1/) record every ambiguity that was
resolved along the way; the [changelog](../notes/changelog/) records what each
step shipped.
