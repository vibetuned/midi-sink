# IMPLEMENTATION ROADMAP: Suminagashi MPE Visualizer Engine
**Companion to `PROJECT_SPEC.md` (spec v2 — it absorbed v1). Historical:
all steps below are DONE; per-step evidence lives in git history under
`docs/evidence/` (removed from the working tree after step 14) and is
condensed in `CHANGELOG.md`.**

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
