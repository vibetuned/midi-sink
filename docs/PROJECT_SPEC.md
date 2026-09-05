# PROJECT SPECIFICATION: Suminagashi MPE Visualizer Engine
**Architecture Paradigm:** Option 2 — Embedded High-Performance C-ABI Core + Host Platform Shells
**Phase 1 Target:** macOS desktop (Metal). **Phase 2:** Windows (D3D11), Linux (OpenGL 4.1 core). **Phase 3:** iOS (SwiftUI + CAMetalLayer), Android (Compose + ANativeWindow + GLES3). **Phase 4 (v0.3 → v0.4):** the tablets as MPE instruments — the Touch & Stylus Play Surface (§8), the shared `hostmpe/` host library, and the v0.4 deformation operators (§4.3(3–7)).
**Reference Hardware:** ROLI Piano + ROLI Airwave (owned), Expressive E Osmose (planned), Roland Aerophone Brisa / Odisei Travel Sax (planned), plus any classic single-channel MIDI keyboard. Play surface: iPad Air 11" (M4) + Apple Pencil Pro, Galaxy Tab S8 Ultra + S-Pen.
**Spec v3** — absorbs the Phase-4 spec and every Part-III decision (`DECISIONS.md`, referenced as `DECISIONS_3 #n`). Where this text and a decision entry still disagree, the entry is the record of what shipped; fix the text.

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
3. **`hostmpe/` (shared host-side MPE generation, Phase 4):** a C++20 library behind a **pure-C header** (`hostmpe.h`, the same discipline as `sumi_core.h`, enforced by `hostmpe_c_compile.c`), consumed by Swift through the module map and by Kotlin through JNI. It owns everything the tablet play surface generates — the voice allocator, the joystick and stylus engines, the per-transport rate limiters, the control-strip value engines, the echo guard — so the two shells cannot drift: one implementation, headlessly unit-tested, the same bytes on both platforms. The core never generates MIDI; `hostmpe` never touches the GPU.
4. **`ios/`, `android/` (tablet shells):** the §5.4 bridges plus the §8 play surface — overlay, control strip, transports — all host-side; the core's only Phase-4 addition is the read-only layout probe (§5.3).

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

