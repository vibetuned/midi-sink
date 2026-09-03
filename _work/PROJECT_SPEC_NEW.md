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
  * Polyphonic Key Pressure (0xA0, keyed by the voice's note on its member channel) → *swirl* (v0.4) — the second pressure dimension. None of the reference hardware sends it (the play surface does, PHASE4 §3.3); it is decoded whenever present.
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
* Mapping model: the single voice is a **wandering ink brush**. Note pitch sets the target canvas position; a legato note change *migrates* the active drop's feed point (drawing a tine-like wake as it travels) rather than spawning a disconnected new drop. Breath modulates the brush's **width**, not an unbounded ink feed — the brush relaxes toward a breath-proportional target width (see §4.4), producing a calligraphic line rather than a growing blob.

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
PaperDip     { }                            // CC64 rising edge, or ABI call
```

### 3.4 Musical → spatial mapping (default profile)
* **Pitch → position** is a pluggable **layout** (`pitch_layout` param, switchable live; a layout is a pure function `(note, params, aspect) → 1–3 positions` plus an optional per-frame field motion). Most layouts return a single position; a layout may return up to `SUMI_MAX_ECHOES = 3` **echoes** — multiple canvas sites that are all the same note (see the echo-set rules below):
  * `0 — SUMI_LAYOUT_FIFTHS`: pitch class around a circle (circle-of-fifths angular layout), octave → radius (low notes outer, high notes inner). One position. The v1 default.
  * `1 — SUMI_LAYOUT_CHROMA_GRID`: chromatic reading-order grid. C1 at top-left, B7 at bottom-right: row = octave (1..7, top to bottom), column = pitch class (C..B, left to right), cells inset so drops at grid edges stay on canvas. Notes outside C1–B7 clamp to the nearest edge cell. One position.
  * `2 — SUMI_LAYOUT_JANKO`: Jankó keyboard grid — 6 staggered rows of whole-tone columns; column = note/2, row parity = note%2, with the classic half-column offset on alternate rows. **The note stamps on all three rows of its parity** (rows {0,2,4} or {1,3,5}) — the full 6-row lattice stays live, which is the entire point of visualizing Jankó. Three echoes.
  * `3 — SUMI_LAYOUT_ROLL_H`: horizontal piano roll. Pitch → y (low at bottom), all drops spawn on a fixed **now-line** at x = 0.12; the whole field translates +x continuously at the roll speed (see *field motion* below). Reads left-to-right like a DAW timeline flowing away from the playhead. One position.
  * `4 — SUMI_LAYOUT_ROLL_V`: vertical piano roll (Synthesia-style). Pitch → x (low at left), now-line at y = 0.12 from the top, field translates downward. One position.
  * `5 — SUMI_LAYOUT_PIANO_GRID`: classical piano grid — the chroma grid's frame (C1–B7, same insets, same out-of-range clamp to the edge octave keeping pitch class) with each octave drawn as a two-row keyboard: 5 accidentals on top at the classic boundary positions (C♯/D♯ and F♯/G♯/A♯ between their naturals; the E–F and B–C gaps stay empty), 7 naturals below. 7 octaves × 2 = 14 rows. One position. Playable (see the play-mode surface): the accidental row's gaps and ends are honest dead zones, R_max is the key's playable footprint (half of min(key width, octave-pair height)), and the semitone axis is the generic shortest-neighbor diagonal — pitch is *not* a function of x alone on this lattice (DECISIONS_3 #29).
* **Echo-set rules (multi-echo layouts):** a voice owns its full echo set for its lifetime. **Voice dynamics — press feed, glide, slide, lift — apply to every echo of the voice**, and all echoes **share one ink band and one aux value**: the drop counter increments once per `VoiceBegin`, not per echo, so the echoes carry identical band parity and hue and read unmistakably as the same note (radial coordinates remain per-echo local). Glide displaces every echo along the lattice's local semitone axis (in Jankó: half-column over, one row up — the same vector for all echoes since the lattice is uniform). Each echo's deformation passes **count individually against the per-frame budget** (§ rate limiting below) — a 10-voice Jankó performance emits up to 30 feed passes/frame, which the budget's overflow merging must absorb, not silently drop whole echoes (merge within an echo across frames, never cull one echo of a set while feeding another).
* **Field motion (roll layouts only):** the scroll is itself a closed-form deformation — a uniform translation pass with inverse lookup `P_src = P − v̂ · s·dt`, where speed `s = (bpm / 60) × roll_speed` in canvas lengths/second (`bpm` and `roll_speed` are params; `roll_speed` is canvas-lengths-per-beat, **default 0.0625 = 1/16** — the full canvas holds 16 beats of history, i.e. 4 bars of 4/4, so at 120 BPM ink lives on screen for 8 seconds; 0.25 would flush the whole canvas every bar, which reads as a waterfall rather than a drifting tray). **Ingress must be an explicit shader branch:** when `P_src` falls outside [0,1], the fragment writes fresh water — `ink = 0`, `aux = 0`, and the **identity coordinates of its own texel**. This cannot be delegated to sampler state: clamp-to-edge would streak the boundary texel's old ink across the entering region, and clamp-to-border cannot work because fresh water is not a constant (the identity coords vary per texel). Old ink slides off the far edge; fresh water enters at the now-line side. All other deformations (tines, vortices, feeds) still apply on top of the scroll, so expressive gestures smear downstream like ink in a current. **BPM is host-supplied** (a param, optionally updated live from a DAW/link source by the host); the core never guesses tempo from MIDI in v2 (MIDI clock ingest is a possible v3 item).
* **Strike → initial drop radius** (perceptually scaled: radius ∝ sqrt(velocity) so ink *area* tracks velocity).
* **Press → sustained ink feed (boundary growth):** while pressure > 0 the voice's drop keeps growing. Because a center expansion of radius r moves an existing boundary R only to sqrt(R² + r²) (area conservation), naive fixed radius steps stall visually. Instead the voice tracks its nominal boundary R; the per-frame growth ΔR ∝ smoothed pressure × dt × expansion_rate is converted to the emitted pass radius **r = sqrt((R + ΔR)² − R²)**. Same closed-form framework, physically meaningful growth rate. This is the key Osmose behavior, and it is deliberately unbounded — sustained pressure can flood the canvas. **`press_mode` (v0.4)** routes the 0xD0 channel-pressure consumer — 0 = this ink feed (default), 1 = the Lamb–Oseen swirl (§4.3(7)) — one consumer owns 0xD0, the bend_mode/slide_mode pattern; it exists so pressure-only hardware (ROLI, Osmose) can reach the swirl voice. The 0xA0 poly-pressure dimension routes to the swirl unconditionally, in either mode.
* **Swirl (0xA0, or 0xD0 when press_mode = 1) → Lamb–Oseen vortex at the voice's center (v0.4):** Γ per pass ∝ smoothed amount × dt × expansion_rate (a delta rate, like every continuous feed — never an absolute value), core radius r_c = the voice's current nominal boundary R (the drop IS the vortex core: its own rings rotate near-rigidly and stay coherent while the far field stirs the neighbors). Rotation sign defaults to the voice's band parity — adjacent notes counter-rotate, zero configuration. Swirl passes fan out to every echo and count against the budget like any feed.
* **Glide (per-note bend) → local tine:** drag *that drop's* center along the pitch axis, emitting a narrow tine stroke along the drag path each frame the bend changes. This must be per-voice, never a global shear, in MPE mode.
* **Slide (CC74) → per-drop palette/ink-density modulation** for the layers that voice owns.
* **Lift → the drop "sets":** stop feeding; optionally emit one final faint surfactant ring proportional to lift velocity.
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

   The naive global saddle (w ≡ 1) acts on the whole canvas; the naive radial window breaks incompressibility. Windowing by the **streamline value s = xy** does neither: s is conserved along each hyperbolic trajectory, so the exponent is constant along any path and the map stays closed-form with det = 1 **exactly**. Honest caveat, stated as a feature: strength cannot decay along the two fold axes themselves (s = 0 there), so the pinch's arms run outward as fading creases — which is what pinched paper physically does. k per pass is always a smoothed **delta** (never an absolute controller value — absolute feeding integrates into runaway strain). Sign of k swaps which axis compresses: a naturally bipolar gesture. **Two looks, one params switch (v0.4, DECISIONS_3 #34):** `pinch_variant` 0 = this Hamiltonian saddle (det = 1 exactly); 1 = **crossed tines** — the step-19 prototype rival kept after the pick-by-eye pair (two perpendicular opposing tine passes through the pinch point; softer, lumpier, directional — costs two passes per emission). Honored by both routes: the CC74 `slide_mode = 1` path and `sumi_add_pinch`.

6. **Sine ripple** (v0.4) — the traditional marbler's waved comb, and a pure shear: x_src = x − A · sin(k·y + φ) (in the ripple frame, rotatable by ripple_angle); y_src = y Jacobian = 1 identically at ANY amplitude; each line translates rigidly, so the map never folds — no sub-stepping. At fixed (k, φ), ripples form a commutative group: applying ΔA per frame composes additively, so an LFO that breathes A up and back to zero leaves the field bitwise unchanged. Changing k or φ between passes breaks commutativity and bakes residue in — that residue is marbling (the waved-comb feathering); the boundary is deliberate and documented. **`bend_mode` governs the PER-NOTE pitch bend** (corrected in v0.4 implementation, DECISIONS_3 #35 — the intent is subtle vibrato: a note's bend wobble shimmers the water instead of wiggling its drop): mode 0 (default) = v1 glide — a note's bend drags its drop along the pitch axis; mode 1 = the bend feeds the ripple and the drop HOLDS (one consumer owns the note bend; flips are clean in both directions — the glide target is left untouched in mode 1 so switching back never emits stale-delta tines). Master bend keeps its v1 shear tine in BOTH modes; CC1/vortex untouched. **The ripple amount IS the bend's distance from center:** amp = |semitones| / 6, clamped (a ±0.5-semitone vibrato breathes ~8%; last-writer-wins across voices, smoothed like any global control) — so the water stills itself: bend re-centers → amount 0, last note's release → amount 0, a 1→0 mode flip zeroes the residual. The wavelength k stays a flavor control (SUMI_CTL_RIPPLE_FREQ, resting mid-range; shells map CC 103, and CC 102 is an external amp handle sharing the slot last-writer-wins). Exists at two insertion points, selected by ripple_bake: live (composite-time view displacement — the ink sampling coordinate ripples, nothing accumulates, the water shimmers and stills; see §4.5) and bake (a real deform pass, delta-driven like the pinch). **Bend-driven bake is PERMANENT by design (DECISIONS_3 #36):** the emitted pass phase drifts with activity (φ += |ΔA|/A_max · 1.5 rad, wrapped), so an excursion never retraces exactly — each vibrato cycle lays a slightly shifted comb, a feathered record that accumulates like glide tines, while the DYNAMIC still stills (the three come-back paths above). CC-driven bake keeps φ fixed — the pure composing-back group property, preserved for the Step-19 gate. On the tablets the Ripple toggle selects bend_mode = 1 AND ripple_bake = 1 together; desktop keeps a manual live/bake override for experimentation.

7. **Lamb–Oseen swirl** (v0.4) — the exact closed-form solution of the 2D Navier–Stokes equations for a decaying line vortex, evaluated analytically. The project's founding rule bans NS *solvers*; here NS enters as an exact solution — the philosophy's best vindication. Tangential velocity v_θ(r) = Γ/(2πr) · (1 − exp(−r²/r_c²)) integrates over the pass interval to the rotation angle:

   θ(r) = (Γ·Δt / 2πr²) · (1 − exp(−r²/r_c²)),   rotate P around the center by −θ(r)

   A pure rotation by θ(r): exactly area-preserving, and — like the Rankine — an **exact map at any angle: no sub-stepping, ever**. Where the Rankine has the crease-ring kink at R, Lamb–Oseen is C∞ — solid-body rotation in the core blending viscously into the same 1/r² far field: the soft organic twist beside the Rankine's mechanical one. **Numerical guard:** the naive expression is 0/0-shaped at small r and float-cancels — evaluate via `expm1` (or below r ≪ r_c use the limit θ(0) = ΓΔt/(2πr_c²) directly). Driven per-voice by the *swirl* dimension (§3.4): r_c = the voice's boundary R, Γ ∝ smoothed amount × dt, sign from band parity.

### 4.4 Continuous feeds
Sustained pressure/breath is realized as **incremental drop expansions re-emitted per frame** at the voice's current center, using the boundary-growth conversion of §3.4 (emitted radius r = sqrt((R+ΔR)² − R²)). This keeps everything inside the same closed-form framework — no velocity field is ever introduced. **Wind mode is the exception to unbounded growth:** a breath-fed brush relaxes toward a breath-proportional *width* (target ≈ 0.006 + 0.05·breath canvas heights; growth only up to the target, and a `VoiceMigrate` clamps the new segment down to the current width). Literal integration would turn a 20-second legato line into a canvas-sized blob; MPE press keeps the unbounded integration — that is the Osmose behavior.

### 4.5 Composite pass (`composite.glsl`)
* Sample the active displacement target, map ink phase → alternating sumi ink rings vs. clear water. **Live ripple (v0.4):** when `ripple_bake = 0` and ripple amplitude > 0, the *ink sampling coordinate* is displaced by the §4.3(6) shear before the field lookup — a non-destructive view displacement; the field itself is untouched. The washi grain does NOT ripple (it is screen-locked, per the invariant below), and **the paper dip always samples the un-rippled field** — the print is what touches the water; the shimmer is surface motion, not ink position.
* Modulate with procedural (simplex) washi mulberry-fiber noise and absorption grain; fiber strength = `paper_roughness`. **Invariant: paper is screen-locked.** Fiber/grain noise is sampled in screen/NDC space, never through the deformed UV field — only ink sampling goes through the field. Paper is the stationary substrate; if fibers warp with tines or drift with roll-layout scrolling, this invariant has been broken (regression check: run a tine and a 120 BPM scroll over textured output and confirm the grain stays put while ink moves).
* Palettes: Sumi black, Indigo, Ochre (id-selected via params; `aux` channel can offset hue per drop).
* Output color space: render in linear — the composite shader applies the exact sRGB encode manually (identical behavior across Metal/D3D11/GL/GLES; the RGBA8 print target gets the same encode, so exports match the screen bit-for-bit in tone).

### 4.6 Orientation convention (one y-down space, flips only at the boundary)
The entire deformation chain works in **texture space, v grows downward, row 0 = top**. Fullscreen-triangle vertex shaders emit `st = (u, 1 − v_clip)` — the texture-space coordinate of the fragment's own texel — and every pass (identity init, deforms, composite) operates purely in that space. Rationale (found the hard way on Metal): sampling at the raw NDC interpolant vertically flips the field every offscreen pass, so consecutive deformations cancel instead of composing; with `st`, passthrough is a true no-op and deformations compose exactly (regression check: 40 passes of a z/40 tine must equal 1 pass of a z tine). Mouse/touch coordinates, texture rows, and screen rows all share this one y-down space.

**Backend rule (critical for GLES3/Android):** Metal and D3D11 share top-left row origins, but OpenGL/GLES defaults to bottom-left. Any vertical flip a backend needs lives **exclusively in the final swapchain composite pass** (and, mirrored, in the print readback path) — the deformation chain must never contain backend-specific branches. A cross-backend regression test (identical deform script → identical field texture readback, bitwise within float tolerance) guards this.

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
    SUMI_CTL_RIPPLE_AMP      = 7,   /* v0.4: sine ripple amplitude A             */
    SUMI_CTL_RIPPLE_FREQ     = 8,   /* v0.4: ripple wavenumber k                 */
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

typedef enum {                   /* pitch → position layouts, see spec §3.4 */
    SUMI_LAYOUT_FIFTHS      = 0, /* circle-of-fifths radial (default)         */
    SUMI_LAYOUT_CHROMA_GRID = 1, /* C1 top-left … B7 bottom-right             */
    SUMI_LAYOUT_JANKO       = 2, /* staggered whole-tone Jankó grid           */
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
    uint32_t bend_mode;          /* PER-NOTE bend routing (DECISIONS_3 #35):
                                    0 glide (v1), 1 sine-ripple amount — the
                                    drop holds; master bend unaffected. One
                                    consumer owns the note bend               */
    uint32_t press_mode;         /* 0xD0 channel-pressure routing: 0 ink feed
                                    (v1 grow, default), 1 Lamb–Oseen swirl —
                                    hardware's door to the swirl voice. 0xA0
                                    poly pressure → swirl in either mode      */
    uint32_t slide_mode;         /* CC74 routing: 0 per-drop aux (v1 behavior),
                                    1 Hamiltonian pinch (delta-driven)         */
    uint32_t vortex_profile;     /* sumi_vortex_profile_t for CC-routed vortex */
    uint32_t ripple_bake;        /* 0 live (composite view), 1 bake (deform)   */
    float    ripple_angle;       /* ripple frame rotation, radians (dflt 0)    */
    uint32_t pinch_variant;      /* 0 Hamiltonian saddle, 1 crossed tines      */
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

/* Manual touch / mouse gestures — render thread only, normalized [0,1] coords. */
SUMI_API void             sumi_add_drop  (sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type);
SUMI_API void             sumi_add_tine  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float alpha /*sharpness*/, float magnitude);
SUMI_API void             sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius,
                                          uint32_t profile /* sumi_vortex_profile_t (v0.4) */);
/* v0.4: dipolar wake — the stylus stroke's fluid signature (spec §4.3(4)).
   NOT expressible as MIDI: this is a gesture-ABI-only deformation, and a MIDI
   recording of a stylus performance replays notes but not wakes (documented
   invariant, PHASE4_SPEC §7). The core sub-steps internally (≤ a/2 per pass). */
SUMI_API void             sumi_add_wake  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float tip_radius);

/* Layout geometry probe (v0.3, Phase 4) — pure, INSTANCE-FREE query for
   host-side play surfaces (hit-testing, bend scaling). Callable from any
   thread; the caller supplies a params snapshot and aspect. Returns false
   for non-playable layouts (FIFTHS, rolls) or coords outside the playable
   area. See PHASE4_SPEC.md §2. */
typedef struct {
    uint8_t  note;            /* nominal MIDI note of the cell under (x, y)   */
    float    cell_center_x;   /* normalized canvas coords of the cell center  */
    float    cell_center_y;
    float    cell_radius;     /* half the smaller cell dimension (R_max)      */
    float    semitone_dx;     /* unit vector toward +1 semitone (glide axis)  */
    float    semitone_dy;
    float    semitone_step;   /* canvas distance of +1 semitone along it      */
} sumi_cell_info_t;
SUMI_API bool             sumi_layout_probe(uint32_t layout /* sumi_layout_t */,
                                            const sumi_params_t* params, float aspect,
                                            float norm_x, float norm_y,
                                            sumi_cell_info_t* out);

#ifdef __cplusplus
}
#endif
#endif /* SUMI_CORE_H */
```

`sumi_version()` returns 0.2.0 once the layout system and params extensions above land (the struct grew — hosts must be rebuilt; the struct is passed by pointer with no size field by design, so version gates compatibility), 0.3.0 once `sumi_layout_probe` lands (Phase 4), and 0.4.0 once the v0.4 batch lands (§4.3(3–7): Rankine profile, wake, pinch, ripple, Lamb–Oseen swirl — `sumi_add_vortex` gained a profile argument, breaking; params grew bend_mode/press_mode/pinch_variant and the swirl dimension entered the normalizer). Host-synthesized MIDI (touch play surfaces, PHASE4_SPEC.md) counts as device MIDI under §5.2: it must flow through the shell's same single producer thread.

### 5.4 Host bridge notes (phase 3)
* **iOS / SwiftUI:** because `sumi_core.h` is pure C, no wrapper code is needed — a `module.modulemap` exposing the header lets Swift `import SumiCore` directly. The shell is ~30 lines: a `UIViewRepresentable` whose backing `UIView` overrides `layerClass` to `CAMetalLayer.self`, passes the layer to `sumi_create` (backend METAL), drives `sumi_update`/`sumi_render` from a `CADisplayLink`, calls `sumi_resize` from `layoutSubviews` (with `contentScaleFactor` as pixel_ratio), and forwards CoreMIDI packets to `sumi_push_midi`. The core's per-frame autorelease pool (see DECISIONS #12) already covers the non-runloop render path.
* **Android / Compose:** a `SurfaceView` inside `AndroidView`; a small JNI wrapper receives the `Surface`, converts via `ANativeWindow_fromSurface`, creates the EGL context (GL backend: host owns the context per §5.1), makes it current on a dedicated render thread, and calls `sumi_create` with `handle = NULL, backend = GL`. AMidi (via libremidi's android backend or directly) feeds `sumi_push_midi` from the MIDI thread — the §5.2 SPSC contract maps 1:1. Default `sim_scale` 0.75 (see params comment).
* **Android teardown contract (hard requirement):** `surfaceDestroyed` fires on the UI thread while the render thread may be mid-`sumi_render`. Android requires that `surfaceDestroyed` not return until nothing touches the surface. The JNI layer must therefore **block** it (mutex + condition variable, or render-thread join) until the render loop has finished its in-flight frame and unbound the surface (`eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)`), and only then call `ANativeWindow_release`. Skipping this yields intermittent `EGL_BAD_NATIVE_WINDOW` / native crashes on rotation or backgrounding that no functional test catches reliably. The core is untouched by this — it is pure host/JNI plumbing, consistent with §5.2 (render-thread-only calls).

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
