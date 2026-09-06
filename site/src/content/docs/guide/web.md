---
title: The web canvas
description: midi-sink in the browser — the same engine compiled to WebAssembly on a WebGPU swapchain, Marble mode with pointer, touch, pen and Web MIDI, and the scene API this site's demos use.
---

**[/marble/](/marble/)** is the sixth shell: the identical core compiled to
wasm, drawing through WebGPU, with a page of JavaScript as the host.

## Requirements

WebGPU in a **secure context** — `https://`, or `localhost` when you serve a
build yourself. Chrome and Edge, Safari 26, Firefox 141+. If the page says the
browser has no WebGPU it means exactly that (a LAN address over plain `http://`
hides it; serve HTTPS).

## What it does

* **Marble mode** — tap or click = drop · drag = comb · two-finger twist =
  vortex · two-finger pinch = fold · pen = stylus wake with pressure · mouse
  right-drag = vortex, Shift-drag = fold, middle-drag = wake.
* **MIDI in** on Chrome and Edge through Web MIDI: connect an instrument and it
  plays straight in, all three dialects. Safari and Firefox degrade to gestures
  only, quietly.
* **The settings panel** at the top left mirrors the desktop settings window
  where Marble mode has the concept — layout and look, expression routing, the
  ripple, paper dip and print export, About. It persists in your browser.
* **Play mode is web-deferred**: there is no WebMIDI *output* here yet.

## The scene API

Every live demo on this site is the web canvas with a scene:

```
/marble/?scene=vortex&A=2&R=0.25&embed=1
```

`scene` is one of `drop`, `feed`, `tine`, `vortex`, `rankine`, `wake`, `viscous`,
`pinch`, `ripple`, `lamb_oseen`, `scroll`; the other parameters are that scene's slider
keys (the formula's symbols — see each [operator page](../../operators/));
`embed=1` hides the settings panel and hint; `pace` is the number of frames between
steps (default 2 — the mathematics is applied as a run of small passes so you see
it take effect; 0 = instant). Every scene works on two ring clusters, top-left and
bottom-right, so one operator shows two orientations or two signs at once. A scene runs a deterministic
script on a fresh sheet, so a URL is a reproducible picture; *Replay* runs it
again. This is also how the site guarantees it contains no second
implementation of any operator: every moving picture here is an `<iframe>` of
this page.

## Fidelity

The web build is part of the same cross-backend regression as Metal, D3D11
and OpenGL: the canonical deformation script produces a field that matches the
Metal reference within a documented tolerance, checked headlessly in the
release gates. What you see in the browser is the engine, not a port of it.
