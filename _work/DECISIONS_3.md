# DECISIONS_3 — Phase 4: Touch & Stylus MPE Play Surface

Ambiguities resolved during Phase 4 (steps 15–18). Prior history:
`docs/DECISIONS.md` (Part I = v1, Part II = spec v2 — references written as
`DECISIONS_2 #n` mean Part II). This file merges into that document as
Part III when the phase ships. Where these entries and the phase docs
conflict, `_work/PHASE4_SPEC.md` wins.

The first five entries were resolved at spec-review time — design review
against the shipped codebase, before any Step 15 code — and are already
folded into the three phase documents.

1. **Bend formula parenthesization (PHASE4 §3.3): round AFTER the 14-bit
   scale.** `pb = 8192 + round(bend_semitones / pb_range * 8192)`, clamped to
   [0, 16383]. The draft's `round(bend_semitones / pb_range) · 8192` rounds
   the semitone ratio first, quantizing every bend to whole ±8192 —
   all-or-nothing, ±48 semitones or silence. The corrected form is also what
   makes the test assertion true by construction: one semitone at ±48 =
   8192/48 = 170.67 → ±171 counts. The spec now carries an explicit note so
   nobody "simplifies" the parentheses back.

2. **The layout probe is instance-free — the signature IS the thread-safety
   story.** `sumi_layout_probe(uint32_t layout, const sumi_params_t*, float
   aspect, float x, float y, sumi_cell_info_t*)`: a pure function like the
   internal layouts API, not a method on `sumi_instance_t`. Rationale: on
   iOS the UI thread is the render thread, but on Android touches arrive on
   the UI thread while the render thread owns the instance (DECISIONS_2
   #32/#33) — an instance-bound, render-thread-only probe would force every
   touch-down through the command queue just to hit-test, spending the
   ≤2-frame latency budget on a round trip. The shells keep a params
   snapshot beside their UI state (stated obligation in spec and roadmap);
   the probe is callable from any thread and golden-testable headlessly with
   no instance at all.

3. **Outbound rate limiting is per-TRANSPORT, not one policy (PHASE4
   §5.3).** Change-only filtering everywhere, then: virtual CoreMIDI /
   Network Session / MidiDeviceService get ≤ 100 Hz per voice-dimension
   (latest-wins); BLE gets a **global ~300 msg/s budget** with round-robin
   per-voice fairness (testable form: every active voice updates within any
   100 ms window — one wiggling finger cannot starve nine). The arithmetic
   forced the split: a 10-touch storm at 100 Hz × 3 dimensions = 3,000 msg/s
   against a link that sustains a few hundred — a correct implementation of
   the single-policy draft would fail its own DONE test. Note On/Off, the
   initial center bend, and pressure-0-before-Note-Off are exempt on every
   transport.

4. **Entering Play mode pushes MCM + RPN 0 into the LOOPBACK, before any
   notes.** The draft configured only the outbound transports; the loopback
   normalizer would have relied on §2.5 heuristic detection flipping to MPE
   mid-performance, with the first notes decoding under the wrong bend
   range. One MCM (RPN 6, lower zone, 15 members) + RPN 0 = 48 on Play-mode
   entry makes the mode flip and the ±48 range deterministic on both pipes —
   and it is a conformance property of the whole phase (roadmap working
   rule), not a per-step detail.

5. **`hostmpe/`'s public header is pure C, enforced by the build.** Same
   contract as `sumi_core.h` (no STL, no C++ types across the header; the
   C++ implementation lives behind it): the Swift module-map pattern only
   imports C headers, and discovering that mid-Step-16 would mean rewriting
   the API surface under pressure. The existing `abi_c_compile` pattern gets
   a mandated sibling, `hostmpe_c_compile.c`, so the constraint is held by
   CI, not memory.

## Step 15

6. **Probe units: positions normalized, distances in canvas-height units,
   direction aspect-corrected.** PHASE4 §2 said "canvas distance" without
   fixing the metric, and normalized coordinates are anisotropic (a "unit"
   vector in them is skewed on screen). Resolution: `cell_center_*` stay
   normalized (they are positions); `cell_radius` and `semitone_step` are
   distances in canvas-height units — the codebase's universal distance unit
   (deform radii, gesture magnitudes) — and `semitone_dx/dy` is a unit
   vector in aspect-corrected space, derived from the SAME #7 normalized
   delta and converted (`(dx·aspect, dy)`, normalized). A shell measures
   touch deltas in the same metric by dividing pixel deltas by the view
   height; circles are circles. Documented field-by-field in sumi_core.h.

7. **The #7 derivation now has one implementation:**
   `sumi_layout_semitone_delta` (layouts.cpp, internal) returns the
   shortest-neighbor delta UNCAPPED in normalized coords; the voice mapper's
   `pitch_axis` applies its SEMITONE_STEP_MAX rendering cap on top (pure
   refactor — behavior pinned by the pre-existing glide-axis tests), and the
   probe reports the true lattice step. Consequence made explicit: a
   1-semitone bend on the play surface will traverse one full lattice step
   under the finger while the DROP's glide wake is capped at 0.030 canvas —
   the visual glide compression is a v2 aesthetic decision (§3.4), not a
   play-surface bug. Flagged for Step 16's feel pass: if capped wakes read
   as "the drop lags my finger", revisit the cap per-layout (question, not
   code — core stays frozen).

8. **`hostmpe/` is seeded in Step 15 with exactly the §3.2 knee** (soft-knee
   + joystick Δ_eff), nothing else. Step 15's DONE demands headless unit
   tests for the indicator math, and implementing it in Swift would create
   the platform drift the working rules exist to prevent — the same math
   must drive Android's overlay in Step 18. The allocator/bend/rate-limiter
   surface still lands in Step 16 (rule 7 respected: the knee is Step 15
   scope, pulled into its permanent home). `hostmpe_c_compile.c` and the
   unit suite are wired into ctest from day one; iOS links libhostmpe.a via
   a second module map (`import HostMPE`).

9. **The iOS lattice is built by SWEEPING the public probe** (220×120 sample
   points at layout/size change, deduped to cells), not by any Swift-side
   geometry: the overlay renders only what `sumi_layout_probe` returns, so
   the "one source of truth" guarantee is structural — there is no second
   lattice implementation to drift, and the sweep doubles as a smoke test of
   the probe over the whole canvas. Jankó's three-rows-one-note highlight
   falls out of the same map (cells sharing the touched note).
