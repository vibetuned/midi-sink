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

## Step 16

10. **The knee is a DEADBAND, not a travel limit, for the bend axis.** §3.2's
    clamp (d ≤ 1) and §3.3's one-column-one-semitone cannot both hold: on
    the grid a column (0.124 canvas) is ~2.2× R_max (0.057 — half the
    SMALLER cell dimension), so a bend saturating at the joystick circle
    tops out at ~0.46 semitones and the ±171 DONE test is unreachable.
    Resolution: `hostmpe_bend_deflection(d)` = soft knee inside the circle,
    IDENTITY beyond it (continuous at d = 1 where both branches equal 1) —
    far from the origin the bend tracks the finger absolutely, so a
    one-column drag is exactly Δx/step = 1.000 semitone → 171 counts, and
    in-tune glissandi span any number of columns. The CLAMPED knee remains
    the law for the bounded axes: CC74 and the visual thumb indicator.

11. **CC74 polarity lives inside hostmpe, which takes SCREEN deltas.**
    `hostmpe_touch_update(dx, dy, …)` receives raw screen-oriented deltas
    (y grows down) and applies the "up = brighter" ROLI polarity itself.
    Rationale: with two shells feeding it, a pre-negation convention WILL
    eventually be applied twice or zero times on one platform; a single
    documented ingestion orientation cannot.

12. **External-occupancy timeout refreshes on ANY channel traffic.** §5.1
    says occupancy clears "after a 30 s stuck-note timeout" without defining
    the clock. From Note On alone, a ROLI note held 31 s would be declared
    stuck WHILE SOUNDING and channel-stolen — precisely what masking exists
    to prevent. A genuinely held MPE note streams pressure continuously, so
    the timeout counts from the channel's last message of any kind: real
    holds never expire, a silent stuck channel frees in 30 s.

13. **Loopback emission is change-only at the byte level (not decimation).**
    `hostmpe_touch_update` suppresses messages whose byte value is unchanged
    per voice per dimension. §5.3's "loopback = full rate" means no
    RATE ceiling and no latest-wins dropping — an identical repeat carries
    zero information and only pollutes the byte log the DONE asserts read.
    Step 17's outbound change-only filter is a separate, per-transport
    stage on top.

14. **iOS producer topology: one serial DispatchQueue owns hostmpe AND
    `sumi_push_midi`.** CoreMIDI callbacks hop onto it (observe-external +
    push), touch handlers post through it (touch-down uses a sync hop —
    allocation must answer before the overlay can track the touch;
    microseconds), the session config is pushed from it, and the byte log
    appends only there. Serialization-with-barriers satisfies §5.2's
    single-producer contract exactly as the spec's iOS note prescribes; the
    UI thread never calls `sumi_push_midi` directly.

