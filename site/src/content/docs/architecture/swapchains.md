---
title: Swapchain ownership per backend
description: Who creates the device and the surface on Metal, D3D11, OpenGL/GLES and WebGPU — including GL's deliberate asymmetry — and where the sokol implementation lives.
---

sokol_gfx has no Vulkan backend; midi-sink's backends are Metal, D3D11,
OpenGL/GLES 3 and WebGPU. Each has one swapchain translation unit in the core
(`swapchain_metal.mm`, `swapchain_d3d11.cpp`, `swapchain_gl.cpp`,
`swapchain_webgpu.cpp`), and that unit hosts the sokol implementation for its
backend — the renderer includes sokol declarations only, and nothing above
those files sees a sokol header.

| Backend | Host passes | Core owns | Notes |
|---|---|---|---|
| **Metal** (macOS, iPadOS) | a `CAMetalLayer*` | the `MTLDevice`, the queue, `nextDrawable` each frame, the layer's drawable size on resize | The host must not touch the layer after `sumi_create` except to destroy it after `sumi_destroy`. Objective-C++ with ARC lives only here. |
| **D3D11** (Windows) | an `HWND` | the device and the DXGI swapchain | |
| **OpenGL 4.1 / GLES 3** (Linux, Android) | `NULL` | — | **The exception:** the host creates the context (GLFW or EGL), makes it current on the render thread, and the core renders into the currently bound default framebuffer; the host presents (swap buffers). Inherent to GL and documented in the header. |
| **WebGPU** (browser) | the host's `WGPUDevice` and a canvas selector | the surface from the selector, its configuration in the canvas's preferred format, the per-frame surface texture, resize reconfiguration | The device is created in JavaScript and handed to the wasm module; readback textures are created with copy-source usage and read through an asynchronous map, so the field regression works without blocking. |

## Why the asymmetry is acceptable

The core's rule is "own the device for the surface you are given". GL cannot
express that: a context is a thread-affine host object. Rather than hide the
difference behind a leaky abstraction, the contract states it — `backend = GL`
means *the host owns the context* — and the Android and Linux shells are
written to it (Android's render thread owns the EGL context and blocks
`surfaceDestroyed` until the surface is unbound).

## Readback seam

Every backend implements the same small seam for the print target and the
field dump: prepare/release hooks around readback-bound images (no-ops on
Metal, D3D11 and GL; CopySrc texture injection on WebGPU) and a non-blocking
begin/poll pair for the field read that the synchronous desktop path wraps
with a bounded yield.

## Shaders

Authored once in sokol-shdc's GLSL dialect; compiled at build time to MSL,
HLSL 5, GLSL 4.10, GLES 3 and WGSL by a pinned prebuilt `sokol-shdc` binary
that CMake downloads (or takes from `SOKOL_SHDC_PATH`). The deformation shader
and the composite shader are the only two.
