# Changelog

Condensed from the per-step DONE evidence (`docs/evidence/` in git history,
removed from the working tree after step 14). Spec: `PROJECT_SPEC.md` (v2);
decision log: `DECISIONS.md`.

## v0.2.0 — spec v2: hardening, layouts, piano rolls, all five platforms

### Step 14 — Android shell (Jetpack Compose)
Compose `AndroidView`/SurfaceView shell with a one-file JNI host
(`android/cpp/sumi_jni.cpp`): dedicated render thread owning a host-created
EGL context, §5.4 blocking teardown (condvar; context outlives the surface),
render-thread command queue for UI→core calls, AMidi + BLE-MIDI
(`MidiManager.openBluetoothDevice`) ingestion, PowerManager thermal listener
driving sim_scale 0.75 ↔ 0.60. `SOKOL_GLES3` split in the shared GL swapchain
TU with ES3 extension checks. On a Galaxy Tab S8 Ultra (Adreno 730): 10-minute
Osmose stress at min 119.6 / avg 120.9 fps (vsync-limited), 1.37 M messages,
0 dropped, PSS flat; live thermal step-down and recovery with the drawing
preserved; ×10 surface destroy/recreate mid-stress with zero EGL errors,
zero crashes, flat memory; GLES3 field dump passes the §4.6 regression vs the
Metal fixture (max 1.51e-2 under the documented mobile tier); ROLI over
Bluetooth MIDI painting MPE end-to-end.

### App icons (all platforms)
`tools/gen_icons.py` generates every platform's icon from
`images/midi-sink.jpg`: Android adaptive/legacy/monochrome mipmaps (cream
keyed to alpha so the mask never crops ink), iOS 1024 asset catalog, Windows
.ico + .rc + runtime window icon, X11 runtime icon, Wayland .desktop + XDG
hicolor theme (with install component and icon-cache refresh), and a macOS
Dock tile set at runtime from an embedded PNG (bare executable — no bundle).

### Step 13 — iOS shell (SwiftUI)
~200-line Swift shell, no Objective-C wrapper: `import SumiCore` via
module.modulemap, `CAMetalLayer`-backed `UIViewRepresentable`, CADisplayLink
(paused when backgrounded — Metal in background is a crash), CoreMIDI with
notification-driven hotplug + CoreAudioKit Bluetooth pairing, touch gestures
(tap drop / drag tine / two-finger twist vortex), settings sheet (layout
picker, sim_scale toggle — default 1.0 on Metal GPU family apple7+),
activity-gated idle-timer hold. On an iPad Air 11" (M4) with the ROLI over
Bluetooth: 5.5-minute traced session at a flat 60.00 fps, zero seconds below
50, thermal Nominal for the entire Instruments trace; backgrounding/rotation
crash-free. Fixed on the way: `metal_ios` added to the shdc dialects (device
abort without it), and **resize now carries the drawing across on every
platform** (passthrough resample of the resolution-independent field;
pristine fields keep exact identity init so the §4.6 dump stays
bit-identical). Core purity held: identical exported-symbol tables macOS↔iOS,
zero platform conditionals in core.

### Step 12 — Linux backend (OpenGL 4.1 core)
`swapchain_gl.cpp` hosts SOKOL_IMPL for GL; host-owned GLFW context (§5.1
exception), ALSA MIDI via the same 1 Hz rescan. §4.6 resolved at the shader
DIALECT level: `@glsl_options flip_vert_y` on offscreen vertex shaders makes
every GL offscreen target top-left-origin exactly like Metal/D3D11 (MSL/HLSL
outputs byte-identical), leaving the final composite as the single divergence
point — zero runtime flip branches; a control build without the directive
fails the regression with the v channel mirrored. Non-blocking PBO+fence
print readback. Field regression vs the D3D11 dump: max|Δ| 1.34e-3, aux
bit-identical, no orientation correction. Dip PNG parity with Metal;
mpe/wind stress green on NVIDIA (Wayland and X11).

### Step 11 — Windows backend (D3D11)
`swapchain_d3d11.cpp` hosts SOKOL_IMPL; DXGI flip-model swapchain, feature
level 11.1 with 11.0 fallback, vsync Present. DECISIONS #7 resolved: objects
compile twice on Windows only (dllexport baked into the shared set; no .def
file), ABI test links both artifacts. The print-readback seam became
backend-neutral (`sg_image` + bytes-per-pixel through the swapchain API;
staging-texture + `Map(DO_NOT_WAIT)` polling). Introduced the §4.6
cross-backend field regression: a canonical 7-pass deform script, harness
`--field-dump`, and `tests/field_dump_compare` — two D3D11 runs bit-identical.
WinMM stress feeder (`mpe_stress_win.cpp`) with absolute-clock pacing;
Steps 3–7 checks green on Windows at 165 Hz.

