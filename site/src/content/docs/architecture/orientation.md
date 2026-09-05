---
title: Orientation discipline
description: One y-down texture space for the whole deformation chain, flips only at the swapchain boundary, and the cross-backend field regression that guards it on every release.
---

## One space

The entire deformation chain works in **texture space: v grows downward, row 0
is the top**. Fullscreen-triangle vertex shaders emit `st = (u, 1 − v_clip)` —
the texture-space coordinate of the fragment's own texel — and every pass
(identity init, each deformation, the composite) operates purely in that
space. Mouse and touch coordinates, texture rows and screen rows all share it.

The rationale was found the hard way on Metal: sampling at the raw NDC
interpolant vertically flips the field on every offscreen pass, so consecutive
deformations *cancel* instead of composing. With `st`, a passthrough is a true
no-op and deformations compose exactly. The regression check is simple to
state: **forty passes of a z/40 tine must equal one pass of a z tine.**

## Flips live at the boundary only

Metal and D3D11 share a top-left row origin; OpenGL and GLES default to
bottom-left; WebGPU is top-left. Any vertical flip a backend needs lives
**exclusively in the final swapchain composite pass** and, mirrored, in the
print readback path. The deformation chain contains no backend-specific
branch — not one `#ifdef`.

## The field regression

Because the chain is identical everywhere, its output must be too. The
regression runs the canonical deformation script (drops, tines, vortices,
wake, pinch, ripple, swirl) from an identity field and reads the raw RGBA16F
field back:

| Backend | Result against the Metal fixture |
|---|---|
| Metal (macOS, iOS) | the reference fixture |
| D3D11 (Windows, real GPU) | bitwise identical |
| OpenGL 4.1 (Linux, real GPU), GLES 3 (Android) | bitwise identical |
| D3D11 on WARP, GL on Mesa llvmpipe (the GitHub runners, no GPU) | mean 6.0e-4 from the Apple fixture — and 1.8e-5 from each other: half-float rounding over the seven passes, judged at the second tier (2.5e-2 / 1e-3) |
| WebGPU (browser, wasm) | within tolerance: max Δ under 1e-3, mean under 1e-8 — an order of magnitude inside the documented 1e-2 / 1e-4 web tier |

The release workflow runs it on macOS, Windows and Linux runners through each
real renderer before any artifact is built, with a **negative control** — a
deliberately perturbed dump — proving the gate can fail. The web tier runs
headlessly in Chrome against the same fixture.

## Paper is screen-locked

A corollary invariant with its own check: the washi fibre and grain are
sampled in screen space, never through the deformed field. Run a tine and a
120 BPM piano-roll scroll over textured output and the grain must stay put
while the ink moves.
