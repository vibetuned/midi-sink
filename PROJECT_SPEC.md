# PROJECT SPECIFICATION: Suminagashi MPE Visualizer Engine
**Architecture Paradigm:** Option 2 — Embedded High-Performance C-ABI Core + Host Platform Shells
**Phase 1 Target:** macOS desktop (Metal). **Phase 2:** Windows (D3D11), Linux (OpenGL 4.1 core). **Phase 3:** iOS (SwiftUI + CAMetalLayer), Android (Compose + ANativeWindow + GLES3).
**Reference Hardware:** ROLI Piano + ROLI Airwave (owned), Expressive E Osmose (planned), Roland Aerophone Brisa / Odisei Travel Sax (planned), plus any classic single-channel MIDI keyboard.

---

## 1. Architectural Philosophy: The Option 2 Pattern

To power native mobile apps later without rewriting graphics or simulation code, the project is strictly partitioned:

1. **`core/` (`libsumi`):** A standalone, headless C++20 library (static and shared).
   * Zero UI, zero windowing, zero platform event loops, zero MIDI device I/O.
   * Exposes a 100% C-compatible ABI (`sumi_core.h`).
   * Owns the GPU device and swapchain for the surface handle it is given (see §5.1 for the per-backend contract).
   * Ingests **raw MIDI byte streams** (any dialect) via a thread-safe queue and normalizes them internally (see §3).
