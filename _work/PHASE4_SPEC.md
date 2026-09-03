# PHASE 4 SPECIFICATION: Touch & Stylus MPE Play Surface
**Companion to `PROJECT_SPEC.md` (v0.3 — see §7 below) and `ROADMAP_3.md`. House rules from ROADMAP working rules apply.**
**Scope: iOS first (iPad + Apple Pencil), Android second (S-Pen). Desktop untouched.**

---

## 1. Two modes, one overlay

The tablet shells gain a **mode toggle** in the settings sheet:

* **Marble mode** — the existing Step-13 direct gestures (tap → drop, pan → tine, twist → vortex). Already shipped; unchanged. This *is* the "Airwave-like expression tool."
* **Play mode** — the new virtual MPE instrument: a joystick-per-touch surface whose cells follow the active layout. Touches generate standard MPE byte streams consumed twice: **loopback** into `sumi_push_midi()` (the visualizer is just another MPE synth) and **outbound** to external DAWs.

Play mode is available on `SUMI_LAYOUT_CHROMA_GRID`, `SUMI_LAYOUT_JANKO` and `SUMI_LAYOUT_PIANO_GRID` (the classical two-row piano grid added mid-phase, DECISIONS_3 #29) only. FIFTHS and the roll layouts stay marble-mode (fifths' adjacent wedges are a *fifth* apart, so angular bend has no sane semitone scaling; rolls are timelines, not keyboards — revisit as a v5 "play the now-line" idea if wanted).

**Core invariance, with one deliberate exception.** All touch tracking, hit-testing, voice allocation, and MIDI output live in the host shells. The single core addition is the read-only layout probe of §2 (ABI v0.3) — geometry must have one source of truth, and duplicating `layouts.cpp` lattice math in Swift *and* Kotlin is a drift bug waiting to happen.

---

## 2. Layout probe ABI (the one core change → v0.3.0)

```c
typedef struct {
    uint8_t  note;            /* nominal MIDI note of the cell under (x, y)   */
    float    cell_center_x;   /* normalized canvas coords of the cell center  */
    float    cell_center_y;
    float    cell_radius;     /* half the smaller cell dimension (R_max)      */
    float    semitone_dx;     /* unit vector: +1 semitone direction (glide    */
    float    semitone_dy;     /*   axis, DECISIONS_2 #7) at this cell         */
    float    semitone_step;   /* canvas distance for +1 semitone along it     */
} sumi_cell_info_t;

/* Pure, instance-free geometry query — a free function of the same inputs the
   internal layouts already consume. Callable from ANY thread (the caller
   supplies a params snapshot); no instance, no rendering, no MIDI, no state.
   This matters on Android, where touches arrive on the UI thread while the
   render thread owns the instance (DECISIONS_2 #32/#33) — an instance-bound
   probe would force a command-queue round-trip per touch-down, spending the
   ≤2-frame latency budget on hit-testing. Returns false when Play mode is
   meaningless for the layout (FIFTHS, rolls) or (x, y) is outside the
   playable area. Golden-testable headlessly like layouts.cpp itself. */
SUMI_API bool sumi_layout_probe(uint32_t layout /* sumi_layout_t */,
                                const sumi_params_t* params, float aspect,
                                float norm_x, float norm_y,
                                sumi_cell_info_t* out);
```

The shells keep a params snapshot beside their UI state (they own every params
write anyway), so probing at touch-down is a plain function call on iOS and
Android alike — no threading exception to document.

For Jankó, the probe returns the note of whichever echo row was touched — touching *any* of a note's three parity rows plays that note; the loopback MIDI then produces all three echoes automatically through the normalizer, so play-surface and visual stay coherent for free. `semitone_step` is what makes bend scaling musically exact (§3.3).

---

## 3. The floating joystick model (corrected)

### 3.1 Touch-centered origin
The point of initial contact becomes the joystick center (x0, y0). Touch-down always emits center pitch bend *before* Note On on the allocated channel — guaranteed in-tune attacks. The cell (from `sumi_layout_probe`) supplies the nominal note and R_max = `cell_radius`.

### 3.2 Deadband with a soft knee (no zipper)
Deadband is **radial** (‖Δ‖, not per-axis) at 3% of R_max, and values outside it are **re-normalized**, never stepped:
```
d = ‖Δ‖ / R_max, clamped to 1
g = 0                     if d ≤ 0.03
g = (d − 0.03) / 0.97     otherwise        // smooth from exactly 0
Δ_eff = Δ̂ · g
```
A hard threshold would make the first vibrato wiggle jump from 0 to 3% — audible zipper on the outbound pipe.

### 3.3 Axis mappings with musical scaling
* **X → 14-bit Pitch Bend, scaled in semitones.** The missing piece of the draft: raw dx → ±8191 on a ±48-semitone channel is ±4 octaves per cell radius. Instead:
  `bend_semitones = Δx_eff_canvas / semitone_step` (probe field), so **dragging one grid column over glides exactly one semitone** — continuous glissando lands in tune by construction. Then `pb = 8192 + round(bend_semitones / pb_range * 8192)`, clamped to [0, 16383], with `pb_range = 48` (the value the MCM declares on both pipes, §5.3). Note the parenthesization: rounding must happen AFTER the *8192 scale — rounding the semitone ratio first would quantize every bend to whole ±8192 (all-or-nothing). One semitone = ±171 counts at ±48 (8192/48 = 170.67) — assert this exact value in tests.
* **Y → Channel Pressure (fingers), upward only.** `pressure = clamp(Δy_eff_up, 0, 1) · 127` — touch-down = 0 (the drop stays as struck), pushing up feeds ink through the engine's boundary-growth math. This is the deliberate inversion of the draft: pressure is the engine's hero dimension and glass fingers have no real Z, so it gets the surface's best continuous axis instead of a contact-radius estimate. It also matches the reference hardware — the ROLI Piano's per-note dimensions are bend + pressure (no slide/CC74; that is Seaboard territory), and the Osmose leads with pressure. The downward half of the axis is **reserved** (sends nothing) — candidate future assignment, not v4 scope. Axes stay perceptually orthogonal: vibrato wiggles X, intensity pushes Y.
* **CC 74 is stylus-only.** The Pencil/S-Pen have real tip force for pressure, freeing their Y axis for CC74: `cc = clamp(64 + round(Δy_eff · 63), 0, 127)`, center 64, up = brighter. The stylus is thus the full three-dimension voice (X bend, Y timbre, Z true pressure); fingers are the honest two-dimension voice (X bend, Y pressure). Fingers emit no CC74 at all (change-only filtering means silence, not a stream of 64s); DAW synths listening on CC74 get it from the pen.
* **Lift → pressure 0, then Note Off** (release tails on synths that gate on pressure), channel returned to the allocator per §5.1.

---

## 4. Velocity & pressure truth table (be honest about glass)

| Input | Strike velocity | Continuous pressure | Notes |
| :-- | :-- | :-- | :-- |
| iOS finger | **Synthesized** — default 96, optionally modulated by `majorRadius` at touch-down (coarse) | **Y-axis upward travel** (§3.3) — full-resolution, deliberate, no hardware estimate involved | No force API: 3D Touch is gone. `majorRadius` is used for optional velocity synthesis only, never for continuous pressure. |
| Apple Pencil | `force` at impact → velocity (real) | `force` continuous, 0–4096-level quality | Also: `altitudeAngle` (tilt), `azimuthAngle`, hover distance on M2+ iPads (`UIHoverGestureRecognizer` + `zOffset`). |
| Android finger | Synthesized (as iOS) unless the panel reports real pressure at impact | **Y-axis upward travel** (§3.3), same as iOS — panel `getPressure()` is ignored for continuous control even where real, so both platforms play identically | OEM panel variance stops mattering: the axis is the sensor. |
| S-Pen | `getPressure()` at impact (real) | `getPressure()` continuous | Tilt/orientation via `AXIS_TILT` / `AXIS_ORIENTATION`; hover via `ACTION_HOVER_MOVE`. |

**Stylus matrix (pencil/S-Pen as first-class expressive voice):** tip pressure → channel pressure (real Z, so Y stays free); Δx → bend like fingers; Δy → CC74 (stylus-only, §3.3); **tilt → assignable global CC** (default CC 1 → vortex strength via the existing CC map); **azimuth → assignable global CC** (default unmapped); **hover → marble-mode wind-brush pre-strike drift** (hover moves a ghost cursor; no MIDI emitted until contact). Pencil-as-lead + fingers-as-chords is the honest expressive story on iOS.

---

## 5. Host MPE generation contract

### 5.1 Voice allocator (fixes to the draft)
* **Least-recently-released round-robin**, not first-free: a rotating cursor over member channels ordered by release time. First-free reuses channel 2 constantly, so a new note's bend/pressure hijacks the release tail of the note that just freed it — the classic MPE allocator bug.
* **External-occupancy masking:** the shell's MIDI merge point (§5.2) sees every byte from hardware devices; channels with an active external note (Note On seen, no Note Off yet) are **unavailable** to the touch allocator. A ROLI played simultaneously with the surface must never be channel-stolen. Occupancy clears on the external Note Off, on device disconnect, or after a 30 s stuck-note timeout.
* **Saturation:** allocation failure (all 15 members busy) is silent note-drop with a HUD blink — never steal.
* Emit order on touch-down: center bend → Note On. On lift: pressure 0 → Note Off (release velocity from lift speed where measurable, else 64).

### 5.2 Single producer, preserved
`sumi_push_midi` keeps its §5.2 contract: **exactly one producer thread per instance.** Each shell funnels *both* device MIDI and touch-generated bytes through its existing single producer path — iOS: the serial MIDI dispatch queue becomes the sole producer, touch handlers post byte-triples onto it; Android: touch bytes are handed to the AMidi poller thread's input side (the DECISIONS #24/#33 mutex already serializes the handoff pattern). The touch path must never call `sumi_push_midi` from the UI thread directly.

### 5.3 Dual-pipe output with per-pipe rate limiting
The dispatcher fans each generated message to two pipes with **independent policies**:
* **Loopback → `sumi_push_midi`:** full rate, zero decimation (the core's coalescing already handles density; it is stress-proven far above touch rates).
* **Outbound → transports: change-only filtering (never resend an identical PB/CC74/pressure value per channel), then a PER-TRANSPORT budget** — one outbound policy cannot serve links whose capacities differ by an order of magnitude:
  * **USB (both platforms) / virtual CoreMIDI source / MidiDeviceService / Network Session:** per-voice, per-dimension latest-wins decimation to ≤ 100 Hz. These links absorb it comfortably. USB is the **primary outbound sink** — lowest latency, wired, and on iOS it costs zero transport code (see §5.4).
  * **BLE MIDI:** a **global send budget of ~300 msg/s** (latest-wins with round-robin per-voice fairness across dimensions, so one wiggling finger cannot starve nine others). Even 100 Hz/dimension does not close on BLE: 10 touches × 3 dimensions × 100 Hz = 3,000 msg/s against a link that sustains a few hundred — a correct implementation of a single-policy spec would fail its own storm test.
  * On every transport: Note On/Off, the initial center bend, and pressure-0-before-Note-Off are never decimated or dropped — the budget applies to continuous dimensions only.
* **MPE configuration on BOTH pipes:** entering Play mode pushes the **MCM (RPN 6: lower zone, 15 members)** followed by **RPN 0 = 48 semitones on the member channels** into the **loopback** — making the normalizer's mode flip deterministic (§2.5 heuristic detection would otherwise flip to MPE mid-performance, and the first notes would decode with wrong bend range). Every outbound transport sends the identical MCM/RPN0 sequence on session open and re-connect — otherwise DAWs assume ±2 and every glide plays 24× too small. Re-send on demand via a settings button ("Re-sync DAW").

### 5.4 Outbound transports (USB first)
* **iOS:**
  * (a) **USB to a Mac — the primary sink — via IDAM (Inter-Device Audio and MIDI):** the app's virtual CoreMIDI source is tunneled over the cable when the user clicks *Enable* on the device in the Mac's Audio MIDI Setup. **No transport code exists for this** — it is the virtual source, wired; it must nonetheless be documented in-app (a one-line hint in the transports sheet: "USB to Mac: plug in, then Enable in Audio MIDI Setup") and tested as a first-class sink. Honest limit: IDAM is macOS-only — Windows/Linux DAWs cannot see an iPad over USB natively; point them at Network Session.
  * (b) **virtual CoreMIDI source** (`MIDISourceCreate`) — every on-device iOS DAW, instantly; the same object IDAM tunnels.
  * (c) **Network Session** (rtpMIDI) for desktop DAWs over Wi-Fi (and the Windows/Linux wired-substitute).
  * (d) **BLE peripheral** (`CABTMIDILocalPeripheralViewController`) — convenience sink, budget-limited per §5.3.
* **Android:**
  * (a) **USB gadget MIDI — the primary sink:** class-compliant USB-MIDI peripheral mode, visible to **any** host OS (macOS/Windows/Linux). The user flips the USB mode to MIDI in the system USB preferences; the app then opens the gadget's port via `MidiManager`. Feature-detect and surface the status ("USB-MIDI: active / device in charge-only mode / unsupported on this device") — the OEM dependency is the mode picker, not the protocol.
  * (b) **`MidiDeviceService` virtual device** — the guaranteed path for on-device DAWs.
  * (c) **BLE peripheral advertise** (API 28+; OEM-variable) — budget-limited per §5.3.
* All transports emit the identical byte stream the loopback got (post-decimation per their policy) — one generator, N sinks. MCM/RPN0 goes out on every sink at session open (§5.3), including when IDAM enable or a USB mode flip brings a sink up mid-session (detect via CoreMIDI/`MidiManager` connection callbacks).

---

## 6. Overlay & feedback UI
* Gesture layer sits over the render view (`UIViewRepresentable` overlay / Compose `pointerInput`), rendered natively (SwiftUI Canvas / Compose Canvas) — **never** inside the core.
* Faint cell lattice at low opacity in Play mode (the marbling stays the star); per-touch joystick indicator: a hairline circle at (x0, y0) of radius R_max and a thumb dot at the current Δ_eff — visualizes the deadband and bounded travel exactly as computed.
* Jankó: highlight **all three echo rows** of a touched note at strike (they are the same note; the UI should say so).
* Latency budget: touch-down → visible drop **≤ 2 rendered frames** (loopback bytes enqueue before the frame's `sumi_update` drains the queue).

---

## 7. Stylus legato & the wake (v0.4)

The pen abandons the joystick: precision earns **absolute-position play**. X becomes literal canvas position, Y keeps CC74 (§3.3), Z stays true tip pressure. Pitch follows the pen per-layout:

* **CHROMA_GRID / JANKO — continuous legato:** the strike anchors the note; thereafter bend = actual canvas distance traveled along the probe's `semitone_dx/dy`, divided by `semitone_step` — the pen is always in tune with the cell under it, glissando by construction. **Re-anchor rule:** ±48 semitones covers 4 octaves but the board spans 7; when |bend| reaches ±47 semitones, emit a new Note On at the current pitch with centered bend on the SAME channel (a same-channel legato retrigger — one audible seam per 4-octave sweep, accepted and documented).
* **PIANO_GRID — cell-quantized legato** (pitch is not a function of position along any axis on this lattice, DECISIONS_3 #29): crossing into a new cell retunes to that cell's note via a bend ramp over a 20–40 ms portamento; dead zones (accidental-row gaps and ends) sustain the last pitch. A real piano glissando is quantized too — this is the honest feel of the lattice.

**The dipolar wake rides every pen stroke** in both Play and Marble modes, via `sumi_add_wake` (tip radius from pen pressure). It is physical, not musical — and therefore **not in the MIDI stream**. Invariant, stated plainly: a DAW recording of a stylus performance replays the notes, bends, CC74 and pressure exactly, but NOT the wakes — a DAW has no stylus in the water. The loopback conformance property (§5) is unchanged for everything MIDI-expressible; the wake is deliberately outside it.

**Pinch via the pen (v0.4):** with `slide_mode = 1`, smoothed CC74 *deltas* drive the Hamiltonian pinch at the pen position, fold axis from the pen's azimuth. Three pen dimensions, three distinct physics: pressure = ink feed, Δy = fold depth, azimuth = fold direction. `slide_mode = 0` keeps the v1 per-drop aux behavior — a params choice, never a silent rebinding.

---

## 8. Performance control strip (Play mode)

A dockable strip (top or bottom edge, setting; hidden in Marble mode) of **wheels and buttons built from the existing joystick primitive** — touch anchors the origin, the §3.2 soft-knee shapes Δy, no new interaction to learn or tune.

* **Channel discipline:** every strip message goes out on the **master channel (ch 1)** — the allocator's member channels (2–16) are never touched; global controls and per-note voices are disjoint by construction, exactly as MPE intends.
* **Widget types:**
  * **Spring wheel** — deflection maps to value while held; on release, ramps back to center over ~50 ms (never a jump: a snap is a zipper on the outbound pipe) and guarantees a final center message. Default: **Pitch** (master-channel bend, ±2 — the MPE master default; the strip does not send RPN 0 on ch 1). With `bend_mode = 1` this wheel plays the sine ripple's wavelength while the Mod latch wheel drives its amplitude — the "softer music" configuration, two strip widgets and zero new UI.
  * **Latch wheel** — relative delta accumulation (drag adds/subtracts from the current value): no pickup jumps, arbitrarily fine control by dragging slowly. Default: **Mod (CC 1)**.
  * **Button** — momentary (127 on press, 0 on release) or toggle. Default: **Sustain (CC 64)**, momentary.
  * Plus **two assignable latch wheels** (any 7-bit CC), edited via long-press.
* **Dual-pipe like everything else:** strip bytes go to the loopback (where the CC map may route them — CC 1 → vortex is the existing default, so the mod wheel stirs the water while it modulates the synth) and to all outbound sinks under their per-transport policies. **CC 64 and button CCs join the never-dropped class** (§5.3) — a decimated sustain-off is a stuck pedal.
* **Values persist** across mode switches and layout changes; the strip re-sends its current latched values after an MCM re-sync so the DAW and the strip never disagree.
* **The strip is a compact floating palette at the top-left, OVER the full-canvas lattice** (revised on device, DECISIONS_3 #31 — the original "displaces the lattice" band remapped the probe to a reduced play area, which broke drop-under-finger: a touched cell and its loopback drop were offset by the strip height, and that kills the instrument feel). The overlay keeps the full bounds — cell and drop stay exactly aligned; the palette consumes its own touches and hides only the corner cells beneath it.

---

## 9. PROJECT_SPEC deltas when Phase 4 lands
* §5.3 gains `sumi_cell_info_t` + `sumi_layout_probe` (pure, instance-free). `sumi_version()` → **0.3.0**; the v0.4 operator batch (Rankine/wake/pinch/ripple, PROJECT_SPEC §4.3(3–6)) → **0.4.0**.
* §5.2's producer contract text gains the sentence: "Host-synthesized MIDI (touch surfaces) counts as device MIDI and must flow through the same single producer."
* Known-limits note (from the phase-3 print review): composite thickness probing (DECISIONS_2 #4) speckles on bands thinner than ~10 texels; cosmetic, tracked, not Phase 4 scope.