**Hard rule:** the marbling simulator never sees MIDI. It consumes only the normalized event vocabulary of §3.3. This is what lets one engine serve MPE, Airwave gesture CCs, monophonic wind MIDI — and the tablets' own play surface, whose generated MPE re-enters through `sumi_push_midi` like any device (§8.1: the visualizer is just another MPE synth) — without special cases leaking into the graphics code.

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
  * Polyphonic Key Pressure (0xA0, keyed by the voice's note on its member channel) → *swirl* (v0.4) — the second pressure dimension. None of the reference hardware sends it (the play surface does, §8.3); it is decoded whenever present.
  * Note Off velocity → *lift*.
* **Voice identity:** a voice is keyed by `(member_channel)` while a note is active on it, since MPE guarantees one active note per member channel (handle same-channel overlap defensively: newest note steals the voice, and a Note Off that names a note the channel is no longer sounding is ignored). The stylus legato of §8.7 relies on exactly this — its per-cell retrigger is `bend → Note On(new) → Note Off(old)` on one channel, the classic legato overlap.

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
* Mapping model: the single voice is a **wandering ink brush**. Note pitch sets the target canvas position; a legato note change *migrates* the active drop's feed point (drawing a tine-like wake as it travels) rather than spawning a disconnected new drop. Breath modulates the brush's **width**, not an unbounded ink feed — the brush relaxes toward a breath-proportional target width (see §4.4), producing a calligraphic line rather than a growing blob.

### 2.4 Classic single-channel MIDI keyboard
* Notes on one channel, global pitch bend (±2 default), CC 1 mod wheel, CC 64 sustain.
* Degrades gracefully: every note is its own voice with strike only; pitch bend and mod wheel act globally (bend → global shear tine, mod → vortex), CC 64 → paper dip — **in classic mode only** (DECISIONS_3 #67). In MPE mode the sustain pedal is a musical control (the §8.8 strip pad, the Pencil squeeze, the S-Pen button) and never touches the canvas; the paper dip is a deliberate host action there (`sumi_trigger_paper_dip`, surfaced as a settings control).

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
* **Overflow safeguard (stuck voices):** drop-oldest can discard a Note Off, leaving a voice feeding ink forever. Whenever the dropped counter increments, the voice mapper arms a per-voice inactivity timeout (~10 s without any message for that voice while other traffic flows → synthesize `VoiceEnd` with lift 0, logged). Timeouts are armed only after an overflow — normal operation never expires voices.
* SysEx is out of scope for v1 (silently ignored). MIDI 2.0 / UMP and per-note controllers (CC in MIDI 2.0 sense) are explicitly out of scope for v1; the normalizer's design should not preclude adding a UMP ingest path later.

### 3.2 Stateful decoding (inside the consumer thread)
* Per-channel accumulators for: 14-bit pitch bend, RPN/NRPN state machines (RPN 0 = bend range, RPN 6 = MCM), running status is already resolved by libremidi but the decoder must not assume it.
* Per-voice state: note number, member channel, strike velocity, smoothed press/slide/glide values, age.

### 3.3 Normalized event vocabulary (the only interface to the simulator)
```
VoiceBegin   { voice_id, positions[1..3], strike (0..1) }  // echo set from the active layout
VoiceGlide   { voice_id, dx }              // from per-note pitch bend, semitone-scaled
VoicePress   { voice_id, pressure (0..1) } // channel pressure
VoiceSlide   { voice_id, timbre (0..1) }   // CC74
VoiceSwirl   { voice_id, amount (0..1) }   // poly key pressure 0xA0 (v0.4)
VoiceMigrate { voice_id, new_x, new_y }    // wind-mode legato pitch change
VoiceEnd     { voice_id, lift (0..1) }
GlobalCtl    { dimension, value (0..1) }   // vortex, viscosity, palette, roughness…
PaperDip     { }                            // CC64 rising edge (classic mode only), or ABI call
```

### 3.4 Musical → spatial mapping (default profile)
* **Pitch → position** is a pluggable **layout** (`pitch_layout` param, switchable live; a layout is a pure function `(note, params, aspect) → 1–3 positions` plus an optional per-frame field motion). Most layouts return a single position; a layout may return up to `SUMI_MAX_ECHOES = 3` **echoes** — multiple canvas sites that are all the same note (see the echo-set rules below):
  * `0 — SUMI_LAYOUT_FIFTHS`: pitch class around a circle (circle-of-fifths angular layout), octave → radius (low notes outer, high notes inner). One position. The v1 default.
  * `1 — SUMI_LAYOUT_CHROMA_GRID`: chromatic reading-order grid. C1 at top-left, B7 at bottom-right: row = octave (1..7, top to bottom), column = pitch class (C..B, left to right), cells inset so drops at grid edges stay on canvas. Notes outside C1–B7 clamp to the nearest edge cell. One position.
  * `2 — SUMI_LAYOUT_JANKO`: Jankó keyboard grid — 6 staggered rows of whole-tone columns; column = note/2, row parity = note%2, with the classic half-column offset on alternate rows. **The note stamps on all three rows of its parity** (rows {0,2,4} or {1,3,5}) — the full 6-row lattice stays live, which is the entire point of visualizing Jankó. Three echoes.
  * `3 — SUMI_LAYOUT_ROLL_H`: horizontal piano roll. Pitch → y (low at bottom), all drops spawn on a fixed **now-line** at x = 0.12; the whole field translates +x continuously at the roll speed (see *field motion* below). Reads left-to-right like a DAW timeline flowing away from the playhead. One position.
  * `4 — SUMI_LAYOUT_ROLL_V`: vertical piano roll (Synthesia-style). Pitch → x (low at left), now-line at y = 0.12 from the top, field translates downward. One position.
  * `5 — SUMI_LAYOUT_PIANO_GRID`: classical piano grid — the chroma grid's frame (C1–B7, same insets, same out-of-range clamp to the edge octave keeping pitch class) with each octave as a two-row keyboard: 5 accidentals above at white-key units {1, 2, 4, 5, 6} (C♯/D♯ and F♯/G♯/A♯ between their naturals), 7 naturals below. 7 octaves × 2 = 14 rows, one position. **Playable geometry (DECISIONS_3 #29, #41, #57, #60, #61 — the final form):** an accidental's cell is **0.6 white-key units wide and 0.6 of an octave pair tall**, centred on its row; a natural's cell is a full key wide and owns the **bottom 0.6 of its octave pair**, sitting flush with the row's bottom edge. The strip above the naturals, off any accidental, is the **glissando corridor — a dead zone**: a stroke through it plays the black-key run (C♯ D♯ F♯ G♯ A♯) because a dead zone sustains (§8.7), while the natural band below plays the white-key run without grazing an accidental. Tapping in the corridor plays nothing, and a stroke must start on a key. **The circle you see IS the cell you touch and the joystick it generates**: R_max = half of the cell's smaller dimension (aspect-corrected), so where height governs — every landscape aspect — natural and accidental knobs are the same size, and the 0.6 width ratio holds at every aspect (footprints are similar rectangles). The semitone axis is the generic shortest-neighbour diagonal (C→C♯ half a key over and 0.9 of a row up) — pitch is *not* a function of x alone on this lattice, so pen legato is quantized here by design (§8.7).
* **Echo-set rules (multi-echo layouts):** a voice owns its full echo set for its lifetime. **Voice dynamics — press feed, glide, slide, lift — apply to every echo of the voice**, and all echoes **share one ink band and one aux value**: the drop counter increments once per `VoiceBegin`, not per echo, so the echoes carry identical band parity and hue and read unmistakably as the same note (radial coordinates remain per-echo local). Glide displaces every echo along the lattice's local semitone axis (in Jankó the axis is **horizontal** — pitch lives on x alone and the parity rows are echoes, so one semitone is half a column straight along +x, the same vector for all echoes; DECISIONS_3 #18). Each echo's deformation passes **count individually against the per-frame budget** (§ rate limiting below) — a 10-voice Jankó performance emits up to 30 feed passes/frame, which the budget's overflow merging must absorb, not silently drop whole echoes (merge within an echo across frames, never cull one echo of a set while feeding another).
* **Field motion (roll layouts only):** the scroll is itself a closed-form deformation — a uniform translation pass with inverse lookup `P_src = P − v̂ · s·dt`, where speed `s = (bpm / 60) × roll_speed` in canvas lengths/second (`bpm` and `roll_speed` are params; `roll_speed` is canvas-lengths-per-beat, **default 0.0625 = 1/16** — the full canvas holds 16 beats of history, i.e. 4 bars of 4/4, so at 120 BPM ink lives on screen for 8 seconds; 0.25 would flush the whole canvas every bar, which reads as a waterfall rather than a drifting tray). **Ingress must be an explicit shader branch:** when `P_src` falls outside [0,1], the fragment writes fresh water — `ink = 0`, `aux = 0`, and the **identity coordinates of its own texel**. This cannot be delegated to sampler state: clamp-to-edge would streak the boundary texel's old ink across the entering region, and clamp-to-border cannot work because fresh water is not a constant (the identity coords vary per texel). Old ink slides off the far edge; fresh water enters at the now-line side. All other deformations (tines, vortices, feeds) still apply on top of the scroll, so expressive gestures smear downstream like ink in a current. **BPM is host-supplied** (a param, optionally updated live from a DAW/link source by the host); the core never guesses tempo from MIDI in v2 (MIDI clock ingest is a possible v3 item).
* **Strike → initial drop radius** (perceptually scaled: radius ∝ sqrt(velocity) so ink *area* tracks velocity).
* **Press → sustained ink feed (boundary growth):** while pressure > 0 the voice's drop keeps growing. Because a center expansion of radius r moves an existing boundary R only to sqrt(R² + r²) (area conservation), naive fixed radius steps stall visually. Instead the voice tracks its nominal boundary R; the per-frame growth ΔR ∝ smoothed pressure × dt × expansion_rate is converted to the emitted pass radius **r = sqrt((R + ΔR)² − R²)**. Same closed-form framework, physically meaningful growth rate. This is the key Osmose behavior, and it is deliberately unbounded — sustained pressure can flood the canvas. **`press_mode` (v0.4)** routes the 0xD0 channel-pressure consumer — 0 = this ink feed (default), 1 = the Lamb–Oseen swirl (§4.3(7)) — one consumer owns 0xD0, the bend_mode/slide_mode pattern; it exists so pressure-only hardware (ROLI, Osmose) can reach the swirl voice. The 0xA0 poly-pressure dimension routes to the swirl unconditionally, in either mode.
* **Swirl (0xA0, or 0xD0 when press_mode = 1) → Lamb–Oseen vortex at the voice's center (v0.4):** Γ per pass ∝ smoothed amount × dt × expansion_rate (a delta rate, like every continuous feed — never an absolute value), core radius r_c = the voice's current nominal boundary R (the drop IS the vortex core: its own rings rotate near-rigidly and stay coherent while the far field stirs the neighbors). Rotation sign defaults to the voice's band parity — adjacent notes counter-rotate, zero configuration. Swirl passes fan out to every echo and count against the budget like any feed.
* **Glide (per-note bend) → local tine:** drag *that drop's* center along the pitch axis, emitting a narrow tine stroke along the drag path each frame the bend changes. This must be per-voice, never a global shear, in MPE mode. On the playable lattices (CHROMA_GRID, JANKO, PIANO_GRID) the rendered glide uses the **true lattice step** — a one-semitone bend moves the drop exactly one cell, so the drop travels under the finger (DECISIONS_3 #20); the other layouts keep the v1 rendering cap.
* **Slide (CC74) → per-drop palette/ink-density modulation** for the layers that voice owns.
* **Lift → the drop "sets":** stop feeding. Nothing is stamped — the v1 "faint surfactant ring proportional to lift velocity" was removed in v0.4 (DECISIONS_3 #41: on an instrument the release mark read as noise; the joystick indicator vanishing is the release feedback).
* **Rate limiting:** continuous dimensions (press, glide, slide, swirl, breath) are coalesced per voice per frame — the simulator consumes at most one value per dimension per voice per `sumi_update`, with exponential smoothing (time-constant configurable, default ~30 ms). Deformation *events* (tine segments from glide) are capped per frame (budget, e.g. 64 deform passes/frame) with overflow merged, so a hyper-expressive Osmose performance cannot starve the frame rate.

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
* `ink`: a **continuous scalar "ink phase"**, not a discrete layer ID — and it must stay in a **small fixed range** to survive RGBA16F. A raw `counter + radial` phase breaks in half float (ULP(512) = 0.5 destroys band parity past ~256 drops), and a wrapped counter speckles seams where far-apart values interpolate across many band thresholds. Use the **parity form**: `ink = 1 + (drop_counter % 2) + radial` ∈ [1, 3), water = 0. Any two field values then interpolate across at most one band threshold, so no speckle at any drop count. The phase remains derived from the monotonic counter and continuous; the composite bands it with a periodic function.
* `aux`: per-drop hue/palette selector — the raw drop counter (incremented once per `VoiceBegin`, so all echoes of a multi-echo layout share one value, §3.4) plus continuous modulation, e.g. slide. Half float degrades aux above ~2048, so the **drop counter is rebased on every paper dip** (the field resets to identity anyway); 2048 drops per sheet is unreachable in practice. If sharp per-drop palette boundaries prove necessary, switch this channel only to `texelFetch` (nearest) lookups — never linearly filter discrete IDs.

### 4.3 Analytical deformation shaders (`deform.glsl`)
1. **Circular drop expansion** (ink or clear surfactant) of radius *r* at center *C* — inverse lookup for points outside the drop:

   P_src = C + (P − C) · sqrt(1 − r² / ‖P − C‖²)  for ‖P − C‖ ≥ r

   For ‖P − C‖ < r: write the new ink phase and local radial coordinate directly.
   All positions in **aspect-corrected normalized space** (correct for non-square canvases before applying the math, or rings become ellipses).

2. **Tine / comb stroke** along unit direction D̂ through point L with sharpness α and magnitude z:

   d = perpendicular distance from P to the line (L, D̂)
   P_src = P − z · D̂ · α / (α + d)

3. **Vortex agitation** centered at V — two profiles (v0.4), both pure rotations by −θ(r), hence exactly area-preserving, and both **exact maps at any angle** (a finite rotation never folds — no sub-stepping, ever):
   * `EXPONENTIAL` (Jaffer): θ(r) = A · exp(−r / R). Shear concentrates near the center — diffuse, breath-like agitation. Default for wind/Airwave/mod-wheel routing.
   * `RANKINE` (v0.4): θ(r) = ω for r < R; θ(r) = ω · R²/r² for r ≥ R. The core rotates **rigidly** — zero interior shear, so ink patterns inside survive intact and spin as a solid disk — while all shear concentrates in the crease ring at r = R (the intentional kink) plus the 1/r² falloff outside. Mechanical, tight, ideal for twist gestures (R = half the finger separation, ω = the twist delta: the gesture literally grabs a rigid disk of water) and rotary-encoder deltas.

4. **Dipolar wake** (v0.4) — the potential-flow doublet: the exact instantaneous flow around a rigid cylinder (the stylus tip, radius a) moving through inviscid fluid. In the stroke frame (x along the per-pass tip motion d⃗, magnitude U·dt):

   for r > a:  x_src = x − d·a²(x² − y²)/r⁴,  y_src = y − d·2a²xy/r⁴
   for r ≤ a:  P_src = P − d⃗   (the rigid tip body — no fluid inside the cylinder; this boundary condition removes the r→0 singularity exactly, and matches the outer field with zero seam on the motion axis)

   (Sign corrected in v0.4 implementation — DECISIONS_3 #32: the earlier draft's `+` contradicted its own zero-seam claim at the front pole and rendered inside-out; the displacement field follows from φ = −U a² x/r², the doublet satisfying the no-penetration boundary.) The magnitude is **not a parameter**: it is the tip displacement d = U·dt itself — wake strength is pen speed, by physics. **Sub-stepping required:** subdivide each stroke segment so per-pass displacement stays strictly inside a/2 — at exactly d = a/2 the inverse-map Jacobian reaches ZERO at the rear stagnation point (1 − 2d/a), so a/2 is the fold threshold itself, not a safe budget; the core sub-steps at ≤ a/4 (same budget accounting as glide tines). Orientation acceptance test: ink directly ahead of the tip bulges *forward*, ink at the flanks streams *backward*; if it renders inside-out, the inverse-lookup sign was flipped. Tip radius a maps from stylus pressure (harder press = fatter tip = wider wake).

5. **Hamiltonian pinch** (v0.4) — a localized area-preserving saddle. In pinch-local coordinates (rotated by the fold angle about the pinch center):

   x_src = x · e^{+k·w(s)},  y_src = y · e^{−k·w(s)},  where s = x·y and w(s) = exp(−|s|/S)

   The naive global saddle (w ≡ 1) acts on the whole canvas; the naive radial window breaks incompressibility. Windowing by the **streamline value s = xy** does neither: s is conserved along each hyperbolic trajectory, so the exponent is constant along any path and the map stays closed-form with det = 1 **exactly**. Honest caveat, stated as a feature: strength cannot decay along the two fold axes themselves (s = 0 there), so the pinch's arms run outward as fading creases — which is what pinched paper physically does. k per pass is always a smoothed **delta** (never an absolute controller value — absolute feeding integrates into runaway strain). Sign of k swaps which axis compresses: a naturally bipolar gesture. **Two looks, one params switch (v0.4, DECISIONS_3 #34):** `pinch_variant` 0 = this Hamiltonian saddle (det = 1 exactly); 1 = **crossed tines** — the step-19 prototype rival kept after the pick-by-eye pair (two perpendicular opposing tine passes through the pinch point; softer, lumpier, directional — costs two passes per emission). Honored by both routes: the CC74 `slide_mode = 1` path and `sumi_add_pinch`. Gesture sources of the fold axis (host-side data with no MIDI path): the stylus azimuth (§8.7), the desktop's Shift-drag angle, and on the tablets' Marble mode a literal **two-finger pinch** — the fold axis is the finger-to-finger line, the squeeze is the delta-driven k (DECISIONS_3 #41).

6. **Sine ripple** (v0.4) — the traditional marbler's waved comb, and a pure shear: x_src = x − A · sin(k·y + φ) (in the ripple frame, rotatable by ripple_angle); y_src = y Jacobian = 1 identically at ANY amplitude; each line translates rigidly, so the map never folds — no sub-stepping. At fixed (k, φ), ripples form a commutative group: applying ΔA per frame composes additively, so an LFO that breathes A up and back to zero leaves the field bitwise unchanged. Changing k or φ between passes breaks commutativity and bakes residue in — that residue is marbling (the waved-comb feathering); the boundary is deliberate and documented. **`bend_mode` governs the PER-NOTE pitch bend** (corrected in v0.4 implementation, DECISIONS_3 #35 — the intent is subtle vibrato: a note's bend wobble shimmers the water instead of wiggling its drop): mode 0 (default) = v1 glide — a note's bend drags its drop along the pitch axis; mode 1 = the bend feeds the ripple and the drop HOLDS (one consumer owns the note bend; flips are clean in both directions — the glide target is left untouched in mode 1 so switching back never emits stale-delta tines). Master bend keeps its v1 shear tine in BOTH modes; CC1/vortex untouched. **The ripple amount IS the bend's distance from center:** amp = |semitones| / 6, clamped (a ±0.5-semitone vibrato breathes ~8%; last-writer-wins across voices, smoothed like any global control) — so the water stills itself: bend re-centers → amount 0, last note's release → amount 0, a 1→0 mode flip zeroes the residual. The wavelength k stays a flavor control (SUMI_CTL_RIPPLE_FREQ, resting mid-range; shells map CC 103, and CC 102 is an external amp handle sharing the slot last-writer-wins). Exists at two insertion points, selected by ripple_bake: live (composite-time view displacement — the ink sampling coordinate ripples, nothing accumulates, the water shimmers and stills; see §4.5) and bake (a real deform pass, delta-driven like the pinch). **Bend-driven bake is PERMANENT by design (DECISIONS_3 #36):** the emitted pass phase drifts with activity (φ += |ΔA|/A_max · 1.5 rad, wrapped), so an excursion never retraces exactly — each vibrato cycle lays a slightly shifted comb, a feathered record that accumulates like glide tines, while the DYNAMIC still stills (the three come-back paths above). CC-driven bake keeps φ fixed — the pure composing-back group property, preserved for the Step-19 gate. On the tablets the Ripple toggle selects bend_mode = 1 AND ripple_bake = 1 together; desktop keeps a manual live/bake override for experimentation.

7. **Lamb–Oseen swirl** (v0.4) — the exact closed-form solution of the 2D Navier–Stokes equations for a decaying line vortex, evaluated analytically. The project's founding rule bans NS *solvers*; here NS enters as an exact solution — the philosophy's best vindication. Tangential velocity v_θ(r) = Γ/(2πr) · (1 − exp(−r²/r_c²)) integrates over the pass interval to the rotation angle:

   θ(r) = (Γ·Δt / 2πr²) · (1 − exp(−r²/r_c²)),   rotate P around the center by −θ(r)

   A pure rotation by θ(r): exactly area-preserving, and — like the Rankine — an **exact map at any angle: no sub-stepping, ever**. Where the Rankine has the crease-ring kink at R, Lamb–Oseen is C∞ — solid-body rotation in the core blending viscously into the same 1/r² far field: the soft organic twist beside the Rankine's mechanical one. **Numerical guard:** the naive expression is 0/0-shaped at small r and float-cancels — evaluate via `expm1` (or below r ≪ r_c use the limit θ(0) = ΓΔt/(2πr_c²) directly). Driven per-voice by the *swirl* dimension (§3.4): r_c = the voice's boundary R, Γ ∝ smoothed amount × dt, sign from band parity. **Numerical note (DECISIONS_3 #37):** in RGBA16F, sub-quantum per-pass displacements near the exact centre round away (a half-float ULP freeze) — protective, never inflating; field measurements at r → 0 read low, not high, and this is not a bug to chase.

### 4.4 Continuous feeds
Sustained pressure/breath is realized as **incremental drop expansions re-emitted per frame** at the voice's current center, using the boundary-growth conversion of §3.4 (emitted radius r = sqrt((R+ΔR)² − R²)). This keeps everything inside the same closed-form framework — no velocity field is ever introduced. **Wind mode is the exception to unbounded growth:** a breath-fed brush relaxes toward a breath-proportional *width* (target ≈ 0.006 + 0.05·breath canvas heights; growth only up to the target, and a `VoiceMigrate` clamps the new segment down to the current width). Literal integration would turn a 20-second legato line into a canvas-sized blob; MPE press keeps the unbounded integration — that is the Osmose behavior.

### 4.5 Composite pass (`composite.glsl`)
* Sample the active displacement target, map ink phase → alternating sumi ink rings vs. clear water. **Live ripple (v0.4):** when `ripple_bake = 0` and ripple amplitude > 0, the *ink sampling coordinate* is displaced by the §4.3(6) shear before the field lookup — a non-destructive view displacement; the field itself is untouched. The washi grain does NOT ripple (it is screen-locked, per the invariant below), and **the paper dip always samples the un-rippled field** — the print is what touches the water; the shimmer is surface motion, not ink position.
* Modulate with procedural (simplex) washi mulberry-fiber noise and absorption grain; fiber strength = `paper_roughness`. **Invariant: paper is screen-locked.** Fiber/grain noise is sampled in screen/NDC space, never through the deformed UV field — only ink sampling goes through the field. Paper is the stationary substrate; if fibers warp with tines or drift with roll-layout scrolling, this invariant has been broken (regression check: run a tine and a 120 BPM scroll over textured output and confirm the grain stays put while ink moves).
* Palettes: Sumi black, Indigo, Ochre (id-selected via params; `aux` channel can offset hue per drop).
* Output color space: render in linear — the composite shader applies the exact sRGB encode manually (identical behavior across Metal/D3D11/GL/GLES; the RGBA8 print target gets the same encode, so exports match the screen bit-for-bit in tone).
* **Known limit (tracked, cosmetic):** the composite's ring-thickness probing (DECISIONS_2 #4) speckles on bands thinner than ~10 texels.

### 4.6 Orientation convention (one y-down space, flips only at the boundary)
The entire deformation chain works in **texture space, v grows downward, row 0 = top**. Fullscreen-triangle vertex shaders emit `st = (u, 1 − v_clip)` — the texture-space coordinate of the fragment's own texel — and every pass (identity init, deforms, composite) operates purely in that space. Rationale (found the hard way on Metal): sampling at the raw NDC interpolant vertically flips the field every offscreen pass, so consecutive deformations cancel instead of composing; with `st`, passthrough is a true no-op and deformations compose exactly (regression check: 40 passes of a z/40 tine must equal 1 pass of a z tine). Mouse/touch coordinates, texture rows, and screen rows all share this one y-down space.

**Backend rule (critical for GLES3/Android):** Metal and D3D11 share top-left row origins, but OpenGL/GLES defaults to bottom-left. Any vertical flip a backend needs lives **exclusively in the final swapchain composite pass** (and, mirrored, in the print readback path) — the deformation chain must never contain backend-specific branches. A cross-backend regression test (identical deform script → identical field texture readback, bitwise within float tolerance) guards this.

## 5. Core C-ABI (`sumi_core.h`)

Must compile under C99/C11 and C++20 with zero includes beyond `<stdint.h>`, `<stdbool.h>`, `<stddef.h>` (enforced by `tests/abi_c_compile.c`).

### 5.1 Graphics ownership contract (per backend)
`sokol_gfx` has **no Vulkan backend** — supported paths are Metal, D3D11, OpenGL/GLES3, WebGPU. Backends for this project:
* **Metal (macOS/iOS, phase 1):** host passes a `CAMetalLayer*`. The core creates the `MTLDevice`/queue, drives `nextDrawable` each frame, and configures the layer's drawable size on resize. The host must not touch the layer after `sumi_create` except to destroy it after `sumi_destroy`.
* **D3D11 (Windows, phase 2):** host passes an `HWND`; core creates device + DXGI swapchain.
* **GL core 4.1 / GLES3 (Linux phase 2, Android phase 3):** exception to the ownership rule — the host creates the context (GLFW/EGL), makes it current on the render thread, and passes `native_surface_handle = NULL` with `backend = SUMI_BACKEND_GL`; the core renders into the currently bound default framebuffer and the host presents (swap buffers).
This asymmetry is inherent to GL and must be documented in the header comments.

### 5.2 Threading contract
* `sumi_create/destroy/resize/update/render/set_params/trigger_*` and gesture calls: **single thread** (the render thread). Not thread-safe with each other.
* `sumi_push_midi`: callable from **exactly one** other thread (the MIDI callback thread) concurrently with the render thread. SPSC — if the host has multiple MIDI sources, it serializes them before pushing. **Host-synthesized MIDI (the §8 play surface) counts as device MIDI:** it flows through the shell's same single producer — iOS: the serial MIDI dispatch queue; Android: the AMidi poller thread, which also owns all `hostmpe` state (DECISIONS_3 #46). The touch path never calls `sumi_push_midi` from the UI thread.
* `sumi_layout_probe` is the deliberate exception to "render thread only": instance-free and pure, callable from any thread with a params snapshot (§5.3).

### 5.3 Header

The shipped `core/include/sumi_core.h`, reproduced verbatim — the header is the contract; when they differ, the header wins and this copy is stale.

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
    SUMI_CTL_RIPPLE_AMP      = 7,   /* v0.4: sine ripple amplitude A          */
    SUMI_CTL_RIPPLE_FREQ     = 8,   /* v0.4: ripple wavenumber k              */
    SUMI_CTL_COUNT           = 9
} sumi_ctl_t;

typedef enum {                       /* v0.4 vortex profiles, spec §4.3(3) */
    SUMI_VORTEX_EXPONENTIAL = 0,     /* Jaffer: diffuse, breath-like       */
    SUMI_VORTEX_RANKINE     = 1      /* rigid core, crease ring at R       */
} sumi_vortex_profile_t;

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

typedef enum {                   /* pitch -> position layouts, see spec 3.4 */
    SUMI_LAYOUT_FIFTHS      = 0, /* circle-of-fifths radial (default)         */
    SUMI_LAYOUT_CHROMA_GRID = 1, /* C1 top-left ... B7 bottom-right           */
    SUMI_LAYOUT_JANKO       = 2, /* staggered whole-tone Janko grid           */
    SUMI_LAYOUT_ROLL_H      = 3, /* horizontal piano roll, BPM-driven scroll  */
    SUMI_LAYOUT_ROLL_V      = 4, /* vertical piano roll, BPM-driven scroll    */
    SUMI_LAYOUT_PIANO_GRID  = 5  /* classical two-row piano grid, C1..B7      */
} sumi_layout_t;

typedef struct {
    float    fluid_viscosity;    /* damping for continuous agitation            */
    float    expansion_rate;     /* pressure/breath-driven drop feed scale      */
    float    paper_roughness;    /* washi fiber composite strength              */
    float    smoothing_ms;       /* expressive-dimension smoothing time const   */
    uint32_t active_palette_id;  /* 0 sumi black, 1 indigo, 2 ochre             */
    uint32_t pitch_layout;       /* sumi_layout_t value                         */
    float    sim_scale;          /* simulation res / output res, clamped (0,2].
                                    Host-chosen: 1.0 desktop/iPad-class GPUs,
                                    ~0.75 on typical Android phones for
                                    sustained thermals under continuous MPE
                                    streams. The core never detects devices —
                                    the host owns this default.               */
    float    bpm;                /* host-supplied tempo, roll layouts (dflt 120)*/
    float    roll_speed;         /* canvas-lengths per beat, rolls (dflt 0.0625:
                                    16 beats = 4 bars of 4/4 span the canvas)  */
    /* v0.4 */
    uint32_t slide_mode;         /* CC74 routing: 0 per-drop aux (v1 behavior),
                                    1 Hamiltonian pinch (delta-driven)         */
    uint32_t vortex_profile;     /* sumi_vortex_profile_t for CC-routed vortex */
    uint32_t ripple_bake;        /* 0 live (composite view), 1 bake (deform)   */
    float    ripple_angle;       /* ripple frame rotation, radians (dflt 0)    */
    uint32_t pinch_variant;      /* 0 Hamiltonian saddle (det = 1 exactly),
                                    1 crossed tines (the softer, lumpier look
                                    kept after the step-19 pick-by-eye pair —
                                    DECISIONS_3 #34). Honored by BOTH pinch
                                    routes: slide_mode = 1 and sumi_add_pinch. */
    uint32_t bend_mode;          /* PER-NOTE pitch-bend routing (§4.3(6),
                                    DECISIONS_3 #35 corrected): 0 = v1 glide
                                    (the bend drags the note's drop along the
                                    pitch axis), 1 = the note bend plays the
                                    sine ripple's amplitude and the drop
                                    holds position. Exactly ONE consumer owns
                                    the note bend; switchable live. Master
                                    bend keeps its v1 shear tine regardless. */
    uint32_t press_mode;         /* 0xD0 channel-pressure routing (§3.4 v0.4):
                                    0 = ink feed (v1 grow, default), 1 = the
                                    Lamb-Oseen swirl — hardware's door to the
                                    swirl voice. 0xA0 poly pressure -> swirl
                                    in either mode.                          */
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

/* Paper dip: freeze canvas, snapshot, reset UV to identity (rebases the drop
   counter, see spec 4.2). The print pipeline is double-buffered: a dip while a
   previous print is still being consumed (e.g. host-side PNG encode) must never
   overwrite the buffer the host is reading — the core keeps two print buffers
   and flips; a third dip before either frees is refused with a warning log. */
SUMI_API void             sumi_trigger_paper_dip(sumi_instance_t* inst);
/* Synchronous readback of the last dipped print (RGBA8, tightly packed).
   Call with pixels=NULL to query size. Returns false if no print exists. */
SUMI_API bool             sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

/* Layout geometry probe (v0.3, Phase 4) — pure read-only query for host-side
   play surfaces (hit-testing, bend scaling). See PROJECT_SPEC.md §8.2.

   Units: cell_center_* are normalized [0,1] canvas coordinates.
   cell_radius and semitone_step are DISTANCES in canvas-height units (the
   project's universal distance unit — same as gesture radii below);
   semitone_dx/dy is a unit vector in aspect-corrected space. A host measures
   touch deltas in the same metric by dividing pixel deltas by the view
   height; to convert a step along the axis back to normalized coordinates:
   dx_norm = step*semitone_dx/aspect, dy_norm = step*semitone_dy. */
typedef struct {
    uint8_t  note;            /* nominal MIDI note of the cell under (x, y)   */
    float    cell_center_x;   /* normalized canvas coords of the cell center  */
    float    cell_center_y;
    float    cell_radius;     /* half the smaller cell dimension (R_max),
                                 canvas-height units                          */
    float    semitone_dx;     /* unit vector: +1 semitone direction (glide    */
    float    semitone_dy;     /*   axis, DECISIONS_2 #7), aspect-corrected    */
    float    semitone_step;   /* distance of +1 semitone along it,
                                 canvas-height units (true lattice step,
                                 NOT the glide-rendering cap)                 */
} sumi_cell_info_t;

/* Pure, instance-free geometry query — a free function of the same inputs the
   internal layouts already consume. Callable from ANY thread (the caller
   supplies a params snapshot); no instance, no rendering, no MIDI, no state.
   This matters on Android, where touches arrive on the UI thread while the
   render thread owns the instance — an instance-bound probe would force a
   command-queue round-trip per touch-down, spending the play surface's
   latency budget on hit-testing. Returns false when Play mode is meaningless
   for the layout (FIFTHS, rolls) or (x, y) is outside the playable area. */
SUMI_API bool             sumi_layout_probe(uint32_t layout /* sumi_layout_t */,
                                            const sumi_params_t* params, float aspect,
                                            float norm_x, float norm_y,
                                            sumi_cell_info_t* out);

/* Manual touch / mouse gestures — render thread only, normalized [0,1] coords. */
SUMI_API void             sumi_add_drop  (sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type);
SUMI_API void             sumi_add_tine  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float alpha /*sharpness*/, float magnitude);
SUMI_API void             sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius,
                                          uint32_t profile /* sumi_vortex_profile_t (v0.4) */);
/* v0.4: dipolar wake — the stylus stroke's fluid signature (spec §4.3(4)).
   NOT expressible as MIDI: a gesture-ABI-only deformation — a MIDI recording
   of a stylus performance replays notes but not wakes (documented invariant,
   PROJECT_SPEC.md §8.7). Magnitude is the tip displacement itself (wake strength IS
   pen speed, by physics); the core sub-steps internally (≤ a/4 per pass —
   a/2 is the fold threshold itself, DECISIONS_3 #32).
   tip_radius in canvas-height units, maps from stylus pressure. */
SUMI_API void             sumi_add_wake  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float tip_radius);
/* v0.4: Hamiltonian pinch (spec §4.3(5)) — localized area-preserving saddle
   at (x, y), fold axis `angle` radians, per-pass strength `k_delta` (always
   feed DELTAS of a smoothed controller — absolute values integrate into
   runaway strain; sign swaps which axis compresses). Gesture-ABI entry
   (DECISIONS_3 #32): the fold axis is host-side data (pen azimuth, drag
   angle) with no MIDI path; the MIDI route (slide_mode = 1) drives the same
   pass from per-voice CC74 deltas at the voice position. */
SUMI_API void             sumi_add_pinch (sumi_instance_t* inst, float x, float y,
                                          float k_delta, float angle);

#ifdef __cplusplus
}
#endif
#endif /* SUMI_CORE_H */
```

`sumi_version()` history: 0.2.0 = the layout system and params extensions (the struct grew — hosts must be rebuilt; the struct is passed by pointer with no size field by design, so version gates compatibility); 0.3.0 = `sumi_layout_probe` (Phase 4); **0.4.0 (current)** = the v0.4 batch — §4.3(3–7): Rankine profile, wake, pinch, ripple, Lamb–Oseen swirl; `sumi_add_vortex` gained a profile argument (breaking), `sumi_add_wake`/`sumi_add_pinch` were added, params grew `slide_mode`/`vortex_profile`/`ripple_bake`/`ripple_angle`/`pinch_variant`/`bend_mode`/`press_mode`, and the swirl dimension entered the normalizer. All gesture calls take normalized [0,1] positions and canvas-height-unit radii.

### 5.4 Host bridge notes (phase 3)
* **iOS / SwiftUI:** because `sumi_core.h` is pure C, no wrapper code is needed — a `module.modulemap` exposing the header lets Swift `import SumiCore` directly. The shell is ~30 lines: a `UIViewRepresentable` whose backing `UIView` overrides `layerClass` to `CAMetalLayer.self`, passes the layer to `sumi_create` (backend METAL), drives `sumi_update`/`sumi_render` from a `CADisplayLink`, calls `sumi_resize` from `layoutSubviews` (with `contentScaleFactor` as pixel_ratio), and forwards CoreMIDI packets to `sumi_push_midi`. The core's per-frame autorelease pool (see DECISIONS #12) already covers the non-runloop render path.
* **Android / Compose:** a `SurfaceView` inside `AndroidView`; a small JNI wrapper receives the `Surface`, converts via `ANativeWindow_fromSurface`, creates the EGL context (GL backend: host owns the context per §5.1), makes it current on a dedicated render thread, and calls `sumi_create` with `handle = NULL, backend = GL`. AMidi (via libremidi's android backend or directly) feeds `sumi_push_midi` from the MIDI thread — the §5.2 SPSC contract maps 1:1. Default `sim_scale` 0.75 (see params comment).
* **Phase 4, both shells:** the play surface and control strip are native views over the render view (iOS `UIView`s; Android `View`s mounted with `AndroidView` — Compose's pointer API lacks `AXIS_TILT`/`AXIS_ORIENTATION`, which the S-Pen gestures need, DECISIONS_3 #42) and keep the FULL canvas bounds so a touched cell and its loopback drop stay aligned. Host-owned params live in a snapshot the UI thread reads for the instance-free probe (#47). Every generated byte crosses one merge point that also feeds `hostmpe_observe_external` and the byte log (§8.5).
* **iOS Phase 4:** the serial `midiQueue` owns `hostmpe_t`, the strip engine, the limiters and the byte log; touch-down hops onto it synchronously for the voice id (microseconds). Publishing virtual MIDI endpoints requires `UIBackgroundModes: [audio]` — without it `MIDISourceCreate` fails with `kMIDINotPermitted` and every send silently goes to endpoint 0 (#24); every CoreMIDI status is logged. Pencil Pro squeeze arrives through `UIPencilInteraction` (#62).
* **Android Phase 4:** the AMidi poller thread is the Phase-4 host thread (#46) — it owns `hostmpe_t`, the strip engine, one limiter per transport and the byte log, and is the only caller of `sumi_push_midi`; the UI thread posts commands, and the two synchronous hops (touch-down / pen-down need the voice id) are **bounded at 250 ms** and fall back to the saturation path so a wedged MIDI thread can never ANR the UI (#53). Outbound wire writes are a JNI upcall because the endpoints are Java objects; the limiters stay native. **Two-byte messages ship as two bytes** — the wire encoder trims the third byte of 0xC0/0xD0 or hosts parse a phantom running-status message (#49). A cold start with Play mode persisted re-sends the loopback MCM once the instance exists (#47). The §4.6 field dump must run from a fresh start — the debug script does not reset the field (#48).
* **Android teardown contract (hard requirement):** `surfaceDestroyed` fires on the UI thread while the render thread may be mid-`sumi_render`. Android requires that `surfaceDestroyed` not return until nothing touches the surface. The JNI layer must therefore **block** it (mutex + condition variable, or render-thread join) until the render loop has finished its in-flight frame and unbound the surface (`eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)`), and only then call `ANativeWindow_release`. Skipping this yields intermittent `EGL_BAD_NATIVE_WINDOW` / native crashes on rotation or backgrounding that no functional test catches reliably. The core is untouched by this — it is pure host/JNI plumbing, consistent with §5.2 (render-thread-only calls).

---

## 6. Directory Layout

```text
midi-sink/
├── CMakeLists.txt                 # Root orchestration, FetchContent pins (also the Android externalNativeBuild root)
├── cmake/
│   └── CompileShaders.cmake       # Fetches pinned sokol-shdc binary, compiles GLSL → MSL/HLSL/GLSL330/GLES3
├── core/                          # libsumi — the C-ABI engine (§5); never generates MIDI
│   ├── include/sumi_core.h        # The pure C-ABI contract (+ module.modulemap for Swift)
│   └── src/
│       ├── engine.cpp             # Instance state, lifecycle, params, gesture ABI (drop/tine/vortex/wake/pinch)
│       ├── layouts.cpp            # §3.4 layouts + the instance-free probe (§5.3)
│       ├── swapchain_metal.mm / swapchain_d3d11.cpp / swapchain_gl.cpp
│       ├── renderer.cpp           # sokol_gfx passes, ping-pong orchestration (the only sokol TU above the swapchains)
│       ├── displacement.cpp       # Deformation queue → shader pass dispatch
│       ├── midi_normalizer.cpp    # SPSC drain, MPE/wind/classic decoding (incl. 0xA0) → §3.3 events
│       ├── voice_mapper.cpp       # §3.4 mapping, smoothing, budgets, bend/press/slide_mode routing
│       └── shaders/deform.glsl, composite.glsl
├── hostmpe/                       # Shared host-side MPE generation (§1.3, §8) — pure-C header, C++ behind it
│   ├── include/hostmpe.h          # allocator, joystick, stylus legato, limiters, strip engine, echo guard
│   └── src/hostmpe.cpp
├── desktop/                       # GLFW harness: gestures, scripted operator tests, libremidi
├── ios/                           # SwiftUI shell: SumiCanvas (bridge, transports, strip host), PlayOverlayView, ControlStripView, MidiOutputs/MidiSource
├── android/                       # Compose shell: sumi_jni.cpp (render thread, AMidi) + sumi_play.cpp (Phase-4 host), PlayOverlayView.kt, ControlStripView.kt, MidiOutputs, BleMidiPeripheral, SumiMidiDeviceService
├── tests/
│   ├── normalizer_tests.cpp       # normalizer/mapper/layout/probe goldens (headless, ~15 k checks)
│   ├── hostmpe_tests.cpp          # allocator, joystick, legato, limiter, strip, echo goldens (~1.6 k checks)
│   ├── abi_c_compile.c, hostmpe_c_compile.c   # both headers are pure C11
│   ├── midi_capture_alsa.cpp      # Linux-side wire capture for the tablet transports
│   └── fixtures/field_512_metal.bin           # §4.6 cross-backend field regression
├── tools/                         # midi_asserts.py, pen_trace.py (byte-log analysers), gen_icons.py
└── docs/                          # PROJECT_SPEC.md (this), DECISIONS.md, ROADMAP.md, CHANGELOG.md
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
* **Tablet shells (Phase 4):** iOS — SwiftUI + UIKit overlay views, CoreMIDI (virtual source, network session, BLE via `MIDISend` to the Bluetooth destination, IDAM destination), `UIPencilInteraction`; Android — Compose chrome + `View` overlays, AMidi, `MidiDeviceService`, a hand-rolled BLE-MIDI GATT peripheral (Android's `MidiManager` is central-only), the USB-MIDI gadget via `MidiManager`. Both consume `hostmpe.h` unchanged.

---

## 8. Touch & Stylus MPE Play Surface (Phase 4, v0.3 → v0.4)

The tablets become MPE instruments. Everything in this section is **host-side**: touch tracking, hit-testing, voice allocation and MIDI generation live in `hostmpe/` and the shells; the core's only addition is the read-only layout probe (§5.3). Formerly `PHASE4_SPEC.md`; corrected to what shipped (DECISIONS_3 #1–#68).

### 8.1 Two modes, one overlay
The tablet shells have a **mode toggle** in the settings sheet (persisted):
* **Marble mode** — the direct gestures: tap → drop, one-finger drag → tine, two-finger twist → vortex (Rankine profile: R = half the finger separation, ω = the twist delta), **two-finger pinch → Hamiltonian pinch** (fold axis = the finger-to-finger line, k from scale deltas; #41). Zero MIDI. This is the "Airwave-like expression tool".
* **Play mode** — the virtual MPE instrument: a joystick-per-touch surface whose cells follow the active layout. Touches generate standard MPE byte streams consumed twice: **loopback** into `sumi_push_midi()` (the visualizer is just another MPE synth) and **outbound** to external DAWs (§8.5).

Play mode exists on `CHROMA_GRID`, `JANKO` and `PIANO_GRID` only. FIFTHS and the rolls stay Marble-only (fifths' adjacent wedges are a *fifth* apart, so angular bend has no sane semitone scaling; rolls are timelines — "play the now-line" is deferred). Entering Play mode pushes the MCM + RPN 0 = 48 into the loopback FIRST, so the normalizer's MPE mode and bend range are deterministic, never heuristic (§2.5). Marble mode is bit-identical to Phase 3: the overlay is hidden and interaction-inert.

**Geometry has one source of truth.** Shells never re-derive lattice math: the lattice they draw is a **probe sweep**, hit-testing is a probe call, and bend scaling uses the probe's step (#9). Duplicating `layouts.cpp` in Swift and Kotlin is a drift bug waiting to happen.

### 8.2 Layout probe
`sumi_layout_probe` / `sumi_cell_info_t` (§5.3): a pure, instance-free free function of `(layout, params, aspect, x, y)` → nominal note, cell centre, `cell_radius` (= R_max, half the cell's smaller aspect-corrected dimension), the local semitone axis and `semitone_step` (the TRUE lattice step, not the glide-rendering cap). Callable from any thread with the shell's params snapshot; returns false for non-playable layouts, coordinates outside the playable area, and dead zones. Units contract (#6): centres are normalized [0,1]; radius and step are canvas-height units — the project's universal distance unit, the same one gesture radii and the shells' touch deltas use (pixel delta ÷ view height); the axis is a unit vector in aspect-corrected space. For Jankó the probe returns the note of whichever echo row was touched — any of a note's three rows plays it, and the loopback then paints all three echoes. Dead zones: the Jankó stagger ends and the piano grid's glissando corridor (§3.4).

### 8.3 The floating joystick model (fingers)
* **Touch-centred origin:** the contact point is the joystick centre; touch-down emits **centre pitch bend before Note On** on the allocated channel — in-tune attacks by construction. The cell supplies the nominal note and R_max.
* **Radial deadband with a soft knee, and an absolute floor** (#16): `d = ‖Δ‖ / R_max`; `g = 0` for `d ≤ 0.03`, else `(d − 0.03)/0.97` — smooth from exactly zero (a hard threshold is an audible zipper on the outbound pipe). The working deadband is `max(0.03·R_max, 0.006 canvas-height)`: finger jitter is an absolute quantity, and 3% of a Jankó cell is half a pixel. The knee reaches 1 exactly at the circle.
* **The knee is a deadband, not a travel limit** (#10): for BEND the deflection is the soft knee inside the circle and the **identity beyond it** (continuous at d = 1), so far from the origin the bend tracks the finger's lattice-pitch displacement absolutely — a one-cell drag is exactly one semitone whatever the R_max/step ratio, and glissandi across many columns stay in tune. The CLAMPED knee stays the law for the bounded axes (Y halves, stylus CC74, the indicator).
* **X → 14-bit pitch bend, scaled in semitones** along the cell's **local pitch gradient** (#17/#18: `semitone_dx/dy ÷ semitone_step`; horizontal on both lattice-uniform layouts, the shortest-neighbour diagonal on the piano grid): `pb = 8192 + round(bend_semitones / 48 · 8192)`, clamped to [0, 16383], with 48 the range the MCM declares on both pipes. Round AFTER the 8192 scale (#1) — one semitone = ±171 counts, asserted in tests.
* **Y is bipolar** (v0.4, #37): **up → channel pressure 0xD0 → ink feed; down → polyphonic key pressure 0xA0 (the voice's note on its member channel) → Lamb–Oseen swirl.** Touch-down = centre = both zeros; one radial knee serves both halves; `value = clamp(|Δy_eff|, 0, 1)·127` on the engaged half; crossing the centre releases the departing half through zero. Push away = feed, pull back = stir, both live with no mode flip and both distinct MIDI messages, so the loopback conformance property holds and DAWs record the swirl. Honesty note: many MPE synths ignore 0xA0 — the down axis is primarily a visualizer dimension that happens to be recordable.
* **Fingers emit no CC 74, ever** — timbre belongs to the stylus (§8.7). Change-only emission means silence, not a stream of 64s.
* **Lift → pressure 0 (and 0xA0 = 0 if the swirl half is engaged) → Note Off** (release velocity 64 when unmeasured); the channel returns to the allocator as most-recently-released.
* Synthesized finger velocity 96 (touch-size modulation behind a setting).

### 8.4 Velocity & pressure truth table (honest about glass)
| Input | Strike velocity | Continuous pressure | Extra |
| :-- | :-- | :-- | :-- |
| iOS finger | synthesized 96 (optional `majorRadius` modulation) | bipolar Y travel (§8.3) — no force API exists | — |
| Apple Pencil | real tip force in UIKit units (1.0 = an average finger touch): `velocity = 96 + (max(force, 1) − 1)·15.5`, clamped 127 — a baseline tap plays at the finger default, a hard stab is maximal, sub-baseline touch-down readings clamp UP (force builds after contact; #38) | true force → 0xD0 | barrel roll (Pencil Pro) = vibrato booster; squeeze = sustain pedal; hover ghost (M2+) |
| Android finger | synthesized, as iOS | bipolar Y travel — panel `getPressure()` is ignored for continuous control even where real, so both platforms play identically | — |
| S-Pen | normalized pressure `t = (p − 0.15)/(0.75 − 0.15)` clamped, `velocity = 96 + 31·t` — the same shape (#43; the two constants are hand-calibrated) | true pressure → 0xD0 | azimuth tail-stir = vibrato booster; barrel button = sustain pedal; hover via `ACTION_HOVER_MOVE` |

**No orientation sensor drives a global control.** The draft's tilt → CC 1 and azimuth → assignable CC are gone (#40): absolute orientation values are noisy, have no zero, and tilt cross-talks into every other axis; every attempt to stir the vortex from pen orientation leaked a phantom vortex and was removed. Orientation is used only as **derivatives, gated, decaying to neutral** — the booster of §8.7 — and the mod belongs to the strip.

### 8.5 Host MPE generation contract (`hostmpe/`)
* **Voice allocator:** least-recently-released round-robin over the 15 member channels (first-free would hijack the release tail of the note that just freed a channel — the classic MPE allocator bug); **external-occupancy masking** — channels holding an active note from a hardware device are unavailable (clears on its Note Off, device disconnect, or a 30 s stuck-note timeout refreshed by activity); **saturation is a silent drop + HUD blink — never steal.**
* **Single producer, preserved** (§5.2): both device MIDI and generated bytes cross one merge point per shell, which also feeds the occupancy mask and the byte log.
* **Echo suppression** (#66): a transport that mirrors our output back (a DAW's thru, a bridged virtual source, a looping network session) would make the shell consume its own bytes — marking its own channels externally held and painting every note twice; measured on an iPad: 99.5% of "external" input, median 0.3 ms. Every byte that actually leaves the device is recorded at the single transport emit point (`hostmpe_echo_record`); device input matching a record inside 300 ms is consumed and dropped (`hostmpe_echo_is_ours`) before the mask, the log and the loopback — one echo per emission, so a real device repeating the same bytes is never swallowed.
* **Dual-pipe output with per-transport policies.** Loopback: full rate, zero decimation (the core coalesces). Outbound: change-only filtering, then a PER-TRANSPORT limiter — **rate class** (per-voice, per-dimension latest-wins decimation to ≤ 100 Hz) for USB / virtual / MidiDeviceService / network, **budget class** (~300 msg/s global, latest-wins with round-robin per-voice fairness) for BLE. 0xA0 and the strip's master-channel CCs are continuous dimensions under the same policies (#30). **Never-dropped class** on every transport: Note On/Off, the centre bend, pressure-0-before-Note-Off, CC 64 and button CCs — and any batch that contains a Note On ships **whole** (a stylus legato crossing must arrive intact).
* **MPE configuration on both pipes:** MCM (RPN 6, lower zone, 15 members) then RPN 0 = 48 on every member channel — into the loopback on Play-mode entry (and again once the instance exists on a cold start, #47), out on every sink at session open, on a sink appearing mid-session (IDAM enable, USB mode flip — CoreMIDI `msgSetupChanged` / `MidiManager` callbacks, debounced, #28/#45), and on demand ("Re-sync DAW"), followed by the strip's announce (§8.8). Two-byte messages (0xC0/0xD0) go on the wire as two bytes (#49).
* **Panic and per-sink silence** (#26): a BLE peripheral cannot disconnect its central, so the meaningful control is a panic — every held voice released (pressure 0 → Note Off) then CC 64 = 0 + CC 123 = 0 on the master and all members, on the loopback and every transport, exempt from limiting. Switching one transport off silences that sink only.
* **Loopback conformance** — every generated stream is valid MPE as the project's own normalizer defines it, permanently tested by the loopback — with **two documented exceptions**: the stylus wake (physical, never MIDI — §8.7) and, under `slide_mode = 1`, the stylus's CC 74, which goes **outbound only** because the shell drives the azimuth-axis pinch through `sumi_add_pinch` directly and a looped CC 74 would pinch twice (#38; a DAW replay still pinches, via the mapper's CC 74 route with the pitch-axis fold).

**Transports.** iOS: (a) the **virtual CoreMIDI source** ("midi-sink Play Surface") for on-device DAWs, which also **sends explicitly to the IDAM destination** when a Mac is tethered — a virtual source is NOT bridged over the cable by itself (#27); wired to a Mac is the lowest-latency sink and needs no Audio-MIDI-Setup "Enable" (that button is IDAM audio); (b) the **MIDI network session** (rtpMIDI) for desktop DAWs over Wi-Fi and the Windows/Linux substitute; (c) **BLE** via `MIDISend` to the Bluetooth-driver destination — never a local mirror, which duplicated delivery (#25). Android: (a) the **USB-MIDI gadget — the primary sink**: the user flips the system USB mode to MIDI, Android publishes a class-compliant peripheral port (a `TYPE_USB` device with no host-side `UsbDevice`; status from the sticky `USB_STATE` broadcast: active / charge-only / unsupported, #45), visible to any host OS; (b) the **`MidiDeviceService` virtual device**; (c) a **BLE-MIDI peripheral** implemented as a GATT server (Android's `MidiManager` is central-only): BLE-MIDI 1.0 framing, one notification in flight per link, messages coalesced into the tail packet, and a backlog cap that drops from the **newest** end so the handshake and note events at the head survive (#44). One generator, N sinks: every transport carries the identical stream under its own policy.

### 8.6 Overlay & feedback UI
* A native gesture layer over the render view (UIKit / Android `View`), **never inside the core**, keeping the full canvas bounds (#31/#42).
* The lattice is a **probe sweep** drawn **two-tone** (#41): a paper-cream halo under each dark ring so cells stay legible over dense ink; accidental rings drawn last, on top — black keys sitting on the keybed. Each cell is drawn **round, at the radius of the joystick it generates**: the circle you see is the touch region and the travel bound (#59). The marbling stays the star: low opacity.
* Per-touch joystick indicator — hairline circle of radius R_max at the origin, thumb dot at Δ_eff — visualizes the deadband and bounded travel exactly as computed (`hostmpe_joystick_eff`). Jankó highlights all three echo rows of a touched note. Saturation blinks the HUD. Stylus hover draws a ghost cursor where the hardware reports hover; a squeeze/button pedal lights the strip's Sus pad.
* Latency budget: touch-down → visible drop **≤ 2 rendered frames** (measured 0.39 ms median on iPad, 3.7 ms on the Android tablet — the loopback enqueue beats the same frame's drain).

### 8.7 The stylus: per-cell legato, boosted vibrato, the wake
The pen abandons the joystick — precision earns **absolute-position play**. Since the finger joystick already bends continuously and semitone-exactly, the stylus's job is **real note changes** (#39):
* **Per-cell legato retriggers, one behaviour on all three playable lattices.** The shell probes the cell under the tip on every move. Crossing into a new cell emits, on the SAME channel, `bend(offset) → Note On(new cell, live velocity) → Note Off(old note)` — the legato overlap idiom: mono/MPE synths glide, DAWs record real terminated notes, the normalizer's same-channel steal ignores the stale Off, and the attack is in tune because the bend precedes the Note On. Sounding pitch is continuous across a crossing on CHROMA_GRID and JANKO (the offset flips sign as the reference cell changes — sub-cent in the goldens, a few cents live; #52's speed-relative analyser asserts it). A crossing commits only once the tip is **±0.65 st past the current note** (boundary hysteresis) — a vibrato wiggle at a cell edge bends, it never machine-guns retriggers. On PIANO_GRID the in-cell bend spans ±0.5 st while a natural→natural step is a whole tone, so ~1–1.6 st of every crossing arrives as a step: **a quantized glissando, by design** (#52) — the white-key run along the natural band, the black-key run through the corridor (§3.4). Dead zones make no call: the last pitch sustains; a stroke must start on a key. The draft's ±47 st re-anchor and 20–40 ms retune ramps are retired — bend never accumulates past half a cell.
* **In-cell offset = the bend, always live at ×1.** Wiggle inside a cell and it bends (vibrato without retriggering); the bend routes through `bend_mode` like any per-note bend — glide drags the drop, ripple shimmers the water.
* **The vibrato booster** (#40, derivative-only gestures, never knobs): the **Pencil Pro barrel roll** multiplies the in-cell bend ×1 → ×3 while rolling (gain 4/rad, per-event spike cap, 0.008 rad jitter floor) and **decays back to ×1 with τ = 0.4 s** — a still hand is a still value, a regrip never jumps it. `hostmpe_pen_glide` takes the scale and multiplies only the EMITTED bend; note tracking and hysteresis stay on raw geometry. Where no roll axis exists (S-Pen, older Pencils) the **azimuth tail-stir** is the feed: lean deltas count only while the tip is planted (< 3 pt travel per event — a moving tip is handwriting, never a gesture), past a 0.02 rad floor, and never near-vertical where UIKit's azimuth swings wildly. Both feeds freeze while altitude is changing (a tilt is posture, not a gesture). **Known and accepted (#68):** with the booster engaged a crossing is no longer pitch-continuous — the boosted offset can carry pitch past the note step. Deep vibrato plus a simultaneous slide is an extreme gesture; options are recorded, none taken.
* **Y → CC 74** (stylus-only, §8.3): knee-shaped Δy about the strike, `cc = clamp(64 + round(Δy_eff·63), 1, 127)`, centre 64 at pen-down, up = brighter, change-only. **Z → 0xD0** from true tip force, change-only.
* **The dipolar wake rides every stroke segment** in both modes via `sumi_add_wake` (tip radius `a = 0.006 + 0.030·force`). It is physical, not musical, and therefore **not in the MIDI stream**: a DAW recording of a stylus performance replays notes, bends, CC 74 and pressure exactly, but never the wakes — a DAW has no stylus in the water.
* **Pinch via the pen:** with `slide_mode = 1`, smoothed CC 74 deltas drive `sumi_add_pinch` at the pen position with the fold axis from the pen's **azimuth**; CC 74 then goes outbound only (§8.5). `slide_mode = 0` keeps the v1 per-drop aux behaviour — a params choice, never a silent rebinding.
* **The pen's pedal:** the Pencil Pro **squeeze** (`UIPencilInteraction`, began = down, ended/cancelled = up; Pencil 2 double-tap latches) and the S-Pen **barrel button** (`ACTION_BUTTON_PRESS/RELEASE` plus a `buttonState` transition on any stylus event, idempotent; a pen leaving the digitizer releases a momentary pedal) drive the **same strip sustain engine** as the palette's pad (#58/#62): the momentary/toggle setting governs both, the pad mirrors what the pen did, the message is master-channel, never-dropped, re-announced after a re-sync, cleared by a panic. Play mode only.
* Palm rejection is the platform's: pencil touches are typed, fingers keep the joystick path. Pen-as-lead, fingers-as-chords is the expressive story.

### 8.8 Performance control strip (Play mode)
A **compact floating palette at the top-left, over the full-canvas lattice** (#31 — the draft's docked band displaced every drop from its touched cell by the strip height, which kills the instrument feel), hidden in Marble mode, built from the joystick primitive: a touch anchors its origin and the §8.3 soft knee shapes Δy (travel bound 60 pt). Display is a mirror; all value state lives in `hostmpe_strip_t`, so values persist across mode and layout switches by construction.
* **Channel discipline:** every strip message goes out on the **master channel** — member channels are never touched; global controls and per-note voices are disjoint, as MPE intends.
* **Widgets:** a **spring wheel** — Pitch, master bend ±2 (the MPE master default; the strip sends no RPN 0 on ch 1), deflection maps to value while held, on release a ~50 ms ramp to centre with a **guaranteed exact-centre final message** (a snap is a zipper); unaffected by `bend_mode`, which governs the per-note bend only. A **latch wheel** — Mod (CC 1): relative delta accumulation, no absolute-set entry point so a regrasp cannot jump. A **button** — Sustain (CC 64), **momentary by default** (press-and-hold pedal feel), toggle behind a setting; a mode switch while ON emits the OFF. **Two assignable latch wheels** (defaults CC 23 / CC 24; natural homes for the ripple's CC 102/103) edited by long-press — the recognizer is restricted to the two assignables (a long-press that fired on the sustain pad cancelled its touch and released a held pedal, #31); protocol CCs (1, 6, 38, 64, 98–101, 120–127) are refused engine-side with the reason shown.
* **Dual-pipe like everything else:** to the loopback (where the CC map routes CC 1 → vortex, so the mod wheel stirs the water while it modulates the synth) and to every sink — wheels **policed** as continuous dimensions, buttons **exempt** (a decimated sustain-off is a stuck pedal). `hostmpe_strip_announce` restates the latched state after every MCM re-sync so DAW and strip never disagree. CC assignments are deliberately not persisted (deferred).
* **CC 64 is a pedal, not a paper dip, in Play mode** (§2.4, #67).

### 8.9 Evidence discipline (how the surface is verified)
The byte log at each shell's merge point tags every message by source — 0 device, 1 finger, 2 session config, 3 strip, **4 stylus** (#63) — so one analyser set serves both platforms: `tools/midi_asserts.py` (handshake order, centre-bend-before-strike, pressure-0-before-Note-Off, no finger CC 74, no allocation on externally-held channels, strip on master only, sustain never stuck, the §8.5 rate policies) and `tools/pen_trace.py` (per-stroke legato reconstruction: same-channel overlaps, speed-relative continuity on the continuous lattices, overshoot guard on all). The headless suites run **on-device** in the Android build; iOS pulls its logs and timed screen captures with `devicectl device copy from` (#64). The two false greens the review caught (#54) are why every assert has a negative control.

---

## 9. Implementation Roadmap

The step-by-step implementation plan, acceptance criteria, and agent working rules live in **`ROADMAP.md`** (Parts 1–3: v0.1, v0.2, Phase 4). Feed it to the implementing agent one step at a time alongside this document. Do not begin a step while the previous step's DONE checks fail.