2. **`desktop/` (Host Harness):** A thin macOS/Windows/Linux app for iteration.
   * Creates the OS window, prepares the native surface (on macOS: creates a `CAMetalLayer` and attaches it to the window's content view), and hands the pointer to `libsumi`.
   * Opens MIDI hardware via `libremidi` and forwards raw bytes to the core, one message at a time, unparsed.
   * Proves out the exact C-ABI contract that iOS and Android will consume.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        HOST LAYER (OS specific)                        │
│  Desktop harness (GLFW)   │   iOS (SwiftUI)      │  Android (Compose)  │
│  window + CAMetalLayer /  │   CAMetalLayer       │  ANativeWindow      │
│  HWND / GLX context       │                      │  + EGL              │
│  libremidi device watcher │   CoreMIDI           │  AMidi              │
└──────────────┬─────────────────────────────────────────────────────────┘
               │  native surface handle + raw MIDI bytes (C-ABI)
               ▼
┌────────────────────────────────────────────────────────────────────────┐
│                    libsumi CORE ENGINE (C++20, sokol_gfx)              │
│                                                                        │
│  MIDI byte queue ─► MIDI Normalizer ─► Voice/Gesture Events            │
│  (lock-free SPSC)   (MPE zones, 14-bit  (device-agnostic, §3)          │
│                      accumulators,                                     │
│                      device profiles)         │                        │
│                                               ▼                        │
│                                    Marbling Simulator                  │
│                                    (Jaffer closed-form, §4)            │
│                                               │                        │
│                                               ▼                        │
│                            Ping-pong UV displacement targets           │
│                                               │                        │
│                                               ▼                        │
│                            Composite pass + washi paper sim            │
└────────────────────────────────────────────────────────────────────────┘
```

**Hard rule:** the marbling simulator never sees MIDI. It consumes only the normalized event vocabulary of §3.3. This is what lets one engine serve MPE, Airwave gesture CCs, and monophonic wind MIDI without special cases leaking into the graphics code.

---

## 2. Input Landscape & Device Profiles

The engine must handle three genuinely different MIDI dialects. Each is described here precisely so the normalizer (`core/src/midi_normalizer.cpp`) can be implemented without guesswork.

### 2.1 MPE Sources — ROLI Piano, Expressive E Osmose
* **Zone layout:** master channel + member channels. Default assumption when no MPE Configuration Message is received: Lower Zone, master = ch 1, members = ch 2–16.
* **MPE Configuration Message (MCM):** parse RPN 6 (`CC 101=0, CC 100=6, CC 6=n`) on ch 1 or ch 16 to configure zone membership dynamically. Support a single zone in v1; log-and-ignore dual-zone configs.
* **Per-note dimensions (arrive on the note's member channel):**
  * Note On velocity → *strike*
  * 14-bit Pitch Bend → *glide* (lateral pitch slide). Default member-channel bend range: **±48 semitones** (ROLI and Osmose default). Honor RPN 0 to change it. Master-channel bend range defaults to ±2.
  * Channel Pressure (aftertouch) → *press*. **The Osmose is extremely aftertouch-dense** — expect continuous high-rate pressure streams on every held note; the queue and per-voice smoothing must be sized for this (see §3.4).
  * CC 74 → *slide* (vertical timbre axis on ROLI).
  * Note Off velocity → *lift*.
* **Voice identity:** a voice is keyed by `(member_channel)` while a note is active on it, since MPE guarantees one active note per member channel (handle same-channel overlap defensively: newest note steals the voice).

### 2.2 Global Gesture Source — ROLI Airwave
* The Airwave tracks the player's hands and emits its Air dimensions (Raise, Tilt, Glide, Slide, Flex per hand) as **standard assignable MIDI CCs, up to ten simultaneous streams**, independent of any note.
* Treat as **global field controls**, not voices. The core exposes a small CC-routing table (`sumi_set_cc_map`, §5) so the host/user can bind e.g.:
  * Left-hand Raise → global vortex strength ("breath/wind over the water")
  * Right-hand Tilt → fluid viscosity / damping
  * Glide → vortex center X drift
  * Flex → paper roughness or palette morph
* Default bindings must exist so it works out of the box, but every CC number is remappable because Airwave assignments are user-configured on the device side.

### 2.3 Monophonic Wind Sources — Roland Aerophone Brisa, Odisei Travel Sax
* Single channel; **CC 2 (Breath)** is the primary continuous expression stream (some devices offer CC 11 or channel aftertouch as alternates — the CC map must allow aliasing CC2/CC11/ChanPressure onto the same *breath* dimension).
* Musical behavior differs from a keyboard: long legato lines, one voice, constant breath modulation.
* Mapping model: the single voice is a **wandering ink brush**. Note pitch sets the target canvas position; a legato note change *migrates* the active drop's feed point (drawing a tine-like wake as it travels) rather than spawning a disconnected new drop. Breath modulates continuous ink flow rate into the active drop.

### 2.4 Classic single-channel MIDI keyboard
* Notes on one channel, global pitch bend (±2 default), CC 1 mod wheel, CC 64 sustain.
* Degrades gracefully: every note is its own voice with strike only; pitch bend and mod wheel act globally (bend → global shear tine, mod → vortex), CC 64 → paper dip.

### 2.5 Dialect auto-detection
The normalizer runs a lightweight heuristic, overridable via `sumi_set_input_mode`:
* MCM received, or note-ons observed spread across ch 2+ with per-channel bend/pressure → **MPE mode**.
* Notes only ever on one channel + dense CC2 → **wind mode**.
* Otherwise → **classic mode**.
Airwave CCs are orthogonal (pure CC streams on their own channel/numbers) and are routed by the CC map in every mode.

---

## 3. MIDI Normalizer & Voice Abstraction

### 3.1 Ingestion
* `sumi_push_midi()` accepts one raw message (status + 2 data bytes) and enqueues it into a **lock-free SPSC ring buffer** (fixed capacity, power of two, e.g. 4096 messages; on overflow drop oldest and increment a diagnostics counter). Producer: host MIDI thread. Consumer: the update/render thread inside `sumi_update()`.
* SysEx is out of scope for v1 (silently ignored). MIDI 2.0 / UMP and per-note controllers (CC in MIDI 2.0 sense) are explicitly out of scope for v1; the normalizer's design should not preclude adding a UMP ingest path later.

### 3.2 Stateful decoding (inside the consumer thread)
* Per-channel accumulators for: 14-bit pitch bend, RPN/NRPN state machines (RPN 0 = bend range, RPN 6 = MCM), running status is already resolved by libremidi but the decoder must not assume it.
* Per-voice state: note number, member channel, strike velocity, smoothed press/slide/glide values, age.

### 3.3 Normalized event vocabulary (the only interface to the simulator)
```
VoiceBegin   { voice_id, norm_x, norm_y, strike (0..1) }
VoiceGlide   { voice_id, dx }              // from per-note pitch bend, semitone-scaled
VoicePress   { voice_id, pressure (0..1) } // channel pressure
VoiceSlide   { voice_id, timbre (0..1) }   // CC74
VoiceMigrate { voice_id, new_x, new_y }    // wind-mode legato pitch change
VoiceEnd     { voice_id, lift (0..1) }
GlobalCtl    { dimension, value (0..1) }   // vortex, viscosity, palette, roughness…
PaperDip     { }                            // CC64 rising edge, or ABI call
```

### 3.4 Musical → spatial mapping (default profile)
* **Pitch → position:** pitch class mapped around a circle (circle-of-fifths angular layout), octave mapped to radius (low notes outer, high notes inner). Provide a second built-in layout (12-column grid) selectable via params.
* **Strike → initial drop radius** (perceptually scaled: radius ∝ sqrt(velocity) so ink *area* tracks velocity).
* **Press → sustained ink feed:** while pressure > 0 the drop keeps expanding at a rate ∝ pressure (re-emitting small Jaffer expansion steps each frame — see §4.4). This is the key Osmose behavior.
* **Glide (per-note bend) → local tine:** drag *that drop's* center along the pitch axis, emitting a narrow tine stroke along the drag path each frame the bend changes. This must be per-voice, never a global shear, in MPE mode.
* **Slide (CC74) → per-drop palette/ink-density modulation** for the layers that voice owns.
* **Lift → the drop "sets":** stop feeding; optionally emit one final faint surfactant ring proportional to lift velocity.
* **Rate limiting:** continuous dimensions (press, glide, slide, breath) are coalesced per voice per frame — the simulator consumes at most one value per dimension per voice per `sumi_update`, with exponential smoothing (time-constant configurable, default ~30 ms). Deformation *events* (tine segments from glide) are capped per frame (budget, e.g. 64 deform passes/frame) with overflow merged, so a hyper-expressive Osmose performance cannot starve the frame rate.

---

## 4. Mathematical Marbling Specification

**Crucial:** Do NOT use Navier–Stokes grid solvers or Eulerian velocity fields. Grid advection introduces numerical diffusion that blurs crisp suminagashi rings into mud.

Model the ink as an **area-preserving, continuous topological deformation** evaluated via inverse lookup on floating-point ping-pong textures (Aubrey Jaffer's closed-form "mathematical marbling" equations).

### 4.1 Ping-pong displacement field
* Two offscreen textures `tex_A`, `tex_B`, format **RGBA16F by default** (RGBA32F available behind a quality flag on desktop; do not assume 32F filtering exists on mobile GPUs).
* Simulation resolution is decoupled from output resolution (param, default = framebuffer size × 1.0, clampable to e.g. 2048² for perf).
* Initial state: identity coordinates (u = x/W, v = y/H).
* Each deformation pass reads `tex_current`, computes the inverse source coordinate, writes `tex_next`, swap. Multiple queued deformations in one frame = multiple ping-pong passes.

### 4.2 Texture payload & sampling (anti-bleed rules)
Each texel stores `(u, v, ink, aux)`:
* `u, v`: accumulated pre-image coordinates — **sampled with linear filtering** (coordinates are continuous; interpolation is safe and keeps edges smooth).
* `ink`: a **continuous scalar "ink phase"**, not a discrete layer ID. When a drop lands, its interior writes an ink phase derived from a global monotonically increasing drop counter plus the local normalized radial coordinate. The composite pass turns ink phase into alternating ring bands via a periodic function. Because the value is continuous, linear filtering cannot create garbage IDs at boundaries.
* `aux`: per-drop hue/palette selector encoded as a continuous value (quantized in the composite pass with a small snap tolerance). If sharp per-drop palette boundaries prove necessary, switch this channel only to `texelFetch` (nearest) lookups — never linearly filter discrete IDs.

### 4.3 Analytical deformation shaders (`deform.glsl`)
1. **Circular drop expansion** (ink or clear surfactant) of radius *r* at center *C* — inverse lookup for points outside the drop:

   P_src = C + (P − C) · sqrt(1 − r² / ‖P − C‖²)  for ‖P − C‖ ≥ r

   For ‖P − C‖ < r: write the new ink phase and local radial coordinate directly.
   All positions in **aspect-corrected normalized space** (correct for non-square canvases before applying the math, or rings become ellipses).

2. **Tine / comb stroke** along unit direction D̂ through point L with sharpness α and magnitude z:

   d = perpendicular distance from P to the line (L, D̂)
   P_src = P − z · D̂ · α / (α + d)

3. **Vortex agitation** centered at V, angular deflection θ(d) = A · exp(−d / R), rotate P around V by −θ(d) for the inverse lookup.

### 4.4 Continuous feeds
Sustained pressure/breath is realized as **small incremental drop expansions re-emitted per frame** at the voice's current center (radius step ∝ smoothed pressure × dt × expansion_rate). This keeps everything inside the same closed-form framework — no velocity field is ever introduced.

### 4.5 Composite pass (`composite.glsl`)
* Sample the active displacement target, map ink phase → alternating sumi ink rings vs. clear water.
* Modulate with procedural (simplex) washi mulberry-fiber noise and absorption grain; fiber strength = `paper_roughness`.
* Palettes: Sumi black, Indigo, Ochre (id-selected via params; `aux` channel can offset hue per drop).
* Output color space: render in linear, write sRGB-encoded swapchain.

---

## 5. Strict C-ABI Interface (`core/include/sumi_core.h`)

Must compile under C99/C11 and C++20 with zero includes beyond `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`.

### 5.1 Graphics ownership contract (per backend)
`sokol_gfx` has **no Vulkan backend** — supported paths are Metal, D3D11, OpenGL/GLES3, WebGPU. Backends for this project:
* **Metal (macOS/iOS, phase 1):** host passes a `CAMetalLayer*`. The core creates the `MTLDevice`/queue, drives `nextDrawable` each frame, and configures the layer's drawable size on resize. The host must not touch the layer after `sumi_create` except to destroy it after `sumi_destroy`.
* **D3D11 (Windows, phase 2):** host passes an `HWND`; core creates device + DXGI swapchain.
* **GL core 4.1 / GLES3 (Linux phase 2, Android phase 3):** exception to the ownership rule — the host creates the context (GLFW/EGL), makes it current on the render thread, and passes `native_surface_handle = NULL` with `backend = SUMI_BACKEND_GL`; the core renders into the currently bound default framebuffer and the host presents (swap buffers).
This asymmetry is inherent to GL and must be documented in the header comments.

### 5.2 Threading contract
* `sumi_create/destroy/resize/update/render/set_params/trigger_*` and gesture calls: **single thread** (the render thread). Not thread-safe with each other.
* `sumi_push_midi`: callable from **exactly one** other thread (the MIDI callback thread) concurrently with the render thread. SPSC — if the host has multiple MIDI sources, it serializes them before pushing.

### 5.3 Header
```c
#ifndef SUMI_CORE_H
#define SUMI_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Three-state export macro: shared-build export, shared-consume import, static no-op. */
#if defined(_WIN32)
  #if defined(SUMI_BUILD_SHARED)
    #define SUMI_API __declspec(dllexport)
  #elif defined(SUMI_USE_SHARED)
    #define SUMI_API __declspec(dllimport)
  #else
    #define SUMI_API
  #endif
#else
  #define SUMI_API __attribute__((visibility("default")))
#endif

typedef struct sumi_instance_t sumi_instance_t;

typedef enum {
    SUMI_BACKEND_AUTO  = 0,
    SUMI_BACKEND_METAL = 1,   /* native_surface_handle = CAMetalLayer*        */
    SUMI_BACKEND_D3D11 = 2,   /* native_surface_handle = HWND                 */
    SUMI_BACKEND_GL    = 3    /* host-owned context; handle must be NULL      */
} sumi_backend_t;

typedef enum {
    SUMI_INPUT_AUTO    = 0,
    SUMI_INPUT_MPE     = 1,
    SUMI_INPUT_CLASSIC = 2,
    SUMI_INPUT_WIND    = 3
} sumi_input_mode_t;

typedef enum {                 /* global control dimensions for CC routing */
    SUMI_CTL_VORTEX_STRENGTH = 0,
    SUMI_CTL_VORTEX_X        = 1,
    SUMI_CTL_VORTEX_Y        = 2,
    SUMI_CTL_VISCOSITY       = 3,
    SUMI_CTL_PAPER_ROUGHNESS = 4,
    SUMI_CTL_PALETTE_MORPH   = 5,
    SUMI_CTL_INK_FLOW        = 6,   /* breath aliases here in wind mode */
    SUMI_CTL_COUNT           = 7
} sumi_ctl_t;

typedef void (*sumi_log_fn)(int level, const char* msg, void* user);

typedef struct {
    void*          native_surface_handle;
    sumi_backend_t backend;
    uint32_t       width;
    uint32_t       height;
    float          pixel_ratio;
    sumi_log_fn    log_cb;        /* optional, may be NULL */
    void*          log_user;
} sumi_config_t;

typedef struct {
    float    fluid_viscosity;    /* damping for continuous agitation           */
    float    expansion_rate;     /* pressure/breath-driven drop feed scale     */
    float    paper_roughness;    /* washi fiber composite strength             */
    float    smoothing_ms;       /* expressive-dimension smoothing time const  */
    uint32_t active_palette_id;  /* 0 sumi black, 1 indigo, 2 ochre            */
    uint32_t pitch_layout;       /* 0 circle-of-fifths radial, 1 semitone grid */
    float    sim_scale;          /* simulation res / output res, (0,1]..2      */
} sumi_params_t;

/* Version & diagnostics */
SUMI_API uint32_t sumi_version(void);                       /* (maj<<16)|(min<<8)|patch */
SUMI_API uint32_t sumi_dropped_midi_count(sumi_instance_t*);/* queue overflow counter   */

/* Lifecycle — render thread only. sumi_create returns NULL on failure (see log_cb). */
SUMI_API sumi_instance_t* sumi_create (const sumi_config_t* config);
SUMI_API void             sumi_destroy(sumi_instance_t* inst);
SUMI_API void             sumi_resize (sumi_instance_t* inst, uint32_t w, uint32_t h, float pixel_ratio);

/* Frame loop — render thread only. */
SUMI_API void             sumi_update (sumi_instance_t* inst, double delta_time);
SUMI_API void             sumi_render (sumi_instance_t* inst);

/* MIDI ingest — exactly one producer thread, SPSC, wait-free. */
SUMI_API void             sumi_push_midi(sumi_instance_t* inst, uint8_t status, uint8_t data1, uint8_t data2);

/* Configuration — render thread only. */
SUMI_API void             sumi_set_params    (sumi_instance_t* inst, const sumi_params_t* params);
SUMI_API void             sumi_get_params    (sumi_instance_t* inst, sumi_params_t* out);
SUMI_API void             sumi_set_input_mode(sumi_instance_t* inst, sumi_input_mode_t mode);
SUMI_API void             sumi_map_cc        (sumi_instance_t* inst, uint8_t channel /*0xFF=any*/,
                                              uint8_t cc, sumi_ctl_t target);       /* Airwave routing */
SUMI_API void             sumi_clear_cc_map  (sumi_instance_t* inst);

/* Paper dip: freeze canvas, snapshot, reset UV to identity. */
SUMI_API void             sumi_trigger_paper_dip(sumi_instance_t* inst);
/* Synchronous readback of the last dipped print (RGBA8, tightly packed).
   Call with pixels=NULL to query size. Returns false if no print exists. */
SUMI_API bool             sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

/* Manual touch / mouse gestures — render thread only, normalized [0,1] coords. */
SUMI_API void             sumi_add_drop  (sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type);
SUMI_API void             sumi_add_tine  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float alpha /*sharpness*/, float magnitude);
SUMI_API void             sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius);

