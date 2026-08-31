# IMPLEMENTATION ROADMAP: Suminagashi MPE Visualizer Engine
**Companion to `PROJECT_SPEC.md` — always provide both files to the agent.**
**Phase 1 target: macOS (Metal). One step per session. Do not begin a step while the previous step's DONE checks fail.**

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
