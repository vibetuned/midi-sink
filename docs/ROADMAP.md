# IMPLEMENTATION ROADMAP: Suminagashi MPE Visualizer Engine
**Companion to `PROJECT_SPEC.md`. Historical: all steps below are DONE
(Parts 1–2 = v0.1/v0.2, Part 3 = Phase 4 / v0.4, Part 4 = Phase 5 / v1.0,
Part 5 = Phase 6 / v2.0 alpha.1); per-step evidence lives in git history
under `docs/evidence/` (removed from the working tree as each phase ships)
and is condensed in `CHANGELOG.md`. The open roadmap (Phases 7–9) is
`_work/ROADMAP_5.md`.**

---

# Part 1 — v0.1 (steps 1–7, macOS/Metal)


---

## Working Rules (apply to every step)

* Read the spec sections referenced by the current step before writing code.
* Keep every sokol call behind `renderer.cpp` / `swapchain_*.{mm,cpp}`; nothing above those files may include sokol headers.
* No exceptions, no STL types, no callbacks-into-C++ across `sumi_core.h`. `sumi_create` failure path = NULL + log callback.
* When a spec ambiguity is found, prefer the choice that keeps the **core identical for iOS/Android** — that is the project's reason for existing. Note the decision in a `DECISIONS.md` at the repo root.
* Commit after each step with its DONE evidence (screenshots / test output) referenced in the commit message.
* Do not implement anything from a later step early, even if convenient.

---

## Step 1 — Skeleton, build system, black window
**Spec sections:** §1 (architecture), §5 (full ABI header — copy it verbatim), §6 (directory layout), §7 (stack & pinned dependencies).

* Root CMake with `FetchContent` (glm, sokol, libremidi, GLFW — pinned tags/commits).
* `cmake/CompileShaders.cmake`: download a pinned `sokol-shdc` release binary for the host OS/arch (or accept `SOKOL_SHDC_PATH`); wire GLSL → MSL compilation (HLSL/GLSL330/GLES3 outputs configured but unused in phase 1).
* `libsumi` builds as static + shared. Add `tests/abi_c_compile.c` that includes `sumi_core.h`, compiles as C11, and links — proving the header is pure C.
* Desktop harness: GLFW `GLFW_NO_API` window; `desktop/src/metal_layer_glue.mm` creates a `CAMetalLayer` on the NSWindow's content view; pass the layer pointer to `sumi_create` (backend = METAL); per-frame `sumi_update` + `sumi_render` clearing to deep indigo; `sumi_resize` wired to the framebuffer-size callback.
* Stub all other ABI functions (no-op bodies) so the shared library exports the full contract from day one.

**DONE when:** app opens and clears at 60 fps; live window resize produces no Metal validation warnings (run with `METAL_DEVICE_WRAPPER_TYPE=1`); clean `sumi_destroy` on close (no leaks in a short Instruments pass); `sumi_version()` returns 0.1.0; the C11 compile test passes.

---

## Step 2 — Ping-pong targets & identity pass
**Spec sections:** §4.1 (ping-pong field), §4.2 (texture payload & sampling rules).

* Two RGBA16F offscreen render targets at simulation resolution (`sim_scale` param respected, decoupled from swapchain size).
* Identity-init shader writing (u, v) = (x/W, y/H) into the current target.
* Temporary composite pass that visualizes the stored coordinates directly (red = u, green = v) onto the swapchain.
* Ping-pong swap machinery in `displacement.cpp`: a per-frame queue of deformation passes, each reading `tex_current` and writing `tex_next`, then swapping.

**DONE when:** screen shows a clean red/green gradient; a stress mode forcing 1,000 ping-pong swaps per frame for 10 s shows no memory growth (Instruments) and no GPU pipeline stalls; window resize re-creates targets at the correct simulation resolution without corrupting state.

---

## Step 3 — Jaffer deformations via mouse
**Spec sections:** §4.3 (the three closed-form deformations — implement the math exactly as written, in aspect-corrected normalized space), §4.2 (ink-phase encoding — continuous scalar, never discrete IDs), §5.3 (gesture ABI signatures).

* Implement drop expansion, tine (with `alpha` sharpness + `magnitude`), and vortex fragment passes in `deform.glsl`.
* Drop interiors write the new continuous ink phase (global drop counter + local radial coordinate) per §4.2.
* Composite maps ink phase → alternating black/white rings.
* Harness wiring: left click → `sumi_add_drop`, left drag → `sumi_add_tine`, right drag → `sumi_add_vortex`.

**DONE when:** 500+ successive drops keep ring boundaries pixel-sharp — capture a screenshot after drop 10 and after drop 500 and confirm no progressive blur; rings are perfectly circular on a deliberately non-square window; a tine dragged through concentric rings produces the classic marbled chevron; a vortex twists rings into spirals that stay sharp.

This is the first playable milestone — stop and let the human play with it before continuing.

---

## Step 4 — MIDI ingest, normalizer, classic mode
**Spec sections:** §3.1 (SPSC queue), §3.2 (stateful decoding), §3.3 (normalized event vocabulary — the simulator consumes ONLY these), §2.4 (classic mode behavior), §2.5 (auto-detection heuristic), §3.4 (pitch → position layouts), §5.2 (threading contract).

* `desktop/src/midi_harness.cpp`: libremidi observer for hotplug; open all inputs; forward every message unparsed via `sumi_push_midi` from the MIDI callback thread.
* Lock-free SPSC ring buffer (power-of-two capacity, drop-oldest on overflow, `sumi_dropped_midi_count` counter).
* Stateful decoder: 14-bit pitch bend assembly, RPN/NRPN state machine (RPN 0 bend range, RPN 6 MCM recognized but only acted on in Step 5), running-status tolerance.
* Classic-mode mapping: note-on → drop at circle-of-fifths position (velocity → radius via sqrt scaling), global pitch bend → shear tine, CC 1 → vortex, CC 64 rising edge → paper dip (stub the dip as a simple UV reset for now).
* `tests/normalizer_tests.cpp`: decoder unit tests that run without a GPU.

**DONE when:** playing a keyboard (or the ROLI Piano in single-channel mode) paints drops at pitch-mapped positions with velocity-scaled radii; `sumi_dropped_midi_count` stays 0 during a dense 30-second performance; normalizer tests pass headlessly (CI-runnable on a macOS runner with no GPU).

---

## Step 5 — Full MPE mode (ROLI Piano, Osmose-ready)
**Spec sections:** §2.1 (MPE zone rules, ±48 member bend default, Osmose pressure density), §2.5 (mode detection), §3.4 (per-dimension mappings, smoothing, coalescing, deformation budget), §4.4 (continuous feeds as incremental expansions).

* MCM (RPN 6) parsing configures the zone; per-member-channel voice table with note-steal handling.
* Per-voice press / glide / slide with exponential smoothing (`smoothing_ms`) and per-frame coalescing; global per-frame deformation budget with overflow merging.
* Mappings: press → per-frame incremental drop expansion at the voice center; glide → per-voice local tine along the pitch axis (never a global shear); slide (CC74) → per-drop `aux` modulation; lift → drop sets + faint surfactant ring scaled by release velocity.

**DONE when:** two simultaneous notes bent in opposite directions each drag *their own* drop (visual check); holding one note with rising pressure grows its rings continuously without disturbing neighboring drops; a scripted Osmose stress stream (10 voices × 200 pressure messages/s, provide the script in `tests/`) holds 60 fps with 0 dropped messages and the deformation budget engaging gracefully.

---

## Step 6 — Wind mode & Airwave CC routing
**Spec sections:** §2.2 (Airwave = global assignable CCs, default bindings), §2.3 (wind instrument behavior: wandering brush, breath aliases CC2/CC11/ChanPressure), §3.3 (`VoiceMigrate`, `GlobalCtl`), §5.3 (`sumi_map_cc` / `sumi_clear_cc_map` / `sumi_set_input_mode`).

* Wind mode: breath drives `SUMI_CTL_INK_FLOW` feeding the single active voice; legato pitch change emits `VoiceMigrate`, drawing a wake tine from old to new position.
* CC routing table with documented Airwave defaults in the README; `GlobalCtl` dimensions drive vortex strength/center, viscosity, roughness, palette morph live.

**DONE when:** a scripted mono breath stream (notes + dense CC2, script in `tests/`) produces one continuous wandering ink line with breath-modulated thickness; remapping an arbitrary CC to vortex strength at runtime via `sumi_map_cc` visibly modulates the swirl; classic and MPE modes are unaffected (regression check with Step 4/5 scripts).

---

## Step 7 — Washi paper, palettes, paper dip
**Spec sections:** §4.5 (composite: ink phase → rings, fibers, palettes, color space), §5.3 (`sumi_trigger_paper_dip`, `sumi_read_print`, `sumi_set_params`/`sumi_get_params`).

* Procedural simplex washi mulberry-fiber noise + absorption grain, strength = `paper_roughness`; three palettes (sumi black, indigo, ochre) with `aux`-channel per-drop hue offsets; linear rendering, sRGB swapchain output.
* Live param tuning: harness keys 1–9 adjust viscosity / expansion / roughness / palette / layout via `sumi_set_params`.
* Paper dip: CC 64 rising edge or ABI call → freeze, async GPU→CPU readback (staging buffer, never a blocking mid-frame stall) into the print buffer, brief "lift the paper" fade, reset to identity. Harness saves the print as PNG (stb_image_write) on keypress via `sumi_read_print`.

**DONE when:** a dip triggered during heavy MPE playing exports a correct full-resolution PNG with no visible hitch longer than 1 frame at 60 fps; all palettes and both pitch layouts switch live without artifacts; the exported print matches the on-screen frame at dip time.

---

