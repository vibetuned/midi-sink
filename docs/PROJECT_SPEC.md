# PROJECT SPECIFICATION: Suminagashi MPE Visualizer Engine
**Architecture Paradigm:** Option 2 — Embedded High-Performance C-ABI Core + Host Platform Shells
**Phase 1 Target:** macOS desktop (Metal). **Phase 2:** Windows (D3D11), Linux (OpenGL 4.1 core). **Phase 3:** iOS (SwiftUI + CAMetalLayer), Android (Compose + ANativeWindow + GLES3). **Phase 4 (v0.3 → v0.4):** the tablets as MPE instruments — the Touch & Stylus Play Surface (§8), the shared `hostmpe/` host library, and the v0.4 deformation operators (§4.3(3–7)). **Phase 5 (v0.5 → v1.0):** packaging, the release spine and five lanes, the WebGPU seam and the marble web, the documentation site, and the feedback batches (§9).
**Reference Hardware:** ROLI Piano + ROLI Airwave (owned), Expressive E Osmose (planned), Roland Aerophone Brisa / Odisei Travel Sax (planned), plus any classic single-channel MIDI keyboard. Play surface: iPad Air 11" (M4) + Apple Pencil Pro, Galaxy Tab S8 Ultra + S-Pen.
**Spec v6** — at the Phase-16 fold (2026-10-26) the v3.0 arc's spec draft (`_work/TO_PROJECT_SPEC.md`: opened at the arc's start as the author's proposals corrected to the tree, each step correcting the subsection it shipped — in git history with the proposals, `git show 21ff974:_work/TO_PROJECT_SPEC.md` and `…:_work/specs/`; `KESSHO §n` is `medium_specs_v3.md` there) transcribed as shipped: §5.3's ABI history, §10.2's exactness rule and its new members, **§10.7 Kessho** with the minerals, **§10.8 Symplectic harmony** — the Arnol'd lock, the chord wave, the braid — **§10.9 the Poincaré strobe**, **§10.10 the strain buffer, the seed, the spark reading and the leader stroke**, **§12.7–§12.12 Voxo DS 2.0**, **§13.8–§13.14** and §13.7 revised, **§14.7** the Boehm layout and the split surface, **§15.5** preset-next, §16 carried past 3.0, §17. Where this text and a decision entry disagree, the entry is the record.
**Spec v5** — absorbs the Phase-4 spec (§8), the Phase-5 spec (§9) and, at the Phase-10 fold (2026-10-13), the v2.0 arc's specs as shipped: **§10 Media**, **§11 Quality of life (shipped)**, **§12 Sound** and **§13 Synth** transcribed from `specs/TO_PROJECT_SPEC.md` (its pointers folded into §1, §4.2, §4.3, §5.3 and §16), **§14 Instruments** from `specs/INSTRUMENT_SPEC.md`, **§15 Quality of life — the undone items** from `specs/QUALITY_OF_LIFE_SPEC.md`, and **§16 what carries past 2.0**; the three files left the tree at that fold (`git show bae21c4:specs/<FILE>.md` — `INSTRUMENT §n`, `QOL §n` and `TO_PROJECT_SPEC §n` in `DECISIONS.md` Parts V–IX and in the code's comments mean those files; `MEDIUM §n` is `git show 76e2f5e:specs/MEDIUM_SPEC.md`, `SOUND §n` `git show 7ceb111:specs/SOUND_SPEC.md`, `SYNTH §n` `git show 5111748:specs/SYNTH_SPEC.md`). Every `[ITERATE]` those specs carried is resolved inline by the decision entry named (`DECISIONS_9 #1`, the table) or marked carried in §16. Every Part-III to Part-IX decision (`DECISIONS.md`, referenced as `DECISIONS_3 #n` … `DECISIONS_9 #n`) is absorbed. Where this text and a decision entry still disagree, the entry is the record of what shipped; fix the text.

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
5. **The host libraries beside `hostmpe/`** (the v2.0 arc, every one pure C at its surface, none linked by the core): `voxo/` — the sound, a sibling core with its own header, the Decent Sampler player and **Suzu** the synth inside it (§12, §13); `presets/` — the one session serializer (§11); `replay/` — the recorder and the player (§15.1); `trace/` — the orbit trace's bridge from Voxo to libsumi, shared by the three native shells (§13.2); `rtpmidi/` — the Tab's network session (§8.5).

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
* Treat as **global field controls**, not voices. The core exposes a small CC-routing table (`sumi_map_cc`, §5) and the shells a CC-map editor. The device ships twelve CCs 20–31 in left/right pairs — Grasp 20/21, Slide 22/23, Glide 24/25, Raise 26/27, Tilt 28/29, Flex 30/31 (measured, DECISIONS_4 #50). **Default map (DECISIONS_4 #69, symmetric hands — each hand stirs its own water):** Raise = strength, Glide = centre X, Slide = centre Y (Y reversed at consumption: hand up = centre up) — the left hand the exponential/Rankine vortex (26/24/22), the right hand the Lamb–Oseen swirl control trio `SUMI_CTL_SWIRL_STRENGTH/X/Y` (27/25/23); Grasp = the pinch (20 saddle at the vortex centre, 21 crossed tines at the swirl centre, delta-driven like the CC 74 route); Tilt = the ripple (28 wavelength, 29 amount); Flex 30/31 free — it cannot be played without disturbing the others. Viscosity, roughness and palette morph have no Airwave route by default (settings sliders; the editor rebinds them).
* Default bindings exist so it works out of the box, but every CC number is remappable because Airwave assignments are user-configured on the device side. A persisted map that is an older stock map upgrades itself on load (DECISIONS_4 #71).

### 2.3 Monophonic Wind Sources — Roland Aerophone Brisa, Odisei Travel Sax
* Single channel; **CC 2 (Breath)** is the primary continuous expression stream (some devices offer CC 11 or channel aftertouch as alternates — the CC map must allow aliasing CC2/CC11/ChanPressure onto the same *breath* dimension).
* Musical behavior differs from a keyboard: long legato lines, one voice, constant breath modulation.
* Mapping model (v1.0, DECISIONS_4 #63 — supersedes the v1 "wandering ink brush"): **one voice played exactly as MPE, plus a wake between notes.** Every note strikes a drop of the MPE radius; breath (CC 2 / 7 / 11 via the `INK_FLOW` route, or channel pressure) is the unbounded §4.4 feed like MPE press; channel pressure honours `press_mode`. On a legato note change the sounding drop is dragged to the new pitch's site as a rigid tip through the §4.3(4) wake (tip = the drop's current radius, profile and spread from `wake_profile` / `wake_spread`, sub-stepped ≤ a/4), then the old voice ends silently and the new note strikes — a phrase is a chain of drops threaded by their wakes, operators only. The expression layer of an IMU-equipped wind controller plays on the same voice (DECISIONS_4 #60): CC 74 → slide, polyphonic pressure → swirl, a bend on a member channel (the controller's MPE mode) → glide with the ±48 member range; a single-channel bend stays the global shear.

### 2.4 Classic single-channel MIDI keyboard
* Notes on one channel, global pitch bend (±2 default), CC 1 mod wheel, CC 64 sustain.
* Degrades gracefully: every note is its own voice with strike only; pitch bend and mod wheel act globally (bend → global shear tine, mod → vortex). **CC 64 never touches the canvas in any mode** (DECISIONS_4 #62 supersedes DECISIONS_3 #67's classic-only dip): the sustain pedal is the synth's, and a fresh sheet is only ever the host's deliberate action (`sumi_trigger_paper_dip`, the settings' Paper dip). In MPE mode a note on a NON-member channel — the master, or a plain keyboard on channel 1 — is itself a per-(channel, note) voice (DECISIONS_4 #60), so Classic remains for a keyboard sending inside the member zone (channels 2–16).

### 2.5 Dialect selection
**The input dialect is a setting, never a detection** (v1.0, DECISIONS_4 #60): every shell has an *Input* row — MPE (default) · Classic keyboard · Wind — persisted with the other settings and applied with `sumi_set_input_mode`. The v1 heuristic (`SUMI_INPUT_AUTO`: MCM or note-ons spread across ch 2+ with per-channel expression → MPE; notes on one channel + dense CC 2 → wind; otherwise classic) stays in the core for the ABI and its tests, and no shell selects it. Airwave CCs are orthogonal (pure CC streams on their own channel/numbers) and are routed by the CC map in every mode.

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
VoiceMigrate { voice_id, new_x, new_y }    // wind legato: wake the voice's drop to the new site (v1.0, DECISIONS_4 #63)
VoiceEnd     { voice_id, lift (0..1) }
GlobalCtl    { dimension, value (0..1) }   // vortex, viscosity, palette, roughness…
PaperDip     { }                            // ABI call only (sumi_trigger_paper_dip); no MIDI path since v1.0 (DECISIONS_4 #62)
```

### 3.4 Musical → spatial mapping (default profile)
* **Pitch → position** is a pluggable **layout** (`pitch_layout` param, switchable live; a layout is a pure function `(note, params, aspect) → 1–3 positions` plus an optional per-frame field motion). Most layouts return a single position; a layout may return up to `SUMI_MAX_ECHOES = 3` **echoes** — multiple canvas sites that are all the same note (see the echo-set rules below):
  * `0 — SUMI_LAYOUT_FIFTHS`: pitch class around a circle (circle-of-fifths angular layout), octave → radius (low notes outer, high notes inner). One position. The v1 default.
  * `1 — SUMI_LAYOUT_CHROMA_GRID`: chromatic reading-order grid. C1 at top-left, B7 at bottom-right: row = octave (1..7, top to bottom), column = pitch class (C..B, left to right), cells inset so drops at grid edges stay on canvas. Notes outside C1–B7 clamp to the nearest edge cell. One position.
  * `2 — SUMI_LAYOUT_JANKO`: Jankó keyboard grid — 6 staggered rows of whole-tone columns; column = note/2, row parity = note%2, with the classic half-column offset on alternate rows. **The note stamps on all three rows of its parity** (rows {0,2,4} or {1,3,5}) — the full 6-row lattice stays live, which is the entire point of visualizing Jankó. Three echoes.
  * `3 — SUMI_LAYOUT_ROLL_H`: horizontal piano roll. Pitch → y (low at bottom), all drops spawn on a fixed **now-line** at x = 0.12; the whole field translates +x continuously at the roll speed (see *field motion* below). Reads left-to-right like a DAW timeline flowing away from the playhead. One position.
  * `4 — SUMI_LAYOUT_ROLL_V`: vertical piano roll (Synthesia-style). Pitch → x (low at left), now-line at y = 0.12 from the top, field translates downward. One position.
  * `5 — SUMI_LAYOUT_PIANO_GRID`: classical piano grid — the chroma grid's frame (C1–B7, same insets, same out-of-range clamp to the edge octave keeping pitch class) with each octave as a two-row keyboard: 5 accidentals above at white-key units {1, 2, 4, 5, 6} (C♯/D♯ and F♯/G♯/A♯ between their naturals), 7 naturals below. 7 octaves × 2 = 14 rows, one position. **Playable geometry (DECISIONS_3 #29, #41, #57, #60, #61 — the final form):** an accidental's cell is **0.6 white-key units wide and 0.6 of an octave pair tall**, centred on its row; a natural's cell is a full key wide and owns the **bottom 0.6 of its octave pair**, sitting flush with the row's bottom edge. The strip above the naturals, off any accidental, is the **glissando corridor — a dead zone**: a stroke through it plays the black-key run (C♯ D♯ F♯ G♯ A♯) because a dead zone sustains (§8.7), while the natural band below plays the white-key run without grazing an accidental. Tapping in the corridor plays nothing, and a stroke must start on a key. **The circle you see IS the cell you touch and the joystick it generates**: R_max = half of the cell's smaller dimension (aspect-corrected), so where height governs — every landscape aspect — natural and accidental knobs are the same size, and the 0.6 width ratio holds at every aspect (footprints are similar rectangles). The semitone axis is the generic shortest-neighbour diagonal (C→C♯ half a key over and 0.9 of a row up) — pitch is *not* a function of x alone on this lattice, so pen legato is quantized here by design (§8.7).
  * `6 — SUMI_LAYOUT_ROLL_H_RIGHT` (v1.0, DECISIONS_4 #64): the horizontal roll mirrored — pitch → y, now-line at x = 0.88, the field translates −x. One position.
  * `7 — SUMI_LAYOUT_ROLL_V_BOTTOM` (v1.0, #64): the vertical roll mirrored — pitch → x, now-line at y = 0.88, the field translates upward. One position. The four rolls are named in every shell by their now-line's edge (left / top / right / bottom); the tablets label the three Play-mode lattices "(playable)" (#65).
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

**As shipped since libsumi 1.2.0 (Phase 8 step 55b, `DECISIONS_7 #1`):** each texel stores its DISPLACEMENT (u − x, v − y, ink, aux) — the pre-image minus the texel's own coordinate — not the pre-image (u, v); the initial state is all zeros ("identity coordinates" becomes "zero displacement"); every pass re-bases what it reads, d′(P) = d(P_src) + (P_src − P), so the map is the one §4.3 describes to rounding, and the half-float quantum follows the displacement (2^-24 canvas heights at rest, 2^-15 at a sixteenth of the canvas) instead of the coordinate (2^-11 over the outer half of the sheet). The ink and aux rules above are untouched — no pass reads the coordinates to decide where to sample. The §4.6 regression fixture was re-captured once, by construction; the print fixture did not move.

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

**The five operators of the electric medium** — wave torsion, Chladni (the keys as eddies), the viscous multipole burst, the spark shear and the composed spark, the Chirikov standard map — are §10.3, each declaring its class under §10.2's table and gate; every medium may use every operator.

### 4.4 Continuous feeds
Sustained pressure/breath is realized as **incremental drop expansions re-emitted per frame** at the voice's current center, using the boundary-growth conversion of §3.4 (emitted radius r = sqrt((R+ΔR)² − R²)). This keeps everything inside the same closed-form framework — no velocity field is ever introduced. Unbounded in every input mode since v1.0 (DECISIONS_4 #63): the v1 wind exception — a breath-fed brush relaxing toward a breath-proportional width — made breath-fed drops too weak and is retired; breath integrates exactly as MPE press does (the Osmose behavior), and the wind legato's wake (§2.3) carries the drop between notes.

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

The shipped `core/include/sumi_core.h`, reproduced verbatim — the header is the contract; when they differ, the header wins and this copy is stale. **The ABI's history since 1.0.0** (the one break, Phase 6 step 41; additive after): 1.1.0 the medium, the probe's state argument, the cell flags, the palette (§10.1, §11); 1.2.0 the field stored as a displacement (§4.2); 1.3.0 the scope view (§13.2); 1.4.0 the trumpet and the trombone, the fingering CCs (§14.2–§14.3); 1.5.0 the stateless layouts — Wicki–Hayden, the strings, the theremin (§14.4). **The v3.0 arc (additive; transcribed at the fold):** libsumi **1.6.0** (Phase 11, step 71 — SHIPPED, `DECISIONS_10 #3`; additive): `SUMI_MEDIUM_KESSHO` = 2; `sumi_ctl_t` 20–23 (`SUMI_CTL_POLARIZER_ANGLE`, `_CAUSTIC_DEPTH`, `_BRAID_COUPLE`, `_ARNOLD_K`; `SUMI_CTL_COUNT` 24 — the proposal's 14–17 were already the torsion's, the Chladni's, the spark's and the Chirikov's; 22 and 23 are declared now and inert until steps 73 and 72); `sumi_params_t` gains at its end `polarizer_mode` (0 global, 1 per voice), `braid_mode` (0 pinned, 1 free — inert until step 73), `kessho_retard` (nm per unit strain, 200..8000, default 2000), `kessho_backlight` (0..2, 0.85), `kessho_grain` (0..1, 0.5), `kessho_tint` (0..1, 0.25), `kessho_caustic` (0..2, 0.5); `slide_mode` 3 (the polarizer's angle), `press_mode` 3 and `bend_mode` 5 (reserved for steps 72 and 73, accepted); the export flag `SUMI_EXPORT_KESSHO_CAUSTIC` = 2u (a bit beside `SUMI_EXPORT_ANOD_ALPHA`); the twelve Kessho palettes through `sumi_palette_preset_count / _preset`; the preset schema stays 1 (additive keys with defaults). `sumi_add_arnold` is step 72's. libsumi **1.7.0** (step 71 amended / 71b, the minerals delta): `sumi_mineral_t` (sized), `sumi_set_mineral` / `sumi_get_mineral` / `sumi_mineral_preset`, `SUMI_OPTIC_*`, `SUMI_MINERAL_*` (six); `sumi_params_t` gains at its end `drop_habit`, `habit_roundness`, `habit_blades`, `tine_profile` (habit 0 and profile 0 bitwise the v2 drop and tine); the Kessho palette library is retired (`sumi_palette_preset_count(2)` = 0 — a mineral is the medium's "palette"). The operators `SUMI_DEFORM_ARNOLD`, `SUMI_DEFORM_BRAID` and `SUMI_DEFORM_CHORD_WAVE` (steps 72–74) fill what 71 declares, or bump again if they add a symbol. A later minor bump for `SUMI_LAYOUT_BOEHM` = 13 (step 91). `trace/`'s `sumi_trace_config_t` gains a mode (`SUMI_TRACE_POINCARE` = 1, step 75) — the host library, not libsumi. The field's payload does not change: the Metal invariant holds, and §10.1's switching gate (the field bitwise across the three media) is the arc's second invariant. libsumi **1.7.0 … 1.11.0** (steps 71b–76b) as the header's history records them (the minerals, the Arnol'd lock, the braid, the chord wave, the seed and the strain buffer, the leader stroke; additive). libsumi **1.12.0** (Phase 12, step 77 — SHIPPED, `DECISIONS_11 #4`; additive; THE LAST CORE-OPEN STEP, the core frozen from step 78 to step 92): `SUMI_LAYOUT_BOEHM` = 13, `SUMI_CC_KEY_THUMB` … `SUMI_CC_KEY_DSHARP` (114–119), `SUMI_LAYOUT_KEYS`, `sumi_layout_boehm_note`; `sumi_params_t` gains `split_mode`, `split_layout`, `split_note` at its end.

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
    SUMI_BACKEND_GL    = 3,   /* host-owned context; handle must be NULL      */
    SUMI_BACKEND_WEBGPU = 4   /* native_surface_handle = sumi_webgpu_surface_t* (below) */
} sumi_backend_t;

/* WebGPU host contract (Phase 5 §5, DECISIONS_4 #15). In a browser the
   adapter and device can only be created asynchronously, so the HOST (the JS
   page) creates them and hands the core the device — imported into the wasm
   as an emdawnwebgpu handle — plus the canvas to draw into and the canvas's
   preferred format (navigator.gpu.getPreferredCanvasFormat()). The core then
   owns the surface: it creates and configures it from the selector, acquires
   the frame texture, resizes it, and runs its readbacks (copy + async map)
   on that device. Pointed to by sumi_config_t.native_surface_handle for
   SUMI_BACKEND_WEBGPU; must outlive the instance. */
enum { SUMI_WEBGPU_FORMAT_BGRA8 = 0, SUMI_WEBGPU_FORMAT_RGBA8 = 1 };
typedef struct {
    const void* device;            /* WGPUDevice handle                       */
    const char* canvas_selector;   /* CSS selector of the <canvas>, e.g. "#sumi" */
    uint32_t    color_format;      /* SUMI_WEBGPU_FORMAT_*                    */
} sumi_webgpu_surface_t;

typedef enum {
    SUMI_INPUT_AUTO    = 0,
    SUMI_INPUT_MPE     = 1,
    SUMI_INPUT_CLASSIC = 2,
    SUMI_INPUT_WIND    = 3
} sumi_input_mode_t;

typedef enum {                 /* global control dimensions for CC routing */
    SUMI_CTL_VORTEX_STRENGTH = 0,
    SUMI_CTL_VORTEX_X        = 1,
    SUMI_CTL_VORTEX_Y        = 2,   /* v0.9: REVERSED at consumption — CC up
                                       moves the centre UP on screen (texture
                                       y is down; a raised hand should raise
                                       the stir, DECISIONS_4 #69)            */
    SUMI_CTL_VISCOSITY       = 3,
    SUMI_CTL_PAPER_ROUGHNESS = 4,
    SUMI_CTL_PALETTE_MORPH   = 5,
    SUMI_CTL_INK_FLOW        = 6,   /* breath aliases here in wind mode */
    SUMI_CTL_RIPPLE_AMP      = 7,   /* v0.4: sine ripple amplitude A          */
    SUMI_CTL_RIPPLE_FREQ     = 8,   /* v0.4: ripple wavenumber k              */
    /* v0.9 (DECISIONS_4 #69): the right hand's water — a Lamb-Oseen stir
       with its own centre (same reversed-Y convention), and two delta-driven
       pinches (0..1 value; each CHANGE emits +/-k toward the new value, like
       the CC 74 pinch — returning to rest nets out in exact math). */
    SUMI_CTL_SWIRL_STRENGTH  = 9,   /* Lamb-Oseen core rotation rate          */
    SUMI_CTL_SWIRL_X         = 10,
    SUMI_CTL_SWIRL_Y         = 11,  /* reversed like VORTEX_Y                 */
    SUMI_CTL_PINCH_SADDLE    = 12,  /* Hamiltonian saddle at the vortex centre */
    SUMI_CTL_PINCH_CROSS     = 13,  /* crossed tines at the swirl centre       */
    /* v0.10 (Phase 6 step 36, MEDIUM §2.1): the wave torsion's flavour
       controls, read by every vortex route whose profile is TORSION. */
    SUMI_CTL_TORSION_K       = 14,  /* wavenumber k: 0..1 -> 2π·4 .. 2π·40 per
                                       canvas height (rests at 0.5)           */
    SUMI_CTL_TORSION_PHASE   = 15,  /* φ: 0..1 -> 0..2π (rests at 0)           */
    /* v0.11 (Phase 6 step 37, MEDIUM §2.2): the Chladni cellular flow —
       A the stirring rate (0..1 of SUMI_CHLADNI_RATE, the cells' rotation in
       rad/s), B the balance between the two diagonal waves the flow splits
       into (0 = the full cellular flow, 0.5 = a single diagonal wave, 1 =
       the cells reversed). */
    SUMI_CTL_CHLADNI_A       = 16,
    SUMI_CTL_CHLADNI_B       = 17,
    /* v0.13 (Phase 6 step 39, MEDIUM §2.4): the spark shear's base
       wavenumber k: 0..1 -> 2π·2 .. 2π·24 per canvas height (the octaves
       stack above it) — a thick channel to fine streamers; rests at 0.5.
       Unmapped in the core; CC 74 lands here under slide_mode 2 (prepared
       for the Anod binding table, step 42). */
    SUMI_CTL_SPARK_K         = 18,
    /* v0.14 (Phase 6 step 40, MEDIUM §2.5): the Chirikov standard map's
       THROW — DELTA-driven: a change of δ in this control applies one step
       of the map with the kick and the drift both scaled by δ, so the
       step's chaos parameter is δ²·params.chirikov_kmax, capped per step
       by the core's erosion ceiling; a negative δ applies the exact
       inverse step. Unmapped in the core; the mod wheel / breath under the
       Anod binding table (step 42). */
    SUMI_CTL_CHIRIKOV_K      = 19,
    /* 1.6.0 (Phase 11 step 71, KESSHO §1; ROADMAP_6's ABI block): the Kessho
       medium's two and the harmony operators' two — declared together, the
       operators' filled by steps 72–73. */
    SUMI_CTL_POLARIZER_ANGLE = 20,  /* the analyzer's angle α: 0..1 -> 0..π/2 (the
                                       brush sin²(2(ψ − α)) has period π/2, so the
                                       range is one period); CC 74 lands here under
                                       slide_mode 3 (Kessho's default)              */
    SUMI_CTL_CAUSTIC_DEPTH   = 21,  /* the bed's depth z under Layer B: 0..1 ->
                                       z = 1.05..8, log-spaced (z = 1 is the flat
                                       plane of --kessho-flat); the mod-wheel
                                       dimension under Kessho's table              */
    SUMI_CTL_BRAID_COUPLE    = 22,  /* 1.9.0 (step 73): the gain 0..1 on the non-bass
                                       circulations Γ_k of the braid (1 unmapped) */
    SUMI_CTL_ARNOLD_K        = 23,  /* 1.8.0 (step 72): the Arnol'd kick as a fraction
                                       0..1 of K_max = 0.8 / max|W′| (the bijection's
                                       bound) — a CC alternative to the press under
                                       press_mode 3: the larger of the two drives  */
    SUMI_CTL_COUNT           = 24
} sumi_ctl_t;

typedef enum {                       /* v0.4 vortex profiles, spec §4.3(3) */
    SUMI_VORTEX_EXPONENTIAL = 0,     /* Jaffer: diffuse, breath-like       */
    SUMI_VORTEX_RANKINE     = 1,     /* rigid core, crease ring at R       */
    SUMI_VORTEX_LAMB_OSEEN  = 2,     /* v0.6: the §4.3(7) swirl as a GESTURE —
                                        strength = Γ·Δt (signed), radius = r_c.
                                        The Marble-mode "pull back to stir"
                                        (DECISIONS_4 #30/#49); CC routing keeps
                                        profiles 0/1.                        */
    SUMI_VORTEX_TORSION     = 3      /* v0.10 (Phase 6, MEDIUM §2.1): WAVE
                                        TORSION — θ' = θ + A·sin(k·r − φ)·e^(−r/R),
                                        r' = r. A rotation by θ(r) like its
                                        siblings: EXACT at any amplitude
                                        (det J = 1; the ±A pair inverts
                                        analytically). strength = A (radians),
                                        radius = R (the e-fold decay length);
                                        k and φ come from SUMI_CTL_TORSION_K /
                                        _PHASE. Reachable through every vortex
                                        route: gesture, CC-routed, and the
                                        note-on sweep (torsion_sweep).        */
} sumi_vortex_profile_t;

typedef enum {                       /* v0.6: sumi_add_drop layer types      */
    SUMI_DROP_INK   = 0,             /* new ink band (counter-derived phase) */
    SUMI_DROP_CLEAR = 1,             /* clear water / surfactant: expands the
                                        field, interior un-inked            */
    SUMI_DROP_FEED  = 2,             /* GROW the ink already under the centre:
                                        the §3.4/§4.4 boundary growth as a
                                        gesture — the interior takes the
                                        centre texel's band, so a held press
                                        widens a band instead of laying rings.
                                        Radius = sqrt((R+ΔR)² − R²), the host
                                        tracks R (DECISIONS_4 #49).          */
    SUMI_DROP_NONE  = 3              /* v0.13: NO drop — sumi_add_drop returns,
                                        sumi_add_spark fires the burst and the
                                        shear episodes without the blast (the
                                        conservation gate's stream: a drop's
                                        exact expansion moves ink off the
                                        canvas, which no mass observable can
                                        tell from loss — DECISIONS_5 #39).   */
} sumi_drop_layer_t;

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
    SUMI_LAYOUT_PIANO_GRID  = 5, /* classical two-row piano grid, C1..B7      */
    SUMI_LAYOUT_ROLL_H_RIGHT = 6,/* v0.8: horizontal roll, now-line at the RIGHT,
                                    the sheet drifts left (DECISIONS_4 #64)   */
    SUMI_LAYOUT_ROLL_V_BOTTOM = 7,/* v0.8: vertical roll, now-line at the BOTTOM,
                                    the sheet rises                           */
    /* 1.0.0 (Phase 6 step 41): NAMED AND RESERVED for the instrument layouts
       (INSTRUMENT_SPEC §2–§4). sumi_set_params clamps the ones still reserved
       to FIFTHS with a warning until each ships; the probe refuses them.
       1.4.0 (Phase 9 step 60): the trumpet and the trombone ship — the
       STATEFUL layouts, reading sumi_layout_state_t (the valves in `buttons`
       bits 0..2, the slide in `slider`); the state travels as MIDI (the
       fingering CCs below) and the engine keeps its own decoded copy
       (sumi_get_layout_state). Their cells are PARTIALS: the trumpet's eight
       (the harmonic series a B♭ trumpet speaks, as sounding notes: B♭1 46,
       B♭2 58, F3 65, B♭3 70, D4 74, F4 77, A♭4 80, B♭4 82), the trombone's
       seven (the 2nd through the 8th); a cell sounds its partial minus the
       valves' offset (1 = −2, 2 = −1, 3 = −3, combinations summing) or the
       slide's 0..6 semitones — the idealised instrument. */
    SUMI_LAYOUT_TRUMPET     = 8,  /* three valves + the harmonic series (stateful; 1.4.0) */
    SUMI_LAYOUT_TROMBONE    = 9,  /* the slide (stateful, continuous; 1.4.0)              */
    /* 1.5.0 (Phase 9 step 61): the stateless additions ship. WICKI: the
       Wicki–Hayden button-field — a hex grid, a whole tone to the right, a
       fifth up-right, a fourth up-left, the octave two rows straight up;
       six buttons a row, fifteen rows, every note on exactly one button
       (one echo); pitch is a plane over the sheet, so the probe's axis is
       its gradient. STRINGS: string-rows × chromatic frets (the open string
       and two octaves), the lowest string at the bottom, the tuning a
       FIXED preset (params.string_tuning, SUMI_STRINGS_*); a note sits on
       every string that reaches it (its sites are echoes); the axis is
       along the string, one fret a semitone — dragging along a string is
       a string bend. THEREMIN: no cells — the cell's CONTINUOUS flag; X is
       pitch across five octaves (C2 at the left to C7), the probe's note
       the nearest semitone and its centre that semitone's x, Y the bipolar
       press axis about the middle (cell_radius = the half height). */
    SUMI_LAYOUT_WICKI       = 10, /* Wicki–Hayden hexagonal isomorphic grid (1.5.0) */
    SUMI_LAYOUT_STRINGS     = 11, /* strings × frets under a tuning preset (1.5.0) */
    SUMI_LAYOUT_THEREMIN    = 12, /* continuous pitch: the cell's CONTINUOUS flag (1.5.0) */
    /* 1.12.0 (Phase 12 step 77, TO_PROJECT_SPEC §14.7; DECISIONS_11 #4): THE BOEHM
       FLUTE — the woodwind key system, STATEFUL through `buttons` bits 0..8: the
       left hand's three fingers on the valves' CCs 110/111/112 (bits 0..2: L1,
       L2, L3), then CC 114..119 (bits 3..8: the thumb, the G♯ lever, R1, R2,
       R3, the D♯ key). Its cells are three REGISTERS on the brass geometry (the
       column or the arc): the fundamental, the octave, the second octave; a
       cell sounds the fingering's chart note plus 0 / 12 / 24. The chart is the
       idealised Boehm flute's vent rule — the highest open hole sounds, the
       holes top to bottom C♯ (L1), B (the thumb), B♭ (closed by L2 OR R1: the
       instrument's two linkages, the "1 and 1" B♭), A (L2), G (L3), G♯ (a
       closed-standing key, open when pressed), F (R1), E (R2), D (R3), D♯
       (closed-standing, open when pressed), all closed D4 — with the one cross
       fingering the chart keeps (R3 alone below an open F hole flattens G to
       F♯): a total function of the 512 states onto D4..C♯5, sumi_layout_boehm_note.
       No intonation, as the brass's: the idealised instrument. */
    SUMI_LAYOUT_BOEHM       = 13  /* the Boehm flute: nine keys + three registers (stateful; 1.12.0) */
} sumi_layout_t;
#define SUMI_LAYOUT_FRETS SUMI_LAYOUT_STRINGS   /* the 1.0.0 name of 11 ("a fretboard"), kept */
/* 1.5.0: the STRINGS layout's tuning presets (params.string_tuning) — fixed
   tables, not the deferred user-editable one. */
#define SUMI_STRINGS_STANDARD_GUITAR 0u   /* six strings, E2 A2 D3 G3 B3 E4                       */
#define SUMI_STRINGS_WHOLE_TONE_TAP  1u   /* twelve strings a whole tone apart, E2 … D4: the tapping grid, an isomorphism */
#define SUMI_STRINGS_ALL_FOURTHS     2u   /* six strings in fourths, E2 A2 D3 G3 C4 F4               */

/* 1.0.0 (Phase 6 step 41, MEDIUM §1): the MEDIUM — what the engine's field
   is READ as. The engine owns the field, the operators, the normalizer, the
   layouts and the budgets; a medium owns the composite, its palette family,
   its print styling and its default binding table. */
typedef enum {
    SUMI_MEDIUM_SUMI = 0,   /* suminagashi: ink on washi — the renderer of 0.x   */
    SUMI_MEDIUM_ANOD = 1,   /* the electric medium (DECISIONS_5 #1): inert until
                               its composite lands (step 43) — renders as SUMI  */
    SUMI_MEDIUM_KESSHO = 2  /* 1.6.0 (Phase 11 step 71, KESSHO §1): the crystal —
                               the same field read as a birefringent slab between
                               crossed polarizers (λ, ψ: the Michel-Lévy fringes and
                               the isoclinic brush) with its iso-trace caustic (tr J:
                               elliptic / hyperbolic tints, the fold of a bed at
                               depth z). Its own binding table and palettes.      */
} sumi_medium_t;

typedef struct {
    float    fluid_viscosity;    /* damping for continuous agitation            */
    float    expansion_rate;     /* pressure/breath-driven drop feed scale      */
    float    paper_roughness;    /* washi fiber composite strength              */
    float    smoothing_ms;       /* expressive-dimension smoothing time const   */
    uint32_t active_palette_id;  /* 0 sumi black, 1 indigo, 2 ochre, 3 = the
                                    custom palette of sumi_set_palette
                                    (SUMI_PALETTE_CUSTOM, 1.0.0)               */
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
                                    1 Hamiltonian pinch (delta-driven), 2 the
                                    spark shear's wavenumber (the latest
                                    voice's slide sets SUMI_CTL_SPARK_K);
                                    1.6.0: 3 = the polarizer angle (the latest
                                    voice's slide sets SUMI_CTL_POLARIZER_ANGLE
                                    — or, with polarizer_mode 1, each voice
                                    owns its own α_k); SUMI_MODE_MEDIUM_DEFAULT
                                    = the medium's table (1.1.0, MEDIUM §4:
                                    Sumi 0, Anod 2; Kessho 3)                 */
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
                                    holds position; 1.1.0: 2 = the bend plays
                                    the torsion's wavenumber (SUMI_CTL_
                                    TORSION_K, ±1.5 semitones = the range),
                                    3 = the spark's (SUMI_CTL_SPARK_K), 4 =
                                    the Chladni stir (SUMI_CTL_CHLADNI_A: the
                                    bend's distance the rate, its sign the
                                    sense — a vibrato stirs back and forth);
                                    1.9.0 (step 73): 5 = the braid's
                                    consonance — the drop holds, the bend
                                    moves the note's interval against the
                                    bass and so its circulation (KESSHO §2.4);
                                    SUMI_MODE_MEDIUM_DEFAULT = the medium's
                                    table (Sumi 0, Anod 4, Kessho 5). Exactly ONE
                                    consumer owns the note bend; switchable
                                    live. Master bend keeps its shear tine. */
    uint32_t press_mode;         /* 0xD0 channel-pressure routing (§3.4 v0.4):
                                    0 = ink feed (v1 grow), 1 = the Lamb-Oseen
                                    swirl — hardware's door to the swirl
                                    voice; 1.1.0: 2 = the torsion sweep FEED
                                    (pressure spends torsion deltas around
                                    the note's drop, the sweep's emitter fed
                                    live); 1.8.0 (step 72): 3 = the Arnol'd
                                    tongue lock — the press is the kick K, the
                                    pitch against the lowest held note sets Ω₀
                                    and q (the bass itself keeps the swirl);
                                    SUMI_MODE_MEDIUM_DEFAULT = the medium's
                                    table (Sumi 0, Anod 2, Kessho 3). 0xA0 poly
                                    pressure -> the
                                    swirl dimension in every mode (the medium
                                    decides its consumer; Kessho's is the
                                    swirl until step 74's chord wave).       */
    /* v0.7 (DECISIONS_4 #53) */
    uint32_t wake_profile;       /* sumi_add_wake's fluid: 0 = the inviscid
                                    potential doublet with a rigid tip (v0.4,
                                    exact, zero seam); 1 = the VISCOUS stroke —
                                    the 2-D unsteady Stokeslet displacement of
                                    an impulse spread over the tip radius,
                                    sub-stepped at <= a/4 like the doublet.    */
    float    wake_spread;        /* viscous profile only: l/a, the momentum's
                                    diffusion length after the impulse over the
                                    tip radius (l^2 = a^2 + 4 nu t). Clamped
                                    [1.5, 12]; default 3. Small = sharp, close
                                    to the tip; large = soft and far-reaching. */
    /* v0.10 (Phase 6 step 36, MEDIUM §2.1) */
    uint32_t torsion_sweep;      /* 0 = off (default). 1 = every note-on also
                                    fires the wave torsion's OUTWARD PHASE
                                    SWEEP at the voice's drop: per-frame
                                    torsion DELTAS (never absolutes) with φ
                                    advancing and a decaying amplitude, k
                                    from the TORSION_K control — the engine's
                                    first time-driven "episode". A stand-in
                                    until the medium's binding tables own the
                                    strike (step 42).                        */
    /* v0.11 (Phase 6 step 37, MEDIUM §2.2): the Chladni cellular flow —
       ψ = Ψ·cos(k_x(x−x0))·cos(k_y(y−y0)), the Taylor–Green vortex, an exact
       Navier–Stokes solution, on THE LAYOUT'S cell lattice: an eddy in every
       cell (adjacent cells counter-rotate), the cell boundaries its
       separatrices. Steadily driven it spins each note's drop in place and
       stretches the ink along the boundaries into the figure that outlines
       the grid. Bake only, into the field; nothing is live. */
    float    chladni_cell;       /* cell size, 0.5..1.5 (dflt 1): the eddy's disc
                                    as a fraction of the layout's display cell —
                                    the circle the shells draw for the key. 1
                                    fills the key, 0.5 leaves a ring of resting
                                    water round each eddy. Above 1 the discs
                                    would overlap: SUMI_CHLADNI_DISCS caps the
                                    size at 1 (exact), SUMI_CHLADNI_FIELD lets
                                    them grow to 1.5 and stir the water between
                                    the keys. Layouts without drawn cells (the
                                    fifths, the rolls) take the largest circle
                                    at each note that touches no neighbour's. */
    /* v0.12 (Phase 6 step 38, MEDIUM §2.3): the viscous multipole burst's
       AGE ENVELOPE (sumi_add_burst). The strike fires sharp at ℓ = a and the
       release grows the diffusion age ℓ (ℓ² = a² + 4νt): the lobes soften
       and reach further as the discharge dies. */
    float    burst_age;          /* the final age ℓ_end/a, 1.5..12 (dflt 4):
                                    1.5 leaves the lobes sharp and close to
                                    the core, 12 soft and far; D is the
                                    displacement over the burst's own age,
                                    so the age shapes the burst, it does not
                                    scale it.                                */
    float    burst_life;         /* seconds for the age to grow from a to
                                    burst_age·a, 0..4 (dflt 0.8); 0 = the whole
                                    burst at once, in the next update. ℓ²
                                    grows linearly in time, so most of the
                                    displacement lands early: the discharge
                                    blooms sharp and dies soft.              */
    uint32_t burst_order;        /* the multipole order m a strike takes when
                                    the gesture passes m = 0, 2..8 (dflt 2):
                                    2 the quadrupole — two lobes eject along
                                    the axis, two draw in across it — and m
                                    lobes eject for order m. 8 is the top the
                                    pass budget table covers (DECISIONS_5
                                    #29). The pitch-class → m table of the
                                    binding tables (step 42) will override
                                    it per note.                             */
    /* v0.13 (Phase 6 step 39, MEDIUM §2.4): the spark shear — the jagged
       streamers of the composed strike (sumi_add_spark). */
    uint32_t spark_stack;        /* octaves in the profile, 1..4 (dflt 3: k,
                                    2k, 4k with weights 1, ½, ¼) — the stack
                                    depth the spec asks to keep out of a magic
                                    constant.                                */
    uint32_t spark_profile;      /* 0 triangle waves (dflt), 1 piecewise-linear
                                    hash noise: either is exact (a shear
                                    inverts for any profile).               */
    float    spark_shear;        /* the episode's total kick A = B as a
                                    multiple of the strike radius, 0..2 (dflt
                                    0.6); 0 = no shear in the composition.    */
    float    spark_tau;          /* the decay time constant, seconds, 0.05..2
                                    (dflt 0.25): A, B ∝ e^(−t/τ); the episode
                                    is over after 4τ.                        */
    /* v0.14 (Phase 6 step 40, MEDIUM §2.5): the Chirikov standard map,
       y1 = y + A·sin(k(x−xc)+φ), x1 = x + ε·(y1−yc), K = A·k·ε. */
    float    chirikov_kmax;      /* K of a FULL throw of the CHIRIKOV_K control
                                    in one frame, 0..2 (dflt 1): Greene's
                                    threshold 0.9716 is where sheets give way
                                    to chaos; 0 disables the route. A throw
                                    eased over m frames is m steps at K/m²
                                    (the pendulum flow), a wheel THROWN is one
                                    hard kick — the depth into chaos is the
                                    wheel's speed. The per-step K is capped
                                    by the core's erosion ceiling.          */
    uint32_t chirikov_periods;   /* the kick's waves per canvas height along
                                    x, 1..8 (dflt 2): k = 2π·periods.         */
    float    chirikov_eps;       /* the drift's scale ε, 0.05..1 (dflt 0.5):
                                    the kick amplitude follows as A =
                                    K/(k·ε) — small ε means steep kicks,
                                    large ε a canvas-scale drift.            */
    /* 1.0.0 (Phase 6 step 41) — THE ONE ABI BREAK of the 2.0 arc. */
    uint32_t medium;             /* sumi_medium_t (dflt SUMI_MEDIUM_SUMI). Live-
                                    switchable: the field is medium-agnostic
                                    (coordinates, phase, aux), so switching
                                    re-reads the same deformation history.
                                    Values above KESSHO clamp to SUMI (1.6.0;
                                    above ANOD before it). 1.1.0
                                    (step 42): the medium also owns the
                                    DEFAULT BINDING TABLE (MEDIUM §4) — what
                                    each MIDI dimension drives when a mode is
                                    SUMI_MODE_MEDIUM_DEFAULT, and, with no
                                    override at all, the strike (Sumi: the
                                    drop; Anod: the spark composition), the
                                    poly-pressure dimension (Lamb–Oseen
                                    swirl / the torsion's and spark's
                                    wavenumbers) and the mod-wheel
                                    dimension (vortex / Chirikov throw). The
                                    CC map and explicit modes override.     */
    /* 1.1.0 (Phase 6 step 42, MEDIUM §3–§4): the Anod medium's knobs. */
    float    anod_glow;          /* the strain-glow scale, 0.2..5 (dflt 0.2): the
                                    glow is 1 − exp(−σ/anod_glow) with σ =
                                    |λ − 1/λ| the accumulated strain read
                                    from the field (‖J‖_F² − 2); smaller = a
                                    hotter, sooner glow.                     */
    uint32_t burst_order_by_class[12]; /* the Anod strike's multipole order per
                                    pitch class C..B, 2..8 (0 = burst_order).
                                    Step 43 (#71) took the burst out of the
                                    Anod strike; #88 brought it back as the
                                    classic spark's, whose order is
                                    burst_order — this table stays in the
                                    ABI, serialized and clamped, unused.    */
    float    anod_pitch;         /* the water grid's pitch at rest as a
                                    fraction of the canvas height, 1/256..1/8
                                    or 0 for no grid (dflt 1/144): Anod water
                                    draws the deformed grid — iso-lines of
                                    position + gain·displacement, one family
                                    per axis — where the displacement exceeds
                                    a texel; a larger pitch is fewer lines.   */
    /* 1.1.0 (Phase 6 step 43, MEDIUM §2.2): how the Chladni stir turns the
       layout's cells — an eddy in every display cell, the ring of each disc
       turning while its core and the water between the discs rest. */
    uint32_t chladni_mode;       /* SUMI_CHLADNI_DISCS (0, dflt): an exact
                                    rotation inside every disc, the discs kept
                                    disjoint (chladni_cell capped at 1).
                                    SUMI_CHLADNI_FIELD (1), the author's
                                    "inverse Chladni": the discs' rings summed
                                    into ONE divergence-free displacement field,
                                    so the discs may grow past their keys
                                    (chladni_cell to 1.5) and overlap, the
                                    water between the keys stirred by both
                                    neighbours; area-preserving to first order
                                    (the burst's class, sub-stepped).         */
    /* 1.1.0 (Phase 6 step 43, QOL §2): THE SUBSTRATE — composite-side by
       construction, so the screen-locked invariant (§4.5) holds: every knob
       below samples in screen space, never through the field. */
    float    paper_tint[3];      /* Sumi: the washi's base tone, LINEAR RGB (dflt
                                    0.900/0.868/0.790, the cream of 0.x); the
                                    mottle, grain and fibers modulate it as
                                    before.                                    */
    float    fiber_scale;        /* Sumi: the fiber strands' spatial frequency
                                    as a multiple of 0.x's, 0.5..2 (dflt 1):
                                    below 1 longer, coarser strands, above 1
                                    finer. The strands' angle drift (±20°) is
                                    the washi's own and is not exposed.       */
    float    anod_dark;          /* Anod: the glass's darkness, 0..1 (dflt 1 =
                                    black; 0.5 the step-42 glass 0.010/0.010/
                                    0.014, 0 twice as bright).                */
    float    anod_grain;         /* Anod: the phosphor speckle's strength, 0..1
                                    (dflt 0.5). Until step 43 it followed
                                    paper_roughness and its CC; now its own. */
    /* 1.1.0 (Phase 6 step 43, the author's glow): THE BLOOM — a screen-space
       glow after the Anod composite. The emission above a small threshold
       is blurred over anod_bloom_levels octaves and added back, then a soft
       shoulder takes the brightest sum toward white: a wide cold halo, a
       neon mid, a white-hot core. Screen-space by construction (§4.5). */
    float    anod_bloom;         /* the bloom's strength, 0..3 (dflt 0.75; 0 = off:
                                    the composite as it stands).               */
    uint32_t anod_bloom_levels;  /* the halo's reach, 1..5 octaves of a half-
                                    height blur (dflt 3: a halo of about a
                                    sixteenth of the canvas height).           */
    /* 1.1.0 (Phase 6 step 43, the author's table; #88; back at Phase 8 step
       55b, DECISIONS_7 #5): THE ANOD STRIKE'S CHARGE. In Anod a strike is a
       charge of this fraction of the Sumi drop's radius and the spark shear
       episode with the SUMI radius as its band and kick base, so the charge
       is torn into long streamers (#71). #88 had made it the classic spark
       on a 0.57 charge for a renderer that lost the threads under the
       coordinate payload; the displacement payload carries them. 1 floods. */
    float    anod_drop;          /* 0.1..1 (dflt 0.33)                        */
    /* 1.4.0 (Phase 9 step 60, INSTRUMENT §2): the brass partial cells — the
       trumpet's eight and, since the author's ask at step 63, the
       trombone's seven (the name is the ABI's) — as a COLUMN (0, dflt: the
       lowest partial at the bottom, a cell a tenth of the canvas height
       tall; the trombone's a seventh of 0.8) or on an ARC (1: the lowest at the
       left, rising over the top to the right — a ring of radius 0.30 canvas
       heights, its centre 0.08 right of the middle at 0.65 of the height,
       cells of radius 0.055). The author chose by eye on the device (step
       63, twice). */
    uint32_t trumpet_arc;
    /* 1.5.0 (Phase 9 step 61, INSTRUMENT §4): the STRINGS layout's tuning —
       SUMI_STRINGS_STANDARD_GUITAR (0, dflt), SUMI_STRINGS_WHOLE_TONE_TAP (1),
       SUMI_STRINGS_ALL_FOURTHS (2). A fixed table each; the user-editable
       table stays deferred (INSTRUMENT §6). */
    uint32_t string_tuning;
    /* 1.6.0 (Phase 11 step 71, KESSHO §1; DECISIONS_10 #3): THE KESSHO MEDIUM'S
       KNOBS and the two A/B modes of the phase. Composite-side, like Anod's:
       the field is untouched, the invariant holds by construction. */
    uint32_t polarizer_mode;     /* SUMI_POLARIZER_GLOBAL (0, dflt): one analyzer
                                    over the whole sheet, its angle the
                                    POLARIZER_ANGLE control (a CC, or the latest
                                    voice's slide under slide_mode 3);
                                    SUMI_POLARIZER_PER_VOICE (1): each held
                                    voice's slide is its own α_k and the sheet
                                    blends the nearest voices' (the A/B item
                                    K-1, decided by viewing at step 71).      */
    uint32_t braid_mode;         /* SUMI_BRAID_PINNED (0, dflt) / SUMI_BRAID_FREE
                                    (1): the braid's vortex kinematics (step 73,
                                    the A/B item B-1): pinned, every voice's
                                    vortex at its cell; free, the Kirchhoff
                                    point vortices integrated on the render
                                    thread — consonant dyads orbit, dissonant
                                    ones travel as a dipole (1.9.0).           */
    float    kessho_retard;      /* THE EXPOSURE, nm at σ = 1 for the reference
                                    mineral, 200..8000 (dflt 2000): Layer A's
                                    colour is the Michel-Lévy chart at Γ_eff =
                                    (kessho_retard / 2000)·mineral.retardation·σ
                                    + mineral.retardation_bias, σ = λ − 1/λ the
                                    strain Anod glows by (1.7.0: the mineral
                                    carries the physical Δn, this knob is one
                                    stop up or down for every mineral; quartz at
                                    2000 is step 71's first capture — the
                                    first-order red at σ ≈ 0.27).              */
    float    kessho_backlight;   /* the lamp behind the crystal, 0..2 (dflt 1):
                                    Layer A multiplies over it through the
                                    mineral's body tint (1.7.0).              */
    float    kessho_grain;       /* the frosted calcite's facet speckle, 0..1
                                    (dflt 0.5), screen-locked like the washi's. */
    float    kessho_tint;        /* Layer B's regime tint, 0..1 (dflt 0.35): the
                                    elliptic field cool, the hyperbolic warm,
                                    by the trace discriminant (tr J)² − 4.     */
    float    kessho_caustic;     /* Layer B's caustic gain, 0..2 (dflt 0.6): the
                                    fold of the bed at depth z, 1 / (|1 + z(1−z)
                                    (tr J − 2)| + ε), added over the tint.     */
    /* 1.7.0 (Phase 11 step 71b, KESSHO §4.2–§4.4 — the minerals delta; DECISIONS_10
       #6): THE MECHANICAL HALF OF A MINERAL — how the crystal admits a strike and
       a stroke. Read at strike / stroke time: a held drop keeps the habit it was
       struck with. drop_habit 0 and tine_profile 0 are BITWISE the v2 drop and
       the v2 tine (the Metal fixture holds). */
    uint32_t drop_habit;         /* SUMI_HABIT_CIRCLE (0, dflt), _RHOMB (the calcite
                                    cleavage rhomb, 78.1°/101.9°), _HEX (the mica
                                    sheet), _ROSETTE (m tabular blades): the drop's
                                    profile R(θ) — the exact crystal drop, ρ_src =
                                    ρ − R(θ)²/2 in the drop's action-angle frame,
                                    det J = 1 for any R(θ) (KESSHO §4.3). The
                                    interior rings are homothetic habits; boundary
                                    growth keeps the habit.                     */
    float    habit_roundness;    /* 0 sharp (the corners crease — a cleavage line in
                                    Kessho) .. 1 a circle (dflt 0.3): a soft-min on
                                    the support form, then a blend to the circle. */
    uint32_t habit_blades;       /* the rosette's blade count m, 2..12 (dflt 5).    */
    uint32_t tine_profile;       /* SUMI_TINE_SMOOTH (0, dflt: Jaffer's α/(α + d),
                                    bitwise v2), _TWIN_KINK (Baumhauer: a piecewise-
                                    linear bump — the shear in two kink lines),
                                    _TERRACE (phyllosilicate slip: a descending
                                    staircase of flat terraces). Every profile a
                                    BUMP, zero beyond 4α (a monotone ramp pulls texels
                                    in from off-canvas and fabricates, KESSHO §4.4).
                                    The glide tine and the master bend's shear tine
                                    inherit it; the pinch's crossed tines do not. */
    /* 1.11.0 (Phase 12 step 76, TO_PROJECT_SPEC §10.10): THE SPARK READING of the
       Anod medium. Anod reads the strain σ as a DENSITY — every stretched texel
       glows in proportion, soft edges, bloom: a plasma. The spark is another
       reading of the same field, composite-side (the field bitwise): the
       RIDGES of σ across the strain axis (non-maximum suppression along ê_ψ⊥,
       ψ the principal axis the Kessho stencil already computes — one function,
       two readers), the finite-time stretching ridges of the performance; a
       steep transfer so a few percent of the canvas is lit; the LEADER (the
       strain added now, bright, white-blue) and the AFTERGLOW (the strain
       buffer's recent activity decaying, dimmer, warmer); the bloom on the
       leader only; a flicker from the screen-locked grain's value at the texel
       used in time. Ridges light on charge only, as the plasma does; the water
       keeps its grid. The branches are GEOMETRY — the leader stroke of step
       76b (a strike's own strain is a plateau the reader cannot thin;
       DECISIONS_11 #2). */
    uint32_t anod_glow_mode;     /* SUMI_GLOW_PLASMA (0, dflt: bitwise 1.10.0) or
                                    SUMI_GLOW_SPARK (1).                           */
    float    spark_afterglow;    /* seconds, 0.05..5 (dflt 0.6): the strain buffer's
                                    time constant — the afterglow's decay, and the
                                    exposure Kessho's Layer A reads (§10.10).       */
    /* 1.12.0 (Phase 12 step 77, SPEC §14.6 carried → TO_PROJECT_SPEC §14.7): THE
       SPLIT SURFACE — two layouts side by side, a hand each. Under split_mode 1
       the sheet's left half is `pitch_layout` drawn at half width and its right
       half `split_layout`; `split_note` divides the notes between them (the
       left plays the notes below it, the right the notes from it), so a wire
       note has one home and a touch's note agrees with where the engine draws
       it. Every layout query follows the split — the probe by the half under
       the touch (a cell whose note belongs to the other half is refused), the
       placement and the pitch axis by the note, the cells as both halves' (not
       filtered: the shells grey the other half's). Both layouts must be Play
       layouts (1, 2, 5, 8..13) or the split is off; the rolls scroll the whole
       sheet and cannot share it. Defaults: off, the chromatic grid, C4 (60). */
    uint32_t split_mode;         /* 0 off (dflt), 1 the two halves                 */
    uint32_t split_layout;       /* the right half's sumi_layout_t (dflt 1)        */
    uint32_t split_note;         /* the first note of the right half, 0..127 (60) */
} sumi_params_t;
#define SUMI_GLOW_PLASMA         0u   /* 1.11.0: params.anod_glow_mode (step 76) */
#define SUMI_GLOW_SPARK          1u
#define SUMI_POLARIZER_GLOBAL    0u   /* 1.6.0: params.polarizer_mode */
#define SUMI_POLARIZER_PER_VOICE 1u
#define SUMI_BRAID_PINNED        0u   /* 1.6.0: params.braid_mode (step 73) */
#define SUMI_BRAID_FREE          1u
#define SUMI_HABIT_CIRCLE        0u   /* 1.7.0: params.drop_habit */
#define SUMI_HABIT_RHOMB         1u
#define SUMI_HABIT_HEX           2u
#define SUMI_HABIT_ROSETTE       3u
#define SUMI_TINE_SMOOTH         0u   /* 1.7.0: params.tine_profile */
#define SUMI_TINE_TWIN_KINK      1u
#define SUMI_TINE_TERRACE        2u

/* 1.7.0 (Phase 11 step 71b, KESSHO §4 — the minerals delta; DECISIONS_10 #6): THE
   MINERAL — the OPTICAL half, a pure composite change: how light reads the
   field under Kessho. A SIZED struct: `size` = sizeof as the host knows it —
   additive by size, the editor edits it, presets fill it; sumi_set_mineral
   takes the fields it knows and defaults the rest. Switching minerals leaves
   the field bitwise (the §10.1 gate extends to every mineral). The Michel-Lévy
   table stays the one physical chart; a mineral sets its retardation scale,
   its bias, its optic class and the optional laws (KESSHO §4.5). */
#define SUMI_OPTIC_ISOTROPIC 0u   /* Γ = 0: Layer A dark, the medium shows Layer B alone (fluorite)        */
#define SUMI_OPTIC_UNIAXIAL  1u   /* the brush sin²(2(ψ − α)) — quartz, calcite, selenite                  */
#define SUMI_OPTIC_BIAXIAL   2u   /* the two-axis product |sin(ψ − α + V)·sin(ψ − α − V)| (a stylisation) */
typedef struct {
    uint32_t size;              /* = sizeof(sumi_mineral_t); the versioning                                   */
    uint32_t optic_class;       /* SUMI_OPTIC_*                                                               */
    float    retardation;       /* Γ: nm of retardation at σ = 1 (Δn × exposure) [ITERATE] per preset; the
                                   exposure knob params.kessho_retard / 2000 multiplies it                  */
    float    retardation_bias;  /* Γ₀: the sensitive-tint plate, nm (0 none; 550 selenite): the sign of a
                                   shear strain shifts the first-order magenta toward blue or yellow         */
    float    two_v;             /* biaxial: the half-angle V, rad (ignored unless _BIAXIAL)                   */
    float    walkoff;           /* δ: the double image's split in texels at σ = 1 (0 none) — calcite         */
    float    dispersion;        /* Cauchy: (z_B / z_R) − 1 (0 none): three caustic depths, a rainbow fold    */
    float    film_n, film_r;    /* Airy thin film over the ink channel: the index, the Fresnel amplitude
                                   (r = 0 none) — muscovite's pearlescence                                   */
    float    conical_ring;      /* the ring kernel's radius in texels on the fold (0 none) — barite          */
    float    body_tint[3];      /* linear RGB: the backlight through the body (the substrate's colour)        */
    float    bloom_gain;        /* Layer B only — the fringes never bloom                                     */
    uint32_t lut_id;            /* 0 the physical Michel-Lévy table; ≥ 1 the custom palette read as a chart  */
} sumi_mineral_t;
#define SUMI_MINERAL_QUARTZ    0u
#define SUMI_MINERAL_CALCITE   1u
#define SUMI_MINERAL_MUSCOVITE 2u
#define SUMI_MINERAL_BARITE    3u
#define SUMI_MINERAL_FLUORITE  4u
#define SUMI_MINERAL_SELENITE  5u
#define SUMI_MINERAL_COUNT     6u
SUMI_API void             sumi_set_mineral  (sumi_instance_t* inst, const sumi_mineral_t* m);   /* validated, stored */
SUMI_API void             sumi_get_mineral  (sumi_instance_t* inst, sumi_mineral_t* out);       /* as stored          */
/* The preset library: fills size and every field; `name` a static string (may be NULL); false past the end.
   Pure, instance-free, callable from any thread. Quartz (0) is the medium as first captured at step 71. */
SUMI_API bool             sumi_mineral_preset(uint32_t id, sumi_mineral_t* out, const char** name);
#define SUMI_CHLADNI_DISCS 0u
#define SUMI_CHLADNI_FIELD 1u
/* 1.1.0: a mode set to this value takes the MEDIUM's default (MEDIUM §4). */
#define SUMI_MODE_MEDIUM_DEFAULT 255u

/* 1.3.0 (Phase 8 step 59, SYNTH §2.7): THE SCOPE VIEW. A shell hands the
   live composite up to 128 line segments a frame — polylines in normalized
   canvas coordinates (v grows down), `points_xy` x,y pairs strip by strip
   with `strip_lengths[strips]` points each — and the composite draws them
   screen-locked in amber: SUMI_SCOPE_OVER lays them over the medium,
   SUMI_SCOPE_REPLACE draws them alone on the scope's dark glass. The live
   path only — like the live ripple, the print, the export and the bloom's
   source never see them, and the field is untouched (the §4.6 fixtures hold
   with the mode at 0, the shipped composite). The desktop draws the synth
   voices' orbits with it (the orbit trace's scope route, DECISIONS_7 #27).
   Mode 0 (or no strips) clears it. The segments are copied: the arrays may
   go out of scope after the call. */
#define SUMI_SCOPE_OFF     0u
#define SUMI_SCOPE_OVER    1u
#define SUMI_SCOPE_REPLACE 2u
SUMI_API void             sumi_set_scope(sumi_instance_t* inst, const float* points_xy, const uint32_t* strip_lengths,
                                         uint32_t strips, uint32_t mode);

/* ---------------------------------------------------------------------------
   libsumi 1.0.0 — THE ONE ABI BREAK OF THE 2.0 ARC (Phase 6 step 41,
   DECISIONS_5 #45). Migrating a 0.x host, mechanically:
     1. sumi_layout_probe gained `const sumi_layout_state_t* state` after
        `aspect`: pass NULL (or zeros) for every layout that exists today.
     2. sumi_cell_info_t gained `flags` at its end (bit 0 = continuous, the
        theremin's; 0 today): read it or ignore it.
     3. sumi_params_t gained `medium` at its end (0 = SUMI_MEDIUM_SUMI, the
        renderer you know; 1 = SUMI_MEDIUM_ANOD, inert until its composite
        lands): zero-initialise params, or set 0.
     4. sumi_set_palette and SUMI_PALETTE_CUSTOM (active_palette_id 3): new
        and optional — the built-in palettes render as before.
     5. sumi_layout_t 8..12 are named and RESERVED for Phase 8;
        sumi_set_params clamps them to FIFTHS with a warning until then.
   Nothing else moved: medium 0 renders bitwise as 0.14.0 — the field gate
   proves the field, the composite gate (tests/fixtures/composite_512_
   metal.rgba) the pixels. From here on, additive growth only.
   1.6.0 (Phase 11 step 71, the v3.0 arc): SUMI_MEDIUM_KESSHO; sumi_ctl_t 20–23
   (SUMI_CTL_COUNT 24); params.polarizer_mode / braid_mode / kessho_*; slide_mode 3,
   press_mode 3 and bend_mode 5 (the last two reserved for steps 72–73);
   SUMI_EXPORT_KESSHO_CAUSTIC. The field's payload is unchanged (the Metal
   fixture holds); media 0 and 1 render bitwise as 1.5.0.
   1.7.0 (Phase 11 step 71b, the minerals delta): sumi_mineral_t (sized),
   sumi_set_mineral / sumi_get_mineral / sumi_mineral_preset, SUMI_OPTIC_*,
   SUMI_MINERAL_* (six); params.drop_habit / habit_roundness / habit_blades /
   tine_profile (habit 0 and profile 0 bitwise the v2 drop and tine); the
   Kessho palette library of 1.6.0 is retired (sumi_palette_preset_count(2)
   = 0 — a mineral is the medium's "palette"; the custom slot reads as a
   stylised chart through mineral.lut_id ≥ 1). Additive.
   ---------------------------------------------------------------------------
   1.8.0 (Phase 11 step 72, KESSHO §2.2): sumi_add_arnold, SUMI_ARNOLD_Q_MAX;
   press_mode 3 is live (the Arnol'd tongue lock; Kessho's press default) and
   SUMI_CTL_ARNOLD_K drives it; the pass SUMI_DEFORM_ARNOLD (exact). Additive;
   the fixtures hold.
   1.9.0 (Phase 11 step 73, KESSHO §2.4): bend_mode 5 is live (Kirchhoff chord
   braiding; Kessho's bend default), params.braid_mode and SUMI_CTL_BRAID_COUPLE
   drive it; no new entry point (the swirl pass composes). Additive; the
   fixtures hold.
   1.10.0 (Phase 11 step 74, KESSHO §2.3): the chord-wave lattice — under Kessho
   the poly pressure (0xA0) is the voice's windowed ripple-tine (the ripple pass
   with a window along its second coordinate, exact); no new entry point.
   Additive; the fixtures hold.
   1.11.0 (Phase 12 step 76, SPEC §10.4 and TO_PROJECT_SPEC §10.10; DECISIONS_11
   #1): Anod finished — sumi_set_seed / sumi_get_seed (the core's draw state,
   so an identical gesture script yields a bit-identical Anod field run to run
   and a replay carries it); the strain buffer behind the composite (a side
   buffer: the field's payload and fixture untouched); params.anod_glow_mode
   (0 plasma, bitwise 1.10.0; 1 the spark reading), spark_afterglow. Step 76b
   (DECISIONS_11 #3): sumi_add_leader — the leader stroke, the Anod strike's
   shape under mode 1 (the pass SUMI_DEFORM_LEADER, sub-stepped). Additive;
   the fixtures hold.
   1.12.0 (Phase 12 step 77, TO_PROJECT_SPEC §14.7; DECISIONS_11 #4) — THE LAST
   CORE-OPEN STEP OF THE ARC (the core freezes from step 78 to step 92):
   SUMI_LAYOUT_BOEHM = 13 (nine keys in `buttons` bits 0..8 — the valves' CCs
   and SUMI_CC_KEY_* 114..119 — three register cells), sumi_layout_boehm_note;
   params.split_mode / split_layout / split_note (the split surface: two
   layouts, a half each, the notes divided). Additive; the fixtures hold.
*/

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

/* 1.0.0 (Phase 6 step 41, QOL §1): a USER PALETTE — an N-stop gradient along
   the ink-depth axis plus the depth curve, a POD the shells own the editor
   and the persistence of. The composite applies it exactly like the
   built-ins (the same washi, the same soak: the medium keeps its rendering
   character, the user chooses the hues — the identity guardrail); the
   built-in palettes 0..2 are untouched and render bitwise as before. The
   Anod medium will read the same data its own way (a glow, not an ink).
   Selected by active_palette_id = SUMI_PALETTE_CUSTOM; until a palette is
   set, a sumi-like default stands in. Validated on the way in: stops
   clamped to 2..8 and to ascending positions in 0..1, RGB to 0..1, the curve
   to its ranges. */
#define SUMI_PALETTE_MAX_STOPS 8
#define SUMI_PALETTE_CUSTOM    3u
typedef struct {
    float    rgb[3];        /* LINEAR RGB, 0..1                                   */
    float    position;      /* 0..1 along the ink-depth axis (0 thin, 1 pooled)   */
} sumi_palette_stop_t;
typedef struct {
    uint32_t stop_count;    /* 2..8                                               */
    sumi_palette_stop_t stops[SUMI_PALETTE_MAX_STOPS];   /* ascending position   */
    float    depth_gamma;   /* the ink-depth curve: u = floor + (1−floor)·depth^γ, 0.25..4 (1 = linear) */
    float    depth_floor;   /* the thinnest visible ink's position, 0..1           */
    float    hue_drift;     /* per-drop variation, 0..1: the drop's aux selector
                               blends the sampled colour toward accent_rgb by
                               drift·hue_t — the built-ins' own hue drift (0.45).
                               1.1.0 (step 43): was a shift of the sampled
                               position along the gradient; the blend is what
                               makes the built-ins expressible in this model.  */
    float    clear_rgb[3];  /* the clear-water band tone, LINEAR RGB (Sumi)        */
    float    accent_rgb[3]; /* 1.1.0 (step 43): the hue drift's target, LINEAR RGB
                               — the built-ins' accent (Sumi: warm soot, teal,
                               burnt sienna) / halo (Anod). Anod reads the same
                               POD as a glow: the gradient sampled by strain is
                               the charge's core, accent its halo, and the charge
                               phase bands the filament between the two.        */
    uint32_t reserved;
} sumi_palette_t;
SUMI_API void             sumi_set_palette  (sumi_instance_t* inst, const sumi_palette_t* palette);
/* 1.1.0 (Phase 6 step 43): the custom palette as stored (validated). */
SUMI_API void             sumi_get_palette  (sumi_instance_t* inst, sumi_palette_t* out);
/* 1.1.0 (Phase 6 step 43, QOL §1): THE PRESET LIBRARY — the medium's palettes in
   the same model. Indices 0..2 are the medium's three built-in palettes, the
   ones active_palette_id names (Sumi: black / indigo / ochre; Anod: electric
   blue / plasma orange / phosphor green), rendered through the same path as a
   custom palette and bitwise as before; indices from 3 are curated additions
   — colour-blind-considerate pairs and perceptual ramps — for a shell to offer
   as starting points for the custom slot. `name` receives a static string
   (may be NULL). false past the end or for an unknown medium. Pure, instance-
   free, callable from any thread. */
SUMI_API uint32_t         sumi_palette_preset_count(uint32_t medium);
SUMI_API bool             sumi_palette_preset(uint32_t medium, uint32_t index, sumi_palette_t* out,
                                              const char** name);

/* Paper dip: freeze canvas, snapshot, reset UV to identity (rebases the drop
   counter, see spec 4.2). The print pipeline is double-buffered: the core keeps
   two print buffers and flips. v0.6 (DECISIONS_4 #51): when both hold an UNREAD
   print the older one is recycled (sumi_read_print copies synchronously, so no
   host ever holds a core buffer) — a dip is refused, with a warning log, only
   while a readback is still in flight (a few frames after the previous dip). */
SUMI_API void             sumi_trigger_paper_dip(sumi_instance_t* inst);
/* Synchronous readback of the last dipped print (RGBA8, tightly packed).
   Call with pixels=NULL to query size. Returns false if no print exists. */
SUMI_API bool             sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

/* 1.1.0 (Phase 6 step 43, QOL §4): PRINTS AT ANY SIZE. The field is
   resolution-independent — every texel stores where its water came from —
   so the composite renders the same field at 4k as it did on screen: detail
   below a field texel is interpolation (the true re-dip is replay, Phase 8).
   sumi_read_field gives the field as it stands (RGBA16F, W×H×8 bytes, row 0
   at the top; NULL pixels = size query) — what a shell keeps per dip to
   re-export it later (the PRINT LEDGER); it blocks briefly (a GPU copy) and
   fails while a print readback is in flight. THE PAYLOAD (1.2.0, Phase 8
   step 55b): each texel is (dx, dy, ink, aux) — the texel's DISPLACEMENT,
   its pre-image coordinate minus its own (u − x, v − y), then the ink phase
   and the per-drop selector; a fresh sheet is all zeros. 1.1.0 stored the
   pre-image itself (u, v); a host keeps a field only within a session, so
   nothing converts. sumi_export_begin composites a
   field — `field` as sumi_read_field gave it, or NULL for the field as it
   stands — at w×h (each <= SUMI_EXPORT_MAX_DIM) with the CURRENT params and
   palette (a shell restores a dip's before re-exporting it), un-rippled like
   a dip; asynchronous like the print: false while a readback is in flight.
   sumi_export_poll returns 0 idle / failed, 1 in flight, 2 done and copied
   (RGBA8, w×h; NULL out = size query, never consumes). SUMI_EXPORT_ANOD_ALPHA
   makes an Anod export straight colour over alpha — the charge's glow and the
   water's grid, the glass transparent; a Sumi export is always opaque. */
#define SUMI_EXPORT_MAX_DIM    8192u
#define SUMI_EXPORT_ANOD_ALPHA 1u
/* 1.6.0 (step 71): under Kessho, ANOD_ALPHA writes the crystal's light (both layers) over alpha;
   KESSHO_CAUSTIC writes Layer B ALONE over alpha — the tint and the fold, no birefringence.
   Without either flag a Kessho export is the full composite, opaque, as an Anod export is. */
#define SUMI_EXPORT_KESSHO_CAUSTIC 2u
SUMI_API bool             sumi_read_field  (sumi_instance_t* inst, uint8_t* out_rgba16f, size_t capacity,
                                            uint32_t* out_w, uint32_t* out_h);
SUMI_API bool             sumi_export_begin(sumi_instance_t* inst, const uint8_t* field_rgba16f,
                                            uint32_t field_w, uint32_t field_h,
                                            uint32_t w, uint32_t h, uint32_t flags);
SUMI_API int              sumi_export_poll (sumi_instance_t* inst, uint8_t* out_rgba8, size_t capacity,
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
    uint32_t flags;           /* 1.0.0: SUMI_CELL_* bits; 0 for every layout
                                 that exists today (the theremin's continuous
                                 pitch is the first bit, Phase 8)             */
} sumi_cell_info_t;
#define SUMI_CELL_CONTINUOUS 1u   /* the cell has no discrete note: pitch is continuous across it */

/* 1.0.0 (Phase 6 step 41, INSTRUMENT §1): the LAYOUT STATE a stateful layout
   (valves, a slide) needs to answer the probe — an explicit snapshot the
   shell keeps beside its params snapshot, so the probe stays a pure function
   callable from any thread. State changes travel as MIDI (Phase 8). Zeros or
   NULL = stateless, which every layout shipping today is. */
typedef struct {
    uint32_t buttons;      /* bitmask: valves, register keys (bit 0 = valve 1 …) */
    float    slider;       /* continuous control position 0..1 (trombone slide)  */
    uint32_t reserved[2];
} sumi_layout_state_t;
/* 1.4.0 (Phase 9 step 60, INSTRUMENT §1; DECISIONS_5 #5, DECISIONS_8 #1):
   THE FINGERING CCs — how the layout state travels, so a DAW records and
   replays the fingering, not just the pitches. The valves are CC 110 / 111 /
   112 (a value ≥ 64 = pressed → `buttons` bits 0 / 1 / 2), the slide CC 113
   (0..127 → `slider` 0..1, smoothed by the normalizer over a 10 ms one-pole
   so the 7-bit staircase reads as a ramp). GLOBAL state: in MPE mode the
   zone's MASTER channel only (a member channel's 110–113 stay per-note
   controllers for the CC map); in classic and wind mode any channel. The
   engine decodes them into its own copy (sumi_get_layout_state); a shell
   mirrors the same bytes into the snapshot it hands the probe — one source
   of truth, the byte stream. Every CC is still forwarded to the CC map. */
#define SUMI_CC_VALVE_1 110u
#define SUMI_CC_VALVE_2 111u
#define SUMI_CC_VALVE_3 112u
#define SUMI_CC_SLIDE   113u
/* 1.12.0 (step 77): THE BOEHM KEYS beyond the three the valves' CCs carry (L1, L2, L3 = 110, 111, 112 → bits 0..2):
   CC 114..119 → `buttons` bits 3..8, ≥ 64 = pressed, the same global rule (the master channel under MPE). */
#define SUMI_CC_KEY_THUMB  114u   /* bit 3: the thumb (the B key)              */
#define SUMI_CC_KEY_GSHARP 115u   /* bit 4: the G♯ lever (opens its hole)      */
#define SUMI_CC_KEY_R1     116u   /* bit 5: the right hand's first finger (F) */
#define SUMI_CC_KEY_R2     117u   /* bit 6: the second (E)                     */
#define SUMI_CC_KEY_R3     118u   /* bit 7: the third (D)                      */
#define SUMI_CC_KEY_DSHARP 119u   /* bit 8: the D♯ key (opens its hole)        */
#define SUMI_LAYOUT_KEYS   9u     /* the keys a layout state holds (the Boehm's nine; the brass read the first three) */

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
                                            const sumi_layout_state_t* state,   /* 1.0.0: NULL = stateless */
                                            float norm_x, float norm_y,
                                            sumi_cell_info_t* out);
/* 1.4.0 (Phase 9 step 60): the engine's own decoded copy of the layout state
   — the fingering CCs as the normalizer last drained them (the slide
   smoothed), the state the engine places notes with. Render thread only, as
   sumi_get_params. A shell with no play surface (the desktop) reads it to
   answer the probe for what the wire says; a play surface keeps its own
   mirror of the bytes it sends. */
SUMI_API void             sumi_get_layout_state(sumi_instance_t* inst, sumi_layout_state_t* out);
/* 1.12.0 (step 77): THE BOEHM CHART as a pure function of the key bits — the note (D4 62 .. C♯5 73) the fingering
   sounds on the fundamental register; the shells' key panels label it, the registers add 12 and 24. Any thread. */
SUMI_API uint8_t          sumi_layout_boehm_note(uint32_t buttons);

/* Manual touch / mouse gestures — render thread only, normalized [0,1] coords.
   layer_type: sumi_drop_layer_t (0 ink, 1 clear, 2 feed — v0.6; 3 none: no pass, v0.13). */
SUMI_API void             sumi_add_drop  (sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type);
SUMI_API void             sumi_add_tine  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float alpha /*sharpness*/, float magnitude);
SUMI_API void             sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius,
                                          uint32_t profile /* sumi_vortex_profile_t (v0.4) */);
/* 1.1.0 (Phase 6, DECISIONS_5 #75 — the author's gesture table): MEDIUM-
   AWARE GESTURES. The shells call these for the marble gestures; each reads
   params.medium. In Sumi each is exactly the operator call beside it (the
   bytes are the 1.0 gesture's); in Anod each plays what the MIDI table plays:
     tap   — Sumi: sumi_add_drop(radius, INK). Anod: the note-on's STRIKE —
             a charge of radius·anod_drop and the shear episode with its
             band and kick on the full radius (#71, back at step 55b),
             along the layout's pitch axis at (x, y) (off the lattice:
             radial from the canvas centre). The long press's first touch
             is a tap.
     pinch — Sumi: sumi_add_pinch(k_delta, angle). Anod: the VISCOUS
             MULTIPOLE BURST (the pinch is its r -> 0 limit, MEDIUM §2.3):
             the squeeze accumulates and fires a burst per step — core a
             quarter of `span` (the finger distance, canvas heights), lobes
             along `angle`, spreading ejects and squeezing draws in, the
             order params.burst_order.
     twist — Sumi: sumi_add_vortex(profile). Anod: the same twist as a
             TORSION vortex (k and φ the ctls').
     press — one frame of a held long press at (x, y); up / down 0..1 the
             push / pull; returns the boundary radius R the host tracks.
             Sumi: pull = the Lamb–Oseen swirl on core R, else the FEED drop
             growing R (DECISIONS_4 #49). Anod: pull = the Chladni stir,
             reversed; hold / push = the torsion sweep feed around the
             charge (a held key's press feed). sumi_gesture_press_end
             stills the stir a press set. */
SUMI_API void             sumi_gesture_tap  (sumi_instance_t* inst, float x, float y, float radius);
SUMI_API void             sumi_gesture_pinch(sumi_instance_t* inst, float x, float y, float k_delta,
                                             float angle, float span);
SUMI_API void             sumi_gesture_twist(sumi_instance_t* inst, float x, float y, float strength,
                                             float radius, uint32_t profile);
SUMI_API float            sumi_gesture_press(sumi_instance_t* inst, float x, float y, float R,
                                             float up, float down, double dt);
SUMI_API void             sumi_gesture_press_end(sumi_instance_t* inst);
/* 1.11.0 (Phase 12 step 76; DECISIONS_7 #25 answered): THE DRAW STATE. The one
   thing in the core that is not a function of the bytes and the gestures it
   is handed is the Anod strike's phase draw (a 32-bit generator the mapper
   advances per strike), so two runs of one gesture script diverged in Anod
   while Sumi read bitwise. The state is one word: set it and the next strike
   draws from it; read it and a recording carries it (the replay's `seed`
   header line, which the player applies before its first frame). Setting it
   also REBASES the mapper's gesture clocks — the press-fed torsion's phase,
   the stir's pending rotation, the running spark and burst episodes, the
   smoothed controls snapped to their targets — the state a recording cannot
   carry, so two runs of one script from one word are bitwise in every
   medium. Sumi and Kessho draw nothing — the word itself changes nothing
   there. */
SUMI_API void             sumi_set_seed(sumi_instance_t* inst, uint32_t seed);
SUMI_API uint32_t         sumi_get_seed(const sumi_instance_t* inst);
/* 1.11.0 (Phase 12 step 76b, TO_PROJECT_SPEC §10.10): THE LEADER STROKE — a
   jagged tree torn from (x, y): a biased random walk (the heading jittered
   about (dir_x, dir_y); branches, depth 3; 8 … 40 steps by `energy` 0..1),
   deterministic from `seed`, each segment a short windowed shear along its
   own direction (CLASS SUB-STEPPED, the local spark's), advanced over the
   frames as an episode. Under Anod with anod_glow_mode 1 the note-on strikes
   as one — the plasma's blob episode stays for mode 0, bitwise — and this is
   the gesture route's call, for the stylus and the scripts. A strike's own
   strain is a plateau the spark reading cannot thin; a stroke's is a ridge. */
SUMI_API void             sumi_add_leader(sumi_instance_t* inst, float x, float y, float dir_x, float dir_y,
                                          float energy, uint32_t seed);
/* v0.4: dipolar wake — the stylus stroke's fluid signature (spec §4.3(4)).
   v0.7: params.wake_profile selects the fluid — 0 the inviscid doublet below,
   1 the viscous 2-D Stokeslet stroke (DECISIONS_4 #53), same call, same units.
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
/* v0.11 (Phase 6 step 37, MEDIUM §2.2): ONE step of the Chladni cellular
   flow as a gesture — the Taylor–Green stream function ψ = psi·cos(k_x(x−x0))
   ·cos(k_y(y−y0)) on a lattice with pitch (sx, sy) and a cell centre at
   (x0, y0) — NORMALIZED canvas units (k = π/pitch) — split into its two
   diagonal waves cos(u−v) and cos(u+v), each an EXACT shear, applied one
   after the other (the kick-drift splitting). `psi` is the step's stream-
   function amplitude (canvas-height² units; the displacement is ~psi·k);
   `balance` weights the second wave 1 − 2·balance (0 = the cellular flow).
   CLASS EXACT: det J = 1 at any amplitude. A NEGATIVE psi applies the step's
   EXACT INVERSE — both shears negated AND in reversed order — so (psi, b)
   then (−psi, b) is the identity; a sign flip alone would not be (crossed
   shears do not commute). */
SUMI_API void             sumi_add_chladni(sumi_instance_t* inst, float psi, float balance,
                                           float sx, float x0, float sy, float y0);
/* v0.12 (Phase 6 step 38, MEDIUM §2.3): the viscous multipole BURST — the
   strike operator. The time-integrated displacement of an impulsive viscous
   2-D multipole of order m (2 = the quadrupole: two ejection lobes along the
   axis theta0, two intake lobes across it; radians, canvas frame, y down),
   the impulse spread over a Gaussian core of radius `a` (canvas heights).
   Method after Jaffer's Lamb–Oseen paper (arXiv:1810.04646), extended to
   m ≥ 2, where the time integral is elementary: Ψ = A_m (a/r)^(m−2)
   sin(m(θ−θ0)) [Φ_m(r²/ℓ²) − Φ_m(r²/a²)], Φ_2 = χ = (1 − e^−S)/S, d = ∇⊥Ψ.
   `D` is the radial displacement at r = a on the ejection axis over the
   whole burst (canvas heights; D < 0 applies the FIRST-ORDER inverse).
   CLASS SUB-STEPPED (the wake's family): d is divergence-free as a field,
   the finite map area-preserving to first order; the core splits the burst
   into passes whose peak displacement stays within β_m × the current core
   (|∇d| ≤ 0.25, the wake's a/4 criterion). THE STRIKE HAS A LIFETIME: it
   fires sharp at ℓ = a and the release grows the age to params.burst_age·a
   over params.burst_life seconds, each frame's increment emitted by the
   mapper under the pass budget — the first increment lands in the next
   sumi_update. Near the core the quadrupole is pure hyperbolic strain (the
   pinch is its r → 0 limit); in the diffused zone a ≪ r ≪ ℓ it decays as
   cos(2θ)/r. `m` clamps to 2..8; m = 0 takes params.burst_order.
   Gesture-ABI only until the medium's binding tables route strikes
   (step 42). */
SUMI_API void             sumi_add_burst(sumi_instance_t* inst, float x, float y, float a, float D,
                                         float theta0, uint32_t m);
/* v0.13 (Phase 6 step 39, MEDIUM §2.4): ONE kick-drift step of the SPARK
   SHEAR as a gesture. In the frame rotated by theta0 about (x, y): the
   x-shear x1 = x + A·w(y)·f(y), then the y-shear y1 = y + B·w(x1)·f(x1),
   with f a stack of triangle waves (k, 2k, 4k, …, params.spark_stack deep;
   params.spark_profile = 1 swaps in piecewise-linear hash noise) and w a
   Gaussian window of half-width `band` ACROSS each shear (0 = the whole
   canvas). A, B in canvas heights (the peak kick), k in radians per canvas
   height, `phase` the base octave's φ. A zero amplitude skips its stage.
   CLASS EXACT for ANY profile: x' = x + g(y) inverts as x = x' − g(y)
   whatever g is — its kinks become creases, legally. The step's EXACT
   INVERSE is the step (0, −B) followed by the step (−A, 0): reversed order
   AND negated — a sign flip in the same order is not (crossed shears do not
   commute, DECISIONS_5 #17). */
SUMI_API void             sumi_add_spark_shear(sumi_instance_t* inst, float x, float y, float band,
                                               float A, float B, float k, float phase, float theta0);
/* v0.13 (Phase 6 step 39, MEDIUM §2.4): the composed SPARK STRIKE — one
   exact drop pass (the Joule blast: radial outflow cannot be divergence-
   free, so the engine's oldest operator does it; `layer_type` as
   sumi_add_drop's: 0 ink, 2 feed, 3 NO blast, else clear water), a viscous multipole
   burst of core r and lobe displacement D along theta0 (the pinch lobes;
   the order params.burst_order, the age params.burst_age/_life), and the
   spark shear as a DECAYING EPISODE: kicks A = B = params.spark_shear·r
   spent as e^(−t/τ) over 4·params.spark_tau seconds in kick-drift steps
   along and across theta0, windowed to twice the radius, k from
   SUMI_CTL_SPARK_K, φ drawn per strike. CLASS SUB-STEPPED BY INHERITANCE
   (the burst is the strictest member); the drop and the shears are exact.
   A strike-route candidate for the binding tables (step 42). */
SUMI_API void             sumi_add_spark(sumi_instance_t* inst, float x, float y, float r, float D,
                                         float theta0, uint32_t layer_type);
/* v0.14 (Phase 6 step 40, MEDIUM §2.5): ONE step of the scaled CHIRIKOV
   STANDARD MAP as a gesture — the kick y1 = y + A·sin(k(x−x)+phase), then
   the drift x1 = x + eps·(y1−y), about (x, y); k = 2π·periods per canvas
   height along x, A = K/(k·eps) so that K is the step's chaos parameter
   (Greene's threshold ≈ 0.9716: sheets below, filamentation and island
   chains above — the elliptic island sits half a kick period from the
   centre). CLASS EXACT (two shears, det J = 1 at any K). A NEGATIVE K
   applies the step's EXACT INVERSE — the drift undone first, then the
   kick. |K| clamps to 2, the resampling medium's own limit: iterating a
   step above the core's route ceiling shreds ink into filaments finer than
   a texel (DECISIONS_5, the erosion table). */
SUMI_API void             sumi_add_chirikov(sumi_instance_t* inst, float x, float y, float K,
                                            uint32_t periods, float eps, float phase);

/* 1.8.0 (Phase 11 step 72, KESSHO §2.2): ONE iteration of the Arnol'd tongue
   lock about (x, y) with the ring at `radius` (canvas heights) — the map
   M = twist ∘ kick in the action-angle frame ρ = r²/2, θ: the kick from the
   generating function F₂ = θρ₁ + (K/q)·W(ρ₁)·cos qθ (ρ₁ = ρ + K W sin qθ,
   θ₁ = θ + (K/q) W′ cos qθ; W a C² window, 1 across the ring, 0 at the
   centre and beyond 1.5× the ring's action), then the twist — the
   Lamb–Oseen rotation with its core at the ring, scaled to `winding_omega`
   TURNS per iteration at the ring and windowed to zero by 2.5 radii. `k` is
   the kick as a fraction 0..1 of K_max = 0.8 / max|W′| (the bijection's
   bound, the clamp); `resonance_q` the petal count 1..SUMI_ARNOLD_Q_MAX (0 =
   the tongue of winding_omega: the Farey fraction p/q of smallest q with
   |Ω₀ − p/q| < 1/(2q²), else q = 6); `twist` 0 drops the twist (the red
   control: the petals then grow without bound). CLASS EXACT (a canonical
   transformation composed with a rotation; one scalar Newton in the
   shader). The MIDI route is press_mode 3: the press is K, Ω₀ = (f_voice /
   f_bass) mod 1 and q from the pitch against the lowest held note, whose
   own press stays the swirl; off the exact ratio the petals precess by
   δ = Ω₀ − p/q turns per frame — the beat. */
#define SUMI_ARNOLD_Q_MAX 6u
SUMI_API void             sumi_add_arnold(sumi_instance_t* inst, float x, float y, float radius,
                                          float winding_omega, float k, uint32_t resonance_q, uint32_t twist);

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
* **Marble mode** — the direct gestures: tap → drop, one-finger drag → tine, two-finger twist → vortex (profile from the settings' *Vortex profile*, as the desktop's right drag — DECISIONS_4 #56; R = half the finger separation, ω = the twist delta), **two-finger pinch → Hamiltonian pinch** (fold axis = the finger-to-finger line, k from scale deltas; #41), **long press (250 ms) → the pressure gesture** (DECISIONS_4 #49: the press lays a drop and becomes Play mode's bipolar Y — hold or push away = the feed drop `SUMI_DROP_FEED`, pull back = the Lamb–Oseen swirl `SUMI_VORTEX_LAMB_OSEEN`; Shift + right drag on desktop, the same constants on every shell), and **the stylus draws its wake here too** (§8.7, DECISIONS_4 #54). Zero MIDI. This is the "Airwave-like expression tool".
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

**Transports.** iOS: (a) the **virtual CoreMIDI source** ("midi-sink Play Surface") for on-device DAWs, which also **sends explicitly to the IDAM destination** when a Mac is tethered — a virtual source is NOT bridged over the cable by itself (#27); wired to a Mac is the lowest-latency sink and needs no Audio-MIDI-Setup "Enable" (that button is IDAM audio); (b) the **MIDI network session** (rtpMIDI) for desktop DAWs over Wi-Fi and the Windows/Linux substitute; (c) **BLE** via `MIDISend` to the Bluetooth-driver destination — never a local mirror, which duplicated delivery (#25). Android: (a) the **USB-MIDI gadget — the primary sink**: the user flips the system USB mode to MIDI, Android publishes a class-compliant peripheral port (a `TYPE_USB` device with no host-side `UsbDevice`; status from the sticky `USB_STATE` broadcast: active / charge-only / unsupported, #45), visible to any host OS; (b) the **`MidiDeviceService` virtual device**; (c) a **BLE-MIDI peripheral** implemented as a GATT server (Android's `MidiManager` is central-only): BLE-MIDI 1.0 framing, one notification in flight per link, messages coalesced into the tail packet, and a backlog cap that drops from the **newest** end so the handshake and note events at the head survive (#44). One generator, N sinks: every transport carries the identical stream under its own policy. **Android, since Phase 10 (`DECISIONS_9 #14`):** (d) a **network session** of the app's own — RTP-MIDI with Apple's session protocol (`rtpmidi/`, pure C over POSIX UDP, no threads), advertised over mDNS as `_apple-midi._udp` so Audio MIDI Setup and rtpMIDI invite it; the fourth outbound sink under the rate class, polled and written on the MIDI thread, and a MIDI input at the merge point (a host's bytes enter as a device's); the iPad's network session is the OS's.

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

## 9. Packaging, Release Engineering, Web & Documentation (Phase 5, v0.5 → v1.0)

Formerly `_work/PHASE5_SPEC.md`; folded verbatim, with what shipped recorded in `DECISIONS.md` Part IV (`DECISIONS_4 #n`) and `ROADMAP.md` Part 4 — where this text and an entry disagree, the entry is the record. Two things the text below still says in its Phase-5 tense: the settings UI's contents spec became the desktop settings window itself, mirrored on every shell (DECISIONS_4 #56); and the store beta gate ran as Steps 32–33 with the feedback batches folded into the core under the scoped unfreeze (§9.4, DECISIONS_4 #49–#80).
**Standing assumption: the author's existing publishing infrastructure is an INPUT, not a deliverable** — Apple Developer ID + App Store Connect, Google Play account, an existing Homebrew tap, an apt repository with its GPG signing key, prior winget manifests. Pipelines are parameterized by these (CI secrets, repo/tap names); no step teaches account setup.

### 9.1 Scope & principles

Phase 5 turns a five-platform engine with harnesses into a released product family:

* **Desktop (macOS, Windows, Linux)** — the harness becomes a product, distributed through the author's existing channels (tap cask, winget, apt) plus GitHub Releases.
* **iOS / Android** — store-published, **beta first**: TestFlight and a Play closed track with real users before any production listing (author's standing practice; the phase has an explicit feedback gate).
* **Web** — a WebGPU/wasm **marble-mode** build: the sixth host shell, and the live-example engine for the docs.
* **Documentation** — a public site: user guide, the operator book with live demos, a performance-video gallery (incl. the Jaffer tribute), the MIDI implementation chart, published design notes (the DECISIONS files, lightly edited) and changelog. **Hard ordering:** the docs need the web build (live examples); the RELEASE LANES need the docs (a cask requires a homepage URL, both stores require a privacy-policy URL, and support/marketing URLs are wanted everywhere — all of which live on the docs site); and the beta needs the docs as its user guide. The chain is therefore: web → docs → lanes → beta → release.

Principles carried over: the core stays frozen except the WebGPU seam (one new swapchain TU, the §4.6 discipline already proven across four backends); everything a tag can build, a tag builds — humans touch only store uploads and review responses; every artifact traces to a commit and a `sumi_version()`.

Versioning: Phase 5 ships **1.0.0**. `sumi_version()` and every platform's marketing version derive from the same git tag (single source: the tag; CI injects it).

### 9.2 Desktop productization

The harness graduates. Known debt from the changelog and DECISIONS:

* **macOS becomes a real `.app` bundle** (currently a bare executable with a runtime Dock-tile icon): Info.plist, icon asset from the existing `tools/gen_icons.py` output, hardened runtime, Developer ID signing, notarization + stapling. The cask installs the bundle.
* **Settings UI** replacing debug keys: the iOS settings sheet is the contents spec (layout picker incl. Piano grid, palettes, bend/slide/press/pinch/vortex/ripple rows, sim_scale, CC map editor, "Paper dip (fresh sheet)" button, MIDI port list with the 1 Hz rescan status). Native-toolkit minimalism is fine (one window, no theming project).
* **`--dev` flag** gates every debug binding (keys 1–9, C/M/K, `--field-dump`, stress feeders, pen tracer hooks). Release builds keep the flag (support asks "run with --dev") but ship with it off.
* **About/version** wired to `sumi_version()` + git describe; **first-run hint** (one dismissible line: where MIDI devices appear, where settings live).
* **Product naming decision** (bundle id, executable name, cask/winget/apt package names) is made once here and recorded in DECISIONS — every later manifest references it.

Windows/Linux inherit the same settings UI and flag; Linux keeps the existing .desktop/hicolor install component (already shipped in Step 14's icon work).

### 9.3 Release engineering (tag → artifacts, all five platforms)

One tag-triggered GitHub Actions release workflow:

* **Matrix:** macos (universal arm64+x86_64 bundle → signed, notarized, stapled DMG), windows (MSVC build → signed-if-cert-present installer + portable zip; unsigned builds are still released, SmartScreen consequence documented in the README, not fought), ubuntu (deb + tarball), and the web lane — the CI-released platforms. **iOS and Android are MANUAL by the author's choice:** Xcode archive → TestFlight and Android Studio signed bundle → Play internal, each walked from a documented `RELEASING.md` checklist against a tagged checkout with the CI-injected version; PR CI keeps mobile build-only compile checks so tags never surprise the archive. Store promotion past the test tracks is human either way (§4).
* **Per-OS pinned `sokol-shdc` fetch** (existing CMake machinery) and the FetchContent pins make builds reproducible; the workflow asserts the ABI/C11 tests, the hostmpe suite, and the §4.6 field regression per backend before packaging anything.
* **Channel automation on release publish:** cask bump PR to the author's tap (version + sha256), winget manifest bump (wingetcreate PR), deb pushed to the existing apt repo with its GPG key from secrets (Release/InRelease re-signed). **Flatpak is a one-session spike, not a commitment:** the open question is ALSA MIDI through the sandbox; if the spike is clean it becomes a channel, otherwise the deb + tarball stand.
* **Release notes generate from CHANGELOG.md** (the condensed-evidence practice continues: each phase's DONE evidence condenses into the changelog, which is the release-notes source — no third format).

### 9.4 Store beta gate (iOS + Android)

Standing practice made explicit: **no production listing before a beta round — and no beta round before the documentation site is live** (testers get a real user guide, and their confusion reports then measure the guide, not its absence). Sequencing is therefore: WebGPU/web → docs → release lanes → beta → release.

* **TestFlight** (external group) and **Play closed testing** track, populated from the same tagged build the workflow produced.
* **Feedback instrument:** a pinned GitHub Discussion (or issue template) per beta wave asking three things — what did you play it with (device/controller), what confused you in the first five minutes, what did you expect Play mode to do that it didn't. First-five-minutes confusion feeds the docs' user guide directly.
* **Store metadata** (screenshots of marbling under real playing, the two modes named plainly, MPE-controller support called out) prepared once, localized later if ever.
* **Exit criteria to production:** one full beta wave (≥ 2 weeks) whose feedback is triaged into classes, then an **incorporation loop** — the one phase-5 step allowed to iterate: bugfix-scoped changes (core included, each fix landing with the regression test that would have caught it), release-candidate tags through the full pipeline, reporter confirmation on the test tracks, author sign-off on any feel change. **1.0 is the promotion of the final confirmed RC, never a build the testers haven't held.** Production promotion is a human click, deliberately.

### 9.5 WebGPU backend & marble web

The sixth host shell, exactly as the architecture intended:

* **Core:** `swapchain_webgpu.cpp` hosts SOKOL_IMPL for the WebGPU backend; `sokol-shdc` gains the WGSL dialect output; the §4.6 cross-backend field regression gains a **web tier** tolerance (like the documented GLES3 mobile tier). RGBA16F filtering is core WebGPU — the field format survives unchanged. The C-ABI is the wasm export surface (Emscripten; exceptions stay off, matching the ABI rules).
* **Host shell:** a page of JS — canvas + WebGPU device/swapchain handed to the core, pointer/touch events → marble gestures (tap drop, drag tine, two-finger twist vortex — pen pointer events additionally drive the wake where the browser reports pen type/pressure), `requestAnimationFrame` loop, resize → `sumi_resize`.
* **Scope: Marble mode only.** Play mode is web-deferred: WebMIDI *input* is wired where available (Chrome/Edge — the ROLI drives the web build there; Safari has never shipped WebMIDI and the page must degrade to gestures-only without a scare banner), but the play surface, allocator, and outbound transports do not exist in the browser in Phase 5.
* **Scene/embed API — the docs contract:** query parameters select an operator demo scene and expose its parameters (`?scene=lamb_oseen&gamma=…&rc=…`, `?scene=pinch&variant=saddle…`), with a minimal-chrome embed mode. **The docs' live examples ARE this artifact** — one wasm binary, no reimplementations to drift.
* Hosted on GitHub Pages beside the docs; built by the same release workflow.

### 9.6 Documentation site

Static site (KaTeX for formulae), deployed to Pages by CI. Four books and a chart:

1. **User guide** — Marble and Play modes (Play flagged iOS/Android-only), per-device setup pages (ROLI Piano, Airwave routing defaults, Osmose, Brisa/Travel Sax wind mode, classic keyboards), layouts (all six incl. Piano grid), the control strip, the stylus (legato feel, per-cell retriggers, barrel dials, wake), paper dips and prints. Beta-wave confusion reports are this book's backlog.
2. **The Operators** — one page per deformation (drop, tine, vortex ×2 profiles, wake, pinch ×2 variants, ripple, Lamb–Oseen swirl, scroll): the formula, its invariants (area-preservation argument, exactness, sub-stepping rule or exemption, ownership/consumer rule), and a **live embedded demo** (the §5 wasm with scene parameters — sliders map to the formula's symbols).
3. **Architecture** — the Option-2 pattern distilled from PROJECT_SPEC for host-shell builders: the C-ABI, threading contract, §4.6 orientation discipline, per-backend swapchain ownership.
4. **Performance gallery** — videos of real performances (recorded by the author; embedded, self-hosted or unlisted-video embeds — author's call on hosting), each captioned with device + layout + modes used, so the gallery doubles as a "what it can do" index into the user guide. Includes the **Jaffer tribute performance**: the piece from his marbling videos, performed by the author on this instrument (identify the exact piece from his video credits at production time; performed-by-author means only the composition's status matters — verify it is public domain, which classical/folk dance pieces are). Optionally paired with a scripted **Turkish-moire scene** — a pattern Jaffer analyzes in his pigment-transport work — as a bridge between the gallery and the operator book.
5. **Design notes & changelog** — the DECISIONS files published (lightly edited: internal paths trimmed, entries kept verbatim otherwise — they are the honest engineering record) and CHANGELOG.md rendered.

**Store-required pages** — the site carries, at stable URLs published before any lane or manifest references them: the **privacy policy** (short and honest: the app collects nothing — no telemetry, no accounts, no network calls beyond the user's own MIDI transports; store-specific clauses as each console requires), a **support/contact page**, and the **homepage** itself (the cask's `homepage` field, the stores' marketing URL). URL stability is a contract: lanes and store listings hardcode these.

**The MIDI implementation chart** — one page, the instrument's contract: message support in/out per mode, MCM behavior and zone, bend ranges (±48 member / ±2 master), the CC table (1, 2/7/11 aliases, 64, 74, 102, 103, map-routable dims), 0xA0 poly pressure, the consumer/ownership rules (bend_mode / slide_mode / press_mode), per-transport rate policies, and what a DAW recording does and does not capture (the wake invariant, stated for users).

**Citations & acknowledgments:** a dedicated page, and the project's happiest discovery belongs on it: the wake and the Lamb–Oseen swirl are not merely in Jaffer's spirit — they are HIS published extensions, independently converged on. Cite at minimum: Lu, Jaffer, Jin, Zhao & Mao, "Mathematical Marbling," *IEEE Computer Graphics and Applications* vol. 32 no. 6 (2012) pp. 26–35; Jaffer, "Oseen Flow in Paint Marbling," arXiv:1702.02106 (the closed-form short-stylus-stroke field — the wake's lineage); Jaffer, "The Lamb–Oseen Vortex and Paint Marbling," arXiv:1810.04646 (the swirl's own paper); his marbling pages at MIT CSAIL. Then Lamb's *Hydrodynamics* (the doublet and the vortex), Rankine (1858), and the MMA/AMEI MPE specification. Final citation details verified against the publications during writing, never from memory. The operator pages for the wake and the swirl each carry a one-line lineage note pointing at the corresponding Jaffer paper. On release, send Professor Jaffer a link with a short note — the Lamb–Oseen extension of his closed-form family is the kind of thing an author enjoys seeing.

### 9.7 Explicitly out of Phase 5 scope
Play mode on the web (WebMIDI output, wasm hostmpe), localization, Flatpak beyond the spike (if it fails), Windows Store / Microsoft Store listing, auto-update mechanisms, telemetry of any kind, Phase 6 instrument layouts (trumpet valves / trombone slide / Wicki–Hayden / fretboard / theremin — parked with the noted architectural question: stateful layouts need a probe-state design).

---

## 10. Media

### 10.1 The media abstraction (as shipped, step 41–42)
The engine is symplectic — bijective, det J = 1, inverse-lookup maps composed on a coordinate field — and suminagashi was its first *medium*, not the engine itself. **The engine owns** the ping-pong field and pass machinery, the whole operator library (every medium may use every operator; media choose defaults), the normalizer and voice model, layouts and the play surface, hostmpe, the strip, transports, dip/readback, the §4.6 orientation discipline, the budgets and the delta rule. **A medium owns** the composite pass (how the field is *read*), its palette family, its print styling and its **default binding table** (§10.5).

ABI: `sumi_params_t.medium` — `SUMI_MEDIUM_SUMI` 0, `SUMI_MEDIUM_ANOD` 1 (values above clamp to Sumi); it landed in the one ABI break of the arc, `libsumi` 1.0.0, with `sumi_layout_probe`'s state argument, `sumi_cell_info_t.flags`, `sumi_set_palette` and the reserved layouts 8–12 (#45–#47). The medium is named **Anod** (#1); Ichisuke Fujioka is honoured in the acknowledgments regardless.

**Switching is live and a feature** (#53): the field is medium-agnostic (coordinates + phase + aux), so a switch re-reads the same deformation history — a marbled sheet as a discharge record; the field is bitwise across a switch. The dip stays one key away for a fresh sheet. In Anod the ink phase reads as **charge phase** (filament banding) and aux as the per-event hue.

### 10.2 Operator classes and the conservation gate (step 35)
Every operator, present and future, declares one class in its header comment, its test and its book page:

| Class | Definition | Members | Discipline |
|---|---|---|---|
| **Exact** | det J = 1 at any magnitude; closed-form invertible as-is | drop, tine, vortex (all four profiles), pinch (saddle), ripple, swirl, scroll, torsion, Chladni (disc mode), spark shear, Chirikov; v3.0: the crystal drop at any habit, the cleavage profiles, the Arnol'd lock (a generating-function kick, one Newton in the shader), the braid (a Strang composition of exact factors), the chord wave (route a) | no sub-stepping; ±k pairs invert analytically |
| **Sub-stepped displacement field** | an exactly divergence-free field applied as a finite step — area-preserving to first order | wake, viscous multipole burst, Chladni (field mode, "Inverse Chladni"); v3.0: the leader stroke (one sub-step at every velocity) | per-pass displacement ≤ a fraction of the core (the wake's ≤ a/4 rule; the burst's β_m); soaked under the wake's numbers |

A composition inherits the strictest class of its members (the composed spark is sub-stepped by its burst, #37). **The four-part gate** (`midi-sink --dev --soak <operator>`, #13–#17, #21): (a) the class stated; (b) ±k pairs invert — mass within ±5 % and the pre-image within 8 texels over 500 strong pairs — 32 since libsumi 1.2.0's displacement payload, the 8 being the coordinate payload's freeze (`DECISIONS_7 #3`, §4.2) — (sub-stepped: one a/4 step keeps the pre-image Jacobian > 0.5, mean within 2·10⁻³ of 1); (c) zero fabrication — mass never grows past 0.5 % or 5·10⁻⁵ per pass, the route proven alive; (d) per-pass erosion ≤ 2× the glide tine's steady 1.15·10⁻⁵. Three negative controls prove the gate red before it is trusted. Findings recorded: the resampled medium gains at ink boundaries under strong pairs (#15); the crossed pinch's ±k is not an inverse (#17, left to the author); drops are excluded by nature — an exact expansion pushes ink off the canvas, which no mass observable can tell from loss (#13, #39).

**The exactness rule** *(the v3.0 arc, KESSHO §2.1; transcribed at the fold)*. A kick ρ₁ = ρ − ∂V/∂θ is det-1 only if V depends on θ alone; a shear u₁ = u + F(v) only if F ignores u. Any spatial window on the coordinate being kicked yields det J = 1 − (window slope)·(kick). Twist maps — a rotation by an angle that depends on the radius — are the one family that is both exact and compactly supported, which is why the Lamb–Oseen swirl, the Chladni disc and the tine already work. Hence the rule every new operator is written under:

> **Exact + localised ⟹ twist-type, or generating-function (implicit, Newton in the shader), or a kick generated by a function of the angle alone.** Everything else is sub-stepped and must say so.

(The third family is the minerals delta's, 2026-10-14 — the crystal drop's theorem, §10.7.8: in the drop's action-angle frame ρ_src = ρ − R(θ)²/2 is a kick generated by V(θ) = R(θ)²/2, exact for any R(θ) at any amplitude; the spec's §2.1 sentence is updated at the phase fold.)

Kessho's z = 1 flatness test (§10.7.3) is the runtime check of the rule: `midi-sink --dev --kessho-flat` forces z = 1, flat for any exact field, and a windowed kick shows as texture. The class table's exact row gains the Arnold lock, the braid (a composition of exact factors) and the chord wave (route a); this arc adds nothing sub-stepped.

### 10.3 The five operators, as shipped
* **Wave torsion** (`SUMI_VORTEX_TORSION`, step 36, #18–#20): θ′ = θ + A·sin(k·r − φ)·e^(−r/R), r′ = r — a third vortex profile (exact), reached through every vortex route; k and φ are controls (`SUMI_CTL_TORSION_K` / `_PHASE`, CC 104/105 by default); **the note-on sweep episode** (`params.torsion_sweep`): per-voice, φ advancing at 2π·1.5 rad/s, amplitude 1.2 rad/s·e^(−t/0.6 s), reach 3× the strike radius floored at 0.05, over after 4τ, per-frame deltas merged under the budget — the pattern the burst and the spark reuse. The **press feed** in Anod spends torsion at 1.2 rad/s at full pressure with its own phase clock (#51).
* **Chladni — the cells are the eddies** (steps 37 and the rework before 43, #23–#27, #59–#62; the roadmap's kick-drift lattice and the Taylor–Green flow were superseded): every display disc the shells draw (`sumi_layout_cells`: the key circles; the polar lattice for the fifths; imaginary circles for the rolls, scaled by `chladni_cell`, capped at the display size so discs never overlap) turns about its centre as a ring — θ(ρ) = θ·(4ρ²(1−ρ²))², still at the core and the rim, peak 0.727 at ρ = 1/√2 — through an RGBA16F index map at field resolution (four owner slots); the odd cells' sense from the balance (0 counter-rotate, ½ rest, 1 all the same way). `chladni_mode` 0 **discs** (exact) / 1 **Inverse Chladni** (the rings summed into one sub-stepped flow that may grow past the keys, `chladni_cell` to 1.5). The stir is banked and emitted only when it carries at least the field's quantum on the smallest disc (#61); its rate 1.5 rad/s at full control; **signed** — the bend's sense (#70). The author's ponderomotive derivation (the Kapitza averaging of a vibrating plate into a conservative Hamiltonian and its kick-drift map) is `specs/chladni.md` in git history — worth a paragraph in the book, not the spec.
* **Viscous multipole burst** (`SUMI_DEFORM_BURST`, step 38, #28–#33): the time-integrated displacement of an impulsive viscous multipole — Jaffer's Lamb–Oseen integration applied to the sin(mθ) family, elementary for m ≥ 2 (Φ_m(S) = (1/S)[1 − e^(−S)Σₖ₌₀^(m−2)(1 − k/(m−1))Sᵏ/k!]; the dipole's E₁ kernel excluded), the dual-time Gaussian core (ℓ² = a² + 4νt), D normalised from the peak lobe at r = a, the far field cos mθ/r^(m−1) then 1/r^(m+1) beyond ℓ (a precision on the draft). Class sub-stepped: peak displacement ≤ β_m × the pass's core (β₂ = 0.13). The gesture is a strike with a lifetime: `sumi_add_burst(x, y, a, D, θ₀, m)` starts an episode whose age grows to `burst_age`·a over `burst_life` seconds; m = 0 takes `burst_order` (2..8). The pinch is this burst's r → 0 limit. The literature check preceded any wording (`docs/evidence/step38/literature.md` in git history); the book page carries the lineage line and the author's signed note.
* **Spark shear** (`SUMI_DEFORM_SPARK`, step 39, #34–#39): a kick-drift shear — rows slide by A·w(y)·f(y), then columns by B·w(x₁)·f(x₁) — with f a stack of triangle waves at k, 2k, 4k, 8k (`spark_stack`) or piecewise-linear hash noise (`spark_profile`; an integer hash, bit-identical across backends), w a Gaussian window across each shear, the frame rotated by θ₀. **Exact for any profile** (the draft's "y₁ = y₁ + …" was a typo for y₁ = y + …). The episode: A = B = `spark_shear`·r spent as e^(−t/τ) over 4·`spark_tau`, dealt in kick-drift steps at the field's quantum, the window 2r, k from `SUMI_CTL_SPARK_K` (`slide_mode` 2 routes CC 74 to it). **The composed spark** `sumi_add_spark(x, y, r, D, θ₀, layer)` = the drop (the Joule blast: radial outflow cannot be divergence-free, the oldest operator solves it exactly) + the burst of core r and lobe D + the shear episode; sub-stepped by inheritance.
* **Chirikov standard map** (`SUMI_DEFORM_CHIRIKOV`, step 40, #40–#44): the kick y₁ = y + A·sin(k(x−x_c)+φ) then the ε-scaled drift x₁ = x + ε·(y₁ − y_c), K = A·k·ε; exact, the inverse undoing the drift first. The route (`SUMI_CTL_CHIRIKOV_K`, CC 109; the mod wheel in Anod) is **delta-driven**: a throw of δ is one step at δ²·`chirikov_kmax`, capped per step at the core's ceiling `SUMI_CHIRIKOV_K_CEIL` = 1.25 (the last K where erosion after the flush holds, above Greene's 0.9716) with the remainder following; a negative δ the exact inverse. The erosion sweep found the drift flushing rotating orbits off a non-wrapping canvas at every K (#43) — the operator's geometry, not resampling. **Open (the author, #89; carried past 2.0, §16):** the feel — "the vibrations are tiny and the displacement is huge; I was hoping for the contrary"; a parameter grid may follow.

### 10.4 The Anod composite (step 42–43, #50–#58, #69)
**Read the field's strain, not its phase bands.** The charge glows 0.22 + 0.78·(1 − e^(−σ/`anod_glow`)), σ = sqrt(‖J‖_F² − 2) from the stored coordinates by a one-sided-min finite-difference estimator that survives the half-float staircase (the stencil widens with the field, the rounding bias is subtracted; #56) — the charge phase bands the filament, aux drifts its hue. **Water never glows by strain: it draws the field's deformed grid** — iso-lines of position + gain·displacement (gains −4 along x, +3.4 along y) from window-averaged displacements, shown where the water has moved by more than a texel, faded where the local pitch would alias (#57); `anod_pitch` is the grid's pitch at rest (default 1/144, 0 = no grid, #58). **The seam mask is found, not stored** (#52): a texel at its own identity coordinates within one ULP with no phase is fresh water, and a charged texel beside it reads no strain. Substrate: near-black glass (`anod_dark`, default 1) with the washi's screen-locked grain as phosphor speckle (`anod_grain`, 0.5) — the §4.5 invariant holds by construction. **The bloom** (#69): the Anod composite rendered once more in linear light at half resolution, the emission above a 0.04 threshold blurred over `anod_bloom_levels` octaves (13-tap down, tent up) and added back times `anod_bloom`, then a per-channel shoulder toward white; prints and exports bloom the same. The author's look: glass 1, grain 0.5, glow scale 0.20, bloom 0.75 over 3 octaves. Three Anod palettes (electric blue, plasma orange, phosphor green) under the same ids; a custom palette reads as a glow (gradient by glow = the core, the accent = the halo).

**Prints** in Anod are the same readback, medium-styled (#54); the export ABI (§11.4) writes them over alpha on request. The `[ITERATE: long-exposure / strain accumulation buffer]` is carried past 2.0 with its question (§16; the Anod page says "Not in 2.0").

### 10.5 The default binding tables, as shipped (#51, #70–#72, #88)

| Dimension | Sumi default | Anod default | Override |
|---|---|---|---|
| strike (note-on) | the drop | **the spark**: a small charge of radius·`anod_drop` (default 0.33) and the shear episode with its band and kick on the full Sumi radius, so the charge is torn into streamers (#71; #88 had made it the classic spark on a 0.57 charge — a burst on the charge, the shear on the charge — for a renderer that lost the threads under the coordinate payload; back to #71 at Phase 8 step 55b, `DECISIONS_7 #5`, the displacement payload carrying them; `burst_order_by_class` stays in the ABI, unused) | `anod_drop`, `spark_shear` |
| press feed (0xD0) | boundary growth (`press_mode` 0) | the torsion sweep feed (`press_mode` 2) | `press_mode` 0 / 1 |
| poly pressure (0xA0) | the Lamb–Oseen swirl | the torsion's and the spark's wavenumbers from mid-range up (k = ½ + ½·pressure), held while pressed and given back at release, each unless the bend or the slide owns it (#70, #72) | a CC shares the slot, last writer wins |
| per-note bend | glide (`bend_mode` 0) / ripple (1) | **the Chladni stir** (`bend_mode` 4): the bend's distance the rate (±1.5 semitones saturate), its sign the sense; stilled when the last bent note lifts or the mode flips away (#70, #72) | `bend_mode` 0–3 |
| CC 74 (slide) | aux hue (`slide_mode` 0) / pinch (1) | the spark's frequency (`slide_mode` 2) | `slide_mode` 0 / 1 |
| mod wheel / breath | the vortex | the Chirikov throw (the vortex quiet) | the CC map |
| master bend | the shear tine | the shear tine (it composes with the scroll already) | — |

`SUMI_MODE_MEDIUM_DEFAULT` (255) is the default of the three modes and resolves to the medium's column; an explicit mode is the user's override. **A mapping is a handle, not a claim** (#72): the desktop maps CC 104–109 to these dimensions by default, and a mapped, silent CC never owns one — last writer wins.

### 10.6 The marble gestures follow the medium (#75)
Five core calls, `sumi_gesture_tap / _pinch / _twist / _press / _press_end`, each reading `params.medium`; in Sumi each is exactly the 1.0 operator call (bitwise); in Anod: tap = the strike (§10.5's — the charge and the shear on the full radius since 55b — along the layout's pitch axis at the touch), pinch = the burst (the squeeze accumulates, a burst per 0.06 of it, core a quarter of the finger span), twist = the torsion vortex, long press = the torsion feed (pull = the Chladni stir, reversed; `_press_end` stills it). The comb and the pen's wake are the same in both media. Every shell calls them.

---

### 10.7 Kessho — the third medium *(AS SHIPPED at step 71, 2026-10-14 — `DECISIONS_10 #3`–`#5`; the minerals at step 71b, `#6`–`#8`)*
Lineage: Frocht's photoelasticity; the Lamb–Oseen swirl of Jaffer's extension (arXiv:1810.04646) reused as the Arnold twist (§10.8.1).

#### 10.7.1 What each medium reads of J
Polar-decompose the per-texel Jacobian of the stored inverse-lookup field, J = R_φ·U(ψ, λ), U symmetric, det U = 1, principal stretches λ ≥ 1 ≥ λ⁻¹ along the axis ψ. J is the inverse map's Jacobian, so JᵀJ is the forward map's left Cauchy–Green tensor — Eulerian, what a polarizer on the screen sees; tr J is conjugation-invariant and equal for the forward and the inverse map when det J = 1, so the regime partition below does not depend on which map is stored.

| Observable | Formula (finite differences, the §10.4 stencil) | Sumi | Anod | Kessho |
|---|---|---|---|---|
| pre-image (u, v) | sampled | ✓ | – | – |
| stretch λ, as σ = λ − λ⁻¹ | σ = sqrt(‖J‖²_F − 2) | – | ✓ | ✓ (retardation) |
| principal axis ψ | ½·atan2(2(J₁₁J₁₂ + J₂₁J₂₂), (J₁₁² + J₂₁²) − (J₁₂² + J₂₂²)) | – | – | ✓ (isoclinics) |
| rotation cos φ | tr J = cos φ·(λ + λ⁻¹) | – | – | ✓ (the trace discriminant, the caustic) |
| handedness sign φ | w = (J₂₁ − J₁₂)/2 | – | – | `[ITERATE]` an optional hue chirality |

**The honest headline.** Rigid rotation has C = I: ψ is undefined and σ = 0, so Kessho's birefringence is dark exactly where Anod is dark. The Maltese cross forms in the differential-rotation annulus of a vortex (where Anod already glows), not on the rigid core. What Kessho adds over Anod is orientation (ψ) and rotation (tr J); the second is the only new observable for elliptic motion. Kessho's retardation IS Anod's σ: λ − λ⁻¹ = sqrt(‖J‖²_F − 2) for det J = 1.

#### 10.7.2 Layer A — photoelastic birefringence
I_A = sin²(2(ψ − α)) · backlight · ML(Γσ) — the isoclinic brush times the Michel-Lévy colour, Γ = `kessho_retard` nm per unit strain (default 2000: the first-order red at σ ≈ 0.27), σ Anod's estimator exactly, ψ the principal axis of C = JᵀJ from the same short stencil. **The LUT is the palette (`#3`, the `[ITERATE]` resolved):** one 1024-texel RGBA16F table over the retardation 0..2200 nm, built by the engine on the CPU when its inputs change and uploaded when its version moves — the same bytes on every backend. Palette 0, "Michel-Lévy", is the physical chart: `tools/kessho_lut.py` integrates sin²(πΓ/λ) against the CIE 1931 2° matching functions (the Wyman–Sloan–Shirley fit) under a 6504 K Planckian backlight, white-balanced, into `core/src/kessho_lut.h` (8-bit sRGB, regenerated bit-identically). The eleven stylised palettes (smoked quartz, calcite, nematic, amber, tourmaline, olivine, labradorite, fluorite, cobalt & amber, viridis, cividis) and the custom slot are `sumi_palette_t` gradients read PERIODICALLY in the order Γσ/550 nm — the first order runs the gradient once, the next orders repeat it fading toward their own grey as the chart's colours do, the first 200 nm darkened so a resting crystal is dark under every palette; the morph ring mixes the two slots texel-wise in the one table; the editor edits stops as before, the hue drift is unused (Kessho reads no aux), the accent is the caustic's light. The three-channel sin² form is not used anywhere (the web runs the same table). The polarizer angle α = ctl·π/2 over `SUMI_CTL_POLARIZER_ANGLE`: the control's range is one period and its end is its start (the top wraps to 0, so α = 0 and α = π/2 render bitwise the same); ownership is the A/B item K-1 (§10.7.5). The §10.4 long-exposure strain buffer (§10.10, step 90) will serve this layer unchanged; it reads instantaneous σ now. **A limit (`#5`):** Layer A lights the water by strain, the case Anod avoided — the far field's small smooth strain reads the half-float staircase as a frosted speckle round every swirl; in character for calcite, `[ITERATE]` the trace's wide window for ψ and σ if the author wants it smooth.

**Amended by the minerals delta (2026-10-14, `KESSHO §4`, step 71 amended / 71b):** a MINERAL replaces the palette. Layer A's inputs are the field's (ψ, σ) and a mineral descriptor (§10.7.8): the Michel-Lévy table stays the one physical LUT; a mineral sets its retardation scale Γ, its bias Γ₀, its optic class and the optional laws of §10.7.8; the eleven stylised gradients of the first landing are retired (a stylised chart survives only as `lut_id` ≥ 1 for a mineral that asks for one — none at ship). The substrate is the mineral's body (`body_tint` as the backlight through it), frosted, screen-locked; the bloom is gated to Layer B only — the fringes never bloom.

#### 10.7.3 Layer B — the trace discriminant and the Lagrangian caustic
Δ = (tr J)² − 4 partitions the canvas: elliptic (Δ < 0 — vortex cores, Chladni eddies) a cool tint, calm; hyperbolic (Δ > 0 — pinches, wakes, Chirikov chaos) warm; parabolic (Δ = 0) the order/chaos boundary. The caustic is the refraction of the stored displacement d = (u − x, v − y) onto a bed at depth z; with det J = 1 the determinant collapses to one line:

det(I + z∇d) = 1 + z(1 − z)(tr J − 2),   I_B = 1 / (|det(I + z∇d)| + ε).

Consequences (they replace the first draft's "fold at |tr J| = 2"): **The fold is an iso-trace contour, not the horizon:** zero at tr J = 2 − 1/(z(1 − z)); for z ∈ (0, 1) only tr J ≤ −2 folds (the flip regime, rare on a marbling canvas; z = ½ hits exactly −2); for z > 1 the fold sits on tr J = 2 + 1/(z(z − 1)) and sweeps from +∞ toward 2⁺ as z → ∞, so the breath or the mod wheel scans a level set of tr J through the hyperbolic regime and approaches the parabolic horizon asymptotically. **As shipped (`#3`):** tr J for this layer comes from the wide window the Anod grid reads (±3 texels at 512, the staircase averaged along each axis), the tint is cool or warm by smoothstep(0.05, 0.8, |Δ|) with the noise floor under 0.05, and the caustic is the contour drawn as a LINE — screen-space, a fragment and a half wide (fwidth), over the soft 1/(|den| + ε) form with the rest value subtracted so resting water is dark; `SUMI_CTL_CAUSTIC_DEPTH` → z = 1.05·(8/1.05)^ctl (log-spaced 1.05..8), the mod-wheel dimension under Kessho's table (whichever of the two is higher); the upper bound `[ITERATE]` by eye — at z ≈ 6 the lines are crisp and the soft term lights the staircase of strongly strained water (`#5`). **"z = 1 is flat" — corrected (`#4`):** the sentence holds in the continuum, and the composite's collapsed form 1 + z(1 − z)(tr J − 2) *assumes* det J = 1 (it is flat at z = 1 for any field, which is what the class declaration promises); a sampled half-float field cannot certify det J to 1e-4 — `--kessho-flat` measures the full determinant by differencing the pre-image and reads a floor of a few thousandths with the drops' rims (the inverse of an exact expansion is singular at its rim) owning everything above it, the exact and sub-stepped families not separable at the soak's amplitudes. The dev gate keeps the identity (0 exactly), its CPU controls (an exact twist 1.4e-3, the first draft's windowed kick 50 — red) and each operator's typical texel within 0.1 of det 1; the operator's class stays declared and proven by the four-part conservation gate, never discovered here. The author's spec text is flagged: a diagnostic of gross non-conservation, not a certificate.

#### 10.7.4 Substrate and composite
Backlit frosted calcite micro-facets, screen-locked (the §4.5 invariant; the same grain test as Anod under a 120 BPM scroll, bitwise). As shipped: a fixed dark tone (0.012 / 0.013 / 0.018, as Anod's glass is fixed, so a custom palette's clear tone never lights the sheet) with the washi's screen-locked grain as the facets (`kessho_grain`). Composite order: substrate → Layer B tint (elliptic / hyperbolic by Δ) → Layer B caustic (additive) → Layer A (the backlight's light through the crystal, additive) → the bloom and the shoulder shared with Anod (the knob is Anod's `anod_bloom`; under its 0.75 the crystal clips toward white — the author settles it, `#5`). A pure `composite.glsl` reader over the same (u − x, v − y, ink, aux) field: switching Sumi ↔ Anod ↔ Kessho leaves the field bitwise (the §10.1 gate reused, gate 1). Export: `SUMI_EXPORT_KESSHO_CAUSTIC` (Layer B alone over alpha, up to 8K) and `SUMI_EXPORT_ANOD_ALPHA` (the crystal's light, both layers, over alpha); without a flag a Kessho export is the full composite, bitwise the dip's print — all through the print readback path.

#### 10.7.5 A/B item K-1 — polarizer ownership (both built; decided by viewing at step 71)

| | A: a global α | B: per-voice α_k |
|---|---|---|
| source | `SUMI_CTL_POLARIZER_ANGLE` (a CC), or the last-touched voice's slide | each held voice's slide (CC 74) → α_k |
| field | constant | α(x) = the angle-blend of the nearest ≤ 4 active voices, inverse-distance weights, blended as the vector (cos 4α_k, sin 4α_k) so the π/2 period wraps; the global α where no voice is near |
| reads as | one crystal under one analyzer; whole-canvas rotation on a master gesture | each vortex owns its crystal; sliding one finger rotates one cross |
| cost | 0 | a small uniform array (≤ 15 voices) + the blend in the composite |
| the one-consumer rule | the slide stays with its Sumi/Anod owner | slide → α_k is Kessho's `slide_mode` 3 consumer |

As shipped: both built — `params.polarizer_mode`, `SUMI_POLARIZER_GLOBAL` (0, the default) / `_PER_VOICE` (1); mode B blends the nearest four held voices by inverse-square distance (a voice's reach 0.03 canvas heights) over the global as the floor (0.35), the vectors (cos 4α_k, sin 4α_k) so the period wraps. The same gesture script and MIDI phrase rendered in both (`docs/evidence/step71/kessho_ab/`), THE DECISION PENDING THE AUTHOR'S VIEWING (`#5`); the loser stays in the enum as a non-default mode unless it is a maintenance burden.

#### 10.7.6 The default binding table — the Kessho column (extends §10.5)

| Dimension | Sumi default | Anod default | Kessho default |
|---|---|---|---|
| strike (note-on) | the drop | the spark charge + the shear episode | a refractive lens drop + an initial caustic ring |
| press (0xD0) | boundary growth (`press_mode` 0) | the torsion feed (2) | Arnold K (`press_mode` 3) — Ω and q from pitch against the bass |
| per-note bend | glide tine / ripple | the Chladni stir (`bend_mode` 4) | braid consonance (`bend_mode` 5) — bends move C_k, flipping saddle ↔ dipole |
| CC 74 (slide) | aux hue / pinch | the spark's frequency (`slide_mode` 2) | polarizer α (`slide_mode` 3) — global or per voice, per K-1 |
| poly pressure (0xA0) | the Lamb–Oseen swirl | torsion and spark wavenumbers | chord-wave amplitude A_k |
| mod wheel / breath | the vortex | the Chirikov throw | caustic depth z — scans the iso-trace fold through the hyperbolic regime |
| sustain (CC 64) | paper dip | paper dip | paper dip |

`SUMI_MODE_MEDIUM_DEFAULT` (255) resolves to the medium's column as before; an explicit mode is the user's override; a mapping is a handle, not a claim (§10.5). **Interim, as shipped at step 71 (`#3`):** the strike is the drop (a lens whose rim Layer B draws as a ring — nothing new was needed); the press is the Lamb–Oseen swirl (`press_mode` 1 — the twist the Arnol'd lock is built on; the ink feed's nested rings read as Sumi's) until step 72 hands it the lock; the bend glides until step 73's braid; poly pressure is the swirl until step 74's chord wave; the slide and the mod wheel are as the table says. The reserved values `press_mode` 3 and `bend_mode` 5 are accepted and resolve to these interim defaults until their steps land.

#### 10.7.7 How the three media read one performance *(KESSHO §3)*

| Property | Sumi | Anod | Kessho |
|---|---|---|---|
| reads of J = R_φ U(ψ, λ) | the pre-image (u, v) | λ (as σ) | λ, ψ, cos φ |
| lights up | pigment rings | stretching filaments | rotating Maltese crosses in the shear annulus, Michel-Lévy fringes, iso-trace caustic folds |
| rigid rotation | spiral bands only via differential rotation | dark | dark in Layer A; an elliptic glow in Layer B |
| substrate | washi | black glass + phosphor | frosted calcite, backlit |
| default chord gesture | drops + glide tines | spark shear + the Chladni stir | the braid + the Arnold lock |
| built-in diagnostic | – | – | z = 1 flatness = the det-J gate |
| diagnostic views (the minerals delta) | none | none | z = 1 flatness; **Fluorite** (Γ = 0: Layer B alone); **Selenite** (Γ₀ = 550 nm: the sign of strain) |

#### 10.7.8 Minerals and crystal habits *(the minerals delta, 2026-10-14 — `KESSHO §4`; step 71 amended and step 71b; lineage: Hamilton 1832 — conical refraction, the first use of what became symplectic geometry, for crystal optics; Frocht; Baumhauer 1879)*

**The split.** A mineral has two halves in two shaders. Optical (`composite.glsl`, a pure reader): how light reads the field — switching minerals leaves the field bitwise (the §10.1 gate extends to every mineral). Mechanical (`deform.glsl`, exact operators): how the crystal *admits* a strike and a stroke — the drop's habit R(θ) and the tine's cleavage profile F(y), which change the field and carry §10.2 gates. The halves are independent parameters (a Calcite optic over a Barite rosette is allowed); the presets pair them as nature does.

**The descriptors.** Optical — a SIZED struct, additive by size, the editor edits it, presets fill it:
```c
typedef struct {
    uint32_t size;              /* = sizeof(sumi_mineral_t); the versioning                                   */
    uint32_t optic_class;       /* SUMI_OPTIC_ISOTROPIC 0 | _UNIAXIAL 1 | _BIAXIAL 2                            */
    float    retardation;       /* Γ: nm of retardation at σ = 1 (Δn × exposure) [ITERATE] per preset          */
    float    retardation_bias;  /* Γ₀: the sensitive-tint plate, nm (0 none; 550 selenite)                     */
    float    two_v;             /* biaxial: the half-angle V, rad (ignored unless _BIAXIAL)                     */
    float    walkoff;           /* δ: the double-image split in texels at σ = 1 (0 none)                        */
    float    dispersion;        /* Cauchy: (z_B / z_R) − 1; z_G the geometric mean (0 none)                     */
    float    film_n, film_r;    /* Airy thin film over the ink channel: index, Fresnel amplitude (r = 0 none)   */
    float    conical_ring;      /* the ring kernel's radius in texels on the fold (0 none)                      */
    float    body_tint[3];      /* linear RGB: the backlight through the body (the substrate's colour)          */
    float    bloom_gain;        /* Layer B only                                                                 */
    uint32_t lut_id;            /* 0 the physical Michel-Lévy table; ≥ 1 a stylised chart                      */
} sumi_mineral_t;
enum { SUMI_OPTIC_ISOTROPIC = 0, SUMI_OPTIC_UNIAXIAL = 1, SUMI_OPTIC_BIAXIAL = 2 };
SUMI_API void sumi_set_mineral(sumi_instance_t*, const sumi_mineral_t*);   /* a pure composite change */
SUMI_API void sumi_get_mineral(sumi_instance_t*, sumi_mineral_t* out);
SUMI_API bool sumi_mineral_preset(uint32_t id, sumi_mineral_t* out, const char** name);
enum { SUMI_MINERAL_QUARTZ = 0, SUMI_MINERAL_CALCITE, SUMI_MINERAL_MUSCOVITE, SUMI_MINERAL_BARITE, SUMI_MINERAL_FLUORITE, SUMI_MINERAL_SELENITE, SUMI_MINERAL_COUNT };
/* Mechanical: four fields appended to sumi_params_t, read at strike / stroke time */
enum { SUMI_HABIT_CIRCLE = 0, SUMI_HABIT_RHOMB = 1, SUMI_HABIT_HEX = 2, SUMI_HABIT_ROSETTE = 3 };   /* params.drop_habit  */
enum { SUMI_TINE_SMOOTH = 0, SUMI_TINE_TWIN_KINK = 1, SUMI_TINE_TERRACE = 2 };                     /* params.tine_profile */
/* params.habit_roundness (0 sharp .. 1 circle), params.habit_blades (the rosette's m) — habit 0 and profile 0 are BITWISE the v2 drop and the v2 tine */
```
*As shipped (`DECISIONS_10 #6`): `sumi_mineral_preset` fills the OPTICAL half only; the pairing of a mineral with its habit and profile is the shells' (the desktop's `app_mineral_preset`) and the six `presets/kessho_<mineral>.json`, which carry both halves through the serializer's `mineral` block. `sumi_mineral_t` is 60 bytes.*

**The symplectic crystal-drop theorem (ratified).** In action-angle coordinates about the centre, ρ = r²/2 and θ (dx∧dy = dρ∧dθ), the v2 drop of radius r₀ is the translation ρ_src = ρ − r₀²/2. Replace r₀ by any profile R(θ) > 0: ρ_src = ρ − ½R(θ)², θ_src = θ — a kick generated by V(θ) = R(θ)²/2, the "kick depends on the angle only" case of §10.2's rule, so J = [[1, −RR′], [0, 1]] and det J = 1 exactly, for any R(θ), any amplitude, no sub-stepping. Four properties: (1) bijective for any R(θ) > 0 — each ray maps onto itself monotonically (r_src = √(r² − R²) for r ≥ R); star-shaped about the centre is the only condition and it is automatic; (2) boundary growth is free — scaling R → sR is the same map with V = (s² − 1)R²/2, so `press_mode` 0 grows a rhomb exactly *(as shipped, `DECISIONS_10 #6`: the feed's law is unchanged — the nominal radius grows linearly in the pressure integral, the area ∝ R²; the proposal's s = √(1 + p) is not implemented, there being no new code path)*; (3) the interior coordinate is r/R(θ) — the ink phase inside a crystal drop is assigned from r/R(θ), not r, so the rings are homothetic copies of the habit; (4) corners crease — where R′ jumps, J jumps: the bands outside an unregularised polygon show a crease along each vertex ray, which Kessho draws as a birefringence line and reads as cleavage; `habit_roundness` regularises it, zero allowed. Habit profiles (in the drop's own frame, rotated by the strike's angle): RHOMB — the calcite cleavage rhomb, face angles 78.1° / 101.9°, the polar support form R(θ) = min_i d_i / cos(θ − φ_i) over the faces with cos > 0, roundness a log-sum-exp soft-min `[ITERATE]` by eye; HEX — the regular hexagon (muscovite's pseudo-hexagonal sheet), the same construction; ROSETTE — m tabular blades, m = `habit_blades`; *as shipped in `drop_fs`* R(θ) = r₀·(1 + 0.6·cos⁸ mθ) / (1 + 0.6·35/128), r₀ the mean radius (the proposal's r₀·(1 + β cos^{2p}(mθ − θ₀)) with β and p free; the shader fixed β = 0.6, p = 4, and every habit is mixed toward the circle by the roundness).

**Cleavage as tine profiles (ratified with a fix).** A shear x_src = x − F(y) in the stroke's frame is exact for any F(y). The first draft's profiles were monotone — the twin ramp displaces the two half-planes against each other, the staircase is a global shear plus a wiggle — and either pulls texels in from outside the canvas, failing the soak at its first part *(as measured, `DECISIONS_10 #6`: an odd profile reversed is the same shear, so the (+z, −z) "pair" is not a pair and carries the ink off the sheet — −99.9 % in 500 pairs; the bench keeps the fault as profile 3, dev only, `--tine-fault`)*. The fix keeps exactness and costs nothing: F(y) is a BUMP, zero outside |y| < H, i.e. a tine profile. TWIN_KINK (Baumhauer): F(y) = γ·pwl(y), a piecewise-linear bump (up at slope γ over the lamella, flat, down) — the shear concentrated in two kink lines where J jumps and Kessho draws two sharp lamella edges. TERRACE (phyllosilicate slip): F(y) = A·stair_N(y)·w(y), stair_N(y) = ky − sin(2πNky)/(2πN) (monotone, N flat terraces), w a C² window to zero at ±H. SMOOTH is Jaffer's α/(α + d), bitwise the v2 tine. The profile is a property of the tine: `bend_mode` 0 (the glide) and the ripple inherit it; no new operator.

**Optical laws by optic class.** Common to all: Γ_eff = Γσ + Γ₀ → the Michel-Lévy LUT; Layer B as §10.7.3. Isotropic (Fluorite): Γ = 0 — Layer A dark, the medium shows Layer B alone, a diagnostic view as much as a mineral. Uniaxial (Quartz, Calcite, Selenite): the brush sin²(2(ψ − α)); WALK-OFF (Calcite): *as shipped* the CHART read twice, at x ± w, w = δ·σ·|sin 2(ψ − α)|·ê_ψ texels, under the point's own brush — the split is closed on the dark arms (the isogyres stay sharp) and where the sheet rests (`DECISIONS_10 #6`; the proposal's ink-layer sampling and cos² : sin² blend are not what ships — Layer A reads no ink); the caustic is not doubled. Biaxial (Muscovite, Barite): the brush is replaced by a two-axis law — *as shipped* |cos² 2V − cos² 2(ψ − α)| / max(cos² 2V, sin² 2V), the Maltese cross splitting into two brushes 2V apart (the isogyres of a biaxial figure separating), kept at the analyzer's π/2 period because the per-voice analyzers carry (cos 4α, sin 4α) and a π-periodic law has no sign to read from them (`DECISIONS_10 #6`; the proposal's product |sin(ψ − α + V)·sin(ψ − α − V)| and its lemniscates are not what ships); stated as a STYLISATION of the conoscopic figure, not Biot–Fresnel (that law lives in angle space, which a texel does not have). THIN FILM (Muscovite): the ink channel read as sheet thickness h through Airy's R(Λ) = 4r² sin²(δ/2) / ((1 − r²)² + 4r² sin²(δ/2)), δ = 4π n h cos θ_s / Λ_RGB, cos θ_s from σ (the sheet's tilt), three evaluations per texel *(as shipped: h = 200 + 600·fract(phase) nm on ink texels, Λ = 610 / 550 / 465 nm, cos θ_s = (1 + σ²)^{−½}, added at half weight)*. DISPERSION (Barite; any mineral may set it): three caustic depths z_R < z_G < z_B — three offset iso-trace folds, a rainbow on the fold for two extra scalar evaluations. CONICAL RING (Barite): where |Δ| < ε the caustic is read on a small ring (N = 8 points, radius `conical_ring`) and averaged — Hamilton's hollow cone on the fold; masked tight (ε = 0.15 and σ > 0.05 as shipped), the one moderately expensive law. *Also as shipped: the dispersion's depths are z/√(1 + d), z, z√(1 + d); the chart fades to high-order white over 2200–6200 nm; the substrate is 0.018·body; the caustic's light is the body; the bloom's source is Layer B × bloom_gain.* SENSITIVE TINT (Selenite): Γ₀ = 550 nm puts the unstrained field at first-order magenta; the sign of a shear strain shifts it toward blue or yellow — the sign of strain becomes visible, which the plain chart cannot show. *As shipped: the plate is fixed to the sheet at 45° — Γ_eff = |Γ₀ + Γσ sin 2ψ| — and replaces the brush (a full-wave plate floods the cross), so the analyzer's turn plays no part under a plate mineral (`DECISIONS_10 #6`).*

**Presets at ship (six, the library open; a preset is a filled `sumi_mineral_t` plus the two habit fields):**

| id | Mineral | Optic | Γ ratio (Δn, physical) | Laws on | Habit | Tine | Body |
|---|---|---|---|---|---|---|---|
| 0 | Quartz | uniaxial | 0.009 (the reference: step 71 as captured) | — | hexagon | smooth | clear |
| 1 | Calcite | uniaxial | 0.172 | walk-off | rhomb | twin-kink | cold white |
| 2 | Muscovite | biaxial, 2V 40° | 0.040 | thin film | hex | terrace | champagne |
| 3 | Barite | biaxial, 2V 37° | 0.012 | dispersion, conical ring | rosette, m = 5 | smooth | honey |
| 4 | Fluorite | isotropic | 0 | — | hexagon (the cube along its diagonal) | smooth | violet-green |
| 5 | Selenite | uniaxial | 0.010 | Γ₀ = 550 nm | rhomb (gypsum's cleavage plate) | smooth | clear |

*As shipped after the author's viewing (`DECISIONS_10 #8`): the closest real habit per mineral, every one at roundness 0, to be reviewed at the phase's end with the new operators (the delta's circles for quartz, fluorite and selenite are not what ships).*

The ratios between Γ values are physical (Δn); the common exposure that maps them to the chart is one `[ITERATE]` number per preset (Calcite sits in high-order white by physics — thick spar does).

**Deferred:** the Suzu conical intersection (`Mineral.md` §2.C.3) — a 2×2 coupled-mode cell with (p, t) → ω± = ω₀ ± √((κ₁p)² + (κ₂t)²), the Berry π flip around the diabolical point; correct as maths, parametric-drive class (the coupling is time-varying), the sign flip inaudible without a second voice to interfere with; the double cone in the trace its only payoff → Phase 13, `[ITERATE]`.

### 10.8 Symplectic harmony — the coupled multi-voice operators *(AS SHIPPED, steps 72–74, 2026-10-14 — `DECISIONS_10 #9`–`#11`; KESSHO §2)*
Lineage: Arnol'd's circle map and tongues; Kirchhoff–Helmholtz point vortices; Lie–Trotter / Strang splitting; Sethares and Plomp–Levelt for consonance.

#### 10.8.1 `SUMI_DEFORM_ARNOLD` — the Arnol'd tongue intonation lock (step 72)
**Class exact.** Local action-angle coordinates about the drop centre: ρ = r²/2 (so dx∧dy = dρ∧dθ), θ. One iteration per frame = twist ∘ kick.

1. **The kick,** from the generating function F₂ = θρ₁ + (K/q)·W(ρ₁)·cos qθ: ρ₁ = ρ + K·W(ρ₁)·sin qθ, θ₁ = θ + (K/q)·W′(ρ₁)·cos qθ. W(ρ) a smooth window — *as shipped (`DECISIONS_10 #9`): ZERO at the centre, a C² rise to 1 over [0, ρ_drop/2] (W = 1 at the centre sends material to negative action), 1 to ρ_drop, a C² fall to 0 by 1.5 ρ_drop*, W′ bounded. The inverse lookup is one scalar Newton on θ (initial guess θ₁; *FIVE iterations as shipped — three leave 1e-3 at the bound*), then ρ explicit. Contraction requires |K W′| < 1 ⟹ *K_max = 0.8 / max|W′| as shipped (the θ-shift's derivative is −K W′ sin qθ: the proposal's factor q is not there; at 0.8·q/max W′ the kick folds for every q ≥ 2); the kick is the press as a fraction of K_max*.
2. **The twist is the existing Lamb–Oseen swirl,** its circulation chosen so the angular advance at the ring radius is Ω(ρ_ring) = Ω₀ turns per frame, with slope τ = dΩ/dρ·ρ_ring. Ω₀ = (f_voice/f_bass) mod 1 from the normalizer (live, bend included); *as shipped: the core at the ring (r_c = R, the swirl voice's convention, τ ≈ −0.42 Ω₀), C²-windowed to zero between 1.75 R and 2.5 R (the bare tail would wind the whole sheet), composed with the kick in ONE lookup; the bass voice keeps the swirl (its press stirs — it has no interval); an integer ratio is q = 1 and twists a full turn plus its detune.* The twist is mandatory: *as measured, not because the petals grow without bound — the window bounds the kick either way — but because without it the kick alone has no islands: its fixed lines are its nodes and the ink collects on them (the bench's red); the twist makes the elliptic points*; islands of width Δρ ∝ sqrt(K/τ) need τ ≠ 0.

q selection: the nearest continued-fraction convergent p/q of Ω₀ with q ≤ q_max = 6 and |Ω₀ − p/q| < 1/(2q²); outside every tongue q = q_max *(as shipped: the convergents of order 2..6 — 0/1 and 1/1 are not tongues, they would swallow every interval; q = 1 is the integer ratio alone)*. K crossfades over 50 ms `[ITERATE]` when q changes so petals do not pop *(as shipped: K to zero and back, q switched at the zero; the records' continuity is a headless gate)*. Precession: off-lock the islands precess by δ = Ω₀ − p/q turns per frame — the beat in bass-period units, scaled by frame rate / f_bass; `[ITERATE]` a slow-motion factor if 60·δ turns/s reads as flicker on wide intervals. *As shipped: the kick's frame φ precesses by 2πδ a frame while the twist turns p/q + δ, so the islands stand in the precessing frame and turn at δ in the lab — the true circle map would lock a small detune into a radius shift and hide the beat. A MIDI fifth is equal temperament's, two cents under 3:2: its islands turn once in 600 frames; the tempered third, fourteen cents over 5:4, six times faster — the temperament's beats on the water.* MIDI binding (the Kessho default): press (0xD0) → K; Ω₀ and q from pitch *(press_mode 3; SUMI_CTL_ARNOLD_K the CC alternative, the larger of the two)*. Gesture route: `sumi_add_arnold(x, y, radius, winding_omega, k, resonance_q, twist)` *(k a fraction 0..1 of K_max; q 0 = the tongue of winding_omega; twist 0 = none — the red control)*. The twin is the Suzu kicked rotor (§13.2): the same (K, Ω) semantics on purpose; a Suzu circle-map voice is out of scope for v3.

#### 10.8.2 `SUMI_DEFORM_CHORD_WAVE` — the chord standing-wave lattice (step 74)
**Class exact (route a).** Per held voice k: the axis ê₁(k) at the pitch class's angle on the circle of fifths, the wavenumber ω_k ∝ octave; coordinates (s₁, s₂) along (ê₁, ê₂). One ripple-tine per voice, s₁′ = s₁ + A_k·sin(ω_k s₂ + φ_k)·w(s₂ − s_{2,k}), windowed ONLY in s₂ — a sinusoidal tine that spans the canvas along ê₁, exactly Jaffer's comb and v2's ripple. Voices compose as separate passes: one note parallel ripples; two a lattice where the bands cross; three and more the crossing region of three bands. MIDI binding: poly pressure (0xA0, per note) → A_k; no gesture route in v3 (the ripple's serves). *As shipped (`DECISIONS_10 #11`): the ripple pass with a window along s₂ (`win_c`, `win_w`; 0 = the v2 ripple bitwise) — no new pass type; the axis fifteen degrees a fifth-step (twelve distinct orientations across a half turn), the wavenumber four periods a canvas at octave 3 doubling an octave up (2..32), the window 2.5 drop radii, a crest at the note, 0.02 canvas at full pressure, delta-driven as the ripple bake; one band a voice.* `[ITERATE]` **route (b) — local rosettes:** a generating-function shear windowed in both coordinates, a 2 × 2 Newton per note per texel, exact and compactly supported — the only route to the quasicrystalline 3/5/6-fold rosettes of the first draft; prototyped only after route (a) has been seen on paper.

#### 10.8.3 `SUMI_DEFORM_BRAID` — Kirchhoff chord braiding (step 73)
**Class exact.** The ink is advected by the superposed stream function ψ(x) = Σ_k Γ_k ψ_LO(x − x_k; r_c). It is never integrated directly: composing the existing exact Lamb–Oseen maps, one per voice with per-frame circulation Γ_k·Δt, in Strang order (half-steps 1…N, then N…1) is a second-order splitting of the superposed flow; every factor is det 1, so the composition is det 1 exactly, and the hyperbolic saddle and the cat's-eye separatrix emerge in the composed field. No sub-step Jacobian gate, no pair merging: 15 voices = 30 passes, within the pass budget. **Circulation from consonance:** C_k = the consonance of voice k against the bass on a continuous curve (Sethares / Plomp–Levelt roughness over the first six partials, mapped to [−1, +1]: unison and octave +1, the minor second −1, the tritone ≈ 0), computed in `core/src/midi_normalizer.cpp` beside the voice table it needs *(as shipped, `DECISIONS_10 #10`: `sumi_consonance`, three anchors — the unison +1, the tritone 0 exactly, the minor second −1 — piecewise linear in the roughness, the register floored at C4; at C4 the fifth +0.68, the fourth +0.34, the major third ≈ 0)*; Γ_k = sign(C_k)·Γ_vel·(0.3 + 0.7|C_k|), `[ITERATE]` the curve and the floor *(as shipped: the floor made continuous — the line 1.7·C for |C| < 0.3 — so the tritone is crossed without a jump; an exact tritone carries no vortex; Γ_vel = 0.036 canvas²/s at full velocity)*; the bass takes Γ_vel with positive sign. Same-sign pairs braid (a co-rotating saddle between them); opposite-sign pairs jet (a dipole). Bending an interval through the consonance curve flips the topology continuously; `SUMI_CTL_BRAID_COUPLE` scales the non-bass circulations. MIDI binding (the Kessho default): `bend_mode` 5 — bends change C_k live; the couple on a global CC. No new gesture entry point: `sumi_add_vortex` already composes (the proposal's `sumi_add_braid` is dropped).

**A/B item B-1 — vortex kinematics (both built; `params.braid_mode`; decided by viewing at step 73):**

| | A: pinned | B: free (Kirchhoff) |
|---|---|---|
| vortex centres | the voice's layout cell, always | x_k integrated on the CPU under the N-vortex Hamiltonian H = −(1/4π)·Σ_{j<k} Γ_jΓ_k ln(‖x_j − x_k‖² + r_c²), implicit midpoint (symplectic; three fixed-point iterations; N ≤ 15) *— as shipped: r_c² = R_j·R_k per pair, the fixed point iterated to 1e-15 (three rounds leave the invariants at 1e-6), the positions in double; one vortex a voice (the first echo)* |
| visual | ink braids and jets between fixed keys | consonant dyads orbit each other; dissonant dyads translate as a dipole and carry their ink |
| the layout invariant | kept | broken by design: the vortex leaves the key; note-on re-anchors at the cell, note-off removes the vortex over the release |
| boundary | n/a | `[ITERATE]` a 5 % clamp for v3; image vortices later |
| cost | 0 extra | ~N² per frame on the render thread; zero on audio |
| conserved | – | H, ΣΓ_k x_k (the centre of vorticity), ΣΓ_k‖x_k‖² — all gated *(measured over ten minutes: 3e-9, 4e-16, 6e-16)* |

Free mode may survive as a non-default "dance" mode even if pinned wins.

*As measured (`DECISIONS_10 #10`): the saddle between a same-sign pair needs the cores apart — two overlapping cores (adjacent cells, cores of a drop's radius) co-rotate about one elliptic centre; the erosion gate reads ×10.9 the glide-tine control for a held dyad, the Strang composition's 2N full-sheet resamplings a frame, informational for the braid.*

### 10.9 The Poincaré strobe — the trace's second mode *(as shipped, step 75 — `DECISIONS_10 #12`; KESSHO §2.5; `trace/` and Voxo 0.17.0, no core change)*
Voxo keeps a clean **reference phasor at the normalizer's bass frequency** (bend included) — not a zero-crossing detector on the rendered signal, which is ill-defined for a Verlet chain or a bore. On each wrap (sub-sample interpolated) it emits the current (x_j, y_j) of every traced voice into the existing trace ring (zero allocation; the audio thread bit-identical with the strobe on or off). The drive clock of a Duffing, rotor or Boris voice may replace the bass phasor per voice. `sumi_trace_config_t` gains the mode: `SUMI_TRACE_CONTINUOUS` = 0 (v2.0's curvature-decimated segments), `SUMI_TRACE_POINCARE` = 1. On the canvas each return stamps a micro-drop (Sumi) or a micro-swirl (Anod, Kessho) at the voice's cell through the gesture ABI; on the `sumi_set_scope` overlay a dust point. 1:1 → one stationary point; 3:2 → a 2-point orbit; incommensurate → a dotted KAM ring; a high-K rotor → fractal dust. The tablets' *Synth trace* rows (DECISIONS_9 #11) gain the mode.

*As shipped (`DECISIONS_10 #12`): the bass is the lowest held voice's frequency, read on Voxo's side, bend included (the lowest sounding one when none is held); the phasor runs per sub-step in double along the block's glide line and at most 64 wraps a block are kept; the returns go into a strobe ring of their own per voice (256; the trace ring keeps its density), raw in the cell's units with the kind's aux pair, through `voxo_set_strobe` / `voxo_strobe_poll` / `voxo_strobe_hz`. The mode is `sumi_trace_config_t.mode` 0 / 1 / 2 — the third strobes a Duffing cell by its drive's phase wrap and the rotor by its kick (its returns are Chirikov's section), the other kinds by the bass. The ink route stamps a micro-drop (Sumi, r 0.003 × scale/0.25), a charge speck (Anod, an ink drop of 0.004 × scale/0.25 — the continuous trace too, one along the orbit at the same rate, since only charge glows there; the author's choice over a micro-spark) or a micro-swirl (Kessho: strength 0.06, core 3r, Lamb–Oseen) through the gesture hook, about 24 returns a second per voice by a fractional stride `[ITERATE]` (every return floods: 876 drops at a stationary point in 4 s composed into one of 0.18 canvas; an integer stride aliases a 2-cycle to one point); the scopes keep every return (the newest 128 a voice). Measured: 1:1 within 0.001 texel; the just fifth two points within 0.120 texel a tenth of a second, the equal-tempered one precessing 1.132 rad/s (1.128 from 1.955 cents); the +49.8 cent bend exact to 1e-5; the rotor's sweep charted — and the strobe's first finding: the rotor's section is the standard map only approximately (the retune's re-basing at each kick) and sinks into a period-4 cycle at K 2.5 within ten seconds; Phase 12's. The fuzz found two Voxo faults, fixed: a kind change over sounding notes, the flute above 6 kHz; and the bus gained a non-finite breaker.*

### 10.10 The strain buffer, the deterministic seed and the spark reading *(as shipped, step 76 — `DECISIONS_11 #1`; the carry-overs moved directly after Kessho, `DECISIONS_10 #13`; the spark reading from `ROADMAP_6_delta_phase_order_and_spark.md` §B.2)*
The §10.4 long-exposure strain buffer — σ accumulated over time, serving Anod's glow by its own question and Kessho's Layer A unchanged; the gesture route's run-to-run reproducibility (DECISIONS_7 #25) — the seed carried through the gesture ABI so an identical script yields bit-identical Anod and Kessho fields; the Chirikov feel grid (DECISIONS_5 #89), the author's choice recorded.

**The spark reading — `params.anod_glow_mode`** (0 plasma, v2 bitwise; 1 spark). Anod reads σ as a density: every stretched texel glows in proportion, soft edges, bloom — a plasma. The spark is another reading of the same field, composite-side, the field bitwise: **ridges, not sheets** — non-maximum suppression of σ across the strain axis ê_ψ⊥ (ψ from the §10.4 stencil shared with Kessho: one function, two readers), the survivors the finite-time stretching ridges, the performance's Lagrangian coherent structures; **sparse** — a transfer (σ_ridge/σ_ref)^γ, γ ≈ 4 `[ITERATE]`, a few percent of the canvas lit; **leader and afterglow** — the instantaneous ridge bright and white-blue, the strain buffer's accumulated ridge the decaying hot channel, dimmer and warmer, `params.spark_afterglow` seconds (default 0.6 `[ITERATE]`; the buffer is the afterglow, nothing new stored); **corona** — the bloom on the leader only; **flicker** — the ridge threshold modulated in time by the screen-locked grain's value at the texel (§4.5 untouched); **branches** — the threshold against σ + κ·grain, κ = `params.spark_branch` ∈ [0, 1], grain crossing a smooth ridge fragmenting it into spurs (lighting, not geometry: a det-1 field's ridges fork only at saddles). Three fields appended to `sumi_params_t`, additive, no CTL. Whether ridges light on water or only on charge is the step's to state. The Lichtenberg operator — the note-on spark torn along a dielectric-breakdown tree from the seed, class sub-stepped — is recorded, 76b or 4.0.

*As shipped (`DECISIONS_11 #1`): the seed is `sumi_set_seed` / `sumi_get_seed` — one word, the strike-phase generator — and the set rebases the mapper's gesture clocks (the torsion sweep's phase, the stir's pending rotation, the running episodes, the smoothers) that a recording cannot carry; the replay header's `seed` line, the player applying it. The strain buffer holds ACTIVITY, not σ — σ of the accumulated map never decays, so a channel that cools is the strain's rate integrated: (σ, the fast activity over 50 ms, the slow over `spark_afterglow`, the exposure EMA) in an RGBA16F side target stepped once a frame by `composite.glsl`'s third program; Kessho's Layer A reads the exposure (its gates against it are open `[ITERATE]`). The spark: the ridge keeps a texel within a tenth of its neighbours' maximum (one-texel dust otherwise); the leader saturates at a fast activity of 0.01; **the branch trick is retired** (`DECISIONS_11 #2`: σ + κ·grain cannot make endpoints on a hot ridge nor on a plateau; `spark_branch` dropped) — branches are geometry, the stroke of step 76b. Ridges light on charge only. Measured on the swirl scene at 1080p: ridges 2.5 texels wide (6.3 with the suppression off), 0.31 % of the canvas lit (the plasma 0.59 %; the roadmap's 3-texel / 12-texel / 30 % figures were a soak scene's guesses), the afterglow 0.3634 of itself after one time constant (1/e 0.3679), the Anod golden bitwise; the A/B renders the author's — **and the viewing found the strike's σ a plateau** (the triangle stack's slope is piecewise constant), so on a strike the reading thins nothing: the line has to be written.*

**The leader stroke** *(AS SHIPPED, step 76b, 2026-10-15 — `DECISIONS_11 #3`; proposed by `ROADMAP_6_delta_76b_leader_stroke.md`, folded `DECISIONS_11 #2`)*: `SUMI_DEFORM_LEADER` = 17, class sub-stepped. In `glow_mode` 1 the Anod note-on strikes a STROKE along a jagged tree from the strike point BESIDE the blob episode — the drop and the shear episode stay, the leader grows out of them (the author's viewing of 2026-10-18: the first ship replaced the episode and the bolt lost its impact); `glow_mode` 0 keeps the strike bitwise: a biased random walk — step ℓ = 0.012 canvas heights, the heading jittered ±30° about the strike's pitch axis (the layout's glide direction at the key; radial off the lattice), a fork with probability p_b = 0.15 per step at ±45° with half the parent's count, three generations, the trunk N = 8 + 32·v steps for the velocity v (the strike's value 0..1: r = r_min + span·√v, so v = ((r − r_min)/span)²) — walked at the strike by a 32-bit LCG seeded from the strike's draw (the stroke's draw first, the blob's phase draw after it), integer arithmetic, bit-identical on every platform. Each segment is a slip along its own direction D̂ of a = h·(0.6 + 1.4 v) (h = 0.003 canvas heights, three texels at 1080p: two to six texels), the lookup P_src = P − a·w(P)·D̂ with w a C² quintic bump across (half-width h) times the same bump along over ramps of 2ℓ past both ends (one over the segment): across, a pure shear (det J = 1 for any profile); along, the ramps compress by at most a·w′_max/ramp = 1.875/4, det J ≥ 0.53 — ONE sub-step at every velocity (a ≤ ramp/4, the class rule as the burst's and the wake's). Every window is finite. THE CHANNEL IS CHARGED (a resolution the proposal did not have: the reading lights charge only and a strike's charge is a small disc, so the stroke would have walked into dark water): within h of the segment, round caps at the joints, the pass writes the strike's ink band (phase base + the radial fraction across, as a drop's) and its aux — the leader is the ionized path; the slip stays. THE EPISODE: 120 sub-steps a second (a fractional credit on dt; one a frame at the scripted clock), every alive branch together, a frame's sub-steps composed in time order in ONE pass per stroke (up to eight entries; the shader applies them last-first, the pre-image of a composition); the budget refused → the stroke waits a frame, nothing lost. The σ of a stroke is a ridge the reading thins; the strain buffer's leader rides the tip; the channel's gain against the leader is 0.4 (0.55 at step 76), the leader three times the channel at the tip. `sumi_add_leader(inst, x, y, dir_x, dir_y, energy, seed)` on the gesture route (a band of its own from the drop counter). Gated by `--soak leader` (the bare shear: the channel's charge off under the soak, a deposit excluded by nature as every strike's drop is — det min 0.763, mean 1.00000; 500 pairs 0.15 texel; the stream 3.4·10⁻⁵ a pass of growth, the along-ramps' resampling, within the class's rate bar) and `--leader-test`. DBM stays `[ITERATE]` (4.0 unless the viewing asks); the drift direction (the pitch axis) and the constants ℓ, h, p_b, the rate `[ITERATE]` at the author's viewing.


---

## 11. Quality of life — shipped (step 43 and the shells)
* **Palettes** (#63–#65): one model, `sumi_palette_t` — 2..8 linear-RGB stops along the ink-depth axis, the depth curve (γ, floor), `hue_drift` toward `accent_rgb`, `clear_rgb` — through one shader path; the built-ins are presets in it (bitwise-proved); a library of twelve per medium (`sumi_palette_preset_count / _preset`: the three built-ins, Cobalt & amber — Okabe–Ito — Viridis and Cividis); the morph ring includes the custom slot (custom → the three built-ins); the curve is fixed per medium (the "advanced fold" deferred). Editors on every shell.
* **Substrate** (#66): `paper_tint[3]` (cream / white / toned presets), `fiber_scale`, `anod_dark`, `anod_grain` — composite-side, screen-locked by construction, bitwise at their defaults.
* **Presets** (#67, #73, #74, #79): one serializer, `presets/` (pure C11, no allocation): params by name, the input dialect, the custom palette, the CC map, the routed controls' values, the strip's wheel assignments, the layout-state defaults — JSON schema 1 stamped with `sumi_version`; unknown keys ignored, missing keys defaulted (`presets/SCHEMA.md`). The desktop's INI keeps only the shell's own keys; `last_session.json` restores at launch; named presets are files (the desktop's config folder, the iPad's Files, Android's SAF, the web's localStorage with export). A preset made on one shell loads on every other byte-for-byte.
* **Prints** (#68, #77, #84–#86): `sumi_read_field` (the field as it stands), `sumi_export_begin / _poll` (the same field composited at any size up to 8192, `SUMI_EXPORT_ANOD_ALPHA`); the print ledger on every shell — dips kept with their field and look, thumbnails, re-export at Screen / 2K / 4K / 8K; the copy states the bound honestly (detail below a field texel is interpolation; the true re-dip is replay). A background write reports its outcome in the UI.
* **The copy** (QOL §6): "Dip the paper — keep the print" / "Clear the canvas — discard" on every shell; the play surface draws light on Anod's glass.

---

## 12. Sound — Voxo, the internal MPE sampler (Phase 7, steps 47–55)

`specs/SOUND_SPEC.md` corrected to what shipped, its sections kept in order (§12.1–§12.6 ↔ SOUND §1–§5 and §7; SOUND §6, the roadmap sketch, became `ROADMAP.md` Part 6). Every `[ITERATE]` it carried is resolved below by the decision named (`DECISIONS_6 #n` = `DECISIONS.md` Part VI); where this text and an entry disagree, the entry is the record.

**Why it exists (the recurrent complaint, stated):** the app was a controller without a sound. On iOS most plugins die in the background; Android lacks a synth ecosystem. Users want to *play it everywhere*. The fix is an internal sampler speaking a format with a real free-instrument ecosystem: **Decent Sampler presets** (`.dspreset` XML + samples, `.dslibrary` zip bundles; Pianobook and friends).

### 12.1 Architecture: a sibling core, not a libsumi feature (#2–#5)
* **A second library beside libsumi: Voxo** (the name confirmed, #2; `voxo/` beside `core/`, version 0.7.0 at the phase's close). `voxo/include/voxo.h` is pure C — the `sumi_core.h` rules verbatim: a three-state `VOXO_API` export macro, `voxo_version`, no STL, no exceptions, no callbacks-into-C++ across it; a `module.modulemap` for Swift — and `voxo/src/` is C++20 without exceptions or RTTI, built as a static archive on every native platform. The rendering engine stays audio-free; the shells wire both. Voxo OFF is the 1.x app.
* **Feed: the same bytes.** The shell's single MIDI producer fans the identical raw stream into two SPSC queues — `sumi_push_midi` and `voxo_push_midi` (the desktop through the harness's tap, the iPad through the canvas's `push()`, the Tab through `shell::push_midi`). Same producer-thread contract; two independent consumers.
* **The front half is shared source:** Voxo compiles `core/src/midi_normalizer.cpp` from the same repo sources and never links `libsumi` — one MPE decoder, two builds, zero runtime coupling. MCM zones, RPN 0 ranges (±48 member / ±2 master), mode detection, sustain, the stuck-voice safeguard: all inherited, all soak-tested already. Past its ingest queue Voxo consumes the normalized vocabulary (VoiceBegin / Glide / Press / Slide / Swirl / End + GlobalCtl), not raw bytes; it keeps its own pool of sixteen performance voices keyed by (channel, note) — the core's voice mapper is not shared. The shell mirrors its input dialect into Voxo (`voxo_set_input_mode`).
* **The audio backend is miniaudio 0.11.25** (#5; confirmed on both tablets by the latency spike, #7–#8 — AAudio's low-latency performance mode granted on the Tab): CoreAudio / WASAPI / ALSA·PulseAudio / AAudio, one implementation TU, Objective-C++ on Apple (no dlopen there), runtime-linked elsewhere. The fallback ladder was recorded correctly and never climbed: RtAudio is desktop-only; the mobile escape hatches were direct AAudio and AVAudioEngine, and neither was needed. The SHELL owns the iOS session (playback, mix-with-others, 48 kHz, a 128-frame IO buffer) and Android's audio focus; the AAudio buffer is two bursts, grown one burst per underrun by a tuner the stats query paces (#7, #28).
* **The callback-thread contract** (#3) — the hardest real-time contract in the app — is written at the top of `voxo.h` and tested by a counting global allocator: on the audio thread Voxo allocates nothing, frees nothing, takes no lock, logs nothing, makes no blocking call; it drains the normalizer's wait-free ring ONCE at block start and applies every voice transition there in event order; the shell's settings (gain, dialect, interpolation, Local Control, the CC routes) arrive through atomics read at block start; the instrument is published by a pending → current → retired swap, the shell freeing the retired one. `voxo_render(v, lr, frames)` is the callback's whole body and runs without a device for tests and bounces (`--voxo-bounce`).
* **Block sizes, per platform, measured** (#4, #9, #33, #35, #39): `voxo_default_block_frames` asks 128 on Apple, 192 on Android, 256 elsewhere. macOS 128 as asked; iOS 128 (a 2.667 ms IO buffer, 9.7 ms session output latency); Android AAudio's burst (192 on the Tab, 4 ms; touch-to-DAC ≈ 32 ms); Windows 256 asked, WASAPI shared mode's 480-frame engine period granted with the callback kept at 256; Linux 256 through PulseAudio on PipeWire's shim, the thread at SCHED_FIFO 70 where allowed.

### 12.2 Voice model (#10–#12, #16–#18, #25)
* **MPE dispatch is the easy part** — one member channel = one performance voice, no stealing heuristics — but a performance voice activates a **sample stack**: up to eight LAYERS, the preset's zones matching the note and velocity, velocity layers crossfading at equal power over their overlaps, round robins by sequence position or random, release samples on note-off, each layer with its own ADSR (linear attack, one-pole decay and release; the preset's ampVelTrack). The dispatcher is 1:1; the voice is small-orchestral inside.
* **Pitch (X):** ratio = 2^((note − root + bend·range)/12) × the sample's rate over the device's, recomputed per block and ramped linearly per sample; **4-point Hermite interpolation** (#10–#11). Linear is kept only as the lab's comparison (`voxo_set_interpolation`): the glide check (`tools/voxo_glide_check.py`, a Blackman–Harris spectrum judged between the sweep's corners) reads content at 0.22 of a sample's Nyquist 12.8–14.8 dB cleaner under Hermite, at 0.44 only 7 dB — and wide glides are this instrument's signature gesture. The loop plays with an equal-power crossfade; one 2-pole state-variable low-pass per voice.
* **Expression (Y/Z):** per-voice one-pole smoothers on pressure and CC 74, their coefficients from the preset's rising/falling smoothing times; the default MPE map is DS's own — pressure → gain (0.35 + 0.65·p), CC 74 → the cutoff (× 2^((t − 0.5)·6)) — with the preset's `<mpePressure>` / `<mpeTimbre>` bindings overriding. Swirl (0xA0) is offered to bindings as an extra source with no default target and no preset syntax (#17: the `[ITERATE]` answered — none). Under MPE the master channel's pedal, bend, pressure and CC 74 reach every member voice of the zone (#25).
* **Sustain** (CC 64) is honoured in the release logic; CC 120/123 are the panic; the strip's volume is the master gain. **Local Control** (CC 122, and the shell's own switch) is tracked by Voxo and APPLIED by the shell, which stops fanning its own play surface's bytes into the sampler while the hardware keeps sounding (#12); internal sound and outbound MIDI are independent switches.

### 12.3 The Decent Sampler format: the v1 subset, honestly bounded (#13–#15, #19–#20, #22, #31)
* **v1 parses** (pugixml over `.dspreset`, miniz over `.dslibrary`, dr_wav / dr_flac, and an AIFF/AIFF-C PCM reader of our own — the stock Basic Piano ships AIFF, the `[ITERATE]` answered, #14): the group cascade (`<groups>` → `<group>` → `<sample>`), sample zones (lo/hi note and velocity, rootNote, tuning), ADSR and ampVelTrack, loops with crossfade, seqMode / seqLength / seqPosition, trigger, tags, one per-voice low-pass, the DS MPE bindings, `<midi>` cc / note / velocity bindings, `<modulators>`, the UI controls' starting values through their bindings (`TAG_VOLUME` included), and **the bus reverb and delay** (the author's call — the two effects even a beginning musician actually uses): a Freeverb-class reverb and a feedback delay, real-time-safe, buffers allocated once per rate, reading the preset's DS effect parameters and knob defaults, post-sum on the bus, never per-voice (#19). The shell's one CC map carries six bus targets numbered from 1000 (`presets/SCHEMA.md`) through `voxo_map_cc`, which switches the effect on for a preset without one — every loader lets those targets through (#31). Every number is clamped or defaulted.
  * **Why loops and the filter are load-bearing, recorded so they never get "simplified" out:** loop points are what let a 3-second sample sustain a 30-second pad — without them every held note dies at file end, killing exactly the instruments beginners reach for (pads, organs, strings); and CC 74 → cutoff is DS's default MPE timbre map — without the filter, the pen's Y axis and every slide gesture is audibly dead.
* **v1 does not parse:** chorus, convolution, EQ and the other filters, the full modulation matrix, note sequences, UI/skin definitions, sample streaming. **Every preset load produces a compat report** (#14): the summary line, then one canonical sentence per note (memory, missing samples, streaming, chorus, convolution, EQ and other filters, an unknown effect, modulators, note sequences, an unknown binding, the custom UI) in a documented order — "uses convolution reverb: will play without it" — shown once, calmly. `voxo/COMPAT_REPORT.md` is the copy and a test asserts it verbatim; refusals only for what is not a preset, the reason given. An hour of mutation fuzzing (25 M loads, `tests/voxo_fuzz.cpp`) found no crash.
* **Memory: preload-first, an advisory gate, foreground only** (#20, #23, #28). Everything decodes to float in RAM — streaming is v1 overkill, and modern Android handles large native allocations (the native heap sits outside the Java limit; only low-RAM devices and the LMK bite). Before decoding, the size is read off the sample headers and compared with the shell's advice (`voxo_set_memory_budget`: the desktop's free memory × 0.6, iOS `os_proc_available_memory` × 0.6, Android the activity manager's `availMem` × 0.6); over it, the memory note leads the report and the load proceeds — **a warning, not a wall**. The desktop loads on a worker thread; the tablets on a background queue with a progress row. Jetsam's dangerous combination — a large preload while BACKGROUNDED — is out of scope entirely (§12.4). Disk streaming is deferred to Voxo Dorean (§12.6).
* **Licensing posture** (the docs page drafted in `site/drafts/voxo/licensing.md`): the format is implementable (formats are not copyrightable); the app **bundles zero libraries** — users load their own, and each library's terms travel with it. **One demo instrument ships so a first launch makes a sound: the VCSL Dan Tranh, CC0** (#22 — the `[ITERATE: record it ourselves]` answered by the author's choice of the Versilian zither): sixteen samples of the f layer at 32 kHz, 2.9 MB, stretched across the keyboard, fetched by `tools/fetch_dan_tranh.py` into `voxo/demo/` and bundled by every shell (the macOS bundle's Resources, `share/midi-sink` on Linux, beside the exe on Windows, the iPad's bundle, the Tab's assets).

### 12.4 Platform notes (#21, #23–#24, #26–#30, #33, #41)
* **iOS: foreground audio only** — the re-scoped truth: internal sound dissolves the background complaint at the root (users needed background plugins because they needed a SECOND app for sound; with sound inside, midi-sink IS the foreground app while being played). The shell's AVAudioSession is playback, mix-with-others, 48 kHz with a 128-frame IO buffer; the device stops with the visuals on backgrounding and on an interruption (calls, Siri), restarts on return, on the interruption's resume and on a route change (#23). The plist keeps the `audio` background mode CoreMIDI already needed — the spec's "no `UIBackgroundModes`" line is superseded by #23: the plist keeps `audio` because iOS refuses virtual MIDI endpoints without it — re-verified at the 2.0 release, a build without the key logs `vsrc=0(-10844) ble=0(-10844)` and drops every send (`DECISIONS_9 #17`); the key is the permission to publish MIDI, not background audio. The Sound page lists Documents/Instruments, imports a `.dslibrary` or a preset's folder through Files, and the declared Decent Sampler document types let AirDrop and "Open in" hand libraries over (#24, #26).
* **Android:** an AAudio low-latency stream; audio focus held while resumed (GAIN, usage game) and yielded to a call; foreground-only (no foreground-service machinery — that question moved to Voxo Dorean). The Sound page uses the Storage Access Framework's document and tree pickers and "Open with"; the demo installs from the assets into the app's Instruments folder (#28–#30).
* **Desktop:** trivial by comparison. The Sound section of the settings window (the switch, the volume, the WAV sample row, the instrument row with its report and the memory advice, the demo button, the status line); miniaudio follows the default output through a device switch without a stop on all three (#33, #41) — the "default-device hotplug handling" the spec asked; output-device SELECTION was not built in 2.0 (carried, §16).
* **Every shell** hides the cells outside the loaded instrument's reach while the sound is on (`voxo_covered_notes`, #27).
* **Web:** deferred (wasm + WebAudio is plausible; memory and fetch size make it a later conversation).

### 12.5 Ties into the rest of the system (#32, #34, #39)
* **Replay re-sounds** (QOL §1): replays feed the sampler like live bytes — record on iPad, replay on desktop with a grander instrument loaded. Deferred: the offline bounce of a replay to WAV (the lab's `--voxo-bounce` is the seed).
* **Presets** (§11, `presets/SCHEMA.md`) carry the six bus targets in the CC map today; a performance preset referencing a loaded library by name/hash (never by embedding samples) is the instrument slot still to come.
* **The sampler is optional at runtime:** OFF = the 1.x controller, nothing regresses. The visual budget is untouched — the audio thread never blocks the render thread — and **the acceptance suite** is the combined stress: `midi-sink --dev --voxo-preset <heavy library> --voxo-storm <s>`, the fifteen-channel MPE storm with a heavy library loaded on the real device; pass = 0 XRuns by Voxo's proxy (the platform's own count is the truth where it has one — AAudio's, PipeWire's), 0 dropped bytes, the visual loop at or above 58 fps; the period reported, not demanded (the `[ITERATE: XRun budget]` answered — zero, #32, #39). Green on the three desktops (#34, #39). The Phase-7 invariant held trivially: Phase 7 never touched `libsumi`.

### 12.6 Voxo Dorean (the sister app — deferred, designed for)
The background use case — the tablet as a standalone sound module while OTHER apps hold the screen — is a different product with different constraints, and it gets one: **Voxo Dorean**, a small standalone app around the same Voxo library (the sibling-library architecture's third payoff: one more tiny host shell, zero engine changes). Its charter, when its time comes: background execution (`UIBackgroundModes: audio` / an Android foreground service), **disk streaming to RAM** as its core competency (the deferred item lives here, where background Jetsam budgets make it mandatory rather than optional), a minimal UI (library picker, MIDI input, meters), and the strict memory discipline the background demands. Nothing in midi-sink 2.0 blocks on it; everything in Voxo (the library, the DS subset, the compat report) is inherited by it for free. Its charter is this section; nothing in 2.0 blocks on it.

**§12.7–§12.12 — Voxo DS 2.0, the Decent Sampler coverage's second generation, and the sound carry-overs** *(Phase 15, steps 87–91 — the roadmap's final numbering; transcribed at the fold)*
In 2.0 the v1 parser handles zones, ADSR, loops, one low-pass SVF, the basic bindings and the bus reverb and delay, and the compat report names what it does not play (§12.3). This arc takes the high-impact features the Pianobook libraries lean on, sustain mechanics and chorus first. The library's version keeps following its ABI (0.16.1 at the opening); "DS 2.0" names the coverage.

### 12.7 Sustain and pedal mechanics *(AS SHIPPED, step 87, 2026-10-25 — `DECISIONS_14 #1`; Voxo 0.27.0, the ABI unchanged)*
CC 64 is the pedal's POSITION on the channel (0..127 → p; the master's reaches the zone). The four mechanisms, in the format's own attributes (read from the Decent Sampler developer guide, `DECISIONS_14 #1` item 1 — the proposal's `<group trigger="cc">` with `onLoCC`/`onHiCC` does not exist): (1) **pedal noises** — a sample with `onLoCCN="lo" onHiCCN="hi"` (the CC number in the attribute name; cascading from the group; `pitchKeyTrack="0"` the idiom) fires ONCE when CC N's value enters [lo, hi] — the thump going down past 64, the damper lift coming up past 63 — as a voice of its own, no note, full velocity, its layers played out; such a zone answers no note. The format retriggers on every message inside the range; Voxo fires on the crossing (recorded). (2) **half-pedalling** — a note released at p > 0 is held and damped: every note layer's envelope takes a second factor decaying at the rate r(p) = (1 − p)·4.6/release + p·4.6/`sustainDecay`, `sustainDecay` VOXO'S OWN group attribute (seconds; absent = 0 = a hold at the full pedal, bit for bit the pedal of 2.0); the pedal moving re-damps. (3) **re-pedalling** — the pedal coming down over a releasing tail still audible freezes its envelope where it is (ENV_HOLD) and holds it under the pedal's damping, no attack, no jump; the lift releases it. (4) **choke groups and the CC filter** — `silencedByTags` with `silencingMode` fast (THE FORMAT'S DEFAULT: a 5 ms fade) | normal (the release phase) and `silencingDecay` (seconds), the pass run before the new layers start so a group silenced by its own tag chokes its predecessor only; and the pedal-up / pedal-down groups through `loCCN`/`hiCCN` (a note-on plays the zone only with CC N's last value in range) — not through `seqMode`, which the format does not gate. A Suzu voice under the pedal takes the same rate through its declared contraction. The gates (`tests/voxo_preset_tests.cpp` 16–20, the constant-level fixtures of `tests/fixtures/dspresets/pedal/`): the hold bit for bit; the full pedal's decay at 4.6 / s and the half-pedal's at 25.1 / s within the formula; the catch at its level, flat for half a second; the noises once per crossing; the filter; the choke in 5 ms and through the release; zero allocation in the callback; the fuzz driven through the pedal. Not built: `trigger="continuous"`, the legato attributes (step 90), `releaseTriggerDecay`, tag polyphony limits, a threshold for switch pedals that send 64.

### 12.8 Chorus, phaser and the bus effects *(the chorus and the phaser AS SHIPPED, step 88, 2026-10-25 — `DECISIONS_14 #2`; Voxo 0.28.0, additive; the EQ, the gain and the wave shaper AS SHIPPED at step 89 — §12.9)*
`<effect type="chorus">` at the bus and at the group level — the format's `mix`, `modDepth` (0..1), `modRate` (Hz, 0..10), read from the Decent Sampler guide (no delay, feedback or width attribute exists; `DECISIONS_14 #2`): a stereo modulated delay, two lines at a 7 ms centre swung by ±4 ms × depth, the LFO a magic-circle cell whose (x, y) are the quadrature pair (the right channel 90° from the left), the fractional read the §12.2 4-point Hermite; the buffers FIXED (16 ms at 96 kHz), so the group-level chorus is ONE INSTANCE A NOTE inside every voice — the format's semantics ("initialized every time a note is started") — with no allocation anywhere, a chorused voice's layers summed into a scratch block before its chorus. `<effect type="phaser">` on the bus alone (the format admits no group-level phaser): six first-order all-pass stages a channel swept ±2 octaves about `centerFrequency` by the same kind of LFO (the coefficient refreshed every sixteen samples), `feedback` (−1..1, held within ±0.95) from the chain's last output into its input, `mix`. Both compiled without fused multiply-add contraction, so their samples are the same bits in every native build at the fixed seed (the LFO at phase 0 on load and at a fresh note). The bindings `FX_MIX`, `FX_MOD_DEPTH`, `FX_MOD_RATE`, `FX_CENTER_FREQUENCY`, `FX_FEEDBACK` resolve to the effect they point at (`position` / `effectIndex`); `enabled="false"` bypasses any effect. The bus order: chorus, phaser, reverb, delay. The CC map's targets `VOXO_CTL_CHORUS_MIX` 1006, `_RATE` 1007 (0..10 Hz), `_DEPTH` 1008 (`presets/SCHEMA.md`; the desktop's and the tablets' pickers). The report's chorus sentence RETIRED (`VOXO_NOTE_CHORUS` stays in the ABI, never raised, its copy empty); the phaser leaves the unknown effects. The gates (`tests/voxo_preset_tests.cpp` 21–25, `tests/fixtures/dspresets/fx/`): mix 0 the dry bit for bit; depth 0 at mix 1 the dry delayed by exactly 336 samples at 48 k, bit for bit; full depth at 2 Hz a ±11 Hz swing on 220 Hz (the formula's ±4 ms × 2π × 2 Hz) with the right channel a quarter period behind; the group chorus on one note the bus chorus bit for bit, on two notes the dry pair delayed within −143 dB (the dry path's fused layer accumulation); the phaser frozen at 440 Hz notching the saw's 440 Hz harmonic 132 dB with 220 Hz within 0.6 dB, its `FX_MIX` binding at 0 the dry and at 1 the chain alone at the dry's level; the sweep with feedback 0.7 bounded, moving the notch 20 dB across its period, two fresh instances identical; zero allocation; the fuzz seeded with the effects. The bus EQ, `<effect type="gain">` and `<effect type="wave_shaper">` shipped at step 89 (§12.9): the bus order since then is the wave shaper, the EQ's stages in their listed order, the gain, the chorus, the phaser, the reverb, the delay.

### 12.9 The filter family, the filter envelope, the bus EQ, the gain and the wave shaper *(AS SHIPPED, step 89, 2026-10-25 — `DECISIONS_14 #3`; Voxo 0.29.0, no new entry point)*
**The family on the one SVF.** The voice's filter is the group's first enabled `lowpass` (`lowpass_4pl` its legacy name, `lowpass_2pl` read the same), `lowpass_1pl`, `highpass`, `bandpass` or `notch`, else the instrument's first enabled lowpass / highpass / bandpass (an instrument-level notch is the bus EQ's, where the format puts it); none = a lowpass at 22 kHz, so CC 74 always moves something (§12's SOUND §3 rule). The Zavalishin TPT loop computes the outputs together — low v₂, high v₀ − k·v₁ − v₂, band at unity k·v₁, notch v₀ − k·v₁ — the mode picking one (`Group::filter_mode`), the state and the coefficient ramp unchanged; `lowpass_1pl` is the TPT one-pole on the same state. **The `resonance` attribute IS the Q** (the format: 0.001..5, default 0.7; `Q`/`q` on the notch), k = 1/Q, held within 0.1..18 — step 51 read it as an amount above 0.7 (Q 0.5 at the default): corrected, so every lowpass preset sits 3 dB higher at its cutoff than before (the proposal's "peak" output is the bus EQ's peaking stage, not an SVF output — the SVF's low − high is a different thing). A lowpass at the top with the default Q is no filter at all (the dry, bit for bit); the other modes always run; `enabled="false"` passes a filter over. Gates (`tests/voxo_preset_tests.cpp` 26–27 on the looped 110 Hz saw): each mode at 1 kHz, Q 0.7 and 2, and the one-pole, within 0.035 dB of its analytic curve over the saw's thirty harmonics — the TPT SVF is the exact bilinear transform of the analog prototype prewarped at the cutoff, |H(f)| the prototype's at Ω = tan(πf/fs)/tan(πfc/fs) — against the roadmap's 0.5 dB bar.
**The filter envelope.** The first `<modulators><envelope>` carrying a binding on `FX_FILTER_FREQUENCY`, `FX_FILTER_Q` or `FX_FILTER_RESONANCE` (instrument level: every group; group level: that group; one a group) plays PER VOICE whatever its `scope`: `attack` (linear), `decay` (exponential, 4.6/T, to `sustain`), `release` (exponential, following the amplitude envelope's release — a note-off, a choke, a retrigger; the pedal's catch freezes it with the amplitude), `modAmount` scaling the envelope before the binding's translation, `modBehavior` (the binding's, else the element's; default set): set replaces the cutoff (under the default timbre's multiplier, so CC 74 still plays), add adds, multiply (and modulate, read as multiply) scales. The envelope is stepped once a block — its value at the block's start and end folded into the cutoff's two coefficients, which ramp per sample already — and the Q read at the block's start. Gates (31): set 200..8000 Hz, add 500 + 0..6000, multiply 500 × 1..16 at modAmount ½: the 3300 Hz harmonic over the fundamental loudest after the attack, 10 dB lower in the sustain, 24 dB lower still in the release; the Q from 5 to 0.7 over half a second rings 13 dB at the start; an envelope with no filter binding raises no note.
**The bus EQ** (`voxo_bus::Biquad`, `EqChain`): every enabled `peak`, `low_shelf`, `high_shelf` and instrument-level `notch` in its listed order, eight stages at most, RBJ's cookbook (direct form I, the shelves by Q) with `frequency`, `Q`/`q` and `gain` — linear by default, decibels under `gainUnit="decibels"` — the coefficients recomputed every block at the device's rate (so a rate change needs no reallocation). `FX_FILTER_FREQUENCY`, `FX_FILTER_Q` and `FX_FILTER_GAIN` bound to an EQ effect's position resolve to its stage (the binding's value linear); bound to a voice filter's position, to the voice filter. Gates (28): the peak (+6 dB at 1 kHz, Q 1, written in dB or linear), the shelves (200 Hz +6, 2 kHz −6), the notch, each within 0.015 dB of RBJ's response; a four-effect chain with a disabled stage within 0.024 dB of the three enabled stages' product; `FX_FILTER_GAIN` on CC 20 lifting the peak 6.02 dB. There is no `<effect type="eq">` in the format (the proposal's name): the EQ IS these three kinds.
**The gain.** `<effect type="gain">` with `level` in dB by default (the `"-6dB"` suffix allowed; −100..24), linear under `levelUnit="linear"` (0..8); absent, the format's default −6 dB. On the bus every enabled gain is folded into one factor applied after the EQ; at the group level folded into the group's gain (the zones'). Gates (29): the dry × 0.5011872 sample for sample (2e-8), the linear 0.5 exactly.
**The wave shaper.** `<effect type="wave_shaper">` with `drive` (1..1000), `driveBoost` (0..1), `outputLevel` (0..1; the format's default 0.1 — a 20 dB trim), `highQuality` read and ignored: y = outputLevel · tanh(x · drive · (½ + ½·driveBoost)) through a rational tanh (Eigen's (13,6), within 3e-7 of libm, no libm call, the same bits everywhere), evaluated AT THE FORMAT'S SCALE — the engine's samples sit at 0.40 of full scale, so the shaper divides by that going in and multiplies going out, and a preset's drive means what it means in Decent Sampler. On the bus (the first enabled one) and at the group level per voice through the scratch path the chorus uses, before the chorus. `FX_DRIVE`, `FX_DRIVE_BOOST`, `FX_OUTPUT_LEVEL` bound live. Gates (30): the bus shaper within 7e-8 of 0.16·tanh(50x) sample for sample; the group shaper on one voice the bus shaper bit for bit; the drive on CC 20 from 1 to 100 squaring the saw (its second harmonic 40 dB down); the level on CC 21.
**The report.** The "EQ and other filters" sentence is RETIRED (`VOXO_NOTE_OTHER_FILTERS` stays in the ABI, never raised, its copy empty); the modulators sentence reads "Modulators (LFOs, sequences): …" — the envelope no longer raises it. A group-level `peak` or shelf (outside the format) is passed over silently. Nothing allocates in the callback (gate 32).
**Found on the way: the loop's edges clicked.** A loop without a crossfade read its outer interpolation taps from the sample around the loop — the padding's zeros when the loop spans the whole sample, as the fixtures' and many libraries' do — a click once a pass (the notch gate's second half-second read 1.2 dB off). The last two frames before `loopEnd` and, once the loop has wrapped, the first frame after `loopStart` read their taps wrapped into the loop (`read_frame_wrapped`, `Layer::looped`); gate 27 asserts the saw repeats its period across the loop point within 1e-4 (1.6e-2 before).

### 12.10 LFOs, the modulator matrix, legato and glide *(AS SHIPPED, step 90, 2026-10-26 — `DECISIONS_14 #4`; Voxo 0.30.0, additive: `voxo_set_tempo`)*
**The format's `<lfo>`** (the guide's modulators page): `shape` sine | square | saw (the SHAPE binding's list adds triangle; Voxo plays all four), `frequency` in Hz or — under `frequencyFormat="musical_time"` — an index into the guide's subdivision list, `delayTime` (seconds; the waveform held at its start until then), `modAmount` (0..1), `scope` global (the default) | voice, `modBehavior` set | add | multiply | modulate, `enabled`. There is NO `offset`, `phase` or `sync` attribute (the proposal's): the tempo sync IS the musical-time format, on the host's tempo — `voxo_set_tempo(bpm)`, which every shell passes from its session's BPM (the desktop's `sound_apply`, the iPad's canvas, the Tab's JNI; 120 until set). The subdivision list is reconstructed from the page's four anchors (10 = 1/8, 13 = 1/4, 19 = 1, 24 = 5/1): a triplet, the plain value and the dotted value for every subdivision from 1/64 to the whole note, then whole bars (`musical_whole_notes`); a quarter at 120 BPM is 2 Hz. **`<envelope>`** gains `delay` and `hold` beside the ADSR of §12.9 (its curves — `attackCurve` −100 logarithmic by default, `decayCurve` / `releaseCurve` 100 — are read as the linear attack and the 4.6/T exponentials: recorded).
**The matrix** (`instrument.h` `Modulator` / `ModTarget`, eight of eight; `voxo.cpp` `mod_read` / `menv_advance` / `mod_apply`): every `<binding>` under an `<lfo>` or an `<envelope>` is a target — `AMP_VOLUME` (on the pressure's gain ramp), `PAN` (−100..100 over the zones'), `GROUP_TUNING` / `GLOBAL_TUNING` (semitones on the block's pitch line), `FX_FILTER_FREQUENCY` / `FX_FILTER_Q` (the voice filter; set under the default timbre's multiplier), a group effect's `FX_MIX` / `FX_MOD_*` / `FX_DRIVE` / `FX_DRIVE_BOOST` / `FX_OUTPUT_LEVEL`, and, by position, the bus effects' parameters and the EQ stages' (`FX_FILTER_FREQUENCY` / `_Q` / `_GAIN`). A modulator is read ONCE A BLOCK at the block's start and end — an LFO as a function of the rendered clock (global) or of the voice's clock since its note-on (voice), an envelope per voice whatever its scope (delay, attack, hold, decay, sustain, release following the voice's release) — and each target scales the raw 0..1 by `modAmount` (the element's × the binding's), reads its curve and folds the result by its behaviour: set replaces, add adds, multiply scales, **modulate adds the curve's value less its neutral** (the LFO's centre, the envelope's zero — "a neutral value contributes no offset", the guide). The bus targets read the global LFOs into a live copy of the bus parameters every block (the CC bindings' and the shell's values stay the base); an envelope on the bus has no voice and contributes its idle value. Step 89's filter envelope is this mechanism with one slot. The `PAN`, `GROUP_TUNING` and `GLIDE_TIME` bindings also reach the group state through CCs and UI controls (`Target::AmpPan` / `GroupTuning` / `GlideTime`).
**Legato and glide** (the guide's groups page and its true-legato topic): `glideTime` (seconds, 0 = none) and `glideMode` (legato — the default — | always | off) on `<groups>`, `<group>` and `<sample>`, cascading; `polyphony` on the groups (0 = no limit, 1 = monophonic — the innermost wins); `trigger="first"` (only with no other note held) and `"legato"` (only with one), `previousNotes` (a comma list; the page's `previousNote` too) and `legatoInterval` on the samples. A note in a gliding group starts at the previous note's CURRENT pitch and walks to its own along a straight line in semitones over glideTime (`Voice::glide_*`; a bend during the glide moves the line's end) — under legato only while that note is still held or pedalled, under always from the group's last note whatever became of it (`GroupState::last_hz`); a group at its polyphony releases its oldest sounding voice as the new one starts; the dispatch reads the other held notes and the previously triggered note (`voxo_t::prev_note`). The Suzu source does not glide (its own pitch path; recorded).
**Gates** (`tests/voxo_preset_tests.cpp` 33–41 on `tests/fixtures/dspresets/mod/`, eighteen presets on the looped sine and saw): the tremolo's peak over its dip 1.994 (2); the global LFO at its peak and the voice LFO at its dip 0.375 s after a note started a quarter second into the instrument's clock; the square's second half silent, the saw's ramp 3.96 (4), the triangle's 2.00; the vibrato ±0.5 semitone within 1 %; the pan hard right and hard left; a quarter note 2 Hz at 120 BPM and 1 Hz at 60; delayTime flat within 0.01 dB; the cutoff modulated ±200 Hz within 0.1 dB of the two analytic responses' ratio; the bus peak's gain under a global LFO 5.99 dB (6); the envelope's delay silent, its hold full, its sustain 0.499, its release −29.9 dB (−30.0 with the amplitude envelope's); the glide's semitone line 222.2 / 233.0 / 246.9 Hz, off 247.1, always from a released note, legato not; the triggers and the previous-note attributes by the saw's second harmonic; zero allocation; the five-minute fuzz.

### 12.11 The compat report, pruned *(AS SHIPPED, step 90)*
`voxo/COMPAT_REPORT.md`'s canonical sentence table (§12.3) lost the chorus (step 88), the other filters (step 89) and the modulators (step 90) — the three bits stay in the ABI, never raised, their copies empty; what remains is the housekeeping (memory, missing samples, an unknown effect, unknown bindings) and the four features genuinely out of scope — the custom interface, disk streaming, convolution reverb, the note sequences. A modulator element Voxo does not know is named with the unknown bindings. The report's test asserts the eight sentences verbatim and the three retired bits' silence.

### 12.12 The instrument slot, the offline bounce, the device picker *(AS SHIPPED, step 91, 2026-10-26 — `DECISIONS_14 #5`; Voxo 0.31.0, additive; the preset schema 3)*
**The instrument slot.** A session preset carries `library` — the library the sound played by `name` (the preset's stem) and `hash` (the FNV-1a 64 of its preset XML, `voxo_preset_hash`; the samples are not hashed) with `path` as a hint — never the library itself (`presets/SCHEMA.md`, schema 3; `sumi_preset_t`'s `library_*`). A shell writes it when its sound holds a library (the desktop from `app_sound_library_set` as a load lands; the iPad from the controller's identity; the Tab in the JNI's session as `nativeVoxoLoad` returns) and a recording's session header carries it, so a replay names its instrument. On loading a preset that carries one the shell resolves it against its own libraries — `voxo_preset_file_hash` reads a .dspreset's or a .dslibrary's XML without a sample — by name first (`<name>.dslibrary`, `<name>.dspreset`, `<name>/<name>.dspreset`), then by hash over the hint's folder, the current instrument's folder and the user's folders (the desktop: `~/Music/midi-sink`, `~/Music/Decent Sampler`; the tablets: their Instruments folder); found, the instrument loads; not found, the preset plays without it and the shell shows the sentence once: "Library '…' is not on this computer: the preset plays without it." (`AppSettings::sound_library_note`; the iPad's and the Tab's `report`).
**The offline bounce** (`replay/src/sumi_replay_bounce.c`, `sumi_replay_bounce`): the replay library walks a recording's frames in order — each frame's bytes through the caller's push, then that frame's worth of samples through the caller's render in 128-frame calls, the fractional frame carried — and writes a 32-bit float stereo WAV at 48 kHz as it goes; it knows no sound engine, so every shell binds the two callbacks to a DEVICELESS Voxo (`voxo_create`, never started: `voxo_render` with no device, as the tests do) configured as its live one — the desktop through `app_sound_configure`, the one body its live `sound_apply` takes too (the gain, the dialect, the tempo, the device, the source and Suzu's patch, the tuning, the CC map's bus routes, the instrument loaded synchronously); the iPad through the controller's `applySource(to:)` / `applyTuning(to:)` and a synchronous load; the Tab from the JNI's record of what its natives applied (`g.vcfg`). The visuals' events are not applied: the bounce is the sound alone. The desktop's Replay section (*Bounce to WAV*), the iPad's Replay page (the waveform button, the WAV shareable) and the Tab's replay sheet (*bounce*, *wav* exporting it) write `<name>.wav` beside the recording. The bench's `--replay-wav` shares the walker. THE GATE AS SHIPPED: the live device's own output is not byte-alignable to the frame clock (its callbacks fall where they fall against the frames' pushes), so "byte-identical to the live render at the same seed" is read as: the bounce IS the deterministic rendering — the same recording and session give the same bytes, on the desktop measured by the bench's `--bounce-test` (two bounces through the host's own path, byte for byte, the hash in the evidence), in the replay suite over a synthetic engine, and across shells by construction (one walker, one engine, one configuration body).
**The device picker** (`voxo_device_count` / `voxo_device_name` — miniaudio's enumeration on a fresh context; `voxo_set_device` / `voxo_get_device`; `voxo_backend_start`'s `want_device`): the desktop's Sound section gains *Output device* — "System default" first, then the names, refreshed every two seconds; the choice in the INI as `sound_device`; a running instance moves to the new device at once, the backend closed and reopened, the voices, the bus and the preset untouched (no stop the user toggles); a name no longer present opens the default and the row says so (DECISIONS_6 #42's rule). The bench's `--device-test` moves a running instance to the first named device, to a name that does not exist and back, the callbacks counted and the stats naming the device each time.
These close §16's sound items; Voxo Dorean (§12.6) stays deferred.


---

## 13. Synth — Suzu, the symplectic phase-space synth inside Voxo (Phase 8, steps 56–59, 59b–59c)

`specs/SYNTH_SPEC.md` corrected to what shipped, its sections kept in order (§13.1–§13.7 ↔ SYNTH §1–§7). Every `[ITERATE]` it carried is resolved below by the decision named (`DECISIONS_7 #n` = `DECISIONS.md` Part VII); where this text and an entry disagree, the entry is the record. The name: **Suzu** (鈴, the small bell), the working name confirmed (#8).

**The thesis, stated once:** the visual engine composes det J = 1 maps on a canvas; the synth composes det J = 1 maps on **phase space** — each oscillator a 2D cell (x, y) where area preservation means *amplitude stability by construction* instead of *ink incompressibility*. Same mathematics, one octave down. Dissipation exists (filters resonate, notes decay) but is **declared, never accidental** — the audio analog of the operator class table. Architecturally Suzu is a **source inside Voxo** (`voxo_set_source`: the sampler, Suzu, or both layered — #8, #17): the same callback, the same normalizer-fed voice model (a `SuzuVoice` inside each `Voice`), the same bus and master gain — a patch beside a preset, never a second audio stack. Voxo 0.8.0 → 0.15.0 across the phase, every version additive. The patch is `voxo_suzu_params_t` (`voxo_set_suzu_params` returns false and keeps the live patch when a load gate refuses, the reason on the log callback; `voxo_suzu_default_params`), carried by the QoL preset file as a `suzu` object (`presets/SCHEMA.md`) and the desktop's INI as `suzu_*`; the tablets compile every Voxo; since Phase 9 their Sound page offers Suzu's patches (`DECISIONS_8 #13`), the table of eleven since Phase 10 (`DECISIONS_9 #10`).

### 13.1 The DSP class table (every element declares one — #8)
The spec's table with the members as built (`voxo/src/suzu.h` carries it at its top; each element's header comment, its test and its lab page name its row):

| Class | det of the per-sample map | Members as shipped | Discipline |
|---|---|---|---|
| **Symplectic** | = 1 exactly | the cell's leapfrog; the shears (cubic, fold; the Duffing cubic on the position; the bell's brass shear in (p, u)); the lattice's coupling kick; the Verlet chain's and the bore's leapfrog; the kicked rotor's kick and re-base | amplitude confined by construction; the drift test (§13.5) is the gate |
| **Dissipative (conformal)** | < 1, declared, uniform | the SVF's q; the cell's release (a T60); per-mode decay γ_k; the chain's and the bore's per-node damping (`string_decay_s`, `bore_wall_s`); the hybrid's one-zero loss and round-trip factor; the bridge cells' decays; the radiation ports (a non-negative real part at every frequency); the valve's Q; the reed's stop at the lay (an inelastic collision) | every decay traces to a coefficient; the energy ledgers prove nothing else leaks |
| **Drive** | injection, not a map | the strike (the orbit set at A); the pluck added to the chain's state; the Duffing drive (a sinusoid at `drive_ratio` × the note by the press); the bow's seed on a resting mode (−20 dB of its target) | bounded per sample |
| **Self-excited** | ⋛ 1, state-dependent, bounded by construction | the breath bow (an energy SERVO per mode, #11); the winds' exciters — the jet (#19), the reed and the lips (#20) | the bow's limit cycle gated; the winds' POWER-LIMITED PORT and the mouth-power ledger — the energy held ≤ ∫P_mouth·Q dt, by arithmetic (#19, #20, #32) |
| **Lossless transport** | = 1 | the hybrid string's ring with its Thiran allpass (#14); the jet's Hermite-interpolated delay (#19) | every coupling INTO a transport passive: the hybrid's junction is a scattering junction balanced exactly per sample, gated by the load-time probe and the soak |
| **Projected** (a declared correction) | a projection after each control step | the chaotic modulator's energy (#16) | measures a no-op (within 9e-8 of 1) and bounds by construction — the spec's "leapfrog" corrected: the double pendulum's Hamiltonian is not separable |

Unconditional anti-dissipation (det > 1 regardless of state) stays banned; the self-excited class is admitted because its injection is servo-bounded or power-limited.

### 13.2 The primitive cells, as built (#8–#11, #13–#16, #18–#27, #30, #33)
* **The magic circle (§2.1, #8):** `x −= ε·y; y += ε·x` with the updated x, ε = 2 sin(π f / fs) — exact (the oscillator within 0.000 cent across MIDI 21–108 at 44.1 k and 48 k); the strike sets the orbit (x = A, y = εA/2); the release a declared contraction per sub-step; the output x through an attack ramp (a mixer stage, not a map on the cell) and the pressure's level. **The MPE correction, resolved:** the invariant is x² + y² − ε·x·y; the SHIPPED form re-bases the state onto the orbit of the same amplitude under the new ε on every retune (`Cell::retune`; what the spec called the Gordon–Smith form is this recurrence — Gordon & Smith's own is the staggered update itself). A −48 → +48-semitone glide over 2 s ripples 0.097 dB; a one-block JUMP 0.082 dB re-based against 0.630 plain. The plain and the stepped forms are the lab's (`retune_mode` 1, 2; `update_mode` 1 the naive simultaneous update); the quadrature y as a stereo width is carried.
* **The SVF (§2.2, #8):** the Chamberlin core at the 2× rate, q = 2 − 1.9·resonance, CC 74 scaling the cutoff by 2^((t − 0.5)·6). The thematic form KEPT, the trapezoidal escape hatch unused — but the exact tuning holds for the undamped core only: with damping the ring sits sharp (2.3–2.5 cents at C8, Q = 50), so the coefficient is solved per block for the damped ringing frequency in double (`svf_f_for`): within 0.008 cent; the cutoff clamped under the classic form's ceiling (fs/3 of the output rate).
* **The kicked rotor (§2.3, #15):** the cell carries the angle; once per nominal cycle (a fixed clock at the note's period, Chirikov's kick period) p += K·sin θ, p wrapped to (−π, π], and the cell RE-BASED onto f₀·(1 + p/2π) — the momentum IS the pitch within the octave about the note, never a jump. K = `rotor_k` + (2.5 − `rotor_k`)·wheel (CC 1), smoothed 20 ms; the modulator may add to it. Gated: K = 0 the pure tone; K = 0.3 undamped ten minutes within 0.0000 dB, |p| ≤ 1.108 (the island's libration 2√K — ±3 semitones at 0.3; FLAG for the ear: the wheel's mapping of K); the sweep 0 → 2.5 charted (the fundamental's share 97 % → 2 % → 20 % → 52 % → 36 %). The kick is not band-limited beyond the 2× section; the noise is the map's own.
* **The shears (§2.4, #8, #9):** x −= ε·(y + s(y)) — a FORCE scaled by ε (a fixed kick escaped at low pitch), acting on the orbit NORMALIZED by its amplitude (u = y/A, s = g·A·shape(u)), cubic or triangle fold, in the 2× section, the gain clamped to 1 (the fold's ×4 `[ITERATE: budget]` carried — a half-band FIR when the budget says; the 2-tap decimator's −0.08 dB at C8 is the ripple that remains). A sheared cell DETUNES, so it calibrates itself at create (`shear_period_ratio`, a 17-point table per shape) and the voice scales ε by the ratio: within 0.19 cent (cubic) and 0.02 (fold) at full gain. A symplectic cubic is a gentle waveshaper (the third harmonic 23–33 dB under); the hard timbres are the rotor's and Duffing's. FTZ/DAZ is set on the rendering thread at its first block (MXCSR / FPCR.FZ, read back into `voxo_stats.ftz`); the 64-voice decay tail runs flat.
* **The modal lattice (§2.5, #10):** N cells (`modes` 1–16, default 8) at f₀·r_k with their declared contractions; per sub-step (1) the coupling kick from PRE-update positions over the nearest-neighbour CHAIN (the topology `[ITERATE]` resolved), κ relative to the LOWEST mode's stiffness and capped at C8's, (2) each cell's drift and kick, (3) the damping — the standard drift–kick leapfrog, the joint bound λ_max(S) < 4. **Not in the spec, found and built:** the spring DETUNES every mode it touches (149 cents at the first default), so at patch load the normal modes of M(κ) are solved (a cyclic Jacobi, Newton on the eigen-derived Jacobian) and each mode's own stiffness moved until the k-th normal mode lands on its ratio — a 129-point table over κ ∈ [0, 4], interpolated per block as the swirl moves, corrected to first order per pitch (J⁻¹ kept per κ point): the partials within 0.08 cent at any admitted κ. A mode above the output Nyquist is a WALL (muted, keeping its springs). **The load gate, two conditions** at κ_patch + the swirl's 0.5: the compensation feasible (every mode keeps 1 % of its own stiffness) and λ_max < 4 across MIDI 21–156; for every shipped preset the first binds (`voxo_suzu_coupling_bound` bisects the patch's); `lattice_gate` 0 is the lab's bypass. **Partials:** placed (the presets — harmonic string r = m·√(1 + B·m²), stiff bar, bell, glass, the plucked string = Karplus–Strong in modal form, w = sin(mπp)/m²), created (the shears), split (two cells at one pitch — measured within 0.01 % of the joint map, charted once; the shipped presets place distinct ratios, so a live splitting needs a unison preset, #34). **The decays:** γ_k = α + β·(r_k² − 1), α = ln 1000 / `decay_s` — the fundamental's T60 IS the declared one (FLAG: the spec's α + βk² reads r, not k, as built); `decay_s` 0 declares no decay. **Swirl → κ** (+0.5 at full, a 20 ms one-pole): built as the spec's thematic binding; the author signs by ear.
* **The breath bow (§2.6, #11, #33):** per sub-step each bowed mode scaled by 1 + g·u, u = clamp((E_t − E)/E_t, −4, +1), g = 1/(τ·rate), τ = `bow_onset_s` (0.15 s; 0 removes the bow) — a limit cycle by construction. E_t per mode = (A·b_k)², A = level·(0.35 + 0.65·breath)·breath, b_k = |sin(kπ·pos)| (`bow_position`: the profile FIXED per patch — the from-CC `[ITERATE]` not built). Breath is the patch's CC (`breath_cc` 2; CC 11 its alias), or the press when `press_blows` (#19); a voice is bowed only while HELD with breath > 0 — silence with no input is gated. A mode whose decay outruns the servo's reach (γ_k > 1/τ) is not bowed; a resting mode is seeded at −20 dB (a declared drive). The steady state sits UNDER the target by the decay's share, E = E_t·(1 − γ_k·τ) — a proportional servo, not an integral one (#33; "the target corrected for the decay" and "μ from the breath's attack rate" carried). Helmholtz stick-slip stays deferred.
* **The orbit trace (§2.7, #24–#27):** Voxo's side — one point in `sub_steps` (default 8) into a 1024-point lock-free ring per traced voice (the mask by KIND, `voxo_set_trace`); `voxo_trace_poll` takes what is new, decimates it curvature-weighted to ≤ 16 segments, unit-normalized with the amplitude; the rendering bit-identical with or without it. THE PAIR per kind: the cell's, Duffing's and the rotor's (x, y); the lattice's (Σx, Σy); the strings and the winds — a chain, no single cell — (s, ṡ/ω), the phase plane of the sound itself (the spec's "the voice's cell", corrected). **The gesture route** (`desktop/src/orbit_trace.cpp`, no core change): the orbit at the note's cell centre from the layout probe's own table (the rolls unplaced), as tine (default, exact) or wake segments (sub-stepped, class by inheritance), budgeted at 24 segments a frame over all voices with the overflow merged within each voice (the gesture ABI bypasses the mapper's budget, so the bridge budgets itself); `trace_scale` 0.25 canvas heights per unit amplitude, 6 segments a voice (4–8), per KIND with the rotor on by default — the demo the spec asked for (`--trace-demo`: the rotor scribbling its chaos into Anod). The toggle is clean (the field bitwise). **The scope route** — the spec's live-composite overlay — is libsumi 1.3.0's `sumi_set_scope` (#27, the phase's one core touch, the author's ask): up to 128 screen-locked segments drawn by the composite over the medium or on the scope's dark glass with the medium hidden, LIVE PATH ONLY (the print, the export, the dip and the bloom never see it; the fixtures hold bitwise), off by default; and the settings window's miniature of the canvas (#25). FOUND, carried for the core's reopening: Anod's strikes are not bit-reproducible run to run through the gesture ABI (#25).
* **The Verlet chain (§2.8, #13):** M interior nodes (`string_nodes` ≤ 80, default 48), symplectic Euler with a per-node conformal damping (`string_decay_s`); the m-th mode rotates 2·asin(s·sin(mπ/(2(M+1)))) per step with s² = k·dt², so THE CFL BOUND s ≤ 1 keeps the top mode under Nyquist. Tuning couples M, k and dt: the chain is REDUCED per note to ⌊rate/(2f₀) − 1⌋ nodes and s re-derived — exact by construction (0.000 cent). The gate rejects a forced k·dt² > 1 (`string_cfl`, the lab's); the red control (1.05, `cfl_gate` off) non-finite within a second. The pluck is a triangle ADDED to the state (re-plucking a ringing string), released from rest; one pickup (`pickup` 0.25; the second pickup and the fret-buzz `[ITERATE]`s not built). A glide saturates at the bound (no re-base — the chain ripples as a retensioned string does). Cost: ten strings at 80 nodes 20.8 % of the callback — the structure-of-arrays SIMD `[ITERATE]` carried.
* **The hybrid string (§2.9, #14, #33):** a ring read N behind the write plus a first-order Thiran allpass, a one-zero loss (`loop_loss`, the KS averager declared) and a round-trip factor, into a bridge of 1–3 cells at `bridge_hz` × {1, 1.618, 2.618} with their decays. **The junction, corrected:** the spec's explicit "kick by c·s, reflect −s + c_back·v" PUMPS energy (the first probe went to infinity at any coupling); shipped is the scattering junction on the cell's SYNCHRONIZED velocity ỹ = y − εx/2 by the midpoint rule — F = (2s − Σỹ)/(1 + Σc/2), the reflection s − g·v̄, g = 1 the conserving junction — balanced EXACTLY per sample (60 s within 0.004 dB undamped; the ten-minute soak −0.018 dB). The bridge is HEAVY (`bridge_coupling` 0.002); its reflection phase is solved in closed form from the junction's one-sample response and folded into the fractional delay, so the fundamental stays on the note (within 0.144 cent) while the partials feel the body — up to 11 dB around the bridge's modes, the wolves as dips. **Passivity gated twice:** the load-time probe (the closed loop at C6 for 300 ms, every declared damping zeroed, rejecting growth over 1.03; `voxo_suzu_passive_bound` reads 1.0016) and the soak; the red control g = 1.05 with `passivity_gate` off. FOUND: the conserving junction is a passive POINT, not a half-line (the spec's "1.05× the passive bound" reads "5 % over the conserving point"). The wave-digital escape hatch unused: no patch failed on its merits.
* **Duffing and the chaotic modulator (§2.10, #15, #16):** `suzu::Duffing` puts the hardening cubic in the KICK, y += ε·(x + βx³) — FLAG: the spec's "g(y) = −βy³·dt" (a shear of y by itself) is not a det-1 map; the clang IS the detune, no compensation (C4 at β 8 +258 cents, settling to +0.00 by 2.5 s; charted). The drive by the press at `drive_ratio` × the note; swept, it bifurcates at 0.87 into a period-multiplying comb (the Ueda-like storm's drive ratio and β `[ITERATE]`, the author's ear). **The modulator:** a double pendulum under RK4 in double at a fixed 0.005 sub-step with its kinetic energy PROJECTED onto the trigger's value after each control step (FLAG: the spec's "leapfrog" is not symplectic for this non-separable Hamiltonian — measured, it drifted and a hard kick went to NaN); the energy set at the trigger from the velocity (a swing below the flip energy, a tumble above), sin θ₂ smoothed 20 ms per block, a unit of pendulum time 50 ms / `mod_rate`, ONE target (`mod_target`: the cutoff ±3 octaves, the coupling +0.5, the rotor's K +1, or the drive, × `mod_depth`) — the mod-matrix corner left as one row; one pendulum per patch (per voice `[ITERATE]`).
* **The acoustic bore (§2.11, #18, #21–#23, #30):** Webster's system on the staggered Yee grid — p at the nodes, u at the half nodes, in the units where the mouth's characteristic impedance is 1, S(x) a per-node weight — symplectic Euler: the chain in acoustic clothes. Profiles: the cylinder; the cone S ∝ (x₀ + x)² closed at the truncated apex (the sax's — normalized at the REED end, a MOUTHPIECE holding the missing apex's volume, Benade's rule; the truncation `cone_apex` 0.25 decides the register, the first peak of a deeply truncated cone being its weakest, #21); the Bessel flare S ∝ (1 − x/x₁)^{−γ} from `bell_start` (the trumpet's — its peaks MEASURED at load and the note placed on the `partial`th, the third, #22). THE CFL: λ ≤ 2/√μ_max with μ_max the weighted Laplacian's top eigenvalue by power iteration — derived, never trusted; `bore_cfl` forces a multiple, the gate rejects over 1, the red control 1.05× non-finite in 57 sub-steps. PITCH IS BORE LENGTH: n = ⌊λ_max·rate/(2f₀) − ends⌋ cells and λ absorbs the fraction (the KS-delay move); `bore_nodes` 128 by default (≤ 256) is the cap — the sax's range from C2, the trumpet's from C3; below, the bass thins (256 cells or a sub-rate `[ITERATE]`, #23). THE SERIES on the primitive: the closed–open cylinder on the odd integers within 0.0 cent, the open–open on the integers within 0.0, the pinned cone with its mouthpiece within 0.1. THE ENERGY in the staggered form the leapfrog conserves exactly holds the closed lossless bore within 0.0003 dB for ten minutes. THE ENDS are declared ports: closed the mirror; open p = ∓z·(u − lp(u)) with the one-pole at `bore_corner_hz` — a radiation resistance rising as (ka)², a non-negative real part at every frequency (it only absorbs), the bore's own selectivity between its registers; FOUND: the port's reactance is an END CORRECTION the tuning counts (the flute read 81 cents flat without it). `bore_wall_s`, a declared per-node damping (the radiation alone left the low notes lossless and slow to speak); the sax's corner and wall loss SCALE WITH THE NOTE (every note the A3 cone in miniature, #21). The bore is LINEAR; v1 brassiness is `brass`, a §2.4 shear in the bell's last cell (p −= brass·u³, det 1, declared); toneholes and the shock front (its (γ+1)/2 coefficient recorded) deferred (§13.7). Carried: the grid's shortest waves under the jet's switching hold 6–21 % of the bore's pressure energy, inaudible at the mouth — a gentle grid-scale damping `[ITERATE]` (#30); the ports' discrete power at the half step (#19).
* **The reed and the lips (§2.12, #20–#23, #33):** one kick–drift cell with a pressure force and a stop — v −= ω²y, v += ±g·Δp, y += v, v ×= damp — inward (the reed, closing with Δp = P_mouth − p[0]) or outward (the lips); the opening h = h₀ + y clamped at 0, the aperture A·h, g = h₀ω²/P_close (the rest opening closes at the declared pressure). **The junction, the hard 20 %, solved IMPLICITLY:** Bernoulli's q = A·h·√|Δp| against the bore's one-step impedance is a quadratic (`flow_implicit`, the closed form) — the naive form (q from the previous step's Δp) is the spec's explicit coupling that blows up though the continuous system is passive, and it is the red control (`valve_naive`). THE LEDGER by arithmetic: the valve's swept volume ±S_r·v moves air (its mass S_r/g); its energy in the kick–drift's own conserved form, (S_r/g)·½(v² + ω²·y·y_prev); the load-time probe blows soft and hard with the bore's losses zeroed and a retune a fourth up, and asserts E_bore + E_valve ≤ 1.05·∫P_mouth·Q after 30 ms (`valve_gate`). THE STOP is the reed's only (inelastic — a loss; without it the first register let go above C4); the lips swing through. The breath's turbulence σ·P_mouth·noise on Δp (the reed's low-passed at 2 kHz). ζ in a real reed's range (`reed_area` 0.14 → 0.8). **The sax** (#21): the reed quasi-static (`reed_hz` 12000, Q 0.7 — the physical reed at 2–3 kHz `[ITERATE]`); P_mouth = P_ref·(1.1 + 0.55·breath), from just under the threshold with an 80 ms attack (the author under the first map: "I hear nothing"); its level follows the breath (0.32 + 0.68·breath, a declared 10 dB where the lips' physics gives 14); a released voice's mouth goes to zero. **The trumpet** (#22): the lips at `lip_ratio` 0.95 of the note at CC 74 centre (an outward valve plays above its resonance); CC 74 bends the resonance ±`lip_range` octaves — THE EMBOUCHURE `[ITERATE]` resolved: CC 74 is the register key (0 → the peak below, −772 cents; 127 → the octave, +1276…+1296, on every note C3–C6); P_mouth = P_ref·(0.5 + 2.5·breath), a 20 ms attack. **Both calibrate their intonation at patch load** (C3, C4, C5, C6 blown 0.45 s at 1.4 P_ref, the period of the last 0.2 s against the note, the bore cut to cancel the pull, lerped in octaves; keyed by the embouchure's fields, ~150 ms, logged) — the PERIOD as the pitch measure (the autocorrelation's first maximum within 0.08 of its top, refined by a 4-cent Goertzel scan), not the lowest partial. Measured: the sax C3–C6 within 5 cents at breath 70. Carried, an engine `[ITERATE]`: the sax at A3 is quasi-periodic — the second harmonic split (440 Hz at −16 dB, +18 cents at 0 dB), ~4.6 Hz sidebands, +18 to +19 cents by the period, bistable about the calibration's pressure (#33; the cone's resonances are where a fix would go). Carried: the reed channel's flow inertia, toneholes and a register vent, the two-mass lips (#23).
* **The jet (§2.13, #19, #30):** found in five probes — (1) the acoustic DISPLACEMENT at the flue (a leaky integral of the bore's velocity, DC-blocked at 10 Hz) carries the jet, not the velocity; (2) τ = `jet_tau`·T·√(P_ref/P_mouth) — d/(αU₀) with the embouchure following the note (the delay in periods of the note at the reference breath; the physical jet with one distance `[ITERATE]`), Hermite-interpolated in a 4096-sample ring; (3) THE RECEPTIVITY BAND — a second-order bandpass (`jet_q`) on the displacement centred at ½/τ, the Strouhal selectivity: the register jump IS the band crossing the modes (a full-period delay alone locked everything into broadband saturation); (4) the labium's tanh partition Q_in = (Q₀/2)(1 − tanh(η − y₀)), η = −G·band(t − τ) + σ·U₀·white (chiff, filtered by the bore), `jet_offset` y₀ the asymmetry (the even harmonics; the odd still 20 dB ahead — the jet's asymmetry `[ITERATE]`); (5) THE DRIVE is the labium's DIPOLE, a pressure port at node 0 ∝ dQ_in/dt (a flow alone does no work at a pinned open end). **The power-limited port:** the dipole never does more work on the bore in a sample than the mouth does on the jet — a declared limiter — so the self-excited row holds by arithmetic (the stored energy ≤ 0.06 % of ∫P_mouth·Q over a phrase). The embouchure FOLLOWS THE NOTE, three laws: τ in periods; the gain G = e^{μd} (`jet_gain` 560 at A4) rising as f/440 (the code's law, and `voxo.h`'s comment since the author had it corrected; #19 writes f² once and f/440 after); the area falling as √(440/f). Breath: P_mouth = P_ref·r, r from 1/√`breath_range` to √`breath_range` about `breath_ref` 0.44 (the note in tune); **the press blows** (`press_blows`, default on): the mouth is the larger of the breath and the pressure, for the winds and the bow alike — an Osmose plays them (FLAG: the press then has two consumers on those voices, §3's one-consumer rule bent where the alternative was silence; a velocity floor for keyboards with neither `[ITERATE]`). Open–open at both ends. **The overblow is emergent, measured:** the breath ramp at A4, 0 → 127 over 12 s, climbs −283 → +265 cents then +1193 — the octave within 7 cents, autonomous; soft blowing flattens 32 cents (the τ-phase lag); in the browser the jump at breath 116/127 (#29). The output is the mouth end's volume velocity, scaled near −24 dBFS at A4.

### 13.3 Voice architecture & MPE mapping (§3 — the table as built, #12; for the author to sign by ear)
Through the normalizer, every dimension one consumer:

| dimension (MPE) | Suzu's consumer |
|---|---|
| strike (velocity) | the kick across the modes by the preset's profile — the plucked string's `pluck` in closed form, sin(kπp)/k²; the chain's triangle; the cell's orbit at A; the modulator's energy |
| press (channel pressure) | the output gain 0.35 + 0.65·p; the Duffing drive's amplitude; and, with `press_blows`, the mouth of the bow and the winds (#19). For the winds the press that blows is the mouth ALONE — the output gain holds at the breath controller's value (0.35 with none), since the lips' +14 dB and the gain's +9 dB compounded to +5.8 dBFS for three trumpet voices under a hard finger (the 2.0 release round, `DECISIONS_9 #16`); the bow keeps both. §3's "sustained small drive into damped modes" is NOT built (`[ITERATE]`; the bow is the press consumer where there is breath) |
| breath (CC 2, the patch's `breath_cc`; CC 11 as alias) | the bow's E_target per mode (#11); the winds' P_mouth (#19, #21, #22) |
| glide (pitch bend) | every cell's ε retuned per frame with the orbit re-based (the cell, the lattice — each partial on its ratio — Duffing, the rotor); the chain's s re-derived, saturating at the bound; the hybrid retuned per block; the bores re-cut per block |
| slide (CC 74) | the SVF's cutoff, ±3 octaves about the patch's; the trumpet's embouchure — the lips' resonance ±`lip_range` octaves (#22) |
| swirl (0xA0, poly pressure) | κ += 0.5·swirl per block (#10) |
| mod wheel (CC 1) | the rotor's K up to 2.5 (#15) |
| lift (note-off) | the release's declared contraction, the tighter of the patch's release and the mode's own; a released wind's mouth goes to zero (#21) |

Not built, flagged: the strike-position profile from the layout's cell (a struck bar — where the layouts know the cell) and the bow profile from a CC. The desktop's Sound section carries every knob of the current voice kind (INI `suzu_*`); the five modal presets are named by their physics; the source and the patch ride the QoL preset file. **Since Phase 10 (`DECISIONS_9 #9–#11`):** Voxo owns the PATCH TABLE (`voxo_suzu_patch_count / _name / _patch`) — the tablets' five patches and six fitted to the Versilian Community Sample Library's instruments (`tools/suzu_fingerprint.py`, `suzu_fit.py`, `suzu_compare.py`; `presets/*_vcsl.json`, pinned to the table by a test gate), one list for the desktop's *Suzu patch* picker and both tablets' Sound pages; the orbit trace is the host library `trace/` (the step-59 bridge under a pure-C surface), the tablets' Sound pages carrying its two routes and its scale.

### 13.4 Engineering constraints (§4 — Voxo's contract, inherited; #8, #10, #13, #17)
CPU by design, as the spec argued and as measured: sixteen cells with the SVF and a shear 3 % of the callback (0.09 ms worst of 2.67 at 128 frames / 48 k); sixteen lattices 3.5 % (the retune only when the pitch moves — the first cut's sine per mode per frame cost ten times that); ten 80-node chains 20.8 %; the storm with the sampler's heavy library layered under the chain and under the hybrid 0 XRuns, render max 0.80 ms. Per-sample recursions in the callback with no locks, allocations or logging (the Voxo contract, §12.1); the state steps at 2× the device rate; parameter changes at block edges, ε ramped per sample across a glide; FTZ/DAZ at the thread's first block; the synth and the sampler share the mixer, the bus, the master gain and Local Control — and sound TOGETHER under `VOXO_SOURCE_LAYERED` (#17). The structure-of-arrays SIMD layout is the carried `[ITERATE]` (#13). The GPU exception (the 2D membrane) stays deferred with its arithmetic (§13.7).

### 13.5 Tests (§5 — the audible gates, as they run in `tests/voxo_suzu_tests.cpp`, the bench and the lab; #8–#11, #13–#16, #18–#20, #24–#25, #27, #29, #31–#33)
* **Drift:** the bare cell at A = 0.5 for ten minutes — +0.0001 dB, +0.000 cent; the naive simultaneous update (det 1 + ε²) past the 0.1 dB bound within a second and +690 dB by the tenth minute — red.
* **Glide:** ±48 semitones over 2 s — 0.097 dB re-based (the gate 0.5); the plain form's ripple printed beside it (0.057 ramped, 0.062 stepped; the one-block jump 0.082 against 0.630).
* **Energy ledger:** the bell, 8 modes, κ 0.3, every decay zero, ten minutes — −0.0026 dB; decays restored, every T60 within 0.1 % of the declared.
* **Tuning:** the oscillator within 0.000 cent and the SVF's ring within 0.008 across MIDI 21–108 at 44.1 k and 48 k; the shears' calibration within 0.19 / 0.02 cent; the lattice's partials within 0.08 cent at any admitted κ; the chain and the bores exact by construction; the hybrid within 0.144 cent.
* **Denormal:** 64 released voices' tail flat to the end (this Apple CPU shows no cliff with FTZ off; the x86 boxes' to show).
* **The bow's limit cycle:** from silence within 5 % in 4.9 τ, from 2× in 1.7 τ, the two orbits agreeing to 0.00 %; zero breath → −66 dB in 2.2 s; the injected and the extracted energy within 0.000 % over a second; the servo that never takes at 1000× in 0.43 s — red.
* **The orbit trace:** ten rotor voices under the budget (peak 20 segments of 24, the overflow merged within each voice, none starved), the bridge 0.17 ms a frame; trace OFF bit-identical to no trace; the rendering bit-identical with the trace on; the scope on the live path only (the field and the print bitwise).
* **The CFL gate:** k·dt² > 1 rejected with its message; 1.05 bypassed non-finite within a second, 0.95 bounded; the sweep MIDI 21–108 at 0.000 cent by re-deriving M.
* **The lattice load gate:** the compensation's and the sampling bound's conditions; the sampling bound's red on the primitive (two cells 5 % over κ*: λ_max 4.106 blows up at once, 5 % under bounded); the compensation's red through the ABI (the fundamental 21 cents off); mode splitting charted once (`tools/mode_splitting_chart.py`).
* **Loop passivity:** the hybrid undamped ten minutes −0.018 dB; g = 1.05 bypassed non-finite in 31 s — red; the load-time probe rejects over 1.03.
* **The Duffing chart:** the clang (+258 → +0.00 cents) and the drive sweep's bifurcation at 0.87, beside the rotor's K sweep (`tools/chaos_chart.py`).
* **The bore's series:** three geometries within cents of theory (the cone with its mouthpiece within 0.1); the closed lossless bore within 0.0003 dB for ten minutes; the CFL red 1.05× non-finite in 57 sub-steps.
* **The mouth-power ledger:** the jet's stored energy ≤ 0.06 % of ∫P_mouth·Q over a phrase; the valves' probe E ≤ 1.05·∫P_mouth·Q at 30 ms (the naive junction 1e300× — red; bypassed, a bend trips it within the second); the ledger live in the inspection, k[9] ≤ k[8] always (#32: at worst 0.52 of the work for the sax, 0.14 for the trumpet).
* **The overblow:** the breath ramp jumps the octave within 7 cents, autonomous, on the desktop and in Chrome (the lab's `?gate=overblow`, the jump at breath 116/127); soft blowing flattens 32 cents.
* **The rotor's chart:** K swept 0 → 2.5 — the KAM transition audible, charted beside its visual sibling; K 0.3 confined to the island for ten minutes.
* **The intonation gates (58c):** the sax C3–C6 within 5 cents at breath 70; the trumpet's staircase on every note C3–C6; the released mouth silent.
* **Every synth voice ships with its sound profile** (the author's rule, #9): `midi-sink --dev --voxo-profile <dir>` strikes MIDI 21–108 at velocity 100; `tools/sound_profile.py` draws the level, the harmonics and the spectrogram — the figure in the step's evidence and drafted for the docs.
* **The web gate** (#29, #32): the wasm against a native build of the same flat surface without fused multiply-add — bit for bit for the cell, Verlet, the hybrid, Duffing, the rotor and the sax; within −80 dB for the lattice, the flute and the trumpet (the math libraries' last bits); the negative control red. **The page gates** in headless Chrome, every panel (#31, #33): `tools/suzu_lab_gate.mjs`.
* **The combined stress** (#17): the fifteen-channel storm with a heavy library layered under the chain and under the hybrid — 0 XRuns, 0 dropped, the visuals at or above 94 fps.

### 13.6 Docs & citations (§6 — the chapter, drafted; #9, #28–#31, #34)
The operator book gained a phase-space chapter mirroring the canvas one — shipped at step 67 as the Suzu book, `site/src/content/docs/suzu/` (nine chapters: the thesis and the class table, the cells, the modal voice, strings and chaos, the winds, the orbit trace, the sound profiles, the patches, the lab), "amplitude-stable" where the canvas says "area-preserving"; the rotor's page cross-links the visual Chirikov page and the operator draft links back (one theorem, two senses). **The lab is the chapter's live scene:** Suzu on the web as itself — all of Voxo in a standalone wasm with no device (`voxo/src/backend_none.cpp`), driven by an AudioWorklet that owns it, messages only (no SharedArrayBuffer: GitHub Pages serves it as it is), `web/suzu/` → `build-web/suzu-dist`; `voxo_suzu_inspect` (0.14.0) copies each sounding voice's state for the pages, `voxo_trace_recent` (0.15.0) the ring at full density with two aux channels per kind; a page per family on a shared core (`web/suzu/site/lab-core.js`), each with its red controls on the engine's own lab parameters, the worklet muting any quantum at full scale and restarting the engine on a blow-up (Voxo's clip would otherwise hide a runaway as a full-scale wave). The pages joined the site at step 67 (`DECISIONS_9 #2`): the lab lives at `/marble/suzu/`, composed from the same tag as the marble app, every page embedded once through the book's `<Lab>` component (the drift check requires it); Safari and Firefox were not run. The citations as the spec lists them (Chamberlin; Gordon–Smith; Zavalishin; Chirikov and Greene; Bilbao; Falaize and Hélie; van der Pol; McIntyre, Schumacher and Woodhouse; Webster; Fletcher and Rossing; Verge, Fabre, Hirschberg et al.; Karplus and Strong, Jaffe and Smith; Cadoz, Luciani and Florens; Verlet; Duffing) plus Benade for the truncated cone's chamber (#21) — verified at writing, never from memory.

### 13.7 Deferred (§7, corrected; revised at the v3.0 fold)
This arc takes from §13.7: the stick-slip bow, the 2D membranes and gongs, the editable ratio tables, the toneholes and the register vent, the shock capture, the two-mass lips, the port-Hamiltonian network, the chain's SIMD, the modulator per voice and its matrix, the reed's flow inertia, the unison preset. What stays deferred after it: the physical reed at 2–3 kHz with the lip's damping; the wave-digital junction (unless a patch asks); the Ueda-like storm's mapping; the physical jet's one distance and its asymmetry; the ports' discrete power; a velocity floor; the bass below the cell cap; the bore grid's gentle damping; a Suzu circle-map voice; compute-shader audio.


**§13.8–§13.14 — Suzu's deferred physics and the new voices** *(Phases 13–14, steps 78–86; transcribed at the fold)*
Every element below declares its class in the §13.1 table, passes its red control, ships its sound profile (DECISIONS_7 #9) and feeds `trace/`.

### 13.8 The four-lane engine and the ×4 half-band oversampler *(AS SHIPPED, step 78, 2026-10-18 — `DECISIONS_12 #1`–`#2`; Voxo 0.18.0, additive)*
**The contract first (#1):** the web gate's native reference is the SCALAR path — `voxo_nofma` with `VOXO_NO_SIMD` and `VOXO_NO_FMA` — and the lab's wasm compiles with `-msimd128`; the gate stays bit for bit for the chain and the lattice (measured: the cell, the chain, the hybrid, Duffing, the rotor and the sax 100 % bit-identical; the lattice, the flute and the trumpet within their declared libm tolerances as before). **The engine:** one header, `voxo/src/simd.h` — four float lanes on NEON (arm64: the Mac, the iPad, the Tab), SSE2 (the x86-64 baseline the Windows and Linux lanes build; AVX2's eight lanes are not taken: they would need a flag the release lanes do not set) and WASM SIMD128, with a scalar fallback that performs the same operations in the same order; `mul_add` / `mul_sub` fused where the scalar compiler's contraction fuses (the shipping native builds, the first product of `v·damp + s2·lap`, the Laplacian's `u[i+1] − 2u[i]`, the rotation's two) and plain under `VOXO_NO_FMA`; the helpers forced inline (a Debug build left as calls cost ten times the scalar). The Verlet chain's sub-step in four lanes (`VerletString::step`, the scalar twin kept as `step_scalar`): the same two passes four nodes at a time, the tail scalar. The modal lattice in four lanes (`lattice_rotate_simd`, `lattice_contract_simd`; the scalar twin `lattice_step_scalar`): the coupling kick from the pre-update positions in the scalar's order, the rotation — sheared or plain, the fold's branches as lane selects with the scalar's expressions — four cells at a time through de-interleaved loads of the (x, y, ε, A) structs (NEON's `vld4q`; shuffles elsewhere), the contraction a second pass once the bow's per-mode factors are known (the bow between the two, as the scalar did), the sums in scalar order; a muted mode comes out as it went in. The bit-for-bit evidence: the no-FMA reference AND the shipping FMA reference records bit-identical before and after the refactor, the strobe returns of step 75 bit-identical, the suite's twin comparison 100 % over 4800 sub-steps. **The oversampler:** `voxo_suzu_params_t.oversample` (2 default, 4) — the cell family's section (voice kinds 0 and 1: the cell, the lattice, their shears) steps 4 times an output sample at 4, the cells, the shears and the SVF at that rate (ε, the SVF coefficient, the decays, the bow's rate and the release's contraction threaded through the strike and the block; the mode ceiling stays the OUTPUT Nyquist), decimated through two cascaded 2:1 half-band stages (`suzu::HalfBand`, `voxo/src/halfband.h` generated by `tools/halfband_design.py`: a Kaiser-windowed half-band of 47 taps, β 5.65, the centre 1/2 and twelve odd taps a side, every other even tap zero — the stopband [0.30, 0.50] of the input rate 64 dB down, the passband ripple 0.005 dB); the 2× section keeps its two-tap average, bit for bit. The strobe's sub-steps stay the 2× section's (the bus precomputes the wraps at that rate): at ×4 a wrap falls in the first or second quarter with its fraction rescaled. The chains, the bores and the chaos voices keep the 2× section (their grids are tuned to it). **The prewarp:** `suzu::prewarp(omega, dt)` = 2·sin(ω·dt/2), the shared helper the Boris gyroscope's spring and field will take (B_num = (2/dt)·tan(ω_B·dt/2) its companion); `eps_for` keeps its own arithmetic so every shipped ε stays bit for bit — the two agree to 1e-7. **The gates (the suite's blocks 15, 29–31):** sixteen chains of 80 nodes against 6 % of the callback on the Release build (a Debug build prints the figure); the four-lane paths against their scalar twins (bit for bit); the ×4 alias floor on the fold at full drive at C7, the band [rate/4, 0.4·rate] below −60 dB of the fundamental against the ×2 average's (the red); the lattice on pitch at ×4; the prewarp. The storm on the chain, the trumpet and the fold at ×4 from the Release build at 0 XRuns; the fuzz and the strobe fuzz clean; the lab gate green — which found step 75's breaker had silenced the lab's two blow-up red controls (the strings' CFL bypass, the winds' naive junction): the bus cuts a non-finite block before the worklet sees it, so the worklet now reads the breaker's trips (`sw_breaker_trips`) and treats a new one as the blow-up it is (#2).

### 13.9 The membrane, the plate and the gong *(AS SHIPPED, step 79, 2026-10-19 — `DECISIONS_12 #3`; Voxo 0.19.0, additive; `voice_kind` 9)*
**The grid.** nx × ny interior nodes (displacement u, momentum p) inside a ring of zeros — THE BOUNDARY IS DECLARED SIMPLY SUPPORTED: u = 0 and Δ_h u = 0 on the edge, so the biharmonic is the five-point Laplacian applied twice through the same ring and the mode table is λ_mn = 4 sin²(mπ/(2(nx+1))) + 4 sin²(nπ/(2(ny+1))), ω²_mn = c²λ + κ²λ², grid units (h = Δt = 1 at the 2× section's rate); the rows 13/16 of the columns (a rectangle: the degenerate pairs split). The step: the kick p += c²Δu − κ²Δ²u from the pre-update u (det J = 1), the declared contraction p ← (γ₀ + γ₂Δ)p (the mode at λ contracts by γ₀ − γ₂λ: `decay_s` the fundamental's T60, `decay_bright` the extra rate per (λ/λ₁₁ − 1), the lattice's law; the factor acts on the momentum, whose rate is twice the amplitude's, so it is set at twice the rate — the suite reads the T60 back within a percent), the drift u += p. Four lanes a row (`simd.h`, the neighbours as unaligned loads, the tail scalar), the stretching summed by lane then in order: the scalar fallback is the same arithmetic, and the web gate's reference is bit for bit (measured: 100 %). `suzu::Membrane<CAP>` in `suzu.h`, `MEMBRANE_MAX` 16 (the per-voice sheet), `PLATE_MAX` 64 (the bus). **Pitch and the cut.** The note is the (1,1) mode through c², continuous (a glide retunes per frame; the grid's λ₁₁ and λ_max are cached at the strike so the glide calls no libm). `drum_kappa` is the biharmonic SHARE of the (1,1)'s stiffness — c²λ₁₁ = (1 − s)ε², κ²λ₁₁² = s·ε² — 0 a membrane (partials as √λ), 1 a plate (as λ), the note on the (1,1) at any share. The CFL number λ_2D = sqrt(8(c² + αE) + 64κ²) is ε₁₁ times a factor of the grid, the share and the swell alone (`cfl_factor`), with the continuum's 8 and 64 (the grid's own λ_max a hair under); THE GRID IS CUT PER NOTE: the largest nx ≤ `drum_size` (4..16) with λ_2D ≤ the bar at E_max, and on the 2 × 2 grid past it ε saturates at the bar (the pitch saturates, as the chain's at its bound). The proposal's "0.000 cent by scaling the grid" is dropped (the roadmap's gate delta a). **The strike, the cap, E_max.** A raised-cosine bump of momentum (radius a tenth of the sheet) at (`pluck`, `pluck`), scaled by ε₁₁ · `level` · 24 so the pickup's peak reads the level across the keyboard (measured: 0.24 … 0.52 at level 0.3, the smaller grids above C7 a few dB louder); the pickup reads u bilinearly at (`pickup`, 1 − `pickup`). `drum_cap` is the strike-energy cap in full-velocity strikes: after every strike the plain energy (the gong's term included) is held at or under H_cap by scaling the momentum to the remainder, so a re-strike on a ringing drum never exceeds it; since H ≥ ½(c² + κ²λ₁₁)E, E_max = 2H_cap/(c² + κ²λ₁₁) by arithmetic (the roadmap's E ≤ 2H₀/c² tightened by the biharmonic term), and the gate reads λ_2D there (gate delta b). **The gong.** E = Σ|∇u|² = −Σ u·Δu is read off the Laplacian pass; the kick's c² becomes c² + αE — an exact momentum shear, one multiply-add a node. `drum_alpha` is the SWELL: c² rises to c²(1 + α) at E_max (α_raw = swell·c²/E_max, re-derived under a glide). Measured on the Gong patch (swell 0.8, cap 1.5): C3 at velocity 127 reads +107 cents over the first 60 ms against +2 at velocity 30, both on the note a second later; the undamped gong's energy stays within 0.33 dB over a second. **THE GATE AND ITS BARS.** `MEMBRANE_CFL_GATE` 1.95 for the linear sheet; `MEMBRANE_CFL_GATE_GONG` 1.80 when the swell is on — MEASURED: a pure membrane at swell 1 displaced to exactly E_max at 1.95 with the top mode seeded diverged (the (1,1)'s stretching modulates the top mode's stiffness by the whole swell at twice its own rate, non-adiabatic that close to the edge), held at 1.90 for swells to 2, and the κ share lowers the depth further; the real strikes of the gong patch held at every note. THE REDS: through the ABI `drum_cfl` forces λ_2D at E_max as a multiple of the bar (`drum_cfl_gate` on: 1.05 rejected with its sentence, 0.95 admitted; bypassed: 1.05 trips the breaker within a second — the top mode past ε = 2); the class's own: the sheet tuned to 1.95 at the cap, displaced to 1.2× the cap's stretching with the top mode seeded (λ_2D 2.045 there), diverges within 100 steps; at the gong's bar the same sheet holds 200 000 steps within 0.8 dB. The undamped 8 × 6 grid's exact invariant H̃ = ½(Σp² + c²E + κ²Σ(Δu)² + Σp·(c²Δu − κ²Δ²u)) drifts −0.00016 dB over ten minutes at 96 k (the bound 0.005; the plain energy's staggering wobble 0.27 dB beside it). **The bus plate.** One `Membrane<64>` on `voxo_t`, stepped once an OUTPUT sample (1×, not the roadmap's 2×: the bus's rate); `plate_on`, `plate_size` (16..64; a `VOXO_MOBILE` build caps 32), `plate_hz` (the (1,1)), `plate_kappa` (the share), `plate_decay_s`, `plate_bright`, `plate_gain`. Its tuning is CLAMPED under the gate at the output rate — the share first, then the pitch — never refused (a bus element), with a log line from `voxo_set_suzu_params` that says what it took; measured: 64² at 110 Hz with the share 0.003 reads λ_2D 1.48, 32² 0.46 (the mobile tier; gate delta c), a share of 0.5 at 64² is clamped to 0.0057. Every Suzu voice writes its mono block beside the sum (`plate_src`, max_voices × 1024 frames; a larger block skips the plate) and injects it a sample at a time at ITS POINT — the shell's `voxo_set_strike_point(channel, note, x, y)` before the note-on (the iPad's and the Tab's play surfaces hand the layout cell's centre; the desktop's keyboard and a bare wire take the default map: the note along x, a golden-ratio scatter along y) — scaled by (n/64)² (the response goes as 1/n²: 32² reads within a dB of 64²); the two pickups at (0.33, 0.62) and (0.67, 0.38) join the stereo sum before the reverb at `plate_gain` × 0.045 (measured: 10 dB under the dry harp chord at 1); the plate rings on by its own contraction and clears with the breaker. The roadmap's `sumi_layout_cells` does not exist — the shells' probes carry the cell (`sumi_cell_info_t.cell_center_x/y`). **The plumbing.** The web table and `sw_strike_point`; `KIND_NAMES` 'membrane'; the preset's eleven fields (`presets/SCHEMA.md`); the desktop's *Suzu drum …* rows under voice 9 and the *Suzu bus plate* section under every voice, the INI keys `suzu_drum_*` / `suzu_plate_*`, the session's fields; the inspection's kind 9 (u, p row-major; k[0..11]); the trace's aux pair (E, c²_eff); the strobe at the bass (261 returns in 2 s at C3; `strobe_membrane.csv`, the chart's last panel); the patch table's fourteen — *Drum*, *Gong*, *Harp on a soundboard* pinned to `presets/drum.json`, `gong.json`, `harp_soundboard.json`. THE BENCH'S `--preset` now reaches the Sound settings too (its Suzu block is the run's patch, the file left unsaved): before this it set the engine's half alone and a storm "on a preset" played whatever the settings held — step 78's three storm lines were the settings' patch. **Cost (Release, NEON).** Sixteen membranes at 16 × 13: 16.5 % of the callback (the bar 30 %); the 64² plate under one voice 16.3 % (the bar 25 %); 32² 4.5 %. The storm on the drum, the gong and the soundboard at 0 XRuns, render max 0.79 ms. The Android emulator from the RELEASE APK: the drum and the gong at 0 device XRuns (render max 10.0 / 2.5 ms of a 20 ms burst), the soundboard 1 device XRun (15.8 ms) — the debug APK's −O0 engine reads hundreds; the iPad (M4) the drum, the gong and the soundboard at 0 XRuns, render max 0.58 / 0.65 / 0.35 ms of the 2.667 ms buffer. Carried: the Tab's own run (unplugged), the clamped (not cut) plate under a glide of its pitch knob, the AVX2 lanes.

### 13.10 The stick-slip (Helmholtz) bow and the strike position from the cell *(AS SHIPPED, step 80, 2026-10-19 — `DECISIONS_12 #4`; Voxo 0.20.0, additive)*
**The bow.** `bow_mode` 1 (the servo of §13.2 stays 0, untouched: bit for bit under its knobs) puts the hyperbolic friction curve μ(v_rel) = μ_d + (μ_s − μ_d)·v₀²/(v₀² + v_rel²) — μ_s 0.8, μ_d 0.3, v₀ a tenth of the bow's speed, the sign smoothed over a hundredth of v₀ so Newton has a derivative — on the string's bowing point, SOLVED IMPLICITLY against the point's mobility G (v = v_pre + G·f(v_b − v), three Newton steps with the slope floored at ¼ where the curve's falling flank folds the equation) and BOUNDED: the point never crosses the bow's speed in a step (it sticks there: the capture) and never takes more than the grip G·F·μ_s. `suzu::StickSlip`. **Two things the proposal left out, found on the probe:** (1) THE HYSTERESIS (McIntyre–Woodhouse) — the curve against the load line meets three times and the string stays on its branch: stuck, it stays stuck while the force to hold it is within the grip; slipping, Newton from the free velocity finds the slip root, and the capture is the bound. Without it Newton picked the slip root every step and the string only whistled (several slips a period). (2) On the Verlet chain the bow acts BETWEEN the kick and the drift (`VerletString::kick` / `drift`, the fused `step` bit for bit): applied after the drift, a strong bow pinned the node into a static deflection — the string's own kick cancelled the bow's velocity before the node moved. **Three bodies.** The modal lattice (kind 1): the point's velocity Σ b_k·w_k (w_k = −ε_k·y_k the cell's velocity of x per sub-step, b_k = sin(kπ·`bow_position`) the profile), G = 1 by the modal masses' choice (Δw_k = f·b_k/Σb_k²), the kick on y after the rotation. The Verlet chain (kind 2): the node at `bow_position` of the length, G = 1 (its mass). THE HYBRID STRING (kind 3) — the textbook waveguide bow, two taps on the loop: the ring holds the right-going wave as it will arrive at the bridge with the nut's inversion folded in, so the bow at β sees R(L_b), L_b = (1 − β)·n/2, about to pass it, and −R(L_b + β·n) coming from the bridge side; v = v_i⁺ + v_i⁻ + f/(2Z), Z = 1 in the loop's units (G = ½), each outgoing wave takes f/2; the loop gains exactly f·v a sub-step. The delay line is dispersionless and the Helmholtz corner stays sharp; the chain's lattice dispersion smears it (measured: the stuck share and the pitch wander). The roadmap's "the string's or the mode's impedance" is read as both: the chain's node and the waveguide's 2Z. **The controls.** The mouth (breath, or the press under `press_blows`) is the SPEED: v_b = `bow_speed`·mouth·v_ref, v_ref = level·ε₁ (the lattice), level·2f₀/(rate2·(1 − β))·2.3 (the chain), level·0.45 (the waveguide: the loop's velocity waves) — the Helmholtz amplitude reads the level. The press and the velocity are the FORCE: F = `bow_force`·v_b·K·(½ + ½·vel/127)·(1 + press), K 1 / 6 / 15 (the lattice / the chain / the waveguide, measured so 0.5 sits inside the Helmholtz range). No mouth lifts the bow (F = 0, v_b = 0). "The press as a sustained drive" (§13.3's [ITERATE]) is this: the press drives the bow. **The ledger** — the winds' discipline by arithmetic: with the sign of f the sign of the relative velocity before and after the step (the bound), the string takes f·v ≤ f·v_b = the bow's work. The string's gain is read at the SYNCHRONIZED velocity — the leapfrog invariant's change under a kick at the half-step is Δp·(the mean of the half-step velocities around it): the chain's balance was wrong by the restoring kick's half-share until it was (measured exact with every loss zeroed: dE = dGain per quarter second); the lattice's adds ½Σ b_k ε_k² x_k; the waveguide's is exact as f·(v_h + f/2). The inspection carries the force, the speed, the work ∫f·v_b, the gain ∫f·v and the branch. **The red:** the three Newton steps taken raw — no bound, no branch (`bow_naive`): a converged root never crosses the bow (its force has the sign of the relative velocity it is evaluated at) and the explicit force's excess is dissipated as friction heat (measured: neither crosses the loose line E_held ≤ ∫f·v_b, which sits a hundred times above the energy held), but the raw steps on the folding flank do: the string takes more than the bow gives — on the chain 0.156 against 1.46 of work, on the waveguide 7.0e4 against 6330. **Measured at A3, the stuck share at the force 0.25 / 0.5 / 1.0:** the waveguide 21 / 48 / 68 % (the pitch +1 / 0 / −4 cents), the chain 49 / 70 / 85 % (0 / −15 / +16), both rising toward 1 − β = 88 % as the corner sharpens; the lattice at 0.5: 69 %, −13 cents (its pull on the pitch is strong: −42 cents at A2). Under the raucous range the chain pins (the point held, the tone growls); under the playable range every body whistles. **The strike position from the cell** — `voxo_set_strike_pluck(channel, note, position)`: the play surfaces hand the touch's distance from the cell's centre in cell radii (the centre plucks the middle, 0.5; the rim the bridge, 0.05) before the note-on; it stands in for the patch's `pluck` on the chain's triangle, the plucked lattice's sin(kπp)/k² and the membrane's strike point; the bar keeps its fitted 1/m (the VCSL fits would move: "a struck bar's" profile not built). THE WEB GATE: the chain and the waveguide under the bow are bit for bit; the lattice under the bow is NOT in the script — the branch is a discontinuity, and the lattice's per-sample sinf (the libm difference its tolerance covers) flips one at a sub-step, which moves the snapshot's decimated trace past any tolerance while the audio stays within −80 dB. The patch table's sixteen: *Bowed string (stick-slip)* (the waveguide) and *Bowed chain*, pinned to `presets/bowed_stickslip.json`, `bowed_chain.json`; the chart `--voxo-chart`'s `bow_force` (the press ramps the force at full breath); the profiles. The storms on both at 0 XRuns; the emulator's release APK at 0 device XRuns on both (the spike sends no press, so the bow lifts there: the load's evidence); the iPad took the build and locked before the launches — "bowing under the finger" on the tablets is the author's.

### 13.11 The bore v2: the toneholes, the two-mode lips, the brass shock, the leak *(AS SHIPPED, step 81, 2026-10-19 — `DECISIONS_12 #5`; Voxo 0.21.0, additive)*
**The toneholes** (`tonehole_mode` 1; the flute and the sax). A hole is a branch port at a pressure node of the Webster–Yee grid: its air an inertance L_h (a momentum q_h kicked by the node's pressure at the half step, as u is), radiating through a resistance R_h that acts above a corner at twice the bore's (the one-pole form of the ends': a hole radiates little at the fundamental — a FLAT resistance ate the flute's, measured); the node's continuity takes q_h beside its neighbours'; a closed hole is a wall (q_h = 0). Both terms are declared: the inertance a symplectic mass, the resistance a conformal loss; the bare bore with lossless holes holds its energy to three digits over a second. The hole's size against a bore cell's: 1/L_h = HOLE_H0·S at the node per unit `hole_size`, HOLE_H0 = 1/8 — a flute's hole (10 mm across, 8 mm effective height, in a 19 mm bore at the grid's 3.6 mm cells) is eight cells' inertance. THE PICKUP: the bore's output is what leaves it — the far end's velocity PLUS the open holes' flows — because the far end alone sits past the lattice, where the fundamental is evanescent (read there, the holed flute's fundamental sat 40 dB under its fifth partial while the oscillation inside was right). The hole lattice's cutoff at these sizes is near 1.7 kHz (the flute's); above it the partials see the whole bore. A hole sits at a FRACTIONAL cell — its conductance split (1 − f) : f over the two nodes about it (two inertances in parallel are one at the weighted place to first order): the top register's semitone is a cell and a half, and a glide moves the holes smoothly. THE CHART is the core's Boehm rule (`sumi_layout_boehm_note`, copied — Voxo never links libsumi): nine keys as the fingering CCs 110–112 (bits 0..2) and 114–119 (bits 3..8), global on the master channel under MPE, any channel otherwise; the highest open hole sounds, D4..C♯5. In hole mode the bore is cut for the register's lowest note (D4; D5 with the vent) and hole j (1..11) sits where the base's j-th semitone would end the bore — n·2^{−(j + corr_j/100)/12} cells, on a cone (n + a)·2^{−…} − a with a the apex's cells (the series counts from the apex; without it the sax's holes were a quarter-tone and more off); for the note's semitone j the holes 1..j are open, the rest closed. THE REGISTER VENT is a real register key — a 2 mm vent in a 15 mm neck, 5 mm deep: 90 cells' inertance (HOLE_VENT_H0 = 0.011·S) at an eighth of the length, open from the second register; its detune (+27 cents on the flute, +48 on the sax) is measured at patch load and cut out of the base. A note the keys disagree with (a keyboard, a wire with no keys) takes the v1 cut with the holes closed (`k[12]` −1); the holes serve TWO registers, D4..C♯6 — at D6 and above a hole's impedance is the bore's own (the lattice's cutoff) and the holes no longer cut: the v1 cut sounds the note. THE CALIBRATION, the woodwind maker's, at patch load, per register, cached per patch slot by the holes' parameters: each of the chart's eleven fingerings placed on the register's bore (cut as the strike cuts it) and the bore pulsed, the first peak read against the chart's note by a coarse-then-fine Goertzel scan, the hole moved by the difference — twice over on the flute (228 ms); on the sax ONCE passively, then TWICE BY BLOWING (the reed's note on each hole bore at the breath map's middle, 0.4 s a blow: the passive peak and the blown pitch differ by up to a quarter-tone on the hole bores, and not by the plain bore's pull) — 770 ms. Measured at breath 70: the flute's first register within +5…+18 cents (the +18 the plain cut's own at that breath), the second within +12…+21; the sax's within ±27 (the reed's note jitters ±15 between measurements), the second register −30…0. The inspection: `k[11]` the holes open as twelve bits, `k[12]` the semitone (−1 the v1 cut), `k[13]` the register. The glides keep the base gliding and the holes riding. The hole mode's room under the CFL bound: a hole adds h = HOLE_H0·`hole_size` to its node's row (Gershgorin: μ ≤ 4 + h), so λ ≤ λ_bound/√(1 + h/4) — measured on the bare bore (h = 1/8 at 0.999 of the bound grew, 0.99 held). `hole_size` 0.1..4 (1 dflt), `hole_loss` 0..2 (0.3). The proposal's "pitch is bore length in v2" is read as: the register's base is the bore's length, the note the holes'.
**The leak** (`bore_leak`, the sax's bell's; dflt 0.4, 0 the lab's red). The proposal's remedy for the quasi-periodic A3 (DECISIONS_7 #33) was the reed channel's acoustic mass; `reed_inertance` SHIPS as proposed (L_r·dq/dt in the implicit junction: the same quadratic with Z + L_r and L_r·q_prev on the pressure's side; dflt 0) but measured partial — it cleans breath 70 at L_r 8 and degrades at 30, and the sidebands at ±15…17 Hz off the harmonics were not the reed's. THE MECHANISM is an undamped slow mode of the bore (the one-pole radiation port leaves the bore's slow modes lossless; a wall loss of 0.3 s cleans everything; a flat radiation breaks the tuning): the cure is a RESISTANCE ON THE BELL'S SLOW FLOW — a one-pole at 40 Hz on u at the open end, p −= (z_l/S)·lp(u): positive-real (it only absorbs), the fundamental barely sees it. A3's sidebands: −62…−63 dB under the harmonics at breath 50/70/100 at z_l 0.4 (0.3 left breath 70 at −45 once the sax's bore was back to v1 without the shock; 0.6 breaks at breath 100; the red, leak 0: +13 dB — the sideband ABOVE the harmonic). ITS REACTANCE is a compliance above the corner (−z_l·ω_l·ω/(ω_l² + ω²)): a NEGATIVE end correction the cut must count (`Bore::leak_correction`, end_correction's twin) — uncounted, the flute read +40 cents sharp. Measured on the profiles, the leak at the flute's two open ends and the trumpet's bell cost them their bass (the flute 22 dB at C2, 16 at G2, 12 at C3; the trumpet 7 at C2): their bass is the radiation's near-lossless, and the leak at 65 Hz is a hundred times it. SO THE LEAK IS THE SAX'S ALONE; the flute and the trumpet take none. A leaky bell takes ROOM UNDER THE BOUND (`SUZU_LEAK_ROOM` 0.96): its reactance sits outside the Laplacian the bound is read from, and the sax at G7 (13 cells) blew up at λ 0.989 under z_l 0.4 — the profile's full-scale spike at one note, which no gate reads; at 0.96 of the bound the range 94..108 holds at 0.3, 0.4 and 0.5.
**The brass shock** (`brass_shock`, the trumpet's; dflt 1, 0 the v1 bore; `shock_limiter` the lab's, 1 dflt, 0 the red; the sax's cone keeps the v1 bore — the shock cost it 80 % of its render for no claim of the spec's). The (γ+1)/2 convective steepening as a LOCAL COURANT NUMBER on both sweeps — λ(1 + κ·p̄), p̄ the cell's mean pressure saturated at ±p_cap (κ = `brass_shock`·1.7, the physical value in reference pressures; p_cap 0.1 = 20 P_REF: the trumpet's bore at fff reads 16–18), the base λ leaving room (λ_max/(1 + κ·p_cap)) so the crests' speed never crosses the bound. The steepening on the momentum sweep alone was too weak; on both the output slew rises from 0.08 to 0.26 of the peak at shock 1. DECLARED DISSIPATIVE: a Burgers shock must lose energy — the flux limiter is a Lapidus viscosity in flux form (C_q·κ·|Δp|·Δp·S between the nodes, C_q 0.1), its loss Σ½S(p_old² − p_new²) logged per strike (`k[10]`) as a known positive term of the mouth-power ledger, where k[9] ≤ k[8] stands (measured under the shock at breath 127: the energy held 0.10 of the mouth's 39 of work). The blare: the trumpet at C4, breath 127 — the 6–20 kHz band against 0–3 kHz −22.5 dB with the shock and its limiter, −32 without the shock (+10 dB of blare); at breath 60 the limiter's loss is a fifteenth of the fortissimo's — the shock is the fortissimo's. THE RED (the limiter off): at κ the Gibbs ringing is in the bore — its profile 1.46 rough (Σ|Δ²p|/Σ|Δp|) against 1.01 limited — and the bell's radiation keeps it out of the output's high band (−20.4 against −22.5 dB: with the leak at the bell it had read 23 dB above, the leak's back-pressure feeding the ringing — the leak is gone from the trumpet); at 2κ the unlimited steepening is REFUSED by the valve gate at patch load — with the bore's declared losses zeroed the bore + valve held 1.1× the mouth's work (the gate of DECISIONS_7 #24, which caught this before a note was heard) — while the limited 2κ is admitted and holds its ledger (|p|max 12 P_REF). Under the limiter more steepening dulls rather than brightens (shock 2: −32 dB, 4: −37): the blare peaks near κ, the default. (The roughness is a RELATIVE measure — a linear bore reads 0.70, a limited shock at κ 1.0 — read against the limited bore's at the same note; its absolute value detects nothing.) THE LOCAL CFL THEORY was wrong for the trumpet as the cause of the ringing (its λ sits at 0.28, the cells capped at 128): the ringing is the dispersion at a shock; the saturation and the room stay as the safeguard. THE COST: the limiter's two passes vectorise (85 µs for thirteen trumpets a block), but a running sum for the loss serialised them — the trumpet rendered 790 µs a block against the v1 bore's 380 and the storm threw XRuns at thirteen voices; the per-cell loss terms are now written beside the updates and summed after with eight accumulators (615 µs; the loss identical to the printed digits). AND THE STRIKE: every wind strike ran the bore's power iteration twice (on the fresh 64-cell bore inside the build and again on the cut bore), 40–60 µs each — a block carrying fifteen strikes cost the trumpet 1.5 ms; the profile's μ_max is taken once per patch at calibration (`WindCal.mu64`), the flute's once per engine (the open–open cylinder's).
**The two-mode lips** (`lip_shear` 0..1 the shearing mode's share of the swing's drive, dflt 0 = the one-mode lips as shipped; `lip_shear_ratio` 0.25..4 its resonance over the swing's, dflt 1.5). The valve carries a second mode — the shearing (mucosal) wave as an INWARD-striking swing beside the outward one: y₂ driven by −g₂·Δp with g₂ = share·g, ω₂ = ratio·ω clamped under the sub-step's Nyquist; the opening h₀ + y + y₂; its mass Sr/g₂ in the valve's energy (the ledger counts it). The registers hold under it (CC 74 at 0/64/127: −803 / +18 / +1280 cents at share 0.3, against −769 / +14 / +1272 at 0); at share 0 the one-mode lips read none of its knobs (bit for bit under the ratio). It ships as a knob, not the trumpet's default: the author's ear decides whether the blare wants it.
**What changed besides.** The web gate: the SAX joins the libm-tolerance class (its calibration's log2f/exp2f on the embouchure's pull: the leak moved the pull to values whose last bit differs between libms; measured −102 dB); the suite's gates 42–45 (the sax's sidebands and their red, the shock's ledger and its red, the two-mode lips and the bit-for-bit at share 0, the holes on both woodwinds with the chart's twelve fingerings, the registers and the disagreeing keys); the winds re-profiled (the flute C2..C7 within 7.8 dB at breath 0.44, the sax within 6.8 at 0.55, the trumpet C3..C6 within 2.2); the desktop's rows (*Suzu toneholes*, *hole size*, *hole loss*, *reed inertance*, *bore leak* on the sax, *brass shock*, *lip shear*, *lip shear ratio*); the presets' eight fields; the web table's nine. The flute's intonation at breath 70 (+18 cents at D4, the jet's own) is what it was. `[ITERATE]` the hole mode's third register (the altissimo: past the lattice's cutoff the holes cannot cut; a real player vents); the sax's hole intonation to ±10 cents (the blown pass measures a jittering reed); the lab's shock-limiter and leak red controls on the winds page; the tablets' key panels driving the holes (the Boehm layout's keys reach the engine; the hole mode is a patch knob).

### 13.12 The editable modal table, the Scala tuning and the unison pair *(AS SHIPPED, step 82, 2026-10-19 — `DECISIONS_12 #6`; Voxo 0.22.0, additive; the preset schema 2)*
**The table.** `modal_preset` 5 is THE TABLE: the lattice takes its ratios, its decays and its strike weights from `modal_ratio[16]`, `modal_t60[16]` and `modal_weight[16]` in `voxo_suzu_params_t`, as a fitted instrument states them (the VCSL fits measured all three: DECISIONS_9 #9); a ratio of 0 is the harmonic m, a t60 of 0 the formula's from `decay_s` and `decay_bright`, a weight of 0 the 1/m; the first `modes` entries are read. `voxo_suzu_table` returns the table as the engine resolves it for any preset — a shell starts an editable table from the formula's values. The chain couples neighbours in the table's order. `modal_comp` (1 dflt) is step 57's detune compensation; at 0 the lattice is RAW — the coupling moves every mode as the spring says — and THE SWIRL ADDS NOTHING to the coupling (0.5·swirl would detune every mode the solve no longer protects: the load gate reads the patch's κ alone), the load gate's spectral radius read without the solve (a unison pair has no separate targets for Newton: the solve diverged, 16.6 against the bound 4, and refused the patch until the raw bound was its own). Measured through the ABI: a six-mode table's ratios 1 / 2.3 / 3.1 / 4.7 / 6.2 / 8.9 as resolved, as inspected (`s[k]`) and at the oscillator (the first four within 0.01 cent at A3), its decays 3 / 2 / 1.5 / 1 s read from the mode energies' fall within a percent.
**The preset schema is 2** (`SUMI_PRESET_SCHEMA`): the suzu block carries `modal_comp` and the three arrays (`[a, b, …]`, sixteen each; extras dropped, a shorter array leaves the rest), `modal_ratio[0]` 0 the sentinel of a file from before; a schema-1 reader keeps what it knows (the rule stands). A table round-trips through a file byte for byte (`preset_tests` 40/40, the suzu block compared whole). `sumi_preset_suzu_to_voxo` (`presets/src/sumi_preset_voxo.c`, types only: the library still links nothing) is the one mapping of a file's Suzu block into Voxo's params with every step's sentinel — the tablets' patch files go through it; the desktop's settings keep their own copy.
**The Scala tuning.** `voxo_set_tuning(scl_text, kbm_text)` / `voxo_load_tuning(scl_path, kbm_path)` parse Scala's .scl (comments on '!', the description, the count, `count` degrees — cents with a '.', a/b with a '/', else an integer ratio — the last the period) and .kbm (the size, the first and last notes, the middle note = degree 0, the reference note and its frequency, the formal octave's degree, then a degree or 'x' per key) without allocation into the engine's 128-note table, double-buffered under an atomic; `voxo_clear_tuning` returns to equal temperament; `voxo_tuning_count` / `_hz` / `_name` read it. A note's frequency = the reference's × the scale's ratio at (the key's degree + its octave × the formal octave) / the reference's own; without a mapping the scale's size is the keyboard's period, MIDI 60 degree 0, MIDI 69 440 Hz. `voice_retune` — the ONE site where a note becomes a frequency, for Suzu and the sampler alike — reads the table at every note-on and bend, the bend an equal-tempered offset on the note's frequency. A malformed file is refused with a line saying why, the tuning standing. Measured: 19-EDO within 0.0001 cent of 440·2^((n−69)/19) over 128 notes and the cell within 0.0003 cent at the oscillator; Pythagoras in ratios with a .kbm (60 at 256 Hz) gives the fifth as 1.5000000. The tuning is the sound's: the layouts' geometry (§14.6's microtonal mapping, the core's) is untouched, and the hole mode's chart keeps its twelve steps on the tuned base.
**The unison pair** (the patch table's seventeenth, `presets/unison.json`): the table in pairs — 1, 1, 2, 2, 3, 3, 4, 4 — at κ 0.006 with the compensation off. The chain's spring splits each pair into its two normal modes: for the first pair, with d = 1 on both and one mode having one neighbour and the other two, the 2 × 2 block (1 + κ, −κ; −κ, 1 + 2κ) has eigenvalues 1 + κ(3 ∓ √5)/2 — the lines at +κ(3 − √5)/4 and +κ(3 + √5)/4 of the frequency, NOT the naive √(1 + 2κ) the proposal's figure assumed. Measured at A4 under κ 0.01: +3.25 and +22.50 cents against +3.30 / +22.37; at the patch's 0.006 the pair beats 11.6 cents apart, 1.5 Hz at A3. The patch is admitted by the load gates; its render is bit for bit with and without the poly pressure (the swirl deaf on the raw lattice).
**The shells.** The desktop's Sound section: *Suzu preset* gains *table (editable)* — sixteen rows of ratio / T60 / weight under the modes, *Start from the preset's table* (the formula's values at the knobs, through `voxo_suzu_table`), *Coupling compensated*; the INI carries the three arrays as comma lists; the session file both ways; and the tuning rows — *Tuning (.scl)*, *Keyboard mapping (.kbm)*, *Load tuning*, *Equal temperament* — applied with the params. The tablets' Sound pages expose the table and the file as FILES through their document pickers: *Import a Suzu patch (.json)* (a midi-sink preset with a suzu block, copied under Patches, applied over the table's patch through `sumi_preset_suzu_to_voxo` — the editable table rides inside) and a *Tuning* section (*Import a Scala scale (.scl)*, copied under Tunings, `voxo_load_tuning`; *Equal temperament*; the scale's name and size as the status). `[ITERATE]` a table editor on the tablets (the file is the table there); the tuning inside the session file (a shell setting now); the lab's table (the web table is scalar: `modal_comp` rides, the arrays do not); the sampler's own root under a tuning (the sample plays at the tuned note's frequency: its root stays the file's).

### 13.13 The four new voices (steps 83–86; the roadmap's order — `DECISIONS_10 #13`)
* **The Toda lattice soliton ring (`voice_kind` 10)** *(AS SHIPPED, step 83, 2026-10-19 — `DECISIONS_13 #1`; Voxo 0.23.0, additive)*. N nodes (`toda_nodes`, 4..64, default 16; fewer at a pitch whose top mode would cross the leapfrog's bound, none above it — the voice is silent there) on a closed ring under H = Σ p_n²/2 + (a/b)·Σ(e^(−b r_n) + b r_n − 1), r_n = q_{n+1} − q_n, the linear stiffness k = a·b set so that the ring's first mode is the note EXACTLY at the step (2√k·sin(π/N) = 2·sin(ωh/2), the leapfrog's own frequency); the kick–drift leapfrog with THE STATE IN DOUBLE (float's rounding of q += p alone drifted a hundredth of a dB a minute, measured — the ten-minute gate asks a thousandth), THE EXPONENTIAL THE RING'S OWN (`suzu::toda_exp`: Cody–Waite range reduction and a degree-11 Taylor, 10⁻¹⁴ relative, the 2^k by the exponent field — plain arithmetic across the nodes, the same bits on every platform; libm's exp was the Tab's whole budget: a voice a millisecond a burst there). The ring runs at ×4 (two steps a 2× sub-step, then the half-band pair to the output rate) AND FINER WHEN THE PULSE ASKS — THE RESOLUTION RULE: a step's displacement over the exponential's length, √k·ĉ·b (ĉ the compression's peak), is held under 0.15 by raising the inner steps a sub-step (m = 2, 4, 6, 8 — at most 4 on the tablets' tier, `VOXO_MOBILE`; √k ∝ 1/m; a change rescales the momenta exactly, H in per-step units by the square), and where m 8 (4) cannot reach it the nonlinearity is CAPPED FOR THE NOTE at the strike (the pulse self-focuses to ~1.3× its struck peak: the allowance; lowered, never raised, if the pulse outgrows it). Measured on the ring alone: energy is made past 0.5 uncapped and past 0.22 with the cap engaged, whatever the cap's shape; and a nonlinearity that followed the live peak re-shaped the potential under the pulse and pumped it (C7 at level 1 blew up under the rule) — a ceiling, not a feedback. THE SOFT CAP: e^(−b r) is E = e up to cap/2 and cap − cap²/(4e) past it, C¹ (a hard min() left a kink the leapfrog fed on at the margin), saturating at e^`toda_cap` (1..12, default 6), clamped under k·cap ≤ 0.9 at the note (the soft cap's stiffness peaks at k·cap/2, 2√0.45 of the leapfrog's 2); H carries the capped potential's integral. `toda_gate` 0 is the red control: the ring as written — two inner steps, the cap and the nonlinearity as asked; a fortissimo C8 makes +19 dB of energy in a second (gate 52; under the gate +0.9 dB, the output under 0 dBFS). THE STRIKE: a Gaussian compression of width `toda_width` nodes (0.5..4, default 1.5) and amplitude level × velocity gain × 4 (strain units; the output divides by the same — the level is the strike's strength, a louder patch presses harder, as the Duffing's) at the cell's position, mean-free around the ring (its length is fixed), launched to the right with p = −√k·r (exact for the linear wave; the soliton outruns it, the rest radiates); the press and CC 74 above centre set b = `toda_b`·4·(0.05 + 0.95·clamp(press + 2(CC 74 − ½))) — at 0 the linear ring, the marimba-like inharmonic modes sin(mπ/N)/sin(π/N) exactly (gate 49: mode 1 on the note within 0.01 cent, modes 2–5 within 0.01 %; the patch's b at a light press +4 cents, the pulse's hardening); at full press the pulse is a soliton train passing the pickup faster than sound — at A3 the fundamental +427 cents above the note, the fifth harmonic 11 dB under it, the top octave −132 dB under the audible band through the ×4 path (the Toda soliton: e^(−br) − 1 = sinh²κ·sech²(κn − t·sinh κ), its amplitude fixing its width 1/κ — the struck 0.76 self-focuses to 0.97, κ 1.83, the width 0.55 nodes). NEAR-INTEGRABLE, NOT INTEGRABLE (gate delta a, kept): the leapfrog conserves a modified H, not the Lax invariants — the MINUTE-MEAN H drifts 0.000017 dB over ten minutes at b_max on the ring alone (the instantaneous H breathes ±0.06 dB under it), the biggest soliton's amplitude × profile width holds within 0.72 % (its amplitude within 0.26 %), Σp within 4·10⁻¹⁴ (gate 50; two minutes on a Debug build). THE SWIRL IS AN IMPULSE (gate delta b, kept): a uniform momentum J·Δswirl·√k a node (J 0.25 of the sound speed) on note-on and on a change, nothing while held, nothing twice for the same value; Σp is conserved by the symmetric force — and HEARD, because THE PICKUP STANDS IN THE LAB against the ring's mean drift (its lattice coordinate ξ runs back at Σp/N a step; it reads the compression at ξ through a Catmull–Rom cubic over the bonds, in the lattice's own coordinates — a fully Eulerian reading, ξ + q(ξ) = x, saw the density spike of a hard soliton, the lattice near-touching: −43 dB of top-octave content, measured; this one keeps the half-bands' floor): the solitons ride the boost past it, +19.7 % on the train's rate at full poly pressure (gate 51; the Galilean (c + v)/c). `decay_s` the declared contraction, a root of it per inner step (the damp decays Σp with the rest: a boost fades at the decay). The inspection: a = q, b = p, k[0] k, k[1] b as applied, k[2] H in the ×4 step's units whatever m, k[3] Σp, k[4] the compression's peak, k[5] the cap, k[6] m, k[7] b asked, k[8] the note's ceiling; the trace's aux pair the biggest soliton and the drift; the strobe at the bass one return a period (gate 53, 440 of 440: the train outruns the note, the points walk the section). The patch table's eighteenth, *Toda ring* (`presets/toda.json`, gate 28). Cost: a voice 1.1 % of a core at A2 (N 16, m 2), 1.7 % at C7 (m 4), 3.0 % at A2 with 64 nodes (9.1 % at C7 with 64: an exponential per node per inner step); sixteen rings C3–C7 at full press 18.6 % of the period (gate 54, the bar 30 %); the web a few percent of the quantum (TOLERANT for kind 10 stays: the half-bands' and the sinf's last bits). The preset schema gains `toda_nodes`, `toda_b`, `toda_cap`, `toda_width` (nodes 0 = a file from before, the defaults stand); the desktop's *Toda ring (solitons)* voice with its four rows; the tablets through the patch table (the tier's caps: the inner steps at most 4, the nodes at most 32; the iPad's spike clean at 0.42 ms of the burst; the Tab's audio thread, some forty times the Mac's cost on any voice, over its 4 ms burst by the ring alone — the emulator at the Mac's speed clean: a thread-placement item for the shells, carried).
* **The Boris–leapfrog magnetic gyroscope (`voice_kind` 11)** *(AS SHIPPED, step 84, 2026-10-21 — `DECISIONS_13 #2`; Voxo 0.24.0, additive)*. A charged particle on the note's harmonic surface (x₁, x₂) with the velocities (y₁, y₂) in the cell's units (y = ẋ/ω₀: a free orbit a circle), under a perpendicular field — the non-canonical 2-form ω = dp₁∧dx₁ + dp₂∧dx₂ + B·dx₁∧dx₂. The Lorentz force does no work; one sub-step at the 2× rate is the half kick y −= (ε/2)x, THE CAYLEY ROTATION of y by θ_B AS THREE SHEARS (t = tan(θ/2), s = sin θ, t: exactly [[cos θ, −sin θ], [sin θ, cos θ]], and in floating point a round-off walk rather than a bias — a cos/sin matrix whose c² + s² misses 1 by a unit in the last place scales the energy by that every step, e^{±1.7} over ten minutes), the half kick, the drift x += ε·y; **volume-preserving, det J = 1 (every factor a shear), the energy's error bounded and breathing with the kick's splitting, not drifting (gate delta a, kept)**: ±0.031 dB over ten lossless minutes at 96 kHz on the struct alone, the minute means drifting 0.000000 dB (gate 57; the state is double, as the ring's). **Both prewarped (gate delta b, kept):** ε = 2·sin(ω₀·dt/2) through `suzu::prewarp`, θ = ω_B·dt — the Boris angle 2·atan(B_num·dt/2) with B_num = (2/dt)·tan(ω_B·dt/2). On z = x₁ + i·x₂, w = y₁ + i·y₂ the map is complex-linear with trace (1 + e^{iθ})(1 − ε²/2) and determinant e^{iθ}: its angles are θ/2 ± ψ, cos ψ = cos(θ/2)·(1 − ε²/2) — the Zeeman doublet ω_B/2 ± sqrt(ω₀² + ω_B²/4) to (ω_B·dt/2)²/6, measured 0.001 cent over A2..B6 × f_B 0..50 Hz from the struct's own map (gate 55: two basis steps, the complex 2×2's eigenvalues) and 0.000 cent in the rendered left channel at A4 under 20 Hz (gate 56); `boris_prewarp` 0 is the lab's red — the raw ε = ω₀·dt and the raw Boris angle, 1.208 cents sharp at B6, charted (`tools/boris_chart.py`). **THE FIELD IS THE POLY PRESSURE'S, in hertz:** ω_B/2π = `boris_b_rest` + (`boris_b` − `boris_b_rest`)·swirl (0..50 Hz each; the patch *Gyroscope (Zeeman stereo)* 2 at rest, 24 at full — the rest field keeps a keyboard without poly pressure in stereo; 0 at rest is the proposal's mono line), smoothed with the swirl, refitted every block with the glide. **(x₁, x₂) straight to (L, R)** — the first stereo voice of `render_suzu_other`, its right channel through a second SVF, the bus plate fed the mean: THE STRIKE IS A DISPLACEMENT ALONG THE CELL'S POSITION by the equal-power pan law (√(1−u), √u): pluck 0.5 the diagonal — both channels the same bits, a mono planar string (gate 56: 48000 of 48000 samples) — 0 the left channel alone, 1 the right; under the field the line precesses into the rosette (L against R correlate 0.000 at 20 Hz). **The trace draws (x₁, x₂)** — the spirograph — with the energy and the field as the aux pair; **the strobe's own clock (clock 2) is the cyclotron period** (gate 58: 39 returns in 2 s at 20 Hz, the section the rosette's lobes; the bass, and the fall-back to it with no field, 440 of 440); the bench's `--boris-demo <dir>` traces it into Kessho and into Anod's spark reading (gate delta c — the author's viewing, `docs/evidence/step84/spirograph/`). The inspection: a = (x₁, x₂), b = (y₁, y₂), k[0] ε, k[1] θ, k[2] H, k[3] the field (Hz), k[4] k[5] the doublet the scheme sounds, k[6] k[7] the continuous one, k[8] the prewarp flag. The preset schema's `boris_b`, `boris_b_rest` (0 = a file from before); the desktop's *Boris gyroscope (Zeeman stereo)* voice with the two fields, the decay and the pluck as the pan; the tablets through the patch table (the nineteenth, `presets/gyroscope.json`). Cost: four doubles and two shears a sub-step — the web 0.24 % of the quantum and bit-identical to the desktop; the storm 0 XRuns at 0.13 ms; the iPad's spike 0.10 ms of the burst (the Tab unplugged at the step; the emulator 1.1 ms of 20).
* **The billiard (`voice_kind` 13)** *(AS SHIPPED, step 85, 2026-10-21 — `DECISIONS_13 #3`; Voxo 0.25.0, additive)*. A piecewise-isometric Hamiltonian system in **4D (x, y)** — the position in the wall's units and the velocity in the cell's (y = ẋ/ω₀): the flight between the walls is the note's oscillator's EXACT rotation by θ = ω₀·dt in each plane (three shears, the gyroscope's) at ×4 (192 kHz, two steps a sub-step, both channels through their half-band pairs), the wall a stadium — two half-ellipses, the half-axes a = `bill_aspect`·2^{2(CC74 − ½)} and b = 1, joined by straight walls of length L = `bill_length_rest` + (`bill_length` − `bill_length_rest`)·wheel — and a step that crosses it is CUT AT THE CROSSING: the fraction of the rotation by sixteen bisections on the angle, the velocity reflected off the wall's normal (v′ = v − 2(v·n̂)n̂, |v| as it was), the rest of the rotation taken, up to four times a step (a corner); **det J = +1 (Liouville) — rotations and reflections, gate delta a kept**: the energy ½(x² + y²) across 66 393 collisions in sixty lossless seconds within 2·10⁻¹² dB (gate 61). THE KICK is `bill_drive` × the velocity gain along the cell's position by the equal-power pan law (pluck 0.5 the diagonal: a radial orbit on the circle — L and R the same bits), in units of the wall: under 1 a sine at the note, past it THE FOLD — the circle's radial orbit bounces at (π/2)/asin(1/v) times the note (measured within 0.01 % at v 1.75: 2.58×; a ball bounces faster in a bowl with walls, and the note returns as the orbit decays under the wall — the default drive 1.4 puts the wall at mezzo-forte, the fold above it), the stadium's orbits ergodic — the strobe at the bass goes from the circle's LINE (24 of 576 cells — a kick from the centre is a radial orbit, with no angular momentum; the roadmap's ring wants a strike away from the centre, not yet a knob) to the stadium's dust (153; gate 62), the wheel pulling the walls under the hand, CC 74 squeezing the ends. THE KINK: a reflection jumps the velocity, so the positions have a slope discontinuity the 192 kHz sampling would alias; each collision adds the two-point polyBLAMP residual of its slope jump Δ = θ(y′ − y) at its fraction f — Δ(1−f)³/6 to the sample before, Δf³/6 to the sample after — through a one-sample output delay: **the aliasing — the off-grid residual of the circle's periodic orbit in the top octave — at most −80.5 dB of the fundamental over A3..A5 (gate 61; the roadmap's −60)**. `bill_exact` 0 is the lab's red: the reflection at the step's end — the bounce quantised to whole ring steps, its period 3.8 % off the geometry against the exact path's 0.009 % (a sample-periodic orbit aliases onto its own grid, so its residual reads clean: the red is the pitch). (x₁, x₂) to (L, R) as the gyroscope's; the trace's pair (x₁, x₂) with the energy and the collisions as aux; the strobe at the bass. The inspection: a the position, b the velocity, k[0] θ, k[1] L, k[2..3] a, b, k[4] H, k[5] the collisions, k[6] the exact flag. The patch *Billiard (stadium)* (the twenty-first; `presets/billiard.json`; a little stadium at rest, the wheel to the full one); the bench's `--voxo-chart` writes `billiard_sweep` (the wheel 0 → 2 over 24 s: the circle's regular bounce to the stadium's grit, `tools/chaos_chart.py`). Cost: 0.2 % of a core a voice.
* **The port-Hamiltonian Dirac network (`voice_kind` 12)** *(AS SHIPPED, step 85, 2026-10-21 — `DECISIONS_13 #3`; Voxo 0.25.0, additive)*. `suzu::Dirac`: n ≤ 36 states under H = ½xᵀQx (Q diagonal), a Dirac structure J (skew) and a dissipation R (diagonal, ≥ 0), stepped by the Cayley transform M = (I − h/2·A)⁻¹(I + h/2·A), A = (J − R)Q, built once by Gauss–Jordan at the strike — **Q-ORTHOGONAL with R = 0 (an isometry in the energy norm, not orthogonal: gate delta b kept), strictly passive with R ⪰ 0**: THE PASSIVITY PROBE (gate 59) reads the Q-norm gain of M (the largest singular value of Q^½MQ^−½, by power iteration at the build — the inspection's k[6]) over twelve random skew J (n 6..16, a step fifty times the engine's): 1 within 2·10⁻¹⁶ lossless, at most 0.99996 damped; the state and M double (a matrix Q-orthogonal to float's last place is a gain or a loss of that every step). THE PREPARED NETWORK the patch ships: a chain of `dirac_masses` unit masses on springs k with both ends held — the states the springs' EXTENSIONS and the masses' momenta, so H is diagonal and J the chain's incidence — its last spring on a bridge mass (`dirac_bridge_mass`; its own spring to the wall from `dirac_bridge_hz`, 700 Hz by default — a body in the playing range, whose loaded mode dips the plucked keyboard ten decibels between A2 and D4; at 2500 Hz the keyboard is flatter (9 dB) and the bow does not sing — the bow is the patch's excitation, so the body stays; its own loss `dirac_bridge_decay`, the radiation, 0.5 s), THE FORCE ON THE BRIDGE — the last spring's extension times k — the voice through the SVF (a saddle pickup: the bridge's velocity tilted six decibels an octave, stiffness-controlled below its mode), the pluck's kick and the bow's reference scaled by 220/f (the chain's impedance rises with the note), the stick-slip bow of §13.10 on the mass at `bow_position` as a port (the force a kick between Cayley steps against the mass's momentum with mobility 1; the breath the speed, the press and the velocity the force at the network's scale `SUZU_DIRAC_KF` 0.18·v_b — the chain's 3·v_b threw the mass to the bow every step), the pluck a kick on the mass at the strike's position (×8 × 220/f), and THE RATTLE STOP: a wall at −`dirac_stop` (in the level's velocity over the bridge's ω: the pluck's static deflection) under the bridge's displacement, the bridge clamped there and its momentum reflected with the declared `dirac_stop_loss`, the energy it takes in the ledger (the reed's lay). The chain's lowest mode is put on the note WITH THE BRIDGE'S LOADING: inverse iteration on the chain-and-bridge stiffness (Thomas on the tridiagonal, the Rayleigh quotient) inside a six-step secant on k, the target prewarped for the midpoint rule (ω′ = (2/h)·tan(ω₀h/2)) — −0.20 cent at A3 (gate 59); lossless and unexcited the energy holds to the float the inspection reports (0.0e+00 over 12 s). THE LEDGER (gate 60): H + the stop's losses − the bow's gain = H₀ within 10⁻⁶ of H under the bow (the gain f·v̄ at the synchronised velocity, never above the bow's work f·v_b: 8.95 against 708 — the friction's heat), the stop hit 632 times in four seconds taking 8.1, the tone +4 cents from A3; no feedback runaway by arithmetic. `decay_s` is R on the momenta (the amplitude's T60), `dirac_bridge_decay` the bridge's own. The profile: 14 dB of peak spread over the keyboard plucked (the body's dip; 39 dB at a flat kick on the bridge's velocity, the first reading). The inspection: a the state, b Q, k[0] H, k[1] the gain, k[2] the losses, k[3] the hits, k[4] the force, k[5] k, k[6] the Q-norm gain, k[7] n, k[8] the work, k[9] the stop. The patch *Prepared network* (the twentieth; `presets/prepared_network.json`). Cost: 0.8 % of a core a voice at eight masses (19 states: a 19×19 double product a sub-step).

### 13.14 The per-voice chaotic modulator and the mod matrix *(AS SHIPPED, step 86, 2026-10-25 — `DECISIONS_13 #4`; Voxo 0.26.0, additive)*
The projected double pendulum (§13.2; DECISIONS_7 #16) is PER VOICE: `SuzuVoice` carries its own, reset and kicked at the voice's strike with `0.5 + 2.5·vel_gain` (a soft note swings, a hard one tumbles), stepped at control rate in the voice's block when a slot reads it (a unit of pendulum time 50 ms / `mod_rate`, the shared clock), its sin θ₂ smoothed 20 ms — two voices are two pendulums (gate 63: A3 at 127 and A4 at 30 read apart after two seconds; A3 struck again restarts its own and leaves A4's the same bits); the instance's one pendulum is gone. THE MOD MATRIX: `mod_src[4]`, `mod_dst[4]`, `mod_amt[4]` in `voxo_suzu_params_t` — four slots of source × target × depth, summed per target every block (`suzu_mod_compute`, the sums in the inspection's `mod_sum[8]`, the voice's pendulum in `mod_out`). THE SOURCES: 0 none, 1 the voice's chaotic modulator (−1..1), 2 the press (0..1), 3 the swirl, 4 the slide (CC 74 about its centre, −1..1), 5 the mod wheel, 6 the breath, 7 the velocity (fixed at the strike). THE TARGETS: 0 none, 1 the cutoff (×2^{3m}: ±3 octaves at ±1), 2 the coupling (+0.5·m, into the lattice's κ beside the swirl's share), 3 the rotor's K (+m), 4 the drive (×(1 + m), floored at 0), 5 the level (×(1 + m), floored at silence — the output ramp), 6 the pitch (×2^{m/12}: ±1 semitone, on the block's frequency and its glide step, every voice). The depths are signed. THE LEGACY PAIR `mod_target`/`mod_depth` is slot 0 when that slot is empty (folded at `voxo_set_suzu_params`; the desktop shows it as slot 0 and clears it), so every patch and file from before routes as it did — and two of its four targets route for the first time: the cutoff and the coupling were declared at step 58 and never applied (found: `mod` reached K and the drive alone). Gate 64: the slide moves the cell ±100 cents at ±1 (measured +100.1 / −99.9), the wheel at −0.5 takes −6.02 dB, the press at −1 drops the bell's eighth partial 12 dB and more under a 4 kHz cutoff, the legacy pair and its slot render the same bits on the rotor. The preset schema: `mod_src`, `mod_dst`, `mod_amt` as three arrays of four (a file from before has none: no slot); the INI's three comma lists; the desktop's *Suzu mod matrix* (four rows of source, target, depth; the modulator's rate); the web engine's twelve scalars; the generator's `--slot i src dst amt`. FOUND beside it, in Voxo since step 56: a cutoff under the ceiling at ZERO resonance blew every voice — `svf_f_for`'s ringing compensation solved for a ring an overdamped filter has not and ran past stability (q 2 at 4 kHz: f 3.6, nan, the breaker); it now falls back to the plain coefficient outside the ringing regime (|cos θ| < 1, fq < 1), gated across the keyboard; and the SVF's cutoff ceiling, a fixed fs/6 since step 57, sat past the classic form's stability bound at low resonance (f² + 2qf < 4: at q 2 a cutoff of 0.13 of the rate) — the ceiling follows q now, the fs/6 kept above it, the eigenvalues at the ceiling gated under 1.


---

## 14. Instruments — stateful and idiomatic layouts (Phase 9, steps 60–66)

`specs/INSTRUMENT_SPEC.md` transcribed at the Phase-10 fold (2026-10-13), its sections in order (§14.1–§14.6 ↔ INSTRUMENT §1–§6), every `[ITERATE]` resolved inline by the decision named (`DECISIONS_8 #n` = `DECISIONS.md` Part VIII) and the shipped form noted per section; where this text and an entry disagree, the entry is the record. The spec's "Phase 8" was the roadmap's numbering before the Suzu phase was cut in: the instruments shipped as Phase 9. The ABI half of §14.1 (the probe's state argument, the cell flags) shipped in Phase 6's step 41 (`DECISIONS_5 #45`); the layouts and the fingering in Phase 9 (libsumi 1.4.0 and 1.5.0, additive).

### 14.1 The probe-state design (the architectural question, answered)

Valve and slide layouts are **stateful**: the note under (x, y) depends on the current valve combination or slide position, not on geometry alone. The instance-free probe — the design that saved Android's touch latency — must survive this.

**Design: state is an explicit snapshot argument, and state changes are MIDI.**

```c
typedef struct {
    uint32_t buttons;      /* bitmask: valves, register keys (bit 0 = valve 1 …) */
    float    slider;       /* continuous control position 0..1 (trombone slide)  */
    uint32_t reserved[2];
} sumi_layout_state_t;

SUMI_API bool sumi_layout_probe(uint32_t layout, const sumi_params_t* params,
                                float aspect, const sumi_layout_state_t* state,
                                float norm_x, float norm_y, sumi_cell_info_t* out);
```

* The probe stays a **pure function** — `(layout, params, aspect, state, x, y)` — callable from any thread; shells keep the state snapshot beside their params snapshot (they own every state write anyway). Stateless layouts ignore `state` (pass zeros). This is the Phase-6 ABI event, batched with MEDIUM_SPEC's `medium` param.
* **State changes travel as MIDI**, so fingering is recorded and replayed like everything else: valves as CCs (CC 110/111/112, ≥ 64 = pressed, on the master channel — `[ITERATE: numbers]` resolved as proposed, `DECISIONS_8 #1`), the slide as CC 113 — `[ITERATE: 14-bit CC pair vs 7-bit + smoothing]` resolved 7-bit, smoothed 10 ms in the engine, 4.7 cents a step (`DECISIONS_8 #1`). The normalizer decodes them into layout state on the engine side; the shells mirror the same bytes into their local snapshot — one source of truth (the byte stream), two synchronized readers. A DAW recording of a trumpet performance replays the *fingering*, not just the pitches.
* The engine's own copy of layout state drives any visual echo of fingering — `[ITERATE: should pressed valves render?]` resolved as proposed: the strip's widgets and the tablets' large fingering panel show it, the canvas stays pure ink, the web's HUD is an overlay (`DECISIONS_8 #9`, `#13`–`#17`, `#25`).

---

### 14.2 Trumpet layout (`SUMI_LAYOUT_TRUMPET`)

The real fingering isomorphism: **partials × valve combination.**

* **Cells: the harmonic series** — 8 partial cells (fundamental through the 8th partial a real Bb trumpet speaks), arranged — `[ITERATE: vertical column low→high, or arc?]` resolved both: a column by default, a ring under `params.trumpet_arc` (radius 0.30 canvas heights, centred 0.08 right of the middle at 0.65 of the height, cells 0.055 — re-cut by the author's eye on the iPad; `DECISIONS_8 #2`, `#15`–`#17`). Touching a cell selects the *partial*; the sounding note = partial pitch − valve offset.
* **Valves: three strip buttons** (momentary, the existing widget) emitting the valve CCs. Offsets: valve 1 = −2 semitones, valve 2 = −1, valve 3 = −3; combinations sum (1+2 = −3, 1+3 = −5, 2+3 = −4, 1+2+3 = −6) — the seven real positions plus open. Real combinations' intonation quirks (1+3 sharp, etc.) are **deliberately not modeled** — this is the idealized instrument — `[ITERATE: a "realistic intonation" toggle is a cheap flavor option]` not built in 2.0 (the idealized instrument stands; carried, §16).
* **Expression:** Y in the cell = the existing bipolar press axis (embouchure as ink feed / swirl); X = per-note bend (lip bend — `[ITERATE]` resolved: one semitone per cell radius along the cell's +x axis, `DECISIONS_8 #2`).
* Changing valves while a voice sounds retunes it: bend ramp over 20–40 ms (the PIANO_GRID cell-quantized machinery, reused verbatim) — real valve legato.

**As shipped (Phase 9 steps 60, 62–64; `DECISIONS_8 #2`, `#9`–`#11`, `#13`–`#21`):** the partials are SOUNDING MIDI notes of a B♭ instrument — B♭1 (46, the pedal), B♭2, F3, B♭3, D4, F4, A♭4, B♭4 — and a note from the wire lands in the partial its fingering selects, else the standard fingering's, else the nearest; the valve offsets as written, summed; the retune of a held voice is `hostmpe_voice_retune`, ramped over 30 ms for the valves (at once for the slide), a touch landing while the valves change beginning between semitones and resolving on its first retune; the valve buttons are change-only CCs (127 down, 0 up), exempt from limiting, the shell's bitmask the mirror of the bytes it sent and the state it hands the probe; on the tablets the three pads live on a large fingering panel at the side or along the bottom, the strip at the opposite corner.

### 14.3 Trombone layout (`SUMI_LAYOUT_TROMBONE`)

* **Cells: partials** (7 typical); **slide: a dedicated wide latch-slider** on the strip (a new widget variant: positional, not accumulating — the hand IS the slide) emitting the high-resolution slide CC. Positions 1–7 rendered as tick detents, **continuous between them** — sounding pitch = partial − slide semitones (0..6 continuous), so playing between positions is simply in tune with itself, exactly like a real trombone and unlike every quantized software one.
* Glissando is the identity gesture: hold a partial cell, move the slide — continuous bend across up to 6 semitones, no re-anchor needed.
* Slide state is one float — the probe's `slider` field.

**As shipped (`DECISIONS_8 #2`, `#9`, `#17`):** seven partial cells on the same column or ring as the trumpet's (one `trumpet_arc` flag for the brass); the slide a positional slider — CC 113, 0 closed to 127 the seventh position, policed like a wheel, smoothed 10 ms in the engine — on the strip and on the fingering panel; a held voice retunes at once as the slide moves (the glissando). On the Galaxy Tab the S Pen's palm rejection drops the fingers while the pen is near the glass, so a fingering change needs the pen away — a platform limit the iPad does not have (`#21`).

### 14.4 Stateless additions (cheap, ride along)

* **Wicki–Hayden hex** (`SUMI_LAYOUT_WICKI`): the other great isomorphic layout; hex cell math beside Jankó's, one echo, all existing machinery applies.
* **Fretboard** (`SUMI_LAYOUT_FRETS`): 6 string-rows × fret columns, standard tuning — `[ITERATE: tuning table in params or fixed?]` resolved fixed: three presets under `params.string_tuning` (standard guitar, the whole-tone tap, all fourths), the layout named `SUMI_LAYOUT_STRINGS` = 11 with `FRETS` kept as an alias (`DECISIONS_8 #6`); dragging along a string is *literally* a string bend (per-note bend along the row axis) — the most instantly-understood MPE layout that exists.
* **Theremin** (`SUMI_LAYOUT_THEREMIN`): no cells — continuous X = pitch (the legato re-anchor machinery), Y = the bipolar press axis. The anti-grid: pure expression, and the natural stage for the ripple and torsion operators.

**As shipped (Phase 9 step 61, libsumi 1.5.0; `DECISIONS_8 #5`–`#8`):** Wicki–Hayden a hex button-field six buttons wide and fifteen rows tall, every note on exactly one button, pitch a plane over the sheet and the bend axis its gradient; the strings as string-rows × chromatic frets under the three tunings, a note stamped at its lowest-fret site on every string that reaches it (`SUMI_MAX_ECHOES` 12, `#14`), the bend axis along the string; the theremin five octaves across the width (C2 at the left edge to C7 at the right, 61 semitone slots), the note the nearest semitone under x, its centre that slot's x at the middle height, Y the bipolar press axis — on a tablet a touch moves as a bend across the width and re-anchors on a new note past 47 semitones (`#11`). Every named layout ships (0–12); the desktop's picker has thirteen entries.

### 14.5 Surface & probe consequences

* `sumi_cell_info_t` is unchanged for trumpet/trombone (cells are still cells); theremin signals "continuous" — `[ITERATE: cell_radius = 0 sentinel vs. a flags field]` resolved: a flags field, `SUMI_CELL_CONTINUOUS` (`DECISIONS_8 #7`).
* Hit-testing, joystick, legato, and echo machinery are untouched — stateful layouts change *which note* a cell means, never how touching works.
* Golden tests extend to state sweeps: every valve combo × every partial; the slide at detents and midpoints.

### 14.6 Explicitly deferred within this spec
Woodwind key systems (Boehm charts — real but a much larger state space), microtonal/scala tunings — `[ITERATE: cheap on theremin and frets — pull forward?]` not pulled forward, not in 2.0 (the instruments guide says so; §16), two-handed split layouts (different layouts per screen half).

### 14.7 The Boehm layout and the split surface (step 77, `DECISIONS_11 #4`; libsumi 1.12.0, additive — the last core-open step; transcribed at the fold)
`SUMI_LAYOUT_BOEHM` = 13 — the Boehm flute's key system through `sumi_layout_state_t.buttons` bits 0..8 (L1, L2, L3 on the valves' CC 110–112; the thumb, the G♯ lever, R1, R2, R3, the D♯ key on `SUMI_CC_KEY_*` 114–119, the same global rule; `SUMI_LAYOUT_KEYS` 9), stateful as the brass's (§14.1): three REGISTER cells on the brass geometry (the column or the arc under `trumpet_arc`) sounding the fingering's chart note + 0 / 12 / 24; the chart `sumi_layout_boehm_note(buttons)` — the idealised flute's vent rule (the highest open hole sounds: C♯ under L1, B under the thumb, B♭ closed by L2 OR R1 — the two linkages, the 1-and-1 B♭ — A, G, the G♯ hole closed-standing, F, E, D, the D♯ hole closed-standing, all closed D4) with the one cross fingering kept (R3 alone under an open F hole: F♯), a total function of the 512 states onto D4..C♯5; a wire note lands on the register that sounds it, else the nearest; the embouchure bend the brass's +x axis. The toneholes of §13.11 are what these keys will drive. **The split surface** (§14.6's two-handed split): `params.split_mode` (0/1), `split_layout` (the right half's), `split_note` (60) — two Play layouts side by side at half width (the aspect halved: canvas-height units carry over), the notes divided at the split note (the left below it, the right from it: a wire note's one home); every layout query dispatches by half — the probe by the touch (a cell across the split refused), the placement and the axis by the note (the axis's normalized x halved back), the cells both halves' (unfiltered; the shells grey). Rolls and FIFTHS on either side switch it off. The desktop's rows; the web's overlay follows the probe; the tablets' surfaces are their sub-steps.

---

## 15. Quality of life — the undone items, as of 2.0 (Phases 7–9)

`specs/QUALITY_OF_LIFE_SPEC.md` transcribed at the Phase-10 fold (2026-10-13), its sections in order (§15.1–§15.4 ↔ QOL §1–§4), every `[ITERATE]` resolved inline or marked carried; the items that shipped in step 43 and the shells (palettes, substrate, presets, prints, the copy) are §11. The two standing product rules hold: no telemetry, and the canvas stays an instrument (no feature that turns playing into menu-diving).

### 15.1 Session replay (the byte stream is the recording) — shipped at Phase 9 step 65 (`DECISIONS_8 #22`–`#24`), on the web at step 66 (`#26`)

* Everything musical is already MIDI — including fingering (INSTRUMENT_SPEC §1) — so **record the timestamped byte stream + params/state changes + gesture calls, replay it through the loopback** and the performance reconstructs deterministically. The pen tracer proved the pattern; this productizes it.
* **Cross-device is a requirement, not a bonus** (recurrent user ask): the replay file is platform-neutral and version-stamped — record on the iPad, replay on the desktop (or any shell). Determinism holds because the field math is identical across backends within the documented §4.6 tiers (Metal bitwise; GL, D3D11 and GLES at their tiers — `DECISIONS_5 #87`); the replay banner states the source device and app version.
* With Voxo (§12), a replay **re-sounds**: the same bytes feed the audio engine, so an iPad session replays on desktop with a bigger instrument loaded. Offline audio bounce of a replay is the natural deferred extension.
* Gestures are recorded beside the bytes (resolved: `DECISIONS_5 #8`), so pen performances replay complete; replay files carry frame boundaries and playback drives the scripted clock through them (`#12`) — wall-time re-bucketing is the documented anti-pattern and the negative test.
* Replay enables: re-dipping a past performance at a new resolution or palette, and the gallery's "watch it again" links.
* **As shipped:** the library `replay/` (pure C11, `sumi_replay.h`; the `.sumireplay` text file of `replay/FORMAT.md`; the recorder's STAGED frame boundary — the shell's producer stages its bytes and the render thread hands them to the core at each frame start, so every byte's frame is exact; the player and the apply unit; one recorded frame = one update at the recorded dt and one render); every shell records and plays, the desktop and the tablets re-sound, the web plays the picture (`?replay=`); the gates (`tools/replay_gate.py`, `web_gate.mjs --replay`: bitwise on the recording device, the wall-time re-bucketing diverging as the negative test). Measured across devices: the Pixel's GLES recording within the mobile tier on the Mac; the iPad's Metal recording a few percent of displacement off the Mac's — `DECISIONS_8 #24`'s open question (§16).

### 15.2 Small UX items — resolved at Phase 9 steps 62–64 (`DECISIONS_8 #12`–`#13`, `#18`) or carried

* **Panic** — the settings action exists on the tablets ("Stop all notes"); — `[ITERATE: a strip button?]` resolved built: the strip's Panic pad on both tablets, the desktop's Panic beside its inputs, the mapper honouring CC 120/123 so the visualizer's voices flush as a synth's (`DECISIONS_8 #12`).
* Layout quick-switch on the strip — `[ITERATE: cycles a user-chosen subset, not all]` resolved built: a user-chosen subset in the order chosen, the strip's Next pad (`DECISIONS_8 #12`).
* A **preset-next** button on the strip — `[ITERATE: a performance feature — which presets cycle?]` NOT built in 2.0 (the presets guide says so; carried, §16).
* Per-device default presets: connecting a known controller (ROLI/Osmose/Brisa profile detected per §2.5 heuristics) **offers** its preset (resolved as offer, never auto-apply — `DECISIONS_5 #7`; built as the per-device profile offered once in an alert, `DECISIONS_8 #12`).
* Left-handed mirroring for the play surface and strip — built: the lattice, the panel and the strip flipped, the mirror handed to every probe (`DECISIONS_8 #12`–`#13`, `#18`).
* **Undo: deliberately absent** (author's decision, recorded): field undo means snapshot ring-buffers and a state model that every later feature must respect — the cost compounds forever. The dip is the instrument's undo: fresh paper.

### 15.3 Small items carried from the shipped sections — as of 2.0

* Substrate: `[ITERATE: expose the fibre angle-drift amount?]` — not exposed; the angle drift is a constant (the medium guide's substrate table says so). The fibre scale shipped.
* Prints: TIFF-16 for print workflows — `[ITERATE: demand-check first]` not built in 2.0; PNG only, over alpha in Anod (the paper-and-prints guide says so; §16).
* Palettes: the ink-depth / glow curve is fixed per medium in 2.0; the "advanced fold" that would expose it stays deferred.

---

### 15.4 Sequencing note (historical)
Replay depended on §14's fingering-as-MIDI to be complete and on Voxo (§12) to re-sound, so it landed after both (Phase 9 step 65); the strip items landed with the hostmpe step of Phase 9 (62) and the shells (63–64).

### 15.5 Preset-next (step 77, `DECISIONS_11 #4`; transcribed at the fold)
**Preset-next** (§15.2): `hostmpe_strip_preset_set / _count / _next` — the quick-switch's shape over the shell's preset ids (`HOSTMPE_PRESET_MAX` 32); the pads are the tablets' sub-steps. hostmpe's nine key widgets (`hostmpe_strip_key_press / _release / _keys / _key_cc`; the announce restates fifteen; `HOSTMPE_QUICK_MAX` 14). `[ITERATE]` the layouts' microtonal mapping (§14.6) stays carried beside §13.12's Scala tables (not taken here).

---

## 16. Carried past 3.0 (the open points at the transcription, 2026-10-26)

Struck by the v3.0 arc, off the 2.0 list: the output-device selection, the instrument slot and the offline bounce (§12.12); the chain's SIMD, the modulator per voice, the reed's flow inertia, the sax's quasi-periodic A3, the unison preset, the editable modal tables with the Scala tuning (§13.8–§13.14); the woodwind key systems and the two-handed split layouts (§14.7); the preset-next button (§15.5); the strain buffer, the Chirikov feel grid and the gesture route's reproducibility (§10.10). What stays, and what the arc adds:

* **The medium (§10):** the local spark (`DECISIONS_5 #72`, not pursued — the stir stands); the crossed pinch's inversion (`#17`, a non-inverse by design); the draft's `[ITERATE]`s by subsection — the handedness hue w = (J₂₁ − J₁₂)/2 as a Kessho chirality (§10.7.1), the LUT beside `sumi_palette_t` (§10.7.2), z's upper bound (§10.7.3), the q crossfade and the precession's slow motion (§10.8.1), the chord wave's route (b), the rosettes (§10.8.2 — prototyped only after route (a) has been seen on paper, the author's call), the consonance curve and its floor and the free vortices' boundary (§10.8.3); the A/B items K-1 and B-1 as decided (`DECISIONS_10 #5`, `#10`); every habit at roundness 0 (`DECISIONS_10 #8`, to be reviewed; a square is not a habit yet); **the Kessho flatness gate at the web tier reads the tier's floor** — the ±3-texel median of |det J − 1| on the §4.6 canonical script 3.4e-3 (±2 4.6e-3, ±1 8.3e-3; `tools/kessho_flat_web.mjs`) against the roadmap's 1e-3, as the desktop's own gate 2 read a few thousandths at step 71 (§10.7.3, `DECISIONS_10 #4`; `DECISIONS_15 #1`): the bar at that tier is the floor, or it is nothing.
* **The sound (§12):** Voxo Dorean (§12.6); fresh byte logs for the chart's 3.0 rows from the store builds (the modes change no output byte — the RC round's); the mineral's habit / profile pairing table in two places, the desktop's and the web's (`DECISIONS_15 #1`; a shared home is a preset or a core change); the tablets' output-device row, a worker thread for the desktop's bounce, a bounce at the device's rate (`DECISIONS_14 #5`); the Tab's audio-thread placement (ADPF, affinity — a shell item: Suzu patch 0 over its 4 ms burst, `DECISIONS_13 #1`, `#4`), the Tab unplugged since step 84 (the emulator stands in).
* **The synth (§13):** §13.7's deferred list as revised; the web gate's contract under SIMD (§13.8, `[ITERATE]`); the layouts' microtonal mapping beside §13.12's Scala tables (§13.12, §14.7); the billiard's off-centre strike for a ring section (`DECISIONS_13 #3`); the rotor's section sinking into a 4-cycle at K 2.5 (`DECISIONS_10 #12`); Suzu does not glide (`DECISIONS_14 #4`); the Dirac network has no red control of its own — its safety is the Cayley step's arithmetic, read live (`DECISIONS_15 #1`).
* **The instruments (§14):** the "realistic intonation" toggle (§14.2); the pen voices' retune through hostmpe's glide on the Tab (`DECISIONS_8 #21`); the tablets' key panels, split surfaces and preset pads (§14.7's and §15.5's sub-steps, the tablets').
* **Quality of life (§15):** TIFF-16 prints; the fibre angle drift as a knob; the advanced palette fold; the iPad's cross-device replay gap (`DECISIONS_8 #24` — the iOS simulator on the Mac's GPU, the phone replaying the iPad's file); the Tab's own replay run; the tablets' recorders' `seed` and Glow-reading rows (§10.10's sub-steps).
* **The network session (§8.5):** the probe of Apple's own stack from a terminal (`tools/rtpmidi_mac_probe.swift`, `DECISIONS_9 #14`); a "connect to host" from the tablet (the library's initiator exists).
* **The lab tooling:** the Tab's lab recorder (`--es recordLab`) draws garbage on screen while its 640×400 lab surface is up, the recording intact; the iOS lab's fingering demo draws nothing in the simulator; the Simulator application was unreachable from the agent's session, so the 2.0 App Store captures are portrait (`DECISIONS_9 #18`).
* **The book:** the cells chapter's 0.63 dB glide figure against the suite's printed plain-form 0.057 / 0.062 dB (the lessons' adaptation notes, for the author); the six older lab pages without a strobe panel (one `strobe` option and a `Portrait` away, `DECISIONS_15 #1`); the drafts in `site/drafts/`, the author's to remove.
* **The release machinery:** the libsumi SDK question (publish `sumi_core.h` and binaries as a versioned artifact against the settled ABI, or defer again — the Part-4 prerequisites); the Linux box, out of commission since 2026-10-05 (the Linux lane runs on GitHub's runner); the 3.0 release's acts — the `v3.0.0-rc.N` candidates on both tracks, the wave's questions, the store builds, the final RC promoted with zero code changes, the tag — the author's.
---

## 17. Implementation Roadmap

The step-by-step implementation plan, acceptance criteria, and agent working rules live in **`ROADMAP.md`** (Parts 1–10: v0.1, v0.2, Phase 4, Phase 5, the v2.0 arc — Phases 6–10 — and the v3.0 arc — Phases 11–16). Every step through 92 is done and both arcs are complete; no roadmap is open; the 3.0 release's acts — the candidates, the wave, the stores, the tag — are the author's. A new phase opens `_work/ROADMAP_7.md` and `_work/DECISIONS_16.md`, fed to the implementing agent one step at a time alongside this document. Do not begin a step while the previous step's DONE checks fail.