15. **Synthesized finger pressure is BASELINE-RELATIVE** (user-reported: "the
    minimal move makes the drop grow really fast"). An absolute majorRadius
    curve reads a resting fingertip as 0.3–0.8 pressure, and since pressure
    first ships on the first touchesMoved, any wiggle unleashed a strong §4.4
    feed. Now each touch records its contact majorRadius as the baseline and
    pressure = clamp((mr/mr₀ − 1.15)/0.6): resting = 0, the drop grows only
    when the pad visibly flattens (deliberate press). The §4 truth-table
    honesty stands — this is still crude, heavily smoothed glass, not force.

16. **The deadband gets an absolute floor: max(0.03·R_max, 0.006 canvas-
    height)** (`HOSTMPE_KNEE_FLOOR_CH`). §3.2's knee is proportional to
    R_max, but finger jitter is absolute — on Jankó (R_max ≈ 0.018) 3% is
    under a pixel and every micro-wobble bent pitch and slid CC74. The floor
    (~5 pt on the iPad) sits above jitter and below intent on every layout;
    the knee stays smooth and still reaches 1 exactly at the circle (capped
    at 0.9 so it can never swallow it), and the one-column-= -one-semitone
    exactness is untouched because identity-beyond-the-circle is
    knee-independent (unit-tested at Jankó geometry). Applied inside
    hostmpe's r_max-aware entry points; the bare reference forms keep the
    normalized 3% knee.

17. **Bend follows the lattice's 2D pitch GRADIENT, not the #7 axis**
    (user-directed: "the line in Jankó is orthogonal to the chromatic —
    odd"). The #7 shortest-neighbor axis is the stagger vector on Jankó
    (mostly vertical), so the natural horizontal drag bent nothing there.
    `hostmpe_touch_begin` now takes the local pitch gradient (gx, gy) in
    semitones per canvas-height unit; bend = deflection · (gx·dx + gy·dy).
    The shell SOLVES the gradient from the probe-swept neighbor cells (no
    lattice math in Swift): gx from the same-row neighbor, gy from the
    nearest other-row neighbor with gx's contribution removed. The solution
    is illuminating: on the grid, gx = 1 semitone/column and gy = 12/row
    (rows are octaves — vertical drags glide through octaves, lattice-true);
    on Jankó, **gy = 0** — pitch there is a function of x alone (each half
    column = +1 semitone; the rows are echoes of the same notes, which is
    exactly what echo sets assert). Consequence: bend reads horizontally on
    BOTH playable layouts, Jankó gets a continuous chromatic glissando at
    one semitone per half column, and vertical drags drive only CC74
    (timbre) as §3.3 intended. Flagged core question (not code — core
    frozen): the drop's visual glide wake still follows the core's #7 axis,
    which on Jankó is the stagger vector — a horizontal Jankó glissando
    paints slightly diagonal wakes; revisit the core's per-layout glide
    vector if it reads wrong in play.
    SUPERSEDED SAME-DAY by #18 — the 2D gradient shipped for one build and
    the user rejected the feel ("bend and timbre make the same line, timbre
    just larger" — the grid's octave rows made vertical drags read as a
    bigger copy of horizontal ones). The gradient FORM stays in the hostmpe
    API (gx, gy — it is the right abstraction and its unit tests stand), but
    the shipped mapping is gy = 0 everywhere.

18. **Jankó's semitone delta is HORIZONTAL in the core** — the flagged core
    question in #17, resolved by the user's direction. `sumi_layout_semitone_
    delta` special-cases Jankó: half a column straight along +x, because
    pitch there is a function of x alone (the parity rows are ECHOES of the
    same notes — the #7 shortest-neighbor rule mis-picked the stagger vector
    toward note±1's echo row, which is an echo-placement artifact, not pitch
    geometry). Consequences, all aligned: the probe's semitone axis is (1,0)
    on both playable layouts; the drop's glide wake stays IN its row and
    reads horizontally like the grid's; a Jankó glissando is one semitone
    per HALF column (the stagger interleaves them); vertical drags are
    timbre's alone (CC74 — visually subtle by §3.4 design: slide modulates
    the ink selector, not geometry; if it should read stronger on canvas,
    that is a composite question for later). AMENDS spec §3.4's echo-set
    sentence "Glide displaces every echo along … (in Jankó: half-column
    over, one row up)" → "half a column along the row"; the spec author
    should fold that into PHASE4/PROJECT_SPEC text.

19. **Fingers: Y → channel pressure, upward only; no CC74 (spec §3.3 rev,
    author's revision).** Supersedes #15's majorRadius pressure entirely —
    "not sure why I was trying to put pressure under the finger radius."
    Pressure = the upward component of the CLAMPED joystick (-ey): exactly 0
    at touch-down and for any downward Δy, monotonic through the soft knee
    (floored per #16), 127 at full-radius straight up. Pushing INTO the
    lattice upward is the growth gesture — deliberate, visible (drop rings
    grow via the §4.4 feed), and impossible to trigger by resting a finger.
    Fingers emit no CC74 ever (byte-log assertable); timbre belongs to the
    stylus matrix in Step 18. Since the bend gradient is horizontal on both
    playable layouts (#18), the axes are fully separated: X = pitch,
    Y-up = growth, and downward drags are musically silent.

20. **Playable lattices glide at the TRUE lattice step (glide cap lifted for
    grid/Jankó).** User-reported "bug": a far bend + release popped a small
    white drop ~two columns toward the bend, seemingly from nowhere. The
    white circle is the §3.4 lift surfactant ring (spec behavior since
    step 5, radius ∝ release velocity); it looked stray because the visual
    glide was capped at SEMITONE_STEP_MAX = 0.030 canvas per semitone —
    a quarter of a grid column — so the drop never visibly traveled with the
    finger, and the release ring materialized at the capped position. The
    cap protected FIFTHS (neighbor steps up to half the canvas); on the
    playable lattices the true step (grid 0.07/column, Jankó half-column)
    IS the correct visual, and the Step 16 DONE text demands it
    ("one-column drags read as clean semitone glides on canvas"). pitch_axis
    now uses the uncapped delta for CHROMA_GRID/JANKO and keeps the 0.030
    cap for fifths/rolls. Bonus: hardware MPE (ROLI) glides on the lattices
    now also land on their true cells. The lift ring itself stays — with the
    drop now visibly arriving where the finger goes, the ring reads as "the
    drop set here", which is its §3.4 meaning.

21. **The lift ring lands at the voice's BASE — the note's home cell — not
    the glide-displaced position** (user: "better if it appears under the
    note, now that the tablet is the instrument"). The release mark belongs
    to the NOTE: after a far bend the ink has wandered, but the ring stamps
    the pitch home it resolves to — one per echo, as before. Applies to all
    MPE sources (a ROLI release after a bend rings its home cell too);
    wind-mode migrate updates the base, so the brush rings where it last
    landed, unchanged in practice.

## Step 17

22. **RETRACTED AND CORRECTED — rtpMIDI does NOT reorder RPN; my analysis
    did.** The original entry claimed Apple's rtpMIDI recovery journal
    regrouped CCs by controller number, breaking the MCM/RPN0 handshake over
    the network session. That was wrong, and the error was mine: the capture
    analyser did `rows.sort()` on `(t, status, d1, d2)` tuples while the
    listener stamps every message in one delivery callback with the SAME
    timestamp — so ties were re-ordered by status/controller byte, which puts
    CC6 before CC100/CC101 and Note On (0x90) before Bend (0xE0). The
    "controller chapter" theory was fitted to an artifact of my own sort.
    Re-analysed in wire order, **every transport delivers correctly**:
    master MCM `101=0, 100=6, 6=15`, all 15 members at RPN 0 = 48, zero
    misordered data entries, and 100% of note-ons preceded by their center
    bend — on USB/IDAM (90/90), rtpMIDI (20/20) and BLE (80/80). The
    handshake DONE is therefore satisfied on ALL sinks, not just a
    "guaranteed path". Lessons kept: the monotonic per-message timestamp
    stays as ordering hygiene (harmless, and correct for any
    timestamp-batching transport), the 4 ms config spacing was reverted
    (it was treating a phantom), and capture analysis must never re-sort
    equal-timestamp records — delivery order IS the data.

23. **The outbound rate/fairness/change-only policies are transport-agnostic
    and were validated on the network capture** (which the journal does NOT
    reorder — notes and bends have their own journal chapters that preserve
    order): worst per-slot 1 s rate 70/s under the 100 Hz policy, 30 active
    slots, notes balanced, change-only holding (the only identical-value
    repeats were sub-20 ms rtp retransmits and legitimate return-to-value
    sweeps seconds apart), and the 1 Hz exempt marker drifted +46 ms over
    60 s (no cumulative lag). The limiter's headless suite (rate ceiling
    95–101/s under a 1 kHz storm; budget ≤650/2 s with every slot inside a
    115 ms fairness window; exempt notes never dropped) is the authority; the
    live capture corroborates on real transport timing.

24. **iOS refuses virtual MIDI endpoints without the `audio` background
    mode.** `MIDISourceCreate` returned **-10844 (kMIDINotPermitted)** and
    both outbound sources came back as endpoint 0, so every virtual-source
    send went to a null endpoint — invisible, because CoreMIDI status codes
    were not being checked. Adding `UIBackgroundModes: [audio]` to the
    Info.plist fixes it (the entitlement iOS requires for an app to publish
    MIDI other apps can see). Two lessons folded into the code: every
    CoreMIDI call in `MidiOutputs` now logs its OSStatus, and the shell shows
    a live per-sink sent counter (`out v…/n…/b…`) so "are we transmitting?"
    is answered by observation, not inference. Side effect, desirable for a
    controller app: the surface keeps streaming MIDI while backgrounded (the
    display link still pauses — Metal in background is a crash, #13 era).
    Also fixed here: `MIDIPacketList()` is a ONE-packet struct, but
    `MIDIPacketListAdd` was being told it had 1024 bytes — a latent stack
    overflow that a limiter drain (up to 64 messages) would have hit. The
    list is now backed by a properly sized buffer.

25. **BLE topology: send to the Bluetooth-driver DESTINATION, and the
    receive-side count is not the sender's rate.** Our "midi-sink (BLE)"
    virtual source only publishes locally; bytes reach a connected central
    by `MIDISend` to the destination iOS creates for the link (matched by
    `kMIDIPropertyDriverOwner` containing "bluetooth", deduped per ENTITY so
    a multi-endpoint driver cannot be sent to twice). Mirroring to our own
    virtual source was removed — iOS bridges device sources over the same
    link, so the mirror duplicated delivery. Measurement finding: the
    receiver logged 483 msg/s where the sender logged **315 msg/s**
    (18,900 in 60.0 s — the ~300 budget plus burst headroom, exactly as
    designed), with the surplus appearing as duplicate values whose delivery
    timestamps are **identical to the microsecond** (100% same receive
    callback) while only ONE destination existed on the sender. Since the
    limiter is token-metered and change-only (it cannot emit a repeated
    value at all), the inflation is CoreMIDI's BLE→UMP running-status
    expansion on the receive side. **Therefore the budget DONE is asserted
    at the sender**, where the policy lives; receiver captures corroborate
    fairness and lag, which duplication does not distort.

26. **A BLE peripheral cannot disconnect its central; "stop" is panic +
    per-sink silence instead.** User asked for a disconnect button. Apple
    exposes no public API for a BLE MIDI *peripheral* to tear down a link —
    the central (Mac/DAW) owns it, and `CABTMIDILocalPeripheralViewController`
    governs only advertising. What the request really needs is the guarantee
    that stopping never leaves sound stuck, so two shared primitives landed
    in hostmpe (Android inherits them in Step 18):
    `hostmpe_panic` releases every live voice through the normal
    pressure-0-then-Note-Off order and then silences the zone (CC 64 = 0 +
    CC 123 = 0 on master and all 15 members), and `hostmpe_silence_zone` is
    the stateless half — controllers only, voice table untouched. The
    explicit **"Stop all notes (panic)"** button uses the former on the
    loopback and every transport (exempt, never decimated); switching a
    transport OFF automatically uses the latter on just that sink, so a synth
    that stops receiving cannot hold notes while voices still sounding on the
    other pipes keep playing. The UI states plainly that dropping the BLE
    link itself is done from the connected device's Bluetooth settings.

27. **Reaching a wired Mac needs an explicit send to the IDAM destination —
    a virtual source alone is NOT bridged (spec correction to §5.4).** The
    revised §5.4 frames the virtual CoreMIDI source as "also the USB/IDAM
    primary sink". Measured: with the iPad tethered, the Mac lists the port
    (`name='iPad', driver='com.apple.AppleMIDIUSBDriver'`) and the iPad lists
    a destination `'Hôte MIDI IDAM' driver='com.apple.AppleIDAMDriver'`, but
    publishing only via `MIDIReceived(virtualSource)` delivered **nothing**
    to the Mac. The wired path is an explicit `MIDISend` to the IDAM
    destination. Implemented inside the SAME sink and the same ≤100 Hz
    policy — one toggle, two delivery mechanisms (virtual source for
    on-device apps, IDAM send for the tethered host) — which keeps the
    spec's one-sink framing while matching reality. Verified: 926 messages
    on the Mac's `iPad` port, handshake ordered, 90/90 note-ons preceded by
    center bend, worst per-slot rate 34/s (policy 100 Hz).
    Also: MIDI over USB needs no "Enable" — the Enable button in Audio MIDI
    Setup's Audio window is for IDAM AUDIO; MIDI Studio shows the device as
    connected on its own. The in-app hint says so. And no transport can
    expose our per-app port name to the host: BLE, USB/IDAM and rtpMIDI each
    present ONE merged port per link, named after the peer DEVICE (the Mac
    sees "iPad"; the iPad sees "LT-… Bluetooth"). Per-app names exist only
    locally on iOS.

28. **A sink appearing mid-session re-sends the handshake.** IDAM is
    typically enabled (and BLE centrals connect) AFTER the session opened,
    so the MCM/RPN0 sent at Play-mode entry would never reach it.
    `MidiOutputs` now watches CoreMIDI `msgSetupChanged` and fires
    `onSinkAppeared` when the world GREW (destinations/sources/devices
    count up — teardown needs no handshake), which the shell debounces to
    one re-send per 2 s and only while Play mode is effective.

## Between Step 17 and Step 18

29. **`SUMI_LAYOUT_PIANO_GRID` (= 5), a third playable lattice** (user-requested,
    amends PHASE4 §1's "grid and Jankó only" sentence and spec §3.4). The
    chroma grid's frame — C1..B7, insets 0.08/0.10, out-of-range clamps to the
    edge octave keeping pitch class — but each octave is a classical two-row
    keyboard: 5 accidentals on top at the classic boundary positions (white-key
    units {1, 2, 4, 5, 6}; the E–F and B–C gaps stay empty), 7 naturals below;
    14 rows, one echo. Resolved here:
    * **Accidental cells are one white-key unit wide**, centered on the
      boundaries, so the black row tiles [0.5, 6.5] with dead zones at the row
      ends and the two gaps — the probe refuses there (same "off the key bed"
      rule as the Jankó stagger ends). Naturals tile their row completely.
    * **R_max is the KEY footprint, not the drawn row: half of min(key width,
      octave-pair height).** First device test read the inscribed single-row
      radius as knobs half the chroma grid's size (14 rows vs 7). The
      black/white split is a drawing convention — a key's playable footprint
      is one key wide by one octave tall — and R_max is a travel bound, not a
      hit region (hit-testing happens only at touch-down). The octave-pair
      height equals the chroma grid's row height exactly (0.8/7), so knob
      size, deadband scale and CC74 travel match the chroma grid's feel; the
      lattice circles now nest diagonally between rows (no two adjacent-row
      cells share an x), which reads as a honeycomb, verified by headless SVG
      render before shipping.
    * **The semitone axis stays on the generic DECISIONS_2 #7 shortest-neighbor
      rule — no Jankó-style special case.** Jankó got a horizontal override
      (#18) because pitch there is a function of x alone and the parity rows
      are echoes; on the piano lattice pitch is NOT a function of x (two rows
      per octave), so the honest per-note axis is the half-key diagonal toward
      the adjacent accidental/natural (ties, e.g. D between C# and D#, resolve
      inside the existing rule toward the +1 neighbor). Consequence a pianist
      will recognize: glides bend toward the nearest key, alternating up/down
      diagonals, rather than along a fictitious straight pitch line.
    * **Joined the true-step lattice set** in the voice mapper (#20): glides
      render the uncapped lattice step, so a one-semitone bend lands the drop
      on the neighboring key cell.
    * Hosts: iOS picker + playable checks updated ("Piano grid"); the play
      overlay needed nothing — its lattice is a probe sweep (#9). Desktop `L`
      key now cycles 6 layouts. Evidence: `docs/evidence/piano-grid-layout/`.

## Step 18

30. **Strip engine resolutions (§8).** The widget value engines live in
    hostmpe (`hostmpe_strip_t`, pure-C API, headlessly tested), master-channel
    only by construction. Ambiguities resolved:
    * **The limiter gains 128 MASTER-channel CC slots.** Before Step 18,
      generic CCs bypassed the per-transport policies entirely (`lim_slot_index`
      returned -1 → pass-through), so a latch wheel would have flooded the BLE
      budget unpoliced. Master-channel CCs are now ordinary continuous
      dimensions (change-only + decimation/budget + round-robin fairness);
      member-channel CCs other than 74 still pass through — hostmpe generates
      none and external-device bytes never enter the limiters. The
      never-dropped class (§8: CC 64 / buttons) rides the existing `exempt`
      flag, asserted headlessly: a 10-voice 1 kHz bend storm through the
      300 msg/s budget passes every CC64 transition immediately, zero dropped.
    * **Latch regrasp is jump-proof by construction:** the API has no
      absolute-set entry point — only `hostmpe_strip_latch_move(delta)`. The
      shell feeds the CHANGE in knee-shaped grab position, so a fresh grab
      contributes delta 0. Sub-unit deltas accumulate in float (fine control
      by slow dragging); emission is change-only on the rounded 7-bit value.
    * **Assignable wheels refuse the protocol CCs** (1, 6, 38, 64, 98–101,
      120–127): a strip-assigned CC 6 on the master would corrupt the DAW's
      RPN handshake state mid-performance. Defaults: CC 23 / CC 24 — mapped
      to viscosity / roughness in the loopback's default CC map, so the
      wheels do something visible before Step 19 rebinds them to the ripple
      controls. Assignments are session-only (preset persistence: deferred).
    * **The spring ramp is time-driven with a guaranteed exact-center final
      message:** 50 ms linear from the release value, emitted change-only from
      `hostmpe_strip_tick` on the shell's frame drain; a single late tick
      still lands exactly at 8192 (unit-tested). Grabbing mid-ramp cancels it.
      A sustain MODE switch while ON emits the OFF — never a stranded pedal.
    * **Lattice displacement is an overlay resize:** the strip takes a docked
      band (≤ 15% of height, min with 96 pt) and the overlay view gets the
      remainder — its bounds-normalized probe coordinates and lattice remap
      automatically, zero core involvement. Consequence, accepted: loopback
      drops land at FULL-canvas layout positions, so a cell and its drop are
      vertically offset by up to the strip height while the strip is docked;
      the §6 pixel-perfect alignment property holds only with the strip
      hidden. Flagged for the user's feel pass rather than silently absorbed.
    * The wheel joystick metric is POINTS (travel bound 60 pt), not
      canvas-height units: the strip is a fixed-height bar, and the §3.2 knee
      only requires Δ and r_max to share one metric.

31. **Device test rejected the docked-band strip; §8 amended to a compact
    floating palette top-left, over the full-canvas lattice.** Two findings
    from the first on-device session:
    * **Drop-under-finger is non-negotiable.** The §8 draft's "displaces the
      lattice" band resized the overlay, remapping probe coordinates to the
      reduced play area — but loopback drops land at FULL-canvas layout
      positions, so every touched cell and its drop were offset by the strip
      height. The #30 entry flagged this consequence for the feel pass; the
      verdict: it "breaks the feeling of the controller". The overlay now
      always keeps the full bounds (alignment exact, the §6 property restored
      everywhere); the strip floats at the top-left (~300×86 pt, translucent),
      consumes its own touches, and hides only the corner cells under it.
      The dock top/bottom setting is gone with the band.
    * **A held sustain released itself after 0.5 s — the CC-editor long-press
      recognizer was the culprit, not the engine.** UILongPressGestureRecognizer
      cancels the view's touches when it fires (UIKit default), so holding the
      pedal for half a second delivered touchesCancelled → sustain up, while
      the engine and byte log looked "correct". Fix: the recognizer's delegate
      only lets it receive touches over the two ASSIGNABLE wheels (the only
      widgets with an editor). Hardened alongside: every widget touch is now
      tracked in the grab table (sustain included), so a release resolves by
      its grab record, never by where the finger happens to lift. Sustain
      stays MOMENTARY by default (the user wants the press-and-hold pedal
      feel); the latch toggle remains a setting.