## Deferred (explicitly out of v1 scope — do not implement)
Windows/Linux backends, iOS/Android shells, dual MPE zones, SysEx / MIDI 2.0 UMP, video or recording export, preset persistence.

---

# Part 2 — v0.2 (steps 8–14: hardening, layouts, rolls, Windows, Linux, iOS, Android)


---

## Working Rules (apply to every step)

* Read the spec sections referenced by the current step before writing code. Where the spec and `DECISIONS.md` conflict, the spec wins — it has absorbed the validated decisions; flag any remaining conflict instead of silently picking one.
* Keep every sokol call behind `renderer.cpp` / `swapchain_*.{mm,cpp}`; nothing above those files may include sokol headers.
* No exceptions, no STL types, no callbacks-into-C++ across `sumi_core.h`.
* **Backend purity rule (§4.6):** the deformation chain never contains backend-specific branches; orientation flips live only in the final swapchain composite and the print readback path.
* Prefer the choice that keeps the core identical across all five platforms; log every resolved ambiguity in `DECISIONS.md`.
* Commit after each step with DONE evidence referenced in the commit message.
* Do not implement anything from a later step early, even if convenient.

---

## Step 8 — v1 hardening & visual polish
**Spec sections:** §3.1 (overflow safeguard), §4.2 (parity phase, aux rebase on dip), §5.3 (`sumi_trigger_paper_dip` double-buffer comment), §4.5 (composite).

Carry-over fixes from the v1 review:
* **Stuck-voice safeguard:** implement the §3.1 overflow-armed per-voice inactivity timeout; unit-test by scripting an overflow that swallows a Note Off and asserting the synthetic `VoiceEnd` fires.
* **Dip double-buffer:** two print buffers, flip on dip, refuse-and-log a third dip while both are in flight; stress-test with two dips 200 ms apart during PNG encode of the first.
* **Aux rebase:** reset the drop counter on paper dip; assert aux values stay < 2048 across a scripted 3000-drop two-dip session.
* Visual polish (from the first dip-print review):
  * Washi fibers: per-region angle drift via low-frequency noise (±20° on the two grain directions), strand break-up along ridge length (short segments, not continuous rules), so no two areas share a coherent crosshatch lattice.
  * Ink depth: fiber modulation under dense ink capped at ~10–15% luminance; pooled sumi centers reach ~0.05–0.1 linear luminance; add an ink-thickness falloff near ring boundaries (thin ink → more paper grain shows).
* **Nested-ring check:** scripted single-position pressure pulses (one voice, 5 pressure pulses at a fixed center) must stamp 5 nested sharp rings. If interiors overwrite instead of ringing, fix the §4.4 feed's ink-phase writes before proceeding.

**DONE when:** all three fixes have passing tests; a new dip print shows organic non-lattice fiber, near-black pooled ink, and the pulse script yields visibly nested rings; no regression in the Step 3/5 sharpness and stress checks.

---

## Step 9 — Layout system: chromatic grid & Jankó
**Spec sections:** §3.4 (layout list, layout = pure function + optional field motion), §5.3 (`sumi_layout_t`, params struct v0.2, version gating).