### Step 10 — Piano-roll layouts & BPM scroll
`SUMI_LAYOUT_ROLL_H`/`ROLL_V`: drops spawn on a fixed now-line and the whole
field translates as a closed-form scroll pass (emitted first, once per frame,
outside the deform budget) at `(bpm/60) × roll_speed` canvas-lengths/s, with
ingress as an explicit fresh-water shader branch (sampler clamp modes cannot
express it). Spec conflict resolved with the author: default roll_speed
0.0625 (16 beats = 4 bars of 4/4 across the canvas). Harness keys B/Shift-B
nudge BPM. Evidence: metronome drops evenly spaced to 0.1%, held-pressure
comet trails, vortex smear over scroll with the washi grain bit-identical
across prints (screen-locked), pristine ingress after 6 canvas-lengths, and a
10-minute soak: RSS flat (+160 KB), 98.2 fps, 0 dropped.

### Step 9 — Layout system: chromatic grid & Jankó
Pluggable pure pitch→position layouts (`layouts.cpp`) with echo sets
(`SUMI_MAX_ECHOES` = 3): Jankó stamps all three parity rows as fully-fed
triplets — one voice owns the set, dynamics fan out, one drop counter tick,
all-or-none budget reservation. Chromatic grid C1–B7 (7×12, edge-row clamping
keeps pitch class). Layout-derived glide axis (shortest-neighbor rule) —
grids glide along their row, including the B→C wrap. Params v0.2
(`sumi_layout_t`, bpm, roll_speed), `sumi_version()` → 0.2.0. Golden-position
tests (128 notes × layouts × aspects); live layout switching under MPE stress
at 98.3 fps with no voice teleport.

### Step 8 — v1 hardening & visual polish
Overflow-armed stuck-voice timeout (~10 s, grace measured from the overflow
moment); double-buffered dip prints (third dip refused with WARN, newest
read first); drop-counter/aux rebase on dip (3000-drop session stays
bounded); §4.4 feed episodes with onset/release hysteresis stamping nested
rings; composite polish — per-region fiber angle drift and segment break-up
(no lattice), near-black pooled ink, ink thickness via neighborhood band
probing (fract-based thickness fails on feed micro-shells). 500-drop
sharpness and Osmose stress unchanged (98+ fps, 0 dropped).

## v0.1.0 — spec v1: core engine + macOS harness (steps 1–7)

### Step 7 — Washi paper, palettes, paper dip
§4.5 composite: simplex mulberry-fiber strands, sizing mottle, absorption
grain scaled by paper_roughness; sumi/indigo/ochre palettes with morph
blending and aux-driven per-drop hue drift; linear math, manual sRGB encode.
Full paper dip: freeze → async GPU readback on a separate queue (never blocks
the loop; dip-window worst frame 14 ms at 2560×1440) → `sumi_read_print` →
identity reset with a "lift the paper" fade. PNG export moved to a detached
thread after an inline encode stalled 1080 ms.

### Step 6 — Wind mode & Airwave CC routing
Wind mode: one wandering-brush voice — breath (CC2/CC7/CC11/chanAT) drives a
breath-proportional line width, legato = VoiceMigrate with a wake tine.
Runtime CC routing table (`sumi_map_cc`) with Airwave defaults; global
controls as smoothed dt-scaled state. Mode detection became
activity-windowed after live playing latched MPE forever (user-reported);
mode changes flush the old dialect's voices.

### Step 5 — Full MPE mode
MCM (RPN 6) zone config, ±48 member bend with RPN 0 override, per-member-
channel voice table with note steal, per-voice smoothing/coalescing, §4.4
press feeds converted to area-correct boundary growth (spec-literal radius
steps looked frozen), per-voice glide tines, lift surfactant rings, slide→aux,
64-pass/frame deform budget with overflow merging. Osmose stress (10 voices ×
200 press/s): 99.6 fps, 0 dropped.

### Step 4 — MIDI ingest, normalizer, classic mode
Lock-free SPSC ring (4096, drop-oldest + diagnostics counter), stateful
decoder (14-bit bend, RPN/NRPN, running status), §2.5 dialect auto-detection,
classic mode lowering (fifths layout, sqrt-velocity radii, bend shear, CC1
vortex, CC64 dip). libremidi quirks found: virtual endpoints need
`track_virtual`, hotplug callbacks never fire → 1 Hz port rescan. 10 k
messages/30 s with 0 dropped.

### Step 3 — Jaffer deformations via mouse
Drop/tine/vortex passes (§4.3 closed forms, aspect-corrected), continuous
ink-phase encoding, ring-banding composite, mouse gestures. Two foundational
bugs fixed: the y-down texture-space convention (raw NDC interpolant flipped
the field every pass — consecutive deformations cancelled) and the
RGBA16F-safe parity ink phase (seam speckle past 256 drops). 500 drops stay
pixel-sharp; 40 × z/40 tine == 1 × z tine.

### Step 2 — Ping-pong targets & identity pass
RGBA16F ping-pong field targets, identity/passthrough passes, deformation
queue, sim_scale end to end. The 1000-swaps/frame stress test exposed a
~4 GB/s autorelease leak in a runloop-less Metal host — fixed with the
core-owned per-frame autorelease pool (applies to iOS unchanged).

### Step 1 — Skeleton, build system, black window
CMake + FetchContent pins (sokol, glm, libremidi, GLFW, stb, pinned
sokol-shdc binary download), static+shared libsumi from one object set, pure
C11 ABI test linking the dylib, GLFW harness with CAMetalLayer glue, clean
create/destroy at ~100 fps with Metal validation on.