#ifdef __cplusplus
}
#endif
#endif /* SUMI_CORE_H */
```

---

## 6. Directory Layout

```text
suminagashi-engine/
├── CMakeLists.txt                 # Root orchestration, FetchContent pins
├── cmake/
│   └── CompileShaders.cmake       # Fetches pinned sokol-shdc binary, compiles GLSL → MSL/HLSL/GLSL330/GLES3
├── core/
│   ├── CMakeLists.txt             # libsumi static + shared
│   ├── include/sumi_core.h        # The pure C-ABI contract (§5)
│   └── src/
│       ├── engine.cpp             # Instance state, lifecycle, param handling
│       ├── swapchain_metal.mm     # CAMetalLayer device/drawable management (ObjC++, macOS/iOS)
│       ├── swapchain_d3d11.cpp    # Phase 2
│       ├── swapchain_gl.cpp       # Phase 2/3 (thin: host owns context)
│       ├── renderer.cpp           # sokol_gfx passes, ping-pong orchestration
│       ├── displacement.cpp       # Deformation queue → shader pass dispatch
│       ├── midi_normalizer.cpp    # SPSC queue drain, MPE/wind/classic decoding → §3.3 events
│       ├── voice_mapper.cpp       # §3.4 musical → spatial mapping, smoothing, budgets
│       └── shaders/
│           ├── deform.glsl        # identity-init, drop, tine, vortex passes
│           └── composite.glsl     # ink phase → rings, washi fibers, palettes
├── desktop/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp               # GLFW window, surface prep, frame loop, mouse → gestures
│       ├── metal_layer_glue.mm    # macOS: create CAMetalLayer on NSView (host side)
│       └── midi_harness.cpp       # libremidi observer: hotplug, open all inputs, forward bytes
├── tests/
│   └── normalizer_tests.cpp       # Header-less MIDI decode unit tests (no GPU needed)
└── assets/                        # (v1 has none — paper is fully procedural)
```

---

## 7. Technology Stack

* **Language:** C++20 core (`std::atomic`, no exceptions across the ABI). Objective-C++ only inside `swapchain_metal.mm` / `metal_layer_glue.mm`.
* **Graphics:** `sokol_gfx`. Shaders authored once in sokol-shdc GLSL dialect, cross-compiled to **MSL, HLSL5, GLSL330, GLES3** (no SPIR-V/Vulkan — sokol_gfx does not support it).
* **Shader compiler:** `sokol-shdc` — a **prebuilt binary**, not a library; `CompileShaders.cmake` must download a pinned release for the host OS/arch (or accept `SOKOL_SHDC_PATH`).
* **MIDI (desktop harness only):** `libremidi` (CoreMIDI/WinMM/ALSA backends; also has AMidi for Android later). The **core never links libremidi** — bytes cross the ABI.
* **Math:** `glm` (header-only) inside the core; never in the public header.
* **Windowing (desktop harness):** GLFW 3.4 (`GLFW_NO_API` window on macOS + manual CAMetalLayer attach).
* **Dependencies:** all via CMake `FetchContent` with pinned tags/commits.

---

## 8. Implementation Roadmap

The step-by-step implementation plan, acceptance criteria, and agent working rules live in **`ROADMAP.md`**. Feed it to the implementing agent one step at a time alongside this document. Do not begin a step while the previous step's DONE checks fail.