* Refactor pitch→position into a pluggable layout module (`core/src/layouts.cpp`): `(note, params, aspect) → (x, y)` plus an optional per-frame field-motion hook (unused until Step 10). Fifths layout becomes layout 0 with zero behavior change.
* Implement `SUMI_LAYOUT_CHROMA_GRID` (C1 top-left → B7 bottom-right, row = octave, column = pitch class, edge-cell clamping for notes outside C1–B7) and `SUMI_LAYOUT_JANKO` (staggered whole-tone rows, duplicate-note row chosen nearest canvas center).
* Params struct grows (`bpm`, `roll_speed`, layout enum); bump `sumi_version()` to 0.2.0; update the C11 ABI compile test.
* Layouts are pure and unit-tested headlessly: golden-position tables for all 128 notes × 3 layouts × 2 aspects.
* Harness: key `L` cycles layouts live; glide axis (§3.4 / DECISIONS #30) must derive from the active layout's local pitch direction — verify a bent note travels along a grid row in grid layouts.

**DONE when:** golden-position tests pass; live layout switching mid-performance never crashes or teleports active voices (existing drops stay where the field has them, only *new* placements change); a chromatic scale played on the ROLI paints a clean left-to-right, top-to-bottom raster in CHROMA_GRID and the staggered Jankó pattern in JANKO; glide follows the local pitch axis in each layout.

---

## Step 10 — Piano-roll layouts & BPM scroll
**Spec sections:** §3.4 (roll layouts, field motion, now-line, host-supplied BPM), §4.6 (the scroll is a deform pass — same y-down space).

* Uniform-translation deformation pass: inverse lookup `P_src = P − v̂·s·dt`. **Ingress is an explicit shader branch** (§3.4): out-of-range sources write ink 0, aux 0, and the identity coords of the fragment's own texel — never sampler clamp modes (edge-clamp streaks old ink; border-clamp cannot express per-texel identity). Speed `s = (bpm/60) × roll_speed`; direction from the active roll layout.
* `SUMI_LAYOUT_ROLL_H` (pitch → y, now-line x = 0.12, drift +x) and `SUMI_LAYOUT_ROLL_V` (pitch → x, now-line y = 0.12, drift down).
* The scroll pass is emitted once per frame *before* the frame's other deformations, outside the §3.4 deformation budget (it is field motion, not an expressive event).
* Live `bpm` / `roll_speed` changes via `sumi_set_params` take effect next frame without popping.
* Harness: key `B`/`Shift-B` nudges BPM ±5 for eyeballing sync against a metronome.

**DONE when:** at 120 BPM and roll_speed 0.25, a drop takes 4 beats to traverse ¼ of the canvas (measure with the scripted clock — frame-count × dt assertion in a headless field-readback test); notes played to a metronome line up as evenly spaced drops; tines and vortices visibly smear downstream while scrolling **while the washi grain stays screen-locked and motionless (§4.5 invariant — eyeball check at high paper_roughness)**; the entry edge shows clean fresh water with no streaked or wrapped ink; sustained-pressure drops stretch into comet trails; scroll runs indefinitely (10-minute soak) with zero drift artifacts at the entry edge and stable memory.

---

## Step 11 — Windows backend (D3D11)
**Spec sections:** §5.1 (D3D11: host passes HWND, core creates device + DXGI swapchain), §4.6 (top-left row origin — no flip expected), §7 (shader outputs: HLSL5). DECISIONS #7 (the OBJECT-library/export-macro split must be resolved now).

* `core/src/swapchain_d3d11.cpp` hosts `SOKOL_IMPL` for the D3D11 build (per DECISIONS #1 pattern); device, DXGI swapchain, resize handling.
* Resolve the Windows export problem flagged in DECISIONS #7: split static/shared object compilation or move to a .def/export-map approach; the C11 ABI test must link against both artifacts on Windows.
* Desktop harness: GLFW Win32 path (`glfwGetWin32Window`); libremidi WinMM backend; the 1 Hz port rescan from DECISIONS #25 already ports as-is.
* Print readback via D3D11 staging texture, same double-buffer contract.

**DONE when:** all Step 3–7 DONE checks pass on Windows (mouse marbling sharpness, MPE stress script at 60 fps with 0 drops, dip export); the cross-backend field regression test (§4.6: identical deform script → field readback matches Metal within float tolerance) passes; both static and shared artifacts link on MSVC.

---

## Step 12 — Linux backend (OpenGL 4.1 core)
**Spec sections:** §5.1 (GL exception: host owns the context, handle = NULL), §4.6 (bottom-left row origin — the flip lands here, only at the swapchain composite + print path), §7 (glsl410 output per DECISIONS #2).

* `core/src/swapchain_gl.cpp`: thin — binds the default framebuffer, applies the §4.6 composite flip, hosts `SOKOL_IMPL` for GL builds.
* Harness: GLFW GL 4.1 core context created host-side, made current on the render thread before `sumi_create`; host swaps buffers. libremidi ALSA backend.
* Print readback via PBO; verify the exported PNG is *not* vertically flipped (the flip must be applied exactly once — this is the classic GL bug).

**DONE when:** cross-backend field regression test passes (proving the deform chain has zero GL branches); on-screen output visually matches macOS captures of the same scripted performance; the dip PNG matches the Metal dip PNG of the same script pixel-for-pixel in orientation and within tolerance in tone; Steps 3–7 checks pass on Linux.

---

## Step 13 — iOS shell (SwiftUI)
**Spec sections:** §5.4 (Swift bridge: module map, ~30-line shell), §5.1 (Metal path identical to macOS), params comment (sim_scale: 1.0 on iPad-class GPUs). DECISIONS #12 (autorelease pool already core-side).

* Xcode target consuming `libsumi.a` + `module.modulemap`; no Objective-C wrapper.
* `UIViewRepresentable` → `CAMetalLayer`-backed view; `CADisplayLink` drives update/render; `layoutSubviews` → `sumi_resize` with `contentScaleFactor`; scene-phase handling pauses the display link in background (Metal rendering in background is a crash on iOS).
* CoreMIDI (including Bluetooth MIDI for the ROLI) → `sumi_push_midi` on the MIDI thread; §5.2 contract holds unchanged.
* Touch: tap → drop, pan → tine, two-finger twist → vortex (reusing the ABI gesture calls).
* sim_scale default 1.0 on iPad Pro / ProMotion-class, 0.75 below; expose a settings toggle.

**DONE when:** the ROLI Piano over Bluetooth MIDI paints MPE drops on an iPad at a sustained 60 fps for a 10-minute session with no thermal collapse below 50 fps (Xcode Instruments thermal log attached as evidence); backgrounding/foregrounding never crashes; touch marbling works; the core static library is bit-identical to the one the macOS harness links (same commit, no `#if TARGET_OS_IPHONE` in core outside the swapchain TU).

---

## Step 14 — Android shell (Jetpack Compose)
**Spec sections:** §5.4 (JNI bridge: SurfaceView → ANativeWindow → EGL host-owned context), §5.1 (GL backend rules), §4.6 (flip isolation — already proven in Step 12), params comment (sim_scale 0.75 default). §7 (glsl300es output).

* Compose `AndroidView` wrapping `SurfaceView`; JNI wrapper (one .cpp) forwards Surface lifecycle: `surfaceCreated` → `ANativeWindow_fromSurface` → EGL context on a dedicated render thread → `sumi_create(NULL, GL)`; `surfaceChanged` → `sumi_resize`; `surfaceDestroyed` → **blocking teardown per the §5.4 Android teardown contract**: the UI thread waits (mutex + condvar or thread join) until the render thread finishes its in-flight frame and unbinds the EGL surface, then releases the ANativeWindow. Never return from `surfaceDestroyed` while the render thread can still touch the surface.
* MIDI: AMidi (API 29+) → `sumi_push_midi` from the MIDI thread; the harness-side producer mutex from DECISIONS #24 ports to the JNI layer.
* sim_scale default 0.75; drop to 0.6 under `THERMAL_STATUS_SEVERE` via PowerManager thermal listener (host-side — the core never detects devices).
* Touch gestures as on iOS.

**DONE when:** the Osmose stress script (Step 5) holds ≥ 55 fps for 10 minutes on a mid-range test device at sim_scale 0.75 with graceful thermal degradation logged; GLES3 output passes the cross-backend field regression test; surface rotate/destroy/recreate cycles (10× scripted, **triggered mid-frame during the heavy stress script** to exercise the teardown race) produce zero EGL errors or native crashes and leak nothing (Android Studio memory profiler evidence); Bluetooth MIDI from the ROLI works end-to-end.

---

## Deferred (explicitly out of v2 scope — do not implement)
MIDI clock / Ableton Link tempo ingest (BPM stays host-supplied), dual MPE zones, SysEx / MIDI 2.0 UMP, video or recording export, preset persistence, D3D12/WebGPU backends.

---

# Part 3 — v0.4 (steps 15–22: Touch & Stylus MPE Play Surface, Android parity — formerly `_work/ROADMAP_3.md`)
**Companions: `PROJECT_SPEC.md` §8 (the Phase-4 spec, folded and corrected), `DECISIONS.md` Part III (`DECISIONS_3 #n`). References below to `PHASE4 §n` mean `PROJECT_SPEC.md` §8.n.**
**Scope as planned: Steps 15–22. Steps 15–18 and 21 ran on the Mac (iOS); Steps 19–20 (core v0.4 work) started on the desktop harness (20 finished on iOS); Step 22 on the Linux box (Android). Historical: all steps are DONE — evidence condensed in `CHANGELOG.md` v0.4.0. Where a DONE gate below was superseded by a decision (Step 21's ±47 re-anchor and piano retune ramps → per-cell legato retriggers, DECISIONS_3 #39; the §3.4 lift ring → removed, #41; CC 64 → paper dip → classic mode only, #67), the decision is the record of what shipped. Two Step-22 gates remain the user's to confirm by hand: the channel-steal run with the ROLI over BLE (mechanism verified against a live external chord) and an on-device Android DAW receiving the virtual device.**

---

## Working Rules (apply to every step)

* All prior working rules hold (backend purity, no sokol above the seam TUs, single-producer §5.2, core-identical-across-platforms).
* **Core stays frozen except Step 15's probe API.** Any other "the core could just…" impulse goes to `DECISIONS_3.md` as a flagged question, not code.
* The allocator, joystick math, and rate limiter live in a **shared host-side C++ library (`hostmpe/`, outside `core/`)** consumed by both shells (Swift via the existing module-map pattern; Kotlin via the existing JNI layer). One implementation, headlessly unit-tested, zero drift between platforms.
* Every generated byte stream must be valid MPE as your own normalizer defines it — the loopback is a permanent conformance test of both sides. Entering Play mode pushes MCM/RPN0 into the loopback (PHASE4 §5.3) so the normalizer's MPE mode and ±48 bend range are deterministic, never heuristic.
* **`hostmpe/`'s public header must be pure C, exactly like `sumi_core.h`** (no STL types, no C++ across it — the C++ lives behind it): the Swift module-map pattern can only import C headers, and discovering this mid-Step-16 means rewriting the API surface under pressure. The existing `abi_c_compile` pattern gets a sibling: `hostmpe_c_compile.c`.

---

## Step 15 — Layout probe ABI + Play-mode overlay skeleton (iOS)
**Spec sections:** PHASE4 §2 (probe struct + semantics), §1 (mode toggle, playable layouts), §6 (overlay, lattice, echo-row highlight). PROJECT_SPEC §3.4 (layout geometry, echo sets), DECISIONS_2 #7 (semitone axis derivation — the probe exposes exactly this).

* Implement `sumi_layout_probe` in `layouts.cpp` as a pure, **instance-free** free function `(layout, params, aspect, x, y) → cell info` (FIFTHS/rolls return false); callable from any thread — no instance, no engine state. `semitone_dx/dy/step` reuse the #7 neighbor-step derivation. Bump `sumi_version()` to 0.3.0; extend the C11 ABI compile test. Shells keep a params snapshot beside their UI state for probing at touch-down (they own every params write already).
* Golden probe tests: for CHROMA_GRID and JANKO, probe the center of every cell for all in-range notes and assert the returned note round-trips; probe Jankó at all three parity rows of 10 sample notes and assert the same note; assert `semitone_step` against golden values at 2 aspects.
* iOS: settings-sheet mode toggle (Marble/Play, persisted); Play mode renders the faint lattice + touch joystick indicators (circle at origin, thumb dot at Δ_eff) with no MIDI yet — touches only drive the indicator math (soft-knee deadband per PHASE4 §3.2).
* Marble mode must be bit-identical to Step-13 behavior (regression: the touch-gesture script).

**DONE when:** probe golden tests pass headlessly; lattice aligns pixel-perfect with where loopback-MIDI drops later land (verify by scripting `sumi_push_midi` note-ons and overlaying); indicator math unit tests cover the soft knee (g is 0 at d = 0.03, continuous, reaches 1 at d = 1); Marble regression passes; version reads 0.3.0.

---

## Step 16 — hostmpe allocator + loopback Play mode (iOS, fingers)
**Spec sections:** PHASE4 §3 (joystick model, bend scaling), §4 (finger truth-table rows), §5.1 (allocator rules), §5.2 (single producer).

* `hostmpe/`: LRU-by-release round-robin allocator with external-occupancy masking, saturation = silent drop + HUD blink; joystick → MIDI mapping with the semitone-exact bend formula and the **Y→pressure (upward-only) finger mapping** of PHASE4 §3.3 — fingers emit no CC74; unit tests: **one grid column of drag = exactly ±171 bend counts at ±48**; pressure is 0 at touch-down, 0 for any downward Δy, monotonic through the soft knee upward, 127 at full-radius up; allocator never reuses the most-recently-released channel while any other is free; occupancy masking blocks externally-held channels and clears on Note Off / disconnect / 30 s timeout.
* Emit order tests: center-bend-before-Note-On on every strike; pressure-0-before-Note-Off on every lift.
* iOS wiring: touch handlers → hostmpe → byte-triples posted to the serial MIDI queue (sole producer) → `sumi_push_midi`. Synthesized finger velocity (default 96, `majorRadius` modulation behind a setting).
* **Channel-steal test (hardware):** ROLI Piano connected and held (chord across ≥ 4 member channels) while 6 touches play the surface — assert (via a byte-log tap at the merge point) zero touch allocations on ROLI-occupied channels, ROLI voices never truncated, and the canvas shows both performances simultaneously.

**DONE when:** hostmpe unit suite passes headlessly; playing the grid feels in tune — touch-down drops always spawn at the touched cell with centered bend (byte-log assert), one-column drags read as clean semitone glides on canvas; pushing a held touch upward visibly grows that drop's rings (the boundary-growth feed) while neighbors are untouched, and a byte-log confirms zero CC74 messages from finger voices; touch-down → visible drop ≤ 2 frames (signpost/Instruments evidence); channel-steal test passes with the ROLI; 10-finger chords allocate 10 distinct channels and release cleanly (no stuck voices after a 5-minute mash session, `sumi_dropped_midi_count` = 0).

---

## Step 17 — Outbound transports + per-pipe rate limiting (iOS)
**Spec sections:** PHASE4 §5.3 (dual pipe, change-only + ≤ 100 Hz decimation, MCM/RPN0 on connect), §5.4 (iOS transports).

* Dispatcher fan-out: loopback full-rate (unchanged from Step 16); outbound gets change-only filtering + per-voice per-dimension latest-wins decimation ≤ 100 Hz; Note On/Off and initial center bend exempt.
* Transports: virtual CoreMIDI source (which is also the **USB/IDAM primary sink** — add the in-app "plug in, then Enable in Audio MIDI Setup" hint per PHASE4 §5.4), BLE peripheral (pairing sheet), Network Session. Each sends MCM (RPN 6, lower zone 15 members) + RPN 0 = 48 on member channels at session open, on a sink coming up mid-session (IDAM enable detected via CoreMIDI connection callbacks), and via the "Re-sync DAW" settings button.
* **DAW round-trip validation:** record the virtual-source stream in a DAW with an MPE synth — chords keep per-note bends independent, a one-column glide sounds as one semitone (proving the MCM/RPN handshake), release tails unaffected by immediately-following notes (proving LRU allocation audibly).
* **BLE saturation test:** scripted 10-touch expressive storm for 60 s over BLE to a Mac; assert the BLE pipe's global budget holds (outbound ≤ ~300 msg/s, round-robin fairness verified: every active voice's dimensions update within any 100 ms window), end-to-end lag < 100 ms sustained (timestamp compare), zero Note On/Off or center-bend messages dropped, while the loopback canvas stays full-rate smooth and the virtual-source pipe keeps its own 100 Hz/dimension policy (per-transport budgets are independent).

**DONE when:** all sinks enumerate and play in a DAW — including the **USB/IDAM round-trip**: iPad wired to the Mac, Enable in Audio MIDI Setup, record in a desktop DAW, assert the MCM handshake there and touch-to-DAW latency comfortably under the BLE path's (log timestamps; wired should be the best of all sinks); enabling IDAM mid-performance triggers the MCM re-send; the MCM handshake test passes (glide = 1 semitone in the DAW, not 1/24th); BLE saturation test passes with no cumulative lag; toggling transports mid-performance never glitches the loopback; decimation unit tests prove change-only and latest-wins semantics.

---

## Step 18 — Performance control strip (iOS, on the Mac)
**Spec sections:** PHASE4 §8 (widgets, master-channel discipline, never-dropped class, persistence, lattice displacement).

* Strip UI from the joystick primitive: spring wheel (Pitch, master bend ±2, ~50 ms return ramp + guaranteed center), latch wheel (Mod CC1, relative accumulation), momentary/toggle button (Sustain CC64), two assignable latch wheels (long-press editor). Dock top/bottom setting; hidden in Marble mode; lattice + probe coordinates remap to the reduced play area (shell-side).
* Rationale for running before the v0.4 batch: the strip is the test rig for it — once Step 19 lands, the assignable wheels route to `SUMI_CTL_RIPPLE_AMP`/`SUMI_CTL_RIPPLE_FREQ` and CC74-delta pinch, so the new operators get exercised with real knobs on-device instead of desktop key bindings. Nothing in this step depends on v0.4.
* hostmpe: widget value engines (spring ramp, latch accumulation) unit-tested; CC64/buttons added to the never-dropped class in every transport policy; strip re-sends latched values after an MCM re-sync.

**DONE when:** widget unit tests pass (spring always lands exactly at center, latch never jumps on regrasp); playing a DAW pad patch with sustain + mod from the strip while chording feels like a keyboard (subjective gate — you); byte log: all strip traffic on ch 1 only, member channels clean; a BLE storm never drops or delays a CC64 transition (assert in the saturation harness); mod wheel simultaneously stirs the loopback vortex and modulates the DAW synth; strip values survive layout/mode switches and re-announce after "Re-sync DAW".

---

## Step 19 — v0.4 deformation operator batch (core, desktop harness)
**Spec sections:** PROJECT_SPEC §4.3(3–6) (Rankine profile, dipolar wake, Hamiltonian pinch, sine ripple — the math, the invariants, the sub-stepping rules), §4.5 (live-ripple composite path, dip samples un-rippled), §5.3 (`sumi_add_wake`, vortex profile arg, params v0.4, new ctl dims).

* All four operators in `deform.glsl` + `displacement.cpp`; `sumi_add_vortex` gains the profile argument; `sumi_add_wake` with internal ≤ a/2 sub-stepping; pinch and bake-ripple are delta-driven; live ripple is one displaced lookup in `composite.glsl`; `bend_mode` param arbitrates the PER-NOTE bend routing (0 glide, 1 ripple amount — the drop holds; master bend untouched; DECISIONS_3 #35–36: amount = |semis|/6, bend-driven bake drifts φ for permanence, CC-driven bake keeps the group property). Version → 0.4.0; C11 ABI test extended.
* Desktop harness bindings for fast iteration: middle-drag → wake (tip radius on scroll), Shift+drag → pinch (drag distance = k delta, drag angle = fold axis), twist keys → Rankine vs. exponential toggle, keys for ripple A/k/φ and live/bake.
* Prototype both pinch variants (Hamiltonian vs. crossed-tine composition) behind a debug key; pick by eye; log the choice in DECISIONS_3.

**DONE when:** wake orientation test passes (ink ahead of the tip bulges forward, flanks stream backward — screenshot pair); a scripted fast flick (one frame, 8×a displacement) shows no folding or tearing (sub-stepping proven); Rankine core test: a ring cluster inside R survives 20 full scripted rotations rotated-but-unblurred (before/after diff), and the crease ring sits exactly at R; pinch soak (four-part gate, DECISIONS_3 #33 — ink MASS Σphase is the observable; strict area-conservation is unsatisfiable on a bilinear-resampled medium for ANY operator, the incumbent tine included): (a) det = 1 verified symbolically, (b) 500 strong (+k, −k) pairs invert analytically and hold ink mass to ±0.5%, (c) zero fabrication — mass never grows > 0.5% (edge ingress rule extended to wake/pinch/ripple-bake passes), (d) per-pass erosion ≤ 2× the glide-tine baseline under the identical stream; ripple group test: a scripted LFO on A (fixed k, φ) returning to zero leaves the field **bitwise identical** (readback compare); live-vs-bake: live ripple during a dip exports the un-rippled print; bend_mode toggle: flipping it mid-performance moves the note bend cleanly between glide and ripple with no double-consumption and no stale-delta tines on the way back (byte-scripted: one member-channel bend sweep emits glide tines XOR ripple passes — `test_bend_mode_single_consumer`); ripple permanence: a bend-driven vibrato ending at center leaves a permanent far-field record while the CC-driven group test still composes back (DECISIONS_3 #36); all Step 3/5 sharpness and stress regressions pass.

---

## Step 20 — Lamb–Oseen swirl & bipolar press (core + iOS, desktop first)
**Spec sections:** PROJECT_SPEC §4.3(7) (the operator, expm1 guard, r_c = R, parity sign), §3.3–§3.4 (VoiceSwirl, press_mode arbitration, budget/echo rules), §2.1 (0xA0 decode); PHASE4 §3.3 (bipolar Y), §4 (truth table), §5.3 (0xA0 policies).

* Core: Lamb–Oseen pass (expm1 + small-r limit), 0xA0 decode → `VoiceSwirl`, `press_mode` routing of 0xD0 (one consumer), Γ ∝ smoothed amount × dt, r_c = voice boundary R, parity-signed rotation, echo fan-out + budget accounting. Desktop harness keys for swirl amount and press_mode; version stays inside uncommitted 0.4.0.
* hostmpe: bipolar Y engine (one radial knee, half-axis dispatch to 0xD0/0xA0); outbound treats 0xA0 as a continuous dimension (change-only, per-transport budgets, BLE fairness).
* iOS: the surface's down-pull plays the swirl; settings gains the press_mode row for hardware routing.

**DONE when:** small-r stability test passes (θ finite and smooth for r → 0, expm1 path verified against the analytic θ(0) limit); a held swirl at constant amount rotates the voice's own rings near-rigidly (core coherence — before/after ring sharpness diff inside r_c) while stirring neighbors; two adjacent notes swirling counter-rotate (parity sign, visual); hostmpe bipolar unit tests pass (center = both zeros, up emits only 0xD0, down emits only 0xA0, knee continuous through center); byte log of a down-pull shows 0xA0 on the voice's member channel with its note number; press_mode = 1 on the desktop routes a scripted Osmose-style 0xD0 stream into swirls with zero grow passes (one-consumer assert); Step 19's soak/regression suite re-passes with the new operator in the build.

---

## Step 21 — Stylus legato, wake & pinch (iOS, on the Mac)
**Spec sections:** PHASE4 §7 (legato per layout, re-anchor rule, wake-not-in-MIDI invariant, pen pinch), §4 (stylus truth-table rows), §3.3 (CC74 stylus-only).

* Pencil absolute-position play: continuous legato on CHROMA_GRID/JANKO (bend from probe axis, re-anchor at ±47 st on the same channel); cell-quantized legato on PIANO_GRID (20–40 ms retune ramp, dead zones sustain). hostmpe grows the legato engine behind the pure-C header, headlessly tested (golden bend traces for scripted pen paths, including a re-anchor crossing and a dead-zone crossing).
* `sumi_add_wake` on every pen stroke segment (both modes), tip radius from pen force; `slide_mode = 1` routes smoothed CC74 deltas → pinch at pen position, fold axis from azimuth; tilt → assignable CC and hover ghost cursor as previously specced; palm rejection unchanged.

**DONE when:** golden legato traces pass; a slow pen sweep across 5 octaves on CHROMA_GRID sounds continuous in a DAW with exactly one audible re-anchor seam (byte log shows the same-channel retrigger with centered bend); on PIANO_GRID the same sweep steps through cell pitches with clean 20–40 ms ramps and sustains across the E–F gap; wakes visibly trail every pen stroke while the DAW recording, played back through the loopback, reproduces notes/bends/pressure but no wakes (the documented invariant, demonstrated); pinch folds follow azimuth rotation on canvas; 5-minute pencil+fingers session: no palm misfires, zero stuck notes.

---

## Step 22 — Android port (on the Linux box)
**Spec sections:** PHASE4 §4 (Android truth-table rows), §5.2 (Android producer path), §5.4 (Android transports — USB gadget is the primary sink), §6 (Compose overlay), §7–§8 (legato, wake, strip — full parity). DECISIONS_2 #32–#34.

* Compose overlay port including the control strip and S-Pen legato/wake/pinch parity (S-Pen pressure/tilt/orientation/hover mapped like Pencil); hostmpe via JNI (same pure-C header); touch bytes through the AMidi poller thread (single producer per DECISIONS_2 #33); fingers bipolar Y (0xD0 up / 0xA0 swirl down) identical to iOS; synthesized velocity with `getTouchMajor()` modulation behind a setting.
* Transports: **USB gadget MIDI as primary** (status surfaced: active / charge-only / unsupported; `MidiManager` port on USB mode flip), `MidiDeviceService` virtual device, BLE advertise where supported. MCM/RPN0 + strip re-announce on every sink open, including a USB mode flip mid-session.
* Parity: full hostmpe suite (allocator + legato + widgets) runs in the Android build; channel-steal test re-runs with the ROLI over BLE.

**DONE when:** the tablet, USB-wired to the Linux box in MIDI mode, appears in `amidi -l` / a Linux DAW, the MCM handshake passes there, and touch-to-DAW latency beats BLE; S-Pen legato golden traces pass on-device; the strip drives a Linux DAW (sustain never sticks under storm); the Android surface passes the full hostmpe suite + channel-steal test; an on-device Android DAW receives the virtual device with a passing handshake; a 10-minute mixed session (surface + S-Pen + ROLI + strip + Linux DAW over USB) ends with zero stuck notes, zero dropped loopback messages, stable memory.

---

## Deferred (explicitly out of Phase 4 scope — do not implement)
Playable FIFTHS/roll layouts ("play the now-line"), the surfactant brush (a `layer_type` variant of the grow consumer — the traditional dispersant second brush; discussed and parked in favor of the Lamb–Oseen swirl), a bipolar per-note surfactant axis via a member-channel CC, per-touch release-velocity estimation beyond lift speed, MIDI 2.0/UMP output, preset persistence for CC maps and strip assignments, desktop touch support, haptics.

---

# Part 4 — v1.0 (steps 23–33: Packaging, Release, Web & Documentation — formerly `_work/ROADMAP_4.md`)
**Companions: `PROJECT_SPEC.md` §9 (the Phase-5 spec, folded), `DECISIONS.md` Part IV (`DECISIONS_4 #n`), `CHANGELOG.md` (v0.5.0 and v1.0.0). References below to `PHASE5 §n` mean `PROJECT_SPEC.md` §9.n; `DECISIONS_4.md` means Part IV.**
**Historical: Steps 23–33 are DONE (23–28 on the Mac, 29 on the Windows box, 30–31 on the Linux box, 33 on all three; Step 32's beta wave ran on the store test tracks). Step 34 — the 1.0 tag — is the promotion of Step 33's final release candidate.**
**Scope: Steps 23–34. ONE PLATFORM PER STEP except 33 — each platform's agent runs on its machine. Hard ordering: WebGPU/web (25) → docs (26) → release lanes (27–31, order-flexible among themselves) → beta (32) → feedback incorporation (33, the only step that loops) → release (34). The docs precede every lane because the lanes and store listings hardcode the site's homepage, privacy-policy, and support URLs; 1.0 is the promotion of 33's final release candidate, never a fresh build.**

---

## Working Rules (apply to every step)

* All prior working rules hold. The core stays frozen except Step 30's WebGPU seam (one swapchain TU + shdc dialect — the Step 11/12/14 pattern, fourth verse).
* **One platform per step.** A step never touches a second platform's build, signing, or store. Shared code (settings UI, workflow spine) is authored once in its designated step; other platforms' steps consume and verify it, never rewrite it.
* **The author's existing infrastructure is an input:** Developer ID, ASC app records, Play account, the tap, the apt repo + GPG key, prior winget manifests. Each step lists the credentials it needs at the top so the author stages secrets before the session; no step creates accounts.
* Version strings come from the git tag via CI injection — no hand-edited version numbers anywhere.
* Evidence condenses into `CHANGELOG.md`; release notes generate from it. Store submissions and beta promotions are HUMAN actions by design; CI stops at "uploaded."

---

## Step 23 — Desktop productization (macOS machine)
**Spec:** PHASE5 §2. **Credentials:** none (ad-hoc signing only).

* Product naming decision (app name, bundle id, cask/winget/deb package names) recorded in DECISIONS_4 first.
* The shared settings window (one implementation, toolkit-minimal): the iOS sheet's rows, CC-map editor, MIDI port list + rescan status, dip button. `--dev` gates every debug binding/tool; About = version + commit; first-run hint.
* macOS `.app` bundle (Info.plist, icons from `tools/gen_icons.py`, hardened-runtime entitlements); the runtime Dock-tile hack retires.
* Windows/Linux compile of the shared UI is CI-verified here but NOT exercised — their lane steps own that.

**DONE when:** on macOS the app launches to a playable instrument with zero debug keys active, every setting mouse-reachable, `--dev` restores the lab bench; the bundle passes `codesign --verify` ad-hoc; a volunteer who has never seen the repo connects the ROLI and marbles within two minutes using only the first-run hint.

---

## Step 24 — Release orchestration spine (platform-neutral, any machine)
**Spec:** PHASE5 §3. **Credentials:** none (dry-run only).

* The tag-triggered workflow's spine: version injection from the tag, the pre-package gates (ABI/C11 + hostmpe suites, per-backend §4.6 field regressions), CHANGELOG→release-notes generation, artifact upload scaffolding, and a `--dry-run` dispatch mode. **Contains zero platform lanes** — the CI lane steps (25 web, 27 macOS, 29 Windows, 30 Linux) each add exactly one job to this spine; iOS and Android (28, 31) are deliberately MANUAL procedures with no lane job, only PR compile checks.
* A deliberately broken field-regression fixture must block packaging (the gate proven red before it is trusted green).

**DONE when:** dry-run on a test tag runs gates on all runners, produces a versioned notes draft, uploads nothing; the broken-fixture test blocks the pipeline; the lane interface (job template + artifact naming contract) is documented in the workflow file for the CI lanes (25, 27, 29, 30).

---

## Step 25 — WebGPU backend & marble web (desktop machine)
**Spec:** PHASE5 §5 (whole section); PROJECT_SPEC §4.6 (web tier joins the field regression).

* Core: WGSL dialect in the shdc pipeline; `swapchain_webgpu.cpp` (SOKOL_IMPL host); Emscripten target with the C-ABI as wasm exports. Shell: JS host page (device/swapchain, RAF loop, resize, pointer/touch → marble gestures, pen pointer-events → wake with pressure where reported), WebMIDI input on Chrome/Edge with silent gestures-only degradation elsewhere.
* Scene/embed API per spec (`?scene=…&param=…`, minimal-chrome embed) covering every operator with slider-mapped formula symbols; Pages deploy joins the Step-24 spine as the web lane (the first lane — it must exist before the docs step publishes).

**DONE when:** the web tier passes §4.6 within its documented tolerance; touch-playable on iPad Safari, mouse-playable on desktop Chrome, the ROLI drives it over WebMIDI in Chrome; every operator scene works via query params; first marble < 5 s on a mid-range phone (Lighthouse evidence); Safari shows gestures-only with no error banner.

---

## Step 26 — Documentation site (any machine; needs Step 25)
**Spec:** PHASE5 §6 (five books + the chart + citations).

* Static site + KaTeX, CI-deployed. User guide; **The Operators** (formula, invariants, ownership rule, live Step-25 embed per page — the wake and swirl pages each carry their one-line Jaffer-lineage note); Architecture; **Performance gallery** (author-recorded videos, captioned device/layout/modes; the **Jaffer tribute performance** — exact piece identified from his video credits, public-domain composition verified, performed by the author on the instrument; optional Turkish-moire scripted scene as a gallery↔operators bridge); Design notes (DECISIONS 1–4, lightly edited) + rendered CHANGELOG.
* **Store-required pages at stable URLs** (PHASE5 §6): privacy policy (the app collects nothing — plus each console's required clauses), support/contact page, and the homepage itself — these URLs are hardcoded by every later lane and store listing, so they are frozen here and recorded in DECISIONS_4.
* The MIDI implementation chart, verified against real byte logs (one session per mode), including what a DAW recording does not capture (the wake invariant, for users).
* Citations page per spec: Lu, Jaffer, Jin, Zhao & Mao (IEEE CG&A 2012); Jaffer, "Oseen Flow in Paint Marbling" (arXiv:1702.02106); Jaffer, "The Lamb–Oseen Vortex and Paint Marbling" (arXiv:1810.04646); his CSAIL marbling pages; Lamb's *Hydrodynamics*; Rankine (1858); the MMA/AMEI MPE specification — details verified against the publications during writing, never from memory.

**DONE when:** every operator demo is the release wasm via the embed API (grep the site: no second implementation); the chart matches the byte logs; the gallery plays with correct captions and the tribute video is in place with its piece identified and its public-domain status verified; all citations resolve; the privacy/support/homepage URLs are live and recorded (lane prerequisite); the site deploys from the same tag as the artifacts. Gallery videos are an author input — the step ships with whatever performances are recorded by then and the gallery accepts additions without a site rebuild.

---

## Step 27 — macOS release lane (macOS machine)
**Spec:** PHASE5 §3. **Credentials:** Developer ID cert, notary API key, tap repo token.

* Lane job: universal (arm64+x86_64) bundle → sign → notarize → staple → DMG → release asset; cask bump PR to the author's tap (version/sha256/URL per the tap's existing conventions; `homepage` = the Step-26 URL).

**DONE when:** a test tag yields a Gatekeeper-clean DMG on a fresh macOS user account and an auto-opened cask PR that `brew install --cask` succeeds from; About shows the tag version.

---

## Step 28 — iOS release procedure (macOS machine, iOS agent) — MANUAL BY DESIGN
**Spec:** PHASE5 §3–4. **Credentials:** the author's existing Xcode signing setup (no CI secrets).

* **No CI lane** — the author archives and uploads by hand. This step's deliverable is the verified procedure: the Xcode project archives cleanly from a tagged checkout with the CI-injected version (Archive → Distribute → TestFlight), documented as an `ios/RELEASING.md` checklist (checkout tag, version sanity, archive, upload, what to click in ASC); store metadata (screenshots from real sessions, both modes described, MPE controllers named; privacy-policy, support and marketing URLs = the Step-26 pages) staged in the repo for the beta wave. PR CI keeps a build-only compile check for iOS so tags never surprise the archive.

**DONE when:** the author has walked the checklist end-to-end once from a test tag: a processing-complete build in TestFlight at the tag version, installed on the author's iPad; the checklist contains every step actually taken (audited against reality, not intention).

---

## Step 29 — Windows release lane (Windows machine)
**Spec:** PHASE5 §2–3. **Credentials:** code-signing cert if available; winget-pkgs fork token.

* Verify + polish the shared settings UI and `--dev` on Windows (this step owns any Windows-only fixes); installer + portable zip; signing-if-cert-present (unsigned still releases; SmartScreen consequence documented in the README, not fought); winget manifest bump via wingetcreate PR per the author's existing manifest conventions.

**DONE when:** a test tag yields an installer that runs on a clean Windows VM, `winget install` succeeds from the bumped manifest, the settings window and WinMM MIDI work, About shows the tag version.

---

## Step 30 — Linux release lane (Linux box)
**Spec:** PHASE5 §2–3. **Credentials:** apt repo GPG key.

* Verify + polish the shared settings UI and `--dev` on Linux (X11 + Wayland); deb + tarball; publish to the author's apt repo (Release/InRelease re-signed) from the lane job. **Flatpak spike, timeboxed to half the session:** manifest + ALSA MIDI through the sandbox on one distro — clean → its own publish hook; not clean → verdict in DECISIONS_4, closed.

**DONE when:** a test tag yields `apt install <name>` working on clean Ubuntu with the ROLI playable over ALSA, the .desktop/icon integration intact (Step-14 work preserved), and the flatpak verdict recorded either way.

---

## Step 31 — Android release procedure (Linux box, Android agent) — MANUAL BY DESIGN
**Spec:** PHASE5 §3–4. **Credentials:** the author's existing keystore in Android Studio (no CI secrets).

* **No CI lane** — the author builds and uploads by hand. Deliverable: the verified procedure — Android Studio builds a signed .aab from a tagged checkout with the CI-injected version (Build → Generate Signed Bundle), documented as an `android/RELEASING.md` checklist (checkout tag, version sanity, signed bundle, Play Console internal-track upload); store metadata staged like iOS's (privacy-policy and support URLs = the Step-26 pages). PR CI keeps a build-only compile check for Android. The closed-testing wave is Step 32.

**DONE when:** the author has walked the checklist end-to-end once from a test tag: a build live on the Play internal track at the tag version, installed on the author's tablet with USB-MIDI to the Linux box working; the checklist audited against what was actually done.

---

## Step 32 — Store beta wave (both stores; human-heavy; needs Steps 26–31)
**Spec:** PHASE5 §4.

* TestFlight external group + Play closed track opened on the current tagged builds; the docs site (live since Step 26) is the linked user guide and privacy/support source from day one. Feedback instrument live (the three questions); crashes triaged from the store consoles into issues. Runs ≥ 2 weeks wall-clock.

**DONE when:** one full wave completed; every piece of feedback triaged into the Step-33 backlog with a class (crash / bug / UX-feel / docs); docs-class items ship to the site immediately (the guide is measured, not backlogged); ≥ 5 external testers actually played (store metrics). Fixing app-side items is NOT this step — it is Step 33's whole job.

---

## Step 33 — Feedback incorporation & release candidates (iterative; any machine per item)
**Spec:** PHASE5 §4. The only step allowed to loop.

* **Scoped unfreeze:** bugfix-scoped changes are permitted ANYWHERE, core included — under the bug → regression-test → fix pattern (every fix lands with the test that would have caught it), and every core touch keeps the full suites green (§4.6 field regressions per backend, hostmpe, ABI). Feature work remains frozen; UX-feel items take the smallest change that addresses the report and need the author's sign-off (taste is not delegable).
* Triage classes from Step 32: crashes (must fix), bugs (fix, or waive with rationale in DECISIONS_4), UX-feel (smallest change + author sign-off, or waive), docs (already shipped in 32).
* **Each fix batch → a release-candidate tag** → the CI lanes fire and the author walks the mobile RELEASING checklists → the RC lands on TestFlight/Play test tracks → **the original reporters confirm** ("does this fix it for you?" via the feedback thread). Substantial batches may warrant a one-week mini-wave; the author bounds the iteration.
* The step ends by nominating a **final RC**: the exact build 1.0 will be.

**DONE when:** zero open must-fix items (every crash and bug fixed-with-test or waived in DECISIONS_4); every fixed report is reporter-confirmed or confirmed-unreachable; all suites green on the final RC across all platforms; the final RC has sat on both test tracks for ≥ 3 days with no new must-fix reports; DECISIONS_4 records the RC → 1.0 nomination.

---

## Step 34 — 1.0 release
**Spec:** PHASE5 §1, §4.

* **1.0 is the promotion of Step 33's final RC — zero code changes between the RC and this tag** (the tag moves; the build is the one the testers confirmed). Tag `v1.0.0`: the spine + CI lanes fire (desktop three + web); channels bump; docs + web deploy from the same tag. Human actions in order: walk the iOS and Android RELEASING checklists from the tag (or promote the RC builds already on the test tracks where the consoles allow), verify the five installables, promote TestFlight → App Store review and Play closed → production, publish the GitHub Release, send Professor Jaffer the note with the link (and the tribute video).

**DONE when:** all five platforms install 1.0.0 through their normal channels; docs + marble web are live at the release tag; both store listings are public or in review with nothing pending on the author's side; the changelog's 1.0 section is the notes everywhere; the note to Professor Jaffer is sent.

---

## Deferred (explicitly out of Phase 5 scope — do not implement)
Play mode on the web (WebMIDI output + wasm hostmpe), localization, auto-update, telemetry, Microsoft Store, Flatpak beyond the spike verdict, a **prebuilt libsumi SDK** (dylib/.dll/.so + `sumi_core.h` as a versioned release artifact — deliberately post-Phase-6: publishing binaries makes the ABI a public contract, and the probe-state redesign is a scheduled break; prerequisites when the time comes: settled ABI, an embedding license decision, semver + deprecation policy, the Architecture book as its manual; note the shaders are baked in via sokol-shdc headers, so no separate metallib/shader artifacts are ever shipped), and all Phase 6 instrument layouts (trumpet valves, trombone slide, Wicki–Hayden, fretboard, theremin) — with the standing architectural note: valve/slide layouts are STATEFUL, and the instance-free probe needs a designed state input before any of them are attempted.


---

# Part 5 — v2.0, Phase 6 (steps 35–46: Medium — formerly the Phase-6 section of `_work/ROADMAP_5.md`)
**Companions: `PROJECT_SPEC.md` (the medium's section is drafted for it in `specs/TO_PROJECT_SPEC.md` — references below to `MEDIUM §n` and `QOL §n` mean the phase's `specs/MEDIUM_SPEC.md` and `specs/QUALITY_OF_LIFE_SPEC.md` as they stood, in git history; `SPEC §n` is `PROJECT_SPEC.md`), `DECISIONS.md` Part V (`DECISIONS_5 #n`), `CHANGELOG.md` (v2.0.0, pre-release alpha.1).**
**Historical: Steps 35–46 are DONE under the author's numbering — 35–43 on the Mac, 44a iOS on the Mac, 44b web on the Mac, 45a Linux and 45b Android on the Linux box, 46 Windows on its box, then the fixes the author asked for (DECISIONS_5 #86–#88) and the phase close (#89). ROADMAP_5's draft numbered the shells 44 / 45 / 46 and had no desktop-verification steps; the two are added below.**

## Working Rules (apply to every step)

* All prior working rules hold. The core is reopened for FEATURE work by this roadmap in Phases 6 and 8 only; Phase 7 never touches `libsumi` (the engine stays audio-free — SOUND §1); Phase 9 reopens it for fixes under bug → regression test → fix. Core changes prove out on the desktop harness FIRST, every time.
* **The phase invariant:** `tests/fixtures/field_512_metal.bin` stays BITWISE on Metal from step 35 to step 66. New operators add passes, media change the composite, layouts change the probe — none touches an existing pass. A step that believes it must change the fixture stops and records the decision first.
* **Every operator declares its class** (MEDIUM §2 table) in its header comment, its test and its operator-book page: *exact* (det J = 1 at any magnitude; proven by a ±k inversion golden) or *sub-stepped displacement field* (soaked under the wake's ≤ a/4 rule and the four-part conservation gate of step 35). Membership is declared, never discovered in a failing soak.
* The delta rule (continuous controllers drive deltas per pass) and the one-consumer rule (`bend_mode`, `slide_mode`, `press_mode`) are unchanged; media add *defaults* for them, never a second consumer.
* **One platform per step.** Core and shared UI are authored on the desktop harness (the Mac); iOS on the Mac; Android and Linux on the Linux box; Windows on its box. A step never touches a second platform's build or store; the other shells consume in their own steps. **Sanctioned exception — verification fan-out:** a step may have OTHER boxes re-run an already-green suite unchanged (step 55's pattern); authoring stays single-platform.
* **Composed gestures inherit the strictest class of their members:** a composition containing a sub-stepped pass (the spark's burst component) gates under the sub-stepped family's numbers, even when its other members are exact.
* **The ABI event is ONE step (41).** Before it, growth is additive only (new enum values, new `sumi_add_*`/ctl dims, appended params fields — the Step-33 minor-bump pattern, `sumi_version` 0.10, 0.11 …); after it, additive only again. `libsumi` becomes **1.0.0** at the break. The prebuilt SDK stays deferred until Phase 9 asks the question.
* Evidence per step under `docs/evidence/<step>/`; at each phase end the fold: that phase's `_work/DECISIONS_<n>.md` merges into `docs/DECISIONS.md` as the next Part, evidence condenses into `CHANGELOG.md` and leaves the tree (git keeps it), scripts worth keeping move to `tools/`. `site/scripts/build-notes.mjs` renders `_work/DECISIONS_{5,6,7}.md` while in flight — Phase 9 extends the loop to 8.
* **Documentation timing:** guide fixes ship to `main` at any time (`pages.yml`). Pages for NEW operators, layouts and Voxo are drafted in the step's evidence folder (the burst page in the author's voice) and move into `site/` in step 63 — the live demos would otherwise point at scenes the released wasm does not know.
* **Pre-release tags** end Phases 6, 7 and 8 (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a pre-release, and the lanes stay proven); the author installs the build on every device and plays it. Phase 9 uses `v2.0.0-rc.N`. Nothing reaches a stable channel before step 66.
* Credentials: Phases 6–8 need none beyond the machines; Phase 9 reuses the Phase-5 set (Developer ID, ASC, Play, tap token, winget token, apt key). Author inputs (recordings, taste sign-offs, the demo instrument) are listed per step so they can be staged before the session.

---

## Phase 6 — Medium (steps 35–46): the order inside the phase
**Order inside the phase:** the conservation gate (35) before any sub-stepped operator; torsion (36) first because it is exact and cheap and proves the pass plumbing; Chladni (37), burst (38), spark (39), Chirikov (40) as MEDIUM §5 sketches; the ABI break (41) once all five operators exist additively; the Anod composite and binding tables (42); palettes/presets/prints authored once (43); then the shells (44 iOS, 45 Android, 46 web) consume.

## Step 35 — Phase opening & the conservation gate (desktop machine)
**Spec:** MEDIUM §2 (class table); SPEC §4.1, §4.3(5); DECISIONS_3 #33 (the four-part gate). **Author inputs:** the medium's name (Anod — decided 2026-09-21), the release number (2.0.0), the slide-CC resolution (see Phase 8 head).

* Open `_work/DECISIONS_5.md`; its first entries record the decisions above and this roadmap's provisional resolutions the author confirms (the list at the end of this file).
* Generalise the step-19 pinch soak (`t19_pinch_soak` in `desktop/src/dev_tools.cpp`, reached through `dev_run_scripted`) into a **per-operator four-part gate**: (a) det J = 1 stated symbolically per operator (a header comment with the derivation, or the class declaration for sub-stepped fields); (b) ±k pass pairs invert — ink mass (Σ phase) held to ±0.5% over 500 strong pairs; (c) zero fabrication — mass never grows > 0.5% under the gesture-rate stream; (d) per-pass erosion ≤ 2× the glide-tine baseline under the identical stream. Parameterised by operator and driven through the REAL ctl/gesture path on the fixed 512² scripted clock; one `--dev` flag per operator soak; `tools/soak_report.py` turns the printed lines into the evidence table.
* **Baselines** recorded for every v1.0 pass (glide tine, pinch saddle and crossed, wake doublet and Stokeslet, ripple bake, Lamb–Oseen swirl): these are the numbers every new operator is compared against.
* **Proven red before trusted green:** run the pinch soak with the ingress mask disabled (the #33 fabrication, +9.5% over 12 000 passes) and watch (c) fail.

**DONE when:** one command soaks any operator and prints the four verdicts; the baseline table is in the evidence; the red run is in the evidence; `DECISIONS_5.md` exists with the opening entries; the fixture is bitwise (nothing in the core changed).

---

## Step 36 — Wave torsion, the proof brick (desktop machine)
**Spec:** MEDIUM §2.1.

* θ′ = θ + A·sin(k·r − φ)·e^{−γr}, r′ = r as a **third vortex profile** (`SUMI_VORTEX_TORSION`) — the vortex passes already rotate by a profile of r, so this is one enum value and one profile function; it is reachable today through every vortex route (mouse right-drag, the vortex ctl trio with the profile setting, `sumi_add_vortex`). The `[ITERATE: profile vs standalone]` resolves as: profile in the core, its OWN page in the operator book (the swirl's precedent).
* k and φ as additive ctl dims (per-note bend → k under the Anod table later).
* **The engine's first "episode":** the note-on outward phase sweep (φ = ωt with decay) as a per-voice, time-driven emitter of per-frame deltas within the pass budget — the pattern the burst's age envelope (38) and the spark's decay (39) reuse. Enabled by an additive params switch until the binding tables (42) own it.
* `--dev` key; web scene `torsion` (A, k, γ sliders, the sweep as `pace`); page draft in the evidence.

**DONE when:** class exact declared; the ±A inversion golden holds to noise (headless); the episode emits deltas, never absolutes; the scene runs; the fixture is bitwise; `sumi_version` bumped additively.

---

## Step 37 — Chladni lattice (desktop machine)
**Spec:** MEDIUM §2.2; SPEC §4.3 (the ripple's two insertion points).

* The quadrature kick-drift pair x₁ = x + (A/k_y)cos(k_y·y)cos(ωt); y₁ = y + (B/k_x)cos(k_x·x₁)sin(ωt) as a pass at BOTH insertion points: **live** (composite-side, breathing, nothing accumulates) and **bake** (delta-driven into the field). Inverse solved y first, then x — the ordering that makes det J = 1 exact. The "simultaneous" variant is written as a NEGATIVE test that must fail the inversion golden, so it can never be "simplified" into being.
* **Harmony as geometry:** k_x : k_y from the two lowest sounding voices, recomputed on voice begin/end and smoothed (the §2.2 proposal), with a fixed-ratio override for scenes and the desktop bench; A, B from poly pressure through additive ctl dims.

**DONE when:** exact declared; ±A inversion golden; the bake path passes the four-part gate (the #33 ingress rule applies to it as to the ripple bake); the live path leaves the dip un-shimmered (the ripple's own test, reused); scene `chladni` with 3:2, 4:3, 5:4 presets; fixture bitwise.

---

## Step 38 — Viscous multipole burst (desktop machine)
**Spec:** MEDIUM §2.3. **Author input:** the serendipity note's wording (the page draft is written in the author's voice and signed by the author).

* **Order fixed:** (1) `tools/multipole_verify.py`, the sibling of `stokeslet_verify.py`: χ(S), Φ_m(S), the divergence-free check of **d**, the elementary form for m ≥ 2 (and the E₁ kernel that excludes m = 1), the r → 0 hyperbolic-strain limit (the pinch) and the cos(mθ)/r far field — all numerically; the **literature check** recorded in the evidence (viscous multipole vortex decay, impulsive Stokes flows) BEFORE any wording that could imply priority. (2) The pass in the wake's family (`core/src/displacement.cpp`): ≤ a/4 sub-steps, the dual-time Gaussian core, D₂ normalised from the peak lobe displacement at r = a, χ via expm1 (the Lamb–Oseen small-r lesson), θ₀ from pen azimuth or glide direction, m from pitch class (quadrupole default; the table is a params-side lookup resolved by ear in 42). (3) The **age envelope**: ℓ² = a² + 4νt grows over the release, per-frame increments through the step-36 episode pattern.
* `sumi_add_burst(x, y, a, D, theta0, m)` for gestures; the strike route waits for the binding tables.

**DONE when:** class sub-stepped declared; the four-part gate holds under a strike stream at gesture rate (thousands of strikes, the wake's numbers as the bar); the verify script and the literature note are in the evidence; scene `burst` with the age slider; fixture bitwise; the page draft carries the lineage line and the author's note.

---

## Step 39 — Spark shear & the composed strike (desktop machine)
**Spec:** MEDIUM §2.4.

* The tri-wave kick-drift shear pass, frequency stacking (k, 2k, 4k) with the stack depth in a params field, decaying episodes (A, B ∝ e^{−t/τ}). The exactness test runs with a triangle AND a noise profile — the printed statement "shears invert for any profile" becomes a test.
* **The spark composition:** one exact drop pass (the Joule blast — radial outflow cannot be divergence-free, the oldest operator solves it) + rotated quadrupole burst sub-passes + the shear episode, as one `sumi_add_spark` gesture and as a strike-route candidate for 42. CC74 → k prepared as a slide-mode-style default.

**DONE when:** the spark SHEAR declares exact (±A inversion golden, triangle and noise profiles); `sumi_add_spark` gates as **sub-stepped by inheritance** (its burst component — the strictest-member rule) under the wake-family numbers; the four-part gate under episode streams; scene `spark` (and the composition visible in it); fixture bitwise; page draft.

---

## Step 40 — Chirikov standard map, the boss gate (desktop machine)
**Spec:** MEDIUM §2.5. **Author input:** the erosion budget vs. depth-into-chaos call, made with the numbers in hand.

* y′ = y + K·sin(k·x); x′ = x + ε·y′ (the ε-scaled drift is mandatory); K **delta-driven** from the mod-wheel/breath ctl through a new ctl dim.
* **The erosion soak:** sweep per-pass K under the four-part gate, find where (d) breaks, clamp the ceiling in the core, and put the `[ITERATE]` (budget vs. how deep into chaos the instrument may go) to the author as a table, not a paragraph.
* Scripted scene `chirikov` with K on a slider through Greene's threshold (≈ 0.9716): smooth sheets below, filamentation and island chains above.

**DONE when:** exact declared; the soak table is in the evidence and the ceiling is clamped and recorded in `DECISIONS_5`; the KAM transition is visible in the scene; fixture bitwise; page draft.

---

## Step 41 — The ABI event: libsumi 1.0.0 (desktop machine; the Mac compiles iOS the same session)
**Spec:** MEDIUM §1; INSTRUMENT §1; QOL §1. **The one break of the arc.**

* `sumi_params_t` gains `uint32_t medium` (`SUMI_MEDIUM_SUMI` = 0, `SUMI_MEDIUM_ANOD` = 1). `sumi_layout_probe` gains `const sumi_layout_state_t* state` (NULL or zeros = stateless; the struct exactly INSTRUMENT §1's, reserved words included). `sumi_cell_info_t` gains `uint32_t flags` (bit 0 = continuous, reserved for the theremin — the `[ITERATE: sentinel vs flags]` resolves as flags). `sumi_set_palette(inst, const sumi_palette_t*)` with the POD of QOL §1 and `SUMI_PALETTE_CUSTOM`. `sumi_layout_t` values 8–12 named and RESERVED (`sumi_set_params` clamps them until Phase 8). `sumi_version` → **1.0.0**; the header carries the migration note.
* Every call site updated mechanically: desktop, `hostmpe`, the iOS shell (compiled on the Mac in-step), the Android JNI (compiled by the Linux box as the first line of step 45), the web shim `sumi_web_probe`, the C11 ABI compile tests and their version pin.
* **Behaviour unchanged by construction:** medium 0 renders bitwise as 0.9.0 — the field gate proves the field, and a composite screenshot compare (new `--dev` tooling on the scripted clock) proves the pixels.

**DONE when:** `sumi_version()` reads 1.0.0; (the step-45 first line verifies ON DEVICE — main is never red between 41 and 45); all desktop suites green; the iOS project compiles; the web builds and `tools/web_gate.mjs` passes; the fixture and the composite screenshot are bitwise on Metal; `DECISIONS_5` records the break and why the probe state ships before any stateful layout.

---

## Step 42 — The Anod medium (desktop machine)
**Spec:** MEDIUM §1 (switching), §3 (composite), §4 (binding tables). **Author input:** an hour on the ROLI Piano + Airwave in Anod, then the binding table signed by eye (there is no sound until Phase 7 — the medium is judged visually); the burst's m-by-pitch-class table by eye, revisited by ear once Voxo lands (a 55-adjacent check).

* The composite branches per medium. **Strain-glow:** finite-difference the stored source coordinates → ‖J‖_F; charge phase → filament banding; aux → per-event hue; the **ingress mask** excludes fresh water (a scroll seam is a discontinuity, not strain); near-black substrate with screen-locked grain (SPEC §4.5 invariant, composite side).
* The three Anod palettes (electric blue/violet, plasma orange, phosphor green) continuing the palette ids.
* **The default binding tables** (MEDIUM §4) as medium-scoped defaults for `bend_mode`/`slide_mode`/`press_mode` and the strike route — the CC map and the mode params override exactly as today. The spark composition becomes Anod's strike; torsion's sweep its press feed (or not — the `[ITERATE]` is answered by playing).
* **Live switching prototyped and decided** (feature vs. forced dip); the same recorded session re-read in both media is the evidence either way.
* Prints styled per medium (an Anod dip is "the photograph"); the drop-edge question — every ring boundary and seam WILL glow in the first prototype — answered in `DECISIONS_5` by taste, not on paper.

**DONE when:** the Sumi composite is bitwise vs. step 41; a recorded session re-reads in Anod; the binding table is signed; every `[ITERATE]` in MEDIUM §1/§3/§4 is resolved in `DECISIONS_5` or explicitly carried to the Phase-9 beta with its question.

---

## Step 43 — Palettes, substrate, presets & prints — authored once (desktop machine)
**Spec:** QOL §1, §2, §3, §4.

* **Palette model:** 2–8 linear-RGB stops + the medium's ink-depth/glow curve (the identity guardrail; the `[ITERATE: curve fixed vs advanced fold]` resolves as fixed in 2.0, the fold deferred); the built-ins re-expressed as presets THROUGH THE SAME PATH (one code path, bitwise-checked); colour-blind-considerate presets; **the morph ring with a custom palette** (missing from QOL §1 — decide: the ring includes the custom slot, or morph is disabled while one is active); aux hue drift as a palette field or global (decide). The editor in the shared settings window with live preview on the running canvas.
* **Substrate:** paper tint / roughness / fiber presets (Sumi); darkness / grain (Anod). Composite side by construction.
* **Presets:** params + CC map + strip assignments + custom palette + layout-state defaults, JSON, **one serializer** — host-side, pure C, beside `hostmpe` — so four shells share one code path and cannot drift; export/import; the last session restores on launch; the schema rule (stamped with `sumi_version`; unknown fields ignored, missing fields defaulted).
* **Prints:** target-size export (the composite at target resolution from the same field — the UI copy states the bound honestly: detail below a field texel is interpolation; the true re-dip is replay, Phase 8), PNG plus TIFF-16 if demand-checked, Anod over alpha; **the print ledger** (session dips, thumbnails, re-export while the session lives).

**DONE when:** the built-in palettes are bitwise through the new path; a preset round-trips in ctest and the serializer has its own headless suite; the ledger re-exports a dip at 4k; the schema is documented for the shells (44–46).

---

## Step 44a — iOS shell (macOS machine, iOS agent) — ROADMAP_5's "44"
**Spec:** MEDIUM §1; QOL §1–§4, §6 (copy).

* Medium switch, Anod palettes, the palette editor, substrate, presets (Files export/import), the print ledger, the clear-vs-dip copy; About shows `libsumi 1.0.0`.

**DONE when:** every new setting is reachable on the iPad; a preset made on the desktop imports and renders the same (screenshot compare); the byte path is untouched (the Phase-4 tests still pass).

## Step 44b — Web marble (any machine) — ROADMAP_5's "46"
**Spec:** MEDIUM §1; QOL §1, §3; SPEC §9.5.

* Medium switch + Anod palettes + the palette editor (lil-gui) + presets in `localStorage` with export; the five new scenes verified by `tools/web_gate.mjs --scenes`; the §4.6 web tier still passes; `site/scripts/check.mjs` learns the new scene names (the pages come in 63).

**DONE when:** the web gate and the scene sweep are green; the marble page plays Anod from a ROLI over Web MIDI in Chrome. **Phase end:** tag `v2.0.0-alpha.1`, the author installs everywhere; fold `DECISIONS_5`.

---

## Step 45a — Phase 6 on the Linux desktop (Linux box) — added at the phase close
Not in ROADMAP_5's draft: every Phase-6 shader, render target and readback had run only on Metal and WebGPU. The handoff (`_work/LINUX_HANDOFF.md`, in git history): build from scratch, ctest, the field and composite gates on GL, every harness test and soak, the Anod look, prints, a desktop preset round-tripped, the gestures, the ROLI over ALSA, a CPack DEB in a clean container.

**DONE when:** the checklist is green with every fix landed under bug → regression test → fix (DECISIONS_5 #76–#78; the ROLI hands-on the author's).

## Step 45b — Android shell (Linux box, Android agent) — ROADMAP_5's "45"
**Spec:** as 44. First line of the step: the step-41 JNI update compiles and the app runs.

**DONE when:** as 44 on the Galaxy Tab, plus the 16 KB page-size build still passes Play's check.

## Step 46 — Phase 6 on the Windows desktop (Windows box) — added at the phase close
Not in ROADMAP_5's draft: the same checklist on D3D11 (`_work/WINDOWS_HANDOFF.md`, in git history), after reading what 45a found.

**DONE when:** green with the fixes landed (DECISIONS_5 #82–#85); the phase end — the tag `v2.0.0-alpha.1` (the author's) and the fold of `DECISIONS_5` — follows this step, not ROADMAP_5's "46". The fixes the author asked for after (#86–#88) landed before the fold.

---

## Provisional resolutions taken while drafting before they become `DECISIONS_5 #1…` when the phase opens
1. **The medium is named Anod** (author, 2026-09-21); Ichisuke Fujioka is honoured in the acknowledgments regardless (MEDIUM §1 `[ITERATE]` closed).
2. **Four phases, one public release, version 2.0.0**; pre-release `alpha` tags close Phases 6–8, `rc` tags run Phase 9.
3. **The ABI break (41) carries the probe's state argument and the cell-info flags two phases before any stateful layout uses them**, so the arc has exactly one break; `libsumi` is 1.0.0 from that step.
4. **Torsion is a vortex profile** in the core with its own page in the book; Chladni and the spark shear are passes of the ripple's shear family at both insertion points; the burst lives in the wake's sub-stepped family.
5. **Fingering CCs:** valves 110/111/112 on the master channel; the slide 7-bit CC 113 with smoothing (not a 14-bit pair — the MSB of a pair would land in CC 0–31, the Airwave's block; and 128 steps over six semitones ≈ 4.7 cents/step, under the ~5-cent JND — the arithmetic is the justification, keep it with the decision).
6. **Presets share one host-side serializer** (pure C, beside `hostmpe`); the core stays stateless about files.
7. **Per-device default presets are offered, not auto-applied.**
8. **Gestures are recorded in replay files**, so pen performances replay complete.
9. **New-feature documentation lands in step 63** and merges with the release tag; guide fixes ship from `main` at any time.
10. **The palette curve is fixed per medium in 2.0**; the theremin is a `flags` bit, not a radius sentinel.
11. **The fretboard generalises to `SUMI_LAYOUT_STRINGS`** with three FIXED tuning presets (standard guitar, whole-tone tap grid, all-fourths) — a params enum of fixed arrays; user-editable tunings stay deferred with microtonal. Wicki–Hayden stays (not a string layout; cheapest item in its step). **"Harpejji" is Marcodi's trademark:** docs may say "inspired by tapping instruments such as the Harpejji"; the word never enters an enum, a setting label, or a product name.
12. **Replay files carry frame boundaries** and playback drives the scripted clock through them (step 61) — wall-time re-bucketing is the documented anti-pattern and the negative test.
