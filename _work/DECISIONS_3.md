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

## Step 19

32. **v0.4 operator batch — resolutions from implementation and measurement.**
    Version 0.4.0; `sumi_add_vortex` gained the profile argument (breaking,
    all call sites updated); params grew (slide_mode, vortex_profile,
    ripple_bake, ripple_angle); ctl dims 7/8 (ripple amp/freq).
    * **Wake sign: the spec draft's outer formula was inside-out.** With the
      draft's `+`, the outer field at the front seam gave P + d⃗ while the
      inside rule gives P − d⃗ — contradicting its own zero-seam claim. The
      doublet satisfying the no-penetration boundary is φ = −U a² x/r², whose
      displacement field enters the inverse lookup as P_src = P − Δ. Spec
      formula corrected; the §4.3(4) acceptance test (front bulges forward,
      flanks stream backward) passes numerically and visually (evidence PNGs).
    * **Sub-step budget: a/2 is the fold THRESHOLD, not a safe budget.** At
      per-pass displacement d the inverse-map Jacobian at the rear stagnation
      point is 1 − 2d/a — exactly zero at the spec's "≤ a/2". The core
      sub-steps at ≤ a/4 (det ≥ 0.5 everywhere; measured min det 0.44 after
      an 8×a one-frame flick). Spec corrected. Also learned: the correct
      no-fold test is the pre-image Jacobian over the FLUID region — the tip
      corridor carries the body's slip surface (a genuine tangential
      discontinuity of potential flow, not a fold), and u-monotonicity along
      a row is wrong off the symmetry axis.
    * **`sumi_add_pinch(x, y, k_delta, angle)` is a public gesture-ABI
      addition** beyond the spec's §5.3 delta list: the fold axis is
      host-side data (pen azimuth in Step 20, drag angle in the harness) with
      no MIDI path, and the roadmap's harness binding requires it. The MIDI
      route stays: slide_mode = 1 drives the same pass from smoothed per-voice
      CC74 DELTAS at the voice position, fold axis defaulting to the voice's
      lattice pitch axis; the first CC74 of a voice snaps (primes) instead of
      pinching, so a controller's rest position never fires a spurious
      gesture. Window S = 0.02 (shared constant). In slide_mode 1, slide no
      longer modulates aux (one controller, one meaning).
    * **Pinch soak semantics.** (+k, −k) pass pairs at one center/angle invert
      exactly (s = xy is conserved along trajectories, so the −k pass sees the
      same w(s) field): 500 strong pairs hold band areas to < 2%. The DONE
      stream runs through the real slide_mode-1 route (gesture-rate CC74
      wobble, 36 000 frames). NOT asserted, deliberately: an adversarial
      schedule (full-strength k, fold axis rotating every pass) is chaotic
      advection — it filaments ink below texel resolution where bilinear
      resampling averages it to gray, the same way real marbling over-folds
      to mud. That is the §4.1 resampling medium at work, not an operator
      area leak (verified: the analytic map has det = 1; the drained ink
      tracks filament width crossing the texel scale).
    * **Pinch variant pick (by eye, from the evidence pair): HAMILTONIAN.**
      The saddle stretches one diagonal while compressing the other with four
      fading crease arms — pinched paper. The crossed-tine composition merely
      translates material along two lines (no stretch/compress pairing) and
      reads as a lumpy directional smear. PNG pair in the step-19 evidence;
      the harness keeps the X-key prototype for re-judging.
    * **Ripple ctl dims ship UNMAPPED in the default CC map.** The spec enum
      comments suggested "dflt: CC1" — but CC1 → vortex strength is
      load-bearing (Step 18 DONE: the mod wheel stirs the vortex), and
      "bend" is not a CC. The harness maps CC 102/103 locally for its keys;
      the strip's assignable wheels take them on-device (Step 18 rationale).
      Spec comments cleaned. φ has no control surface in v0.4 (fixed 0):
      ripple_angle already rotates the pattern frame, and the k control
      demonstrates the same commutativity boundary a φ control would.
    * **The ripple group-identity DONE runs on the LIVE path** — an LFO on A
      through the composite view displacement leaves the field BITWISE
      identical because live ripple never writes (2 097 152 bytes memcmp).
      Bake-mode passes compose additively in exact math, but each ping-pong
      pass resamples (like every operator); k/angle changes bake residue by
      design. The amp = 0 live branch keeps the un-rippled composite
      bit-identical to v0.3.
    * **Crease-ring measurement is one-sided by construction:** the true
      |dα/dr| is zero inside R and maximal immediately OUTSIDE (1/r³ decay),
      so a sampled max-gradient centers just past R plus a texel of bilinear
      smear. Asserted as [R, R + 3 texels], never inside the rigid core —
      the sharp half of "exactly at R". Interior rigidity: 20 full rotations
      leave interior ink at mean |Δ| 0.00000 vs 0.758 for the exponential
      control at the same total angle.
    * `sumi_midi_harness_inject` (desktop): synthetic bytes through the SAME
      §5.2 producer mutex as device callbacks — the ripple keys ride the real
      MIDI/ctl path without a second producer.

33. **Mass conservation on a finite canvas: the ingress rule extends to the
    v0.4 operators, and the pinch-soak DONE is re-grounded on the medium's
    own baseline.** Measured on the way to the §4.3(5) soak gate:
    * **Edge-clamp FABRICATES ink.** The pinch's fold-axis corridors cross
      the canvas edge at full strength (w does not decay on the axes); with
      clamp-to-edge sampling, every compression half-cycle duplicates
      boundary content inward — measured +9.5% ink mass over 12 000
      gesture-rate passes. The §3.4 scroll ingress rule ("a source beyond
      the canvas is fresh water — this cannot be delegated to sampler
      state") now applies to the WAKE, PINCH and RIPPLE-BAKE passes too.
      Drop/tine/vortex keep their v1 clamp behavior (fixture-pinned;
      flagged, not changed): measurable only under torture — a 36 000-pass
      glide-tine stream NETS +5% mass as clamp fabrication overtakes
      erosion. Normal play never sustains such chains on one voice;
      extending the ingress rule to the v1 operators would break the
      committed cross-backend fixture and belongs to a deliberate future
      decision, not this step.
    * **Bulk erosion is the resampled medium, not the operator.** After the
      ingress fix, a 6 000-pass CC74 stream still lost 10.6% ink mass —
      spatially uniform across the inked region, ZERO edge component. The
      control: the identical stream driven through the v1 GLIDE TINE erodes
      6.9% (1.15e-5/pass vs the pinch's 1.76e-5/pass — same mechanism,
      same order). Every sub-texel warp pass pays a small bilinear-resample
      mass fade; 36 000 passes of ANY operator fail a strict "conserves
      within noise" reading, the incumbent tine included. Chaotic schedules
      (full-strength k, rotating fold axis) additionally filament ink below
      texel resolution — over-folded real marbling mixes to gray the same
      way.
    * **The soak gate, re-grounded:** (a) det = 1 verified symbolically
      (the streamline window keeps the Jacobian exactly 1 — derived in
      review); (b) 500 strong (+k, −k) pairs invert analytically (s = xy
      conserved along trajectories) and hold ink mass to ±0.5%; (c) zero
      fabrication (mass never grows > 0.5%); (d) the pinch's per-pass
      erosion ≤ 2× the glide-tine baseline under the identical stream.
      Level-set areas and band-parity histograms were rejected as
      observables — blur moves any threshold's contour and parity mixes to
      the regional mean; ink MASS (Σ phase) is what the analytic map
      conserves. **The roadmap DONE's literal wording ("10-minute stream
      conserves total ink band area within measurement noise") is
      unsatisfiable on this medium for any operator and needs the user's
      amendment to the four-part gate above.**

34. **Both pinch looks ship; `pinch_variant` params switch (user override of
    the #32 pick).** Shown the step-19 pair, the user kept the crossed-tine
    variant ("worth having") beside the Hamiltonian saddle. Resolution: a
    v0.4 params field (0 = saddle, 1 = crossed tines; 0.4.0 was uncommitted,
    so the struct amendment folds into the same version) honored by BOTH
    pinch routes — the MIDI path (slide_mode = 1) and `sumi_add_pinch` —
    via one shared constructor (`sumi_deform_crossed_pinch`, displacement.cpp:
    the step-19 prototype verbatim, one tine along the fold axis + one along
    the perpendicular; |k| → magnitude × 0.2, the demo's calibration; k's
    sign reverses both drags). Costs two passes per emission (the mapper
    reserves 2 × echo_count). The crossed look is NOT area-preserving in the
    saddle's exact sense — it is two ordinary tines, with the tine's known
    behavior. Surfaced on iOS ("Slide (CC74)" section: Hue/Pinch routing +
    Saddle/Crossed style) and Android (same rows in the settings dialog);
    desktop key `C`. The pinch-demo evidence pair now renders the crossed
    variant through the real params path. Also fixed while wiring Android:
    `nativeSetLayout` still rejected layout > 4, silently ignoring the
    Piano-grid picker entry added in #29.

35. **`bend_mode` — the sine ripple's toggle, CORRECTED SAME-DAY: it governs
    the PER-NOTE pitch bend, not the master bend, and the mod wheel / vortex
    routing is untouched.** The first implementation misread the user's spec
    note as master-bend routing and paired it with a CC1→ripple-amp remap;
    the stated intent: "make the vibrato more subtle when the music requires
    that" — a note's bend wobble should shimmer the water instead of
    wiggling its drop. As implemented:
    * **Mode 0 (default)** = v1 glide: a note's bend drags its drop along
      the pitch axis. **Mode 1** = the per-note bend feeds the sine ripple's
      wavelength; the drop HOLDS (one consumer owns the note bend). Master
      bend keeps its v1 shear tine in BOTH modes; CC1/vortex untouched.
    * **The amount IS the bend's distance from center** (second same-day
      refinement, the user's design: "as with the glide — we even test that
      we can come back from a ripple"). amp ctl = |semis| / 6, clamped
      (±0.5-semitone vibrato breathes ~8%, |±6| saturates; last writer wins
      across voices, smoothed like any global control). The water stills
      ITSELF: bend re-centers → amount 0 (and the bake deltas compose back —
      the group property); the last note's release → amount 0; a mode flip
      1→0 zeroes the residual target (mapper tracks the flip). The
      wavelength k stays a flavor ctl (RIPPLE_FREQ, resting mid-range 0.5;
      CC 103 / a strip wheel adjusts it). No amount slider — removed.
    * **Clean flips:** in mode 1 the voice's glide target is left untouched,
      so switching back to glide lets the smoothed glide catch up to
      wherever the bend actually is — no jumps, no stale-delta tines
      (unit-tested both directions in `test_bend_mode_single_consumer`,
      which also holds the roadmap's OR-never-both gate: a member-channel
      bend sweep emits glide tines XOR ripple passes).
    * **Shells:** iOS/Android map CC 102/103 → amp/freq (otherwise-unused
      CCs, external/strip handles; in mode 1 a CC-102 writer and the bend
      share the amp slot last-writer-wins). iOS "Note bend" section
      (Glide/Ripple); Android row; desktop key `M` (R/T = CC 102, F/G =
      CC 103 as before).
    * Params grew inside the still-uncommitted 0.4.0 (`bend_mode`, dflt 0).
      Housekeeping: spec/roadmap files are edited by the USER only — agents
      record here and the user folds decisions into the documents.

36. **Ripple vibrato is PERMANENT, like glide (user request: "it fixes
    itself, contrary to the glide — make it permanent").** The self-healing
    the user saw was the LIVE insertion point (a view-only displacement,
    §4.5). Resolution, using the spec's own designed mechanism (§4.3(6):
    "changing φ between passes bakes residue in — that residue IS marbling"):
    * The Ripple toggle now selects `bend_mode = 1` AND `ripple_bake = 1`
      together on iOS/Android/desktop-M — there is no separate live/bake
      control on the tablets; the toggle IS the choice. (Desktop keeps `K`
      as a manual live/bake override for experimentation.)
    * Under BEND-driven bake, the emitted pass phase drifts with activity
      (φ += |ΔA|/A_max × 1.5 rad per pass, wrapped): an excursion never
      retraces exactly, so each vibrato cycle lays a slightly shifted comb —
      a faint feathered record that accumulates with the music, the way
      glide leaves tines. The DYNAMIC still stills (#35's three come-back
      paths hold: the amp ctl goes home on re-center/release/mode-flip);
      the MARK stays.
    * CC-driven bake (bend_mode 0) keeps φ fixed — the pure composing-back
      group property, so the step-19 ripple-group DONE gate is untouched
      (re-verified). The LIVE path also survives unchanged underneath: a
      CC-102-driven shimmer while in Glide mode remains the self-healing
      view effect.
    * Proven at field level: `--ripple-permanence-test` — rings + a 3-cycle
      ±2-semitone vibrato ending at center, note released, amp settled to
      zero → 76,540 far-field texels permanently changed (u channel,
      bitwise), while the same scene through the group test's CC path
      composes back (mean |du| 0.0003). Unit level: the mode-1 sweep's
      ripple passes carry drifting phases (asserted); glide-XOR-ripple and
      all #35 gates re-pass.

## Step 20

37. **Lamb–Oseen swirl & bipolar press — resolutions from implementation and
    measurement.**
    * **Swirl rate is CORE-ANGULAR:** the accumulated per-frame step is core
      rotation in radians (ω = SWIRL_OMEGA·amount·expansion_rate, ω_max =
      2 rad/s), converted at emission to the pass strength S = θ_core·2π·r_c²
      with the CURRENT r_c — so the core's felt speed is amount-proportional
      and the far field scales with the drop's own size (a grown drop stirs
      farther, by physics). Steps below 0.0008 rad merge (budget starvation
      carries over, like press growth); echo sets emit all-or-none.
    * **GLSL has no expm1** — the shader guards the 0/0 form with the series
      branch below x = r²/r_c² < 1e-3 (θ = S·(1 − x/2)/(2π·r_c²), whose x = 0
      value IS the analytic θ(0) limit). The guard is verified CPU-side
      against the analytic limit (1e-3 relative) with branch continuity
      2.3e-5; the field shows no NaN within 2 r_c and a bounded core.
    * **Half-float ULP freeze at the core, found while measuring:** per-pass
      swirl displacement θ·r falls below the u/v channel's storage quantum
      near the center (the quantum depends on the texel's VALUE — one voice's
      neighborhood accumulated while another's froze at identity, u ≈ 0.14
      vs 0.28). PROTECTIVE in practice — sub-quantum stirring cannot erode
      the core — but field measurements near r → 0 read low, never high;
      the swirl-test asserts boundedness there and does its profile checks
      at radii above the quantum. Same medium family as #33.
    * **press_mode does not reroute the wind brush** (breath is the brush's
      life; §2.3 semantics win) — it arbitrates 0xD0 on MPE member channels
      only. 0xA0 routes to the swirl unconditionally, keyed by the voice's
      note (a stray note number is ignored).
    * **Lift releases an engaged swirl half:** touch_end emits 0xA0 0 between
      pressure-0 and Note Off when the down axis was engaged — a synth
      latching poly AT must not stick. Buffer contracts grew (touch_update/
      end need 3; panic 77) — header comments updated, all call sites already
      passed larger buffers.
    * **Counter-rotation is drop-counter parity** (phase_base odd/even), so
      consecutively struck notes counter-rotate regardless of pitch —
      field-verified: +1.50 vs −0.65 rad about two cells' centers.
    * Core coherence, field-verified: ring sharpness inside 0.7 r_c retained
      EXACTLY (0.0143 → 0.0143 mean |∂ink/∂x|) through ~4 rad of stirring
      while 29 014 texels moved in the 2–3 r_c annulus — the drop really is
      the vortex core.
    * Full regression: the entire step-19 battery + fixture (bitwise) + ctest
      re-pass with the new operator in the build (19 ok / 0 fail).

## Step 21

38. **Stylus legato, wake & pinch — resolutions.**
    * **Engine split:** the SHELL owns the probe (cell under the pen,
      displacement projected on the anchor cell's semitone axis); hostmpe
      owns pitch state (`pen_begin/bend/retune/slide/pressure/tick`) so the
      golden traces are headless and Android inherits the engine verbatim.
      Pen voices come from the same §5.1 allocator and end through
      `hostmpe_touch_end` (the Note Off releases the CURRENT anchor after
      re-anchors — unit-asserted).
    * **Re-anchor accounting:** the shell keeps sending TOTAL displacement D
      from the strike; hostmpe subtracts its accumulated `pen_offset`. At
      |D − offset| ≥ 47: offset += round(eff), note += the clamp-safe same
      amount, emit center bend → Note On → residual bend as ONE batch. The
      shell sends any batch containing a Note On WHOLE as strike class
      (exempt) — a re-anchor seam must arrive intact on every transport.
      Golden: a 60-semitone sweep tracks pitch within 1 cent ACROSS the seam,
      exactly one re-anchor, monotone throughout.
    * **Piano retune ramp:** 30 ms (inside the spec's 20–40), tick-driven on
      the frame drain like the strip spring; lands EXACTLY on target; a
      mid-ramp retune restarts from the current interpolated pitch (golden:
      the first post-restart bend lies strictly between the half-ramp pitch
      and the new target). Dead zones = no calls = sustain, by construction.
    * **slide_mode = 1 + pen: CC74 goes OUTBOUND ONLY.** The shell drives the
      azimuth-fold pinch through `sumi_add_pinch` (azimuth has no MIDI path);
      pushing the same CC74 into the loopback would have the mapper emit a
      SECOND pinch at the voice position. A DAW replay still pinches — via
      the mapper's CC74 route with the pitch-axis fold (the humbler axis);
      live keeps the azimuth. slide_mode = 0 sends CC74 to both pipes (aux
      hue, the v1 meaning). Logged as the §5 loopback-conformance exception
      beside the wake (which is physical, never MIDI).
    * **Velocity from real force** (§4) — CORRECTED after the first DAW test
      (strikes recorded near-silent): touch-down force is sampled BEFORE
      contact force builds, and normalizing by maximumPossibleForce (≈4.17
      finger-units on the Pencil) compressed everything toward zero. The
      calibration is UIKit's native unit (UITouch.force: 1.0 = an average
      finger touch): velocity = 96 + (max(force, 1) − 1)·15.5, clamped 127 —
      a baseline tap equals the finger default (96), force 3 is maximally
      loud, and sub-baseline touch-down readings clamp UP to baseline so the
      pen never whispers by accident. Wake tip radius stays
      a = 0.006 + 0.030·(force/maximumPossibleForce).
    * **Tilt → master CC 1 by default** (altitude → 0 upright..1 flat,
      change-only): stirs the vortex through the default map — remappable via
      the CC map like every assignable. Hover (M2 iPads) draws a ghost ring
      in the overlay; inert elsewhere. Palm rejection: unchanged, deliberately
      (pencil touches are typed; fingers keep the joystick path).
    * CC74 default for pens is CENTER (64) at pen-down — the §3.3 stylus law
      — with change-only emission from there.

39. **Stylus legato REDESIGNED to per-cell same-channel retriggers (user:
    "the stylus was mainly introduced to make a legato–glissando... right now
    it never changes note and behaves like the finger").** The diagnosis was
    architectural: the finger joystick ALREADY bends continuously and
    semitone-exactly (#10's identity-beyond-the-circle), so §7's
    continuous-bend pen added nothing audible — the stylus's reason to exist
    is REAL NOTE CHANGES. Replaces #38's bend/retune design:
    * `hostmpe_pen_glide(voice, cell_note, offset_semis, velocity)` — the
      shell probes the cell UNDER the pen each move; crossing into a new cell
      emits the legato overlap idiom: bend(offset) → Note On(new, velocity
      from the CURRENT force — glissando dynamics are live) → Note Off(old).
      Same channel throughout: mono/MPE synths glide, the DAW records real
      terminated notes, our normalizer's same-channel steal ignores the stale
      Off. Inside a cell the offset from its center is the bend — vibrato
      without retriggering. Sounding pitch is CONTINUOUS across crossings
      (the offset flips sign as the reference cell changes) — golden: a
      12-cell sweep shows 12 retriggers, 12 Offs, sub-cent tracking
      everywhere, monotone.
    * The ±47 re-anchor rule is RETIRED (bend never accumulates past ±½ cell)
      and with it the piano-grid 20–40 ms retune ramp (the synth's own
      legato/portamento is the smoothing; retriggering IS the note change).
      Dead zones still sustain by construction (no probe hit → no call).
      One behavior for all three playable lattices.
    * Retrigger batches ship WHOLE as strike class (the bend→On→Off crossing
      must arrive intact); bend-only batches stay policed continuous.
    * **Boundary hysteresis (same-day refinement — "bend should still exist
      inside the cell"):** a crossing commits only once the pen is ±0.65 st
      past the CURRENT note; until then the true pitch offset keeps bending
      the current note, so vibrato near a cell edge bends instead of
      machine-gunning retriggers (golden: a ±0.1-st wobble across a boundary
      emits bends and ZERO Note Ons; a deep push commits exactly once). The
      end-to-end path is also unit-proven: the pen's in-cell offset bend
      routes through the bend_mode machinery like any per-note bend — mode 0
      glide tines, mode 1 ripple breathing (the amp then tops out at
      |0.5|/6 ≈ 8% because a cell bounds the pen's bend — flagged: raise the
      /6 saturation if the pen's shimmer needs more presence).
    * Visual consequence, deliberate: each retrigger is a VoiceBegin — a pen
      glissando paints a trail of drops across the lattice (band parity
      alternating), unlike the finger's single dragged drop. The pen finally
      LOOKS different too.

40. **Pen barrel controls are DERIVATIVE-ONLY dials; tilt→CC1 removed
    (user: azimuth-class sensors "generate a lot of noise and are nearly
    impossible to go back to zero").** The math that makes derivative-only
    right: Σdeltas telescopes to (current − value-at-strike), so sensor noise
    never integrates beyond instantaneous jitter and the reference is where
    YOUR hand started, not a compass zero. As implemented:
    * **Azimuth derivative → bend multiplier** (latch-style, ×[0.25, 3],
      quarter-turn ≈ ×2, shortest-angle deltas): twisting the pen dials the
      depth of the in-cell bend live. `hostmpe_pen_glide` gained
      `bend_scale`, which multiplies the EMITTED bend only — note tracking
      and the #39 hysteresis stay on raw geometry, so cells commit where the
      pen physically is (golden: 0.3 st at ×2 emits bend14(0.6); a scaled
      boundary wobble still never retriggers). This also lifts the pen's
      ripple-mode ceiling flagged in #39 (~8% → ~25% at ×3).
    * **Barrel roll (Pencil Pro rollAngle, iOS 17.5+) derivative → the mod**
      (CC 1, latch-style 0..127, quarter-roll ≈ full sweep) — and the vortex
      it stirs sits AT THE PEN, not the canvas center: the shell rides
      CC 21/22 (vortex X/Y in the default map) with the pen position,
      change-only, master channel, both pipes. Non-Pro pencils simply have
      no roll: the dial is inert, nothing else changes.
    * **Tilt (altitude) → CC 1 REMOVED** — a nuisance: altitude drifts with
      natural hand posture, exactly the always-on absolute sensor the
      derivative rule exists to avoid.
    * **Posture gate (same-day correction — "tilt still triggers CC 21/22"):**
      tilting cross-talks into the reported roll and azimuth, so a posture
      change was walking the roll dial and firing the vortex-position CCs.
      Both dials now FREEZE while altitude is moving (|Δalt| > 0.02/event —
      a tilt is posture, not a gesture); azimuth is additionally ignored when
      the pen stands near vertical (altitude > 1.2 rad, where UIKit's azimuth
      estimate swings wildly); per-event deltas are spike-clamped (±0.2 az,
      ±0.3 roll) and the roll dial has a 0.004 rad jitter floor. Latch-style
      + deltas remains the point: a still hand is a still value — the dial
      moves only when the hand does, holds where left, and a regrip never
      jumps it.

41. **Step-21 polish batch (user-directed): one bug, two usability fixes, one
    visual-feedback removal.**
    * **Marble pinch on iOS (the bug — never implemented):** a literal
      two-finger UIPinchGestureRecognizer → `sumi_add_pinch` at the gesture
      centroid; the fold axis IS the finger-to-finger line (point space is
      isotropic, so its angle is the aspect-corrected fold angle directly),
      the squeeze is the delta-driven k (×1.5 per unit scale change, spike
      floor 0.0015). Runs simultaneously with tap/pan/twist; disabled in
      Play mode with the other marble recognizers.
    * **Piano grid: narrow accidentals + white-key tops** — accidentals
      shrink to 0.6 white units (real black-key proportions) and the
      black-row area they do not cover belongs to the NATURAL below, so a
      horizontal glissando passes natural→natural without grazing
      accidentals — "the whole point of the piano layout". Consequences:
      the E–F / B–C gaps and row ends are white-key tops (NO dead zones
      remain on this lattice — supersedes #29's dead-zone rule and the pen's
      piano-gap sustain), and the accidental R_max/knob is proportionally
      smaller (0.6 keys), like a real black key. Goldens updated (gap top =
      the natural; 0.25 units from an accidental center = still the
      accidental; accidental radius < natural radius).
    * **Two-tone lattice:** each cell ring gets a paper-cream halo (3 pt,
      α 0.55) under the dark stroke, so cells stay legible over dense ink;
      ACCIDENTAL rings render last (on top, slightly darker) — black keys
      sitting on the keybed. "Reverse the order of the accidental" realized
      as z-order.
    * **The lift ring is REMOVED** (the §3.4 "drop sets + surfactant ring"
      and #21's home-cell placement are superseded): the user — the clear
      drop at note-off "is really ruining the experience; the joystick's
      disappearance is feedback enough". A lift now simply stops the feed;
      nothing is stamped. Goldens updated (zero rings at any lift velocity,
      zero per echo).

## Step 22 (Android, on the Linux box)

42. **The play surface is a Kotlin `View` hosted in Compose, not a
    `pointerInput` modifier.** Compose's pointer API carries pressure and
    tool type but not `AXIS_TILT` / `AXIS_ORIENTATION`, which the S-Pen
    posture gate and the azimuth tail-stir booster (#40) both need — and
    `onHoverEvent` is where `ACTION_HOVER_MOVE` arrives for the hover ghost.
    So `PlayOverlayView` and `ControlStripView` are Views (the iOS
    `UIView`s' siblings, event for event) mounted with `AndroidView` inside
    the same `Box` as the `SurfaceView`. Compose still owns the chrome and
    the settings dialog. Consequence, deliberate: the overlay keeps the FULL
    canvas bounds and the strip floats over it at the top-left (§8 rev /
    #31), so a touched cell and its loopback drop stay exactly aligned.

43. **S-Pen velocity calibration is normalized-pressure based, not UIKit
    force units.** `MotionEvent.getPressure()` on the Wacom EMR digitizer
    (`sec_e-pen`, ABS_PRESSURE 0..4095) is normalized to 0..1, so the iOS
    formula (`96 + (force − 1)·15.5` in units where 1.0 = an average finger
    touch) has no meaning here. Android maps `t = (p − 0.15)/(0.75 − 0.15)`
    clamped to [0, 1], velocity = 96 + t·31 — the same SHAPE as #38: a
    baseline tap plays at the finger default (96), a hard press is 127, and
    sub-baseline readings clamp UP to 96 (touch-down pressure is sampled
    before contact force builds). **The two constants are the one thing in
    this step that a human hand must confirm** — `adb shell input stylus`
    synthesizes pressure 1.0, which only exercises the top of the curve
    (velocity 127, verified in the byte log). Flagged for the user's device
    pass; they are two named constants in `PlayOverlayView.kt`.

44. **The BLE-MIDI PERIPHERAL is a hand-rolled GATT server, and its packet
    queue must drop from the NEWEST end.** Android's `MidiManager` implements
    only the BLE-MIDI *central* role (that is how the ROLI reaches us), so
    §5.4(c) needs an explicit `BluetoothGattServer` on service
    `03B80E5A…` with the `7772E5DB…` characteristic + CCCD, BLE-MIDI 1.0
    framing (header timestamp-high, then per message a timestamp-low byte),
    one notification in flight per link, the next sent on
    `onNotificationSent`. Two findings, both measured against the Linux
    central (BlueZ 5.83's MIDI GATT profile, which publishes the link as an
    ALSA sequencer port):
    * **A bug this shipped with for one build:** every `send()` built its own
      packet, so the 80-message MCM/RPN0 handshake (dispatched one message at
      a time) queued ~79 single-message packets; the backlog cap then
      `pollFirst()`ed the OLDEST — silently eating the master MCM and the
      first four members (capture: "RPN 0 = 48 on 11/15 members", MCM
      missing). Now messages COALESCE into the tail packet while a
      notification is in flight (an 80-message burst becomes one or two
      packets at MTU 517), the safety cap drops from the NEWEST end so the
      head's handshake and note events always survive, and a link that takes
      nothing clears the queue instead of spinning the backlog into the void.
    * Timestamps inside one packet must not go backwards (BLE-MIDI 1.0), so
      appending to the tail packet is gated on the same timestamp-high byte
      and a non-decreasing timestamp-low.

45. **USB gadget MIDI: the peripheral port is a `TYPE_USB` device with NO
    host-side `UsbDevice`.** When the user flips the system USB mode to MIDI,
    Android publishes the class-compliant gadget as a `MidiDeviceInfo` named
    (localized) "Android USB Peripheral Port"; host-mode MIDI devices carry
    `PROPERTY_USB_DEVICE`, the gadget does not. Either signal identifies it.
    Status for the §5.4 surfacing (active / charge-only / unsupported) comes
    from the sticky `android.hardware.usb.action.USB_STATE` broadcast, whose
    extras are `connected` plus one boolean per ACTIVE gadget function
    (`midi`): mode on with no port after ~3 s means the OEM has no ALSA MIDI
    gadget. Measured on the SM-X906B: the Linux host lists the tablet within
    ~150 ms of the flip (`amidi -l` → `hw:4,0 SAMSUNG_Android MIDI 1`), the
    port opens, and `nativeSinkAppeared` re-sends the handshake — the
    mid-session USB-mode-flip case of #28, on Android.

46. **The Phase-4 host half lives ON the AMidi poller thread** (§5.2 /
    DECISIONS_2 #33), which is now more than a poller: it owns `hostmpe_t`,
    the strip engine, the three per-transport limiters and the byte log, and
    it is the only thread that calls `sumi_push_midi`. The UI thread reaches
    it through a command queue (the iOS serial `midiQueue`'s sibling, #14);
    touch-down, pen-down and the strip-state read are SYNC hops (the voice id
    must answer before the overlay can track the touch — microseconds); its
    1 ms poll cadence became a condvar wait so a posted command wakes it
    immediately. Outbound WIRE writes go the other way: a JNI upcall
    (`NativeBridge.outboundWrite(sink, bytes, len)`) because the endpoints
    are Java objects (`MidiInputPort`, the `MidiReceiver` of the
    `MidiDeviceService`, the GATT server) — the limiters stay native, so the
    policy has one implementation for both shells.

47. **The shell owns a params SNAPSHOT, and a cold start re-sends the
    loopback handshake.** The probe is instance-free precisely so hit-testing
    runs on the UI thread (#2), so the host-owned params fields
    (sim_scale, layout, slide_mode, pinch_variant, bend_mode, ripple_bake,
    press_mode) now live in a mutex-guarded snapshot the UI thread reads and
    the render thread applies — `nativeLayoutProbe` / `nativeLatticeSweep`
    answer from it with no queue round trip. Fallout found on device: with
    Play mode PERSISTED, `nativeSetPlayMode` fires before the surface exists,
    so the MCM/RPN0 push reached a null instance and the normalizer never got
    its mode flip. The render thread now calls `play_instance_ready()` right
    after `sumi_create`, which re-sends the config if Play mode is already
    effective (the transports had theirs; only the loopback was missing).

48. **The §4.6 field dump must run on a FRESH app start.**
    `sumi_debug_run_field_script` queues the canonical seven passes onto
    whatever the field already holds — it does not reset. Dumping after a
    play session compared at max |Δ| 6.0 (aux) against the Metal fixture,
    which looks exactly like an orientation break and is nothing of the kind.
    From a cold start the dump reproduces the step-14 numbers EXACTLY —
    max 1.513672e-02, mean 3.757610e-04, the values recorded in DECISIONS_2
    #30 — so the v0.4 core is unregressed on GLES3 under the documented
    mobile tier (max ≤ 2.5e-2, mean ≤ 1e-3; the strict desktop 1e-2/1e-4
    default still fails by construction on the Adreno's fp16 lerp profile).

49. **Two-byte messages must ship as two bytes.** iOS hands CoreMIDI complete
    messages and lets it frame them; on Android both wire paths
    (`MidiInputPort.send`, the BLE framer) take RAW bytes, and
    `hostmpe_msg_t` always carries three. Channel pressure (0xD0) and program
    change (0xC0) are two-byte messages: a third byte would be parsed by the
    host as running-status data — a phantom pressure/note after every
    pressure message. The wire encoder trims them; the loopback is unaffected
    (`sumi_push_midi` takes the triple).

50. **Evidence tooling for a tablet whose transports land on THIS box.**
    * `tests/midi_capture_alsa.cpp` — subscribes to every matching ALSA
      sequencer port and logs `t_s,port,port_name,status,d1,d2` with one
      CLOCK_MONOTONIC stamp, so arrival-time DIFFERENCES between transports
      are meaningful with no device clock sync (that is how "USB beats BLE"
      is asserted: matched Note Ons, median BLE − USB = **+32.5 ms**). It
      also watches the System Announce port, so a sink appearing MID-capture
      (the USB mode flip) is captured too.
    * `tools/midi_asserts.py` — the emit-order / no-finger-CC74 /
      channel-steal / handshake / sustain-balance asserts over both the
      device byte log and the Linux capture (the iOS step-16 assert script,
      generalized). Note the sustain assert: the strip's ANNOUNCE repeats
      CC 64 = 0 by design, so the invariant is "every ON is followed by an
      OFF and the log ends released", not a 1:1 count.
    * `tools/pen_trace.py` — a LIVE S-Pen legato trace: reconstructs
      sounding pitch (note + bend/171) per stroke and asserts same-channel
      bend→On→Off overlaps, continuity across crossings, monotonicity, and a
      released end. Pen releases are logged as src 4 (`nativePenEnd`) so a
      stroke reads whole.
    * The full headless suites run ON-DEVICE: `tests/hostmpe_tests.cpp` and
      `tests/normalizer_tests.cpp` compile into `libsumi-shell.so` with
      `main` renamed by a `COMPILE_DEFINITIONS` (`main=hostmpe_tests_main`),
      and `nativeRunSelfTests` redirects stdout/stderr into app files.
      Result on the SM-X906B: **1,559 + 14,997 checks pass** — the same
      counts as the desktop ctest run, same code, arm64.

51. **The USB gadget's rawmidi buffers while nothing is subscribed.** The
    first ALSA capture after connecting delivers the whole backlog in one
    burst at subscribe time (458 messages stamped inside one millisecond),
    which reads like a broken clock. Drain once (a short capture to
    /dev/null) before any timing measurement — the evidence runs do.

52. **The live pen trace's continuity assert is SPEED-RELATIVE, and it does
    not apply to PIANO_GRID.** Writing the analyzer produced two corrections
    worth keeping, both found by measuring on device:
    * **A fixed cents threshold measures the pen's speed, not the engine.**
      The seam at a crossing (sounding pitch after the retrigger versus just
      before the crossing's bend) is bounded by how far the tip moved between
      two touch events, because the bend precedes the Note On and the ±0.65 st
      hysteresis holds the crossing until the pen is into the new cell. A
      hand-speed chroma sweep seams at **4.5 cents** (bound 12.3); the same
      swipe scripted at ~40 st/s seams at 51.8 (bound 100). The assert is
      therefore 3× the stroke's own median per-event pitch step (floor
      0.05 st) — an engine discontinuity fails at any speed, a fast synthetic
      sweep does not. Jankó, slow: 19 crossings of a whole tone each, seam
      **17.8 cents** against a 36.9 bound.
    * **On PIANO_GRID continuity is structurally unreachable, and that is the
      design.** The in-cell bend spans ±0.5 st (half a key along the half-key
      diagonal, #29) while a natural→natural crossing is a WHOLE TONE, so
      ~1–1.6 st of the step has to arrive as a jump: measured **1.63 st** on
      a slow horizontal sweep whose note steps were [1, 2] — the white-key run
      D-E-F-G-A-B, which is #41's "a horizontal glissando passes
      natural→natural without grazing accidentals" heard as pitch, and
      PHASE4 §7's quantized piano glissando. `tools/pen_trace.py` takes
      `--layout`: the seam is asserted on chroma/Jankó and reported on piano,
      where the guard is instead that no crossing OVERSHOOTS the note change
      it stands for (all lattices).
    Recorded because the first version of the analyzer hid both facts behind
    one loose 1.35 st threshold — which would have passed a genuine
    discontinuity on the two lattices where continuity is the contract.

53. **The UI thread's synchronous hop onto the MIDI thread is BOUNDED.**
    Touch-down and pen-down need the allocated voice id before the overlay can
    track the touch (#14's iOS `midiQueue.sync`), so they block the UI thread
    on the play queue. Unbounded, that is an ANR waiting for a wedged or
    already-torn-down MIDI thread — and the `engines_ready` guard has a
    check-then-post race by construction. `play_post_sync` now waits 250 ms
    and returns false (the caller proceeds with "no voice", i.e. exactly the
    saturation path), which is four orders of magnitude of headroom over the
    real cost: the queue drains every ~1 ms poll iteration and each body is
    microseconds. The queued lambda carries a shared abandon flag, because
    every call site captures by reference — a timed-out call must not run
    `fn` later against dead stack.

54. **Review batch: an independent read of the Step-22 diff found real
    defects, including two false-green asserts in the evidence tooling.** All
    fixed; recorded because several are the kind that only a second pair of
    eyes finds, and two of them mean earlier green lines were worth less than
    they looked.
    * **`nativeSurfaceDestroyed` could block the UI thread forever** — the
      §5.4 contract inverted. The render loop's outer wait predicate did not
      include `release_requested`, so a destroy arriving while the thread was
      parked WITH NO SURFACE (which is exactly the state after a failed
      `attach_surface`: no ES3 config, `eglCreateWindowSurface` or
      `sumi_create` failure) was never observed: ANR, force-stop. The
      predicate now covers it and a release with nothing attached is
      acknowledged on the spot. This bug predates Step 22 — it was latent in
      the step-14 loop.
    * **Lost wakeup in the render-thread command queue**: `shell::post`
      pushed under `q_mu` and notified `state_cv` while the waiter evaluated
      its predicate under `state_mu`, so a notify landing between
      "predicate false" and `wait()` was lost. The signal is now taken under
      `state_mu`. (The play half already had this right.)
    * **`AMidiDevice` double-free** for any device with more than one output
      port: `AMidiDevice_fromJava` yields ONE reference, and teardown released
      it once per PORT. Exactly one port entry now owns the release. Alongside
      it: **removed devices never left the poller** — no `nativeRemoveMidiDevice`
      existed, so `AMidiOutputPort_receive` kept being called on a dead port
      every millisecond and a replug appended a second set. Kotlin now passes
      `MidiDeviceInfo.getId()` in and calls the removal.
    * **`MidiInputs` never unregistered its `DeviceCallback`.** With
      `launchMode="singleTask"` the process outlives a back-out, so a relaunch
      left two callbacks live, each with its own `openedIds` — the next device
      to appear was opened TWICE and every incoming message was parsed and
      pushed to the loopback twice (doubled notes, doubled occupancy, doubled
      byte log). Also leaked the destroyed Activity. Related, same shape:
      `onDestroy` did its cleanup only `if (isFinishing)`, so a non-finishing
      destroy (locale/fontScale change, "don't keep activities") left a held
      voice sounding on every sink forever; the cleanup is now unconditional
      and only the process-global native shutdown is gated. The thermal
      listener is removed too.
    * **BLE flow control was one global in-flight counter for N links.** A
      central that dropped (or unsubscribed) mid-notification never acked, and
      the counter only reset when the subscribed set emptied — so with two
      centrals, one leaving killed the pipe for the other for the rest of the
      session. The count is now re-clamped on every membership change and a
      250 ms watchdog resumes a pipe whose ack never came. And the safety
      valve no longer discards the never-dropped class: it drops CONTINUOUS
      packets newest-first and touches a packet carrying notes only when
      nothing else is left (#44 fixed the head, this fixes the tail). MTU
      selection now considers only SUBSCRIBED devices, defaulting to 23 for
      one that never negotiated.
    * **Evidence tooling — two false greens.** (1) The sustain assert was a
      no-op: `stuck = (v == 127)` in a loop **assigns** instead of
      accumulating, so it only ever restated `cc64[-1] == 0`; a session that
      held the pedal throughout with one trailing 0 printed `ok`. It now
      tracks the held state and fails on an unanswered ON. (2) `capture` mode
      **passed on an empty file** — every check is conditional on ports
      derived from the rows, so zero rows asserted nothing and printed ALL
      ASSERTS PASS, which matters because #51 has us running a throwaway
      DRAIN capture before every timing run. Row and port presence are now
      asserted, including that `--usb`/`--ble` actually matched. Added while
      there: the §5.3 rate policies are now ASSERTED via `--policy`
      (≤100 Hz per voice-dimension, ~300 msg/s global) rather than printed;
      the `# dropped_loopback_messages` line the shell writes is now read and
      asserted zero; and "every Note On preceded by a bend" gained the assert
      it was named after — a STRIKE must carry a CENTER bend (§3.1/§5.1's
      in-tune attack), only legato retriggers carry a cell offset.
    * Smaller: `sumi_dropped_midi_count` read under the producer mutex; the
      per-sink counters made atomic; `attach_surface` no longer leaves a live
      surface with no current context; `files_dir` assigned inside the init
      guard and the play half's session state reset on re-init (a stale
      `play_effective` swallowed the new session's transport handshake); the
      teardown panic's messages now reach `midi_log.csv`; `nativeRunSelfTests`
      is once-per-process (the suites keep file-static counters, so a second
      run reported doubled counts and could never pass again); the virtual
      device's client count made Compose-observable; the 26,400-call probe
      sweep moved off the draw path.

55. **The USB-MIDI gadget is BIDIRECTIONAL, which gives the channel-steal
    mechanism a scripted external source.** The peripheral port Android
    publishes in MIDI mode has one input and one output port, and the shell
    already opens every device with output ports for ingest — so the Linux box
    can SEND into the tablet over the same cable it receives on, and those
    bytes reach `hostmpe_observe_external` through AMidi exactly as a hardware
    controller's would. Verified on device: an MCM plus a held four-note chord
    on member channels 2–5 from `amidi -p hw:4,0,0 -S ...`, then six touches —
    all six allocated to members 6–11, zero steal violations. This does not
    replace the DONE gate's ROLI-over-BLE run (the wording names the ROLI),
    but the mask, the merge point and the allocator are now proven against
    live external notes rather than only in the headless suite. Practical
    note learned the hard way: toggling the USB gadget function KILLS adb when
    adb rides the same cable, so any USB-mode work runs over `adb tcpip 5555`.

56. **FLAGGED QUESTION (core geometry, frozen — not changed): the piano
    grid's narrow accidentals stop reading as narrow past aspect 1.5873, and
    the Tab S8 Ultra in landscape sits 1% past that line.** Raised by the user
    ("the piano grid doesn't look like the newer version"). Diagnosis, with
    the Android shell cleared: a headless render straight from the core, using
    the shells' own 220×120 probe sweep, is cell-for-cell identical to the
    tablet screenshot (84 cells, same voids, same sizes) — Android draws
    exactly what the probe returns, and the core does carry #41 (hit-testing
    accidentals are 0.6 white-key units; the E–F gap in the accidental row
    probes to the natural below, note 89 = F, so the dead zones really are
    gone).
    * The knob radius is `0.5 · min(key width in aspect-corrected units,
      octave-pair height)` (#29's R_max bullet). The 0.6 narrowing therefore
      only survives while `0.6 · (0.84/7) · aspect < 0.80/7`, i.e. **aspect <
      1.5873**. Measured: iPad Air 11 landscape (1.439) → accidental R 0.0518
      vs natural 0.0571, **91%**, visibly smaller; Tab S8 Ultra landscape
      (1.602) → both 0.0571, **100%**, identical; the same tablet in portrait
      (0.624) → **60%**, dramatic. One build, three looks — which is exactly
      why it reads as "the Android one wasn't updated".
    * The golden pins `accidental radius < natural radius` at **aspect 1.0
      only** (`normalizer_tests.cpp`), so the aspect-dependence is untested.
    * Options for the user, none taken here (rule: core stays frozen, flag
      instead of coding): (a) scale the accidental's R_max by 0.6 of the
      NATURAL's radius rather than re-running the min against the octave-pair
      height — one line, makes the proportion aspect-independent, changes a
      travel bound and the deadband scale on accidentals; (b) decouple the
      DRAWN ring from R_max in both shells and draw accidentals at 0.6 —
      shell-only, honest visually, but then the ring stops meaning "your
      travel bound"; (c) leave it — the lattice is a hint, R_max is a feel
      decision, and the hit-testing (which is what plays) is already #41.
    * Second, related: #41 removed the dead zones, but the lattice still shows
      VOIDS at E–F, B–C and the row ends, because it draws one circle per
      cell and the white-key-top area belongs to the natural centred a row
      below. Those spots play (the natural sounds); they just do not look
      playable. Shell-side drawing question, same three-way choice.

57. **RESOLVES #56 (core, one line): the piano grid's accidental footprint is
    a SIMILAR rectangle, so the 0.6 proportion is aspect-invariant.** Applied
    on the Linux box at the user's direction (the core's frozen rule yields to
    an explicit instruction; #56 had flagged it and stopped). `probe_piano_grid`'s
    hit region was never the problem — it works in white-key units, so the
    natural-to-natural glissando was correct on every screen. The knob was:
    `cell_radius = 0.5 · min(cell width · aspect, cell height)` with the
    accidental's width scaled by 0.6 but its HEIGHT left at the full octave
    pair, so the narrowing survived only while the width was the limiting
    dimension — below aspect (0.80/7)/(0.6·0.84/7) = **1.5873**. Fix:
    `ch_norm = key_w * (1 - 2·PIANO_INSET_Y) / 7` (one line, `layouts.cpp`),
    which makes the footprint similar and the inscribed circle scale with it
    whichever dimension governs. Measured against the rebuilt library, ratio
    now **0.600 at 1.19 / 1.44 / 1.60 / 1.78 / 2.16 / 0.62** — one look
    everywhere, where before it ran 0.63 → 0.91 → 1.00 → 1.00.
    * **The golden now pins the ratio at seven aspects**, four of them past
      the old crossover. It had asserted only `accidental < natural` at
      aspect 1.0 — the one place the defect could not show — which is why a
      shipped build read "not updated" on a 16:10 tablet. Negative control
      run: with the old line restored the extended golden fails at every
      aspect (0.630, 0.756, 0.907, 1.000, and the bare `<` at 16:9 and
      beyond); with the fix, 15,037 checks pass and ctest is 4/4.
    * **The feel change, measured, is 45% — not the 40% the radius suggests.**
      hostmpe's absolute deadband floor (#16, 0.006 canvas-height) takes a
      proportionally bigger bite out of a smaller knob: 10.5% of R on a
      natural, **17.5%** on an accidental, so an accidental's usable travel
      lands at **55.3%** of a natural's (0.02828 vs 0.05114 canvas-height).
      That governs pressure, the 0xA0 swirl half-axis and stylus CC74; bend
      is untouched (identity beyond the circle, #10). Accepted as honest — a
      smaller key has shorter travel — and the floor is `hostmpe`, not the
      core, if black keys ever feel sticky.
    * Verified on device after reinstalling: the lattice reads as a keyboard
      (small accidental rings nested between the naturals), and the pen
      glissando still plays C6→D6→E6→F6→G6→A6→B6 with **zero accidentals
      grazed** — the hit region was untouched, as intended.
    * **The Mac must NOT re-apply this.** The other agent offered the same
      one-line fix; it is done here, so that side should pull rather than
      patch, or the next merge conflicts on `layouts.cpp` and
      `normalizer_tests.cpp`.

58. **The S-Pen's barrel button is a sustain pedal (user request, Android
    first — iOS has nothing equivalent wired).** Numbered 58 to leave 57 for
    the core piano-grid R_max fix being applied on the Mac. Implementation:
    the button drives the SAME `hostmpe_strip_t` sustain engine as the §8
    palette's pad — not a second CC-64 emitter. Consequences, all of them the
    reason to do it this way: the momentary/toggle setting governs the pen
    exactly as it governs the pad, the pad's display mirror follows what the
    pen did (the overlay calls back into the host, which re-syncs it), the
    message is master-channel by construction, it rides the never-dropped
    class, `hostmpe_strip_announce` re-announces it after every MCM re-sync,
    and a panic clears the pen's pedal along with everything else.
    * **Two event paths, one idempotent setter.** `ACTION_BUTTON_PRESS` /
      `ACTION_BUTTON_RELEASE` (with `actionButton` = `BUTTON_STYLUS_PRIMARY`
      or `_SECONDARY`) is the documented path, and it arrives through
      `onTouchEvent` while the tip is down but through `onGenericMotionEvent`
      or `onHoverEvent` while the pen only hovers — so all three call the same
      handler. On top of that, a `buttonState` transition seen on ANY stylus
      event is honoured, because OEM stacks vary in whether they dispatch the
      explicit actions. `setPenButton` acts only on a CHANGE, so the two paths
      can never double-fire one press.
    * **A pen that leaves the digitizer releases a momentary pedal**
      (`ACTION_HOVER_EXIT` with no pen touching): you cannot hold a pedal with
      a pen that is not near the glass, and a stranded CC 64 is the one thing
      §8 names as unacceptable. In TOGGLE mode the engine ignores the release,
      so a deliberate latch survives the pen being put down — which is what
      makes the toggle setting worth having for the pen.
    * Play mode only, like the strip itself: sustain has no meaning without
      notes, and in Marble mode the overlay is gone.
    * Verified on device through the plumbing: a simulated click (the new
      `--es penButton click|down|up` debug intent) emits CC 64 = 127 then 0 on
      the master channel, tagged as strip traffic, and the byte-log asserts
      pass ("strip traffic on the master channel only", "sustain never
      sticks"). The REAL button press could not be scripted — `sendevent` on
      `/dev/input/event8` is denied to the shell user — so the OEM delivery
      path is the user's one-click check; the pen device does report
      `BTN_STYLUS`, which is what maps to `BUTTON_STYLUS_PRIMARY`.
    * Flagged for the Mac: iOS's sibling is the Pencil Pro squeeze
      (`UIPencilInteraction`, `.squeeze` phase on M2+ iPads with a Pencil
      Pro), which would give the same pedal to the iOS shell. Not implemented
      here; recorded so the parity gap is visible rather than discovered.

59. **ROLLED BACK, IN FULL — the black-key glissando experiment. Recorded so
    the next person does not repeat it.** The user asked for a glissando
    between two accidentals; two attempts were made and BOTH were withdrawn
    by the user, the second with "the touching surface needs to be the same as
    the cells — you break the entire purpose of the app with these random size
    modifications." The tree is back to #41 + #57 and nothing of this entry
    ships.
    * What was measured, and stands as fact: a stylus sweep along the
      accidental row plays the FULL CHROMATIC SCALE — C C# D D# E F F# G G# A
      A# B — because every 0.4-unit gap between two 0.6-wide accidentals is
      the natural's top (#41). A pianist expects the black-key run.
    * Attempt 1 (core): widen the accidental's HIT region so adjacent
      accidentals meet, leaving the drawn knob at 0.6. It worked — the sweep
      played C C# D# E F F# G# A# B — but it decoupled the touch region from
      the drawn cell, which is the thing the user identified as breaking the
      instrument: **the circle you see IS the cell you touch and the joystick
      it generates.** An intermediate 0.1-unit lane was tried first and
      rejected on measurement: a moving pen crosses 35 px in a couple of
      samples, so whether the passing natural sounded depended on hand speed.
    * Attempt 2 (shell): draw the piano grid as keys tiling their row, so the
      lattice would show the contiguity. Withdrawn as a deviation from PHASE4
      §6, which the working rules say wins: a cell is drawn ROUND, at the
      radius of the joystick it generates.
    * **The trilemma, stated for whoever picks this up.** With round cells you
      may have any two of: (a) the touch region equals the drawn circle,
      (b) accidentals visibly smaller than naturals, (c) adjacent accidentals
      contiguous so a black-key glissando works. #41 + #57 chooses (a) + (b).
      The untried third option is (a) + (b) + dead space: let the black row
      refuse OUTSIDE an accidental — touch still equals the drawn circle, and
      because a dead zone SUSTAINS (a pen only retriggers on a probe hit,
      #39) a slide would give a clean pentatonic C# D# F# G# A#. The cost is
      that tapping between two black keys does nothing, and #29's dead zones
      return where #41 removed them. It needs the user's decision, not an
      agent's — which is where this stopped.

60. **PRE-EXISTING BUG, found by the user and fixed: on the piano grid alone,
    a natural's drawn cell hung HALF A ROW below its own touch region.**
    Reported as "the touch squares are not aligned with the cells, they are a
    little bit upper, so the bottom of a cell is dead or is occupied by its
    down neighbour" — an accurate description of the geometry, and the reason
    this layout kept feeling wrong.
    * **Measured before the fix** (aspect 1.60, note 60): the cell is DRAWN at
      y 0.4714..0.5857 (centre 0.5286, r 0.0571) while the probe gives that
      note y 0.4430..0.5570 (centre 0.5000). Half a row (0.0286) of offset.
      The bottom quarter of every natural's circle played the octave BELOW on
      screen, and the playable strip above it was not drawn at all. The chroma
      grid measured 0.0000 offset, which is why this was piano-only.
    * **Cause:** `layout_piano_grid` centred a natural on its own drawn ROW,
      but its playable region is the octave PAIR — its row plus the white-key
      tops above it (#41) — and `cell_radius` has been the PAIR's half-height
      since #29 ("a key's playable footprint is one key wide by one octave
      tall"). Centre and radius were describing two different rectangles.
      Accidentals were always right: their region IS one row, and they are
      centred on it (measured offset 0.0001).
    * **Fix, one line:** a natural's y is the pair's centre (`row`, not
      `row + 0.5`, in 14ths). After: drawn 0.4429..0.5571 against a touch
      region of 0.4430..0.5570. On device, tapping the top edge, the centre
      and the bottom edge of C4's circle now all play C4 — the bottom edge
      used to be the octave below.
    * **Consequence, stated plainly:** the semitone axis follows the geometry
      (generic #7 shortest-neighbour rule), so C→C# is now half a key over and
      HALF a row up rather than a whole row. The piano grid's `semitone_step`
      drops (0.0829 → 0.0665 at aspect 1.0), which means a given drag bends
      ~20% further on THIS layout. That is the corrected geometry rather than
      a tuning choice, and both goldens were updated to it. Naturals' drops
      also land half a row higher — the lattice moves with them, which is the
      alignment being fixed.
    * **The invariant is now pinned, not just the numbers:** the golden scans
      the vertical line through C4's centre, and asserts that the region which
      probes to note 60 has the same centre as the drawn cell and the same
      height as its diameter. A future refactor cannot drift them apart again.
    * **Left alone deliberately:** an accidental's circle is ~10% taller than
      its one-row region (0.0686 vs 0.0567 at this aspect) because its
      radius comes from 0.6 of the pair height (#57). It is centred correctly,
      it reads as a black key overlapping the whites, and shrinking it to the
      row would change #57's clean 0.6-at-every-aspect ratio to 0.5. Not the
      reported bug; flagged rather than touched.

61. **The glissando corridor: a natural shrinks to the accidental's size and
    sits flush with its old BOTTOM, freeing the strip above it for the black
    keys (the user's design, given as an instruction).** This is what #59 was
    reaching for and got wrong twice — the difference is that the cell and the
    region that plays it stay the same thing throughout, which was the
    condition all along.
    * **Geometry.** A natural now owns the octave pair's BOTTOM
      `PIANO_NATURAL_H` = 0.6 rather than all of it; its cell centre is the
      middle of that band, so its bottom edge is exactly where it was and its
      cell still matches its touch region (#60's invariant, re-verified:
      drawn 0.4886..0.5571 against a touch region of 0.4888..0.5570). Both
      cells are now 0.6 of a pair TALL, so where height governs — every
      landscape aspect — the natural and the accidental are the SAME SIZE,
      which is what the user asked for. WIDTH still separates them (0.6 of a
      key against a full one), so on tall screens the accidental is the
      smaller of the two.
    * **The corridor.** The strip above the natural band, off any accidental,
      is deliberately EMPTY. A pen sliding through it sustains rather than
      sounding a white key (a dead zone makes no call, #39), so the black keys
      play as the run a pianist expects. Measured on device: a corridor sweep
      gives **C#4 D#4 F#4 G#4 A#4** — five accidentals, ZERO naturals — where
      the same gesture used to give the full chromatic scale. The natural band
      below is untouched: **C4 D4 E4 F4 G4 A4 B4**, zero accidentals.
    * **This supersedes #41's white-key tops.** The uncovered part of the
      accidental row was the natural's, which is what made that slide
      chromatic. The naturals lose nothing: their own band still tiles
      completely (asserted across 70 sample points), and the E-F / B-C gaps
      and row ends are now part of the corridor.
    * **Known and accepted:** tapping in the corridor off a black key does
      nothing, and a glissando must START on a key — a stroke that begins in
      dead space never allocates a voice, so nothing sounds for its whole
      length. Both follow from "the cell is the touch region"; neither is a
      bug to chase.
    * Goldens moved with the geometry: C4's landmark position, the C→C# axis
      (now 0.9 of a row apart), the natural's R_max (a full key wide by 0.6 of
      a pair), the corridor's emptiness at three x positions, the natural
      band's complete tiling, and the cell-size formula for both kinds at
      seven aspects. 15,117 checks, ctest 4/4.

## Back on iOS (after the Android port)

62. **The Pencil Pro's SQUEEZE is the S-Pen barrel button's twin (user
    request, translating Android #58).** `UIPencilInteraction` on the play
    overlay; `didReceiveSqueeze` (iOS 17.5+) drives the SAME strip sustain
    engine as the panel's pad: `.began` -> `hostmpe_strip_sustain_press`,
    `.ended`/`.cancelled` -> release, then `syncStripMirrors()` so pen and
    palette never disagree. Transition-only (a squeeze's `.changed` stream
    cannot double-fire), Play-mode only (`isHidden` guard), and the sustain
    MODE setting still decides momentary vs latch. Pencil 2 fallback: the
    double-tap (`pencilInteractionDidTap`) LATCHES the pedal, since a tap has
    no hold. One pedal, two platforms, one engine.

63. **The iOS byte log adopts Android's src taxonomy** (0 external, 1 finger,
    2 session config, 3 strip, **4 stylus**): pen bytes were tagged as finger,
    which made `tools/pen_trace.py` blind on iPad logs and `midi_asserts.py`
    report the pen's legitimate CC74 as "finger CC74" (the §3.3 stylus-only
    rule reads as violated). `playTouchEnd` gained `isPen` so a pen's release
    is tagged too. Both platforms' logs now feed the same analysers.

64. **Evidence tooling on iOS: in-app capture + log flush** (settings ->
    Evidence). `startCaptureBurst` writes N full-screen PNGs to Documents on
    a delay (so the sheet can be dismissed and the instrument played) via
    `drawHierarchy(afterScreenUpdates: true)` — the render-server path, the
    only snapshot that can carry CAMetalLayer content; `flushLogsNow` writes
    the byte/latency/session logs mid-session. Everything is pulled with
    `xcrun devicectl device copy from --domain-type appDataContainer
    --domain-identifier com.vibetuned.midi-sink --source Documents`, which is
    how the step-21 device evidence was gathered.

65. **FLAGGED QUESTION (core, unchanged — user's call): a sustain press WIPES
    THE CANVAS in every mode.** `sumi_voice_mapper_normalize` maps any CC 64
    rising edge, on any channel, to `SUMI_VEV_PAPER_DIP` -> a RESET pass.
    That is §2.4's CLASSIC-keyboard mapping ("CC 64 -> paper dip"), but it is
    not gated by mode, so the §8 strip's Sustain pad — and now the Pencil
    squeeze and Android's S-Pen button — dip the paper mid-performance.
    Proven headlessly: MCM (MPE mode) + CC64 = 127 -> PAPER_DIP -> RESET
    queued. The user's own iPad log carries two such presses. Proposed
    one-line gate: honour the dip only when the normalizer is NOT in MPE
    mode (or only outside Play mode), leaving §2.4's keyboard behaviour
    intact. NOT changed here: it is a spec'd mapping and a musical decision,
    and it affects both platforms identically.

66. **Echo suppression: the shell must not consume its own output (user-
    requested after the finding).** Measured on a real iPad session:
    **99.5% of "external" MIDI (14,549 of 14,629 messages) was our own
    output mirrored back**, median round trip 0.3 ms. Consequences, both
    real: `hostmpe_observe_external` marked OUR OWN channels externally
    held — the §5.1 mask can then starve the allocator into silent drops —
    and the loopback painted every note twice (two drops per strike, the
    counter advancing twice). The shared guard lives in hostmpe so both
    shells inherit it: `hostmpe_echo_record` on every byte that actually
    LEAVES the device (iOS hooks `MidiOutputs.emit`, the single delivery
    point — it covers the limiter's drained batches too) and
    `hostmpe_echo_is_ours` at the device input, before the mask, the byte
    log and the loopback. A match inside a 300 ms window is CONSUMED, so a
    device legitimately repeating the same bytes is never swallowed and at
    most one echo is dropped per emission; `hostmpe_echo_dropped` surfaces
    the count in the session status line. Why Android never saw it: its
    sinks are a USB gadget, a MidiDeviceService virtual device and a BLE
    peripheral — none of which iOS's virtual-source/IDAM/network topology
    mirrors back into the app's own input scan (#25/#27 are the same family
    of bridging surprises).

67. **The sustain pedal no longer wipes the canvas (user decision, resolves
    the #65 flagged question).** `CC 64 -> paper dip` is §2.4's CLASSIC-
    keyboard mapping — a plain keyboard has no held-note semantics here, so
    its otherwise-unused pedal dips the paper. In MPE that pedal is a REAL
    musical control (the §8 strip's pad, the Pencil squeeze, the S-Pen
    button), and dipping mid-performance wiped the marbling under the
    player's hands. The mapper now honours the dip only when the input mode
    is not MPE; the classic path is untouched (unit-tested both ways).
    Because the dip is a feature, not just a side effect, iOS gains a
    deliberate **"Paper dip (fresh sheet)"** control in settings — Android
    already has `nativeTriggerDip` and should surface the same button.

68. **FLAGGED (behaviour, unchanged — user's call): with the #40 bend booster
    engaged, a cell CROSSING is no longer pitch-continuous.** Found by running
    `tools/pen_trace.py` over a free-play iPad session: 13 of 115 strokes trip
    the tracer's overshoot assert, and the cause is measurable — **69% of the
    401 crossings carried a bend beyond a half-cell (up to 2.88 st)**, which
    only the multiplier can produce (raw geometry is bounded by the ±0.65 st
    hysteresis). Because the in-cell offset FLIPS SIGN as the reference cell
    changes, a boosted crossing moves the sounding pitch by up to the note
    step plus twice the boosted half-cell — the tracer measured a crossing
    whose note went +2 while the pitch went −2.4. #39's "the retrigger lands
    where the pen already is" therefore holds exactly at scale ×1 and
    degrades as the boost rises. Options if it bothers the player: (a) leave
    it — deep vibrato plus a simultaneous slide is an extreme gesture and the
    jump is arguably the sound of it; (b) suppress crossings while the boost
    is engaged (deep vibrato means holding a note, not gliding) by scaling
    the hysteresis with the boost; (c) emit crossings at scale ×1 and let the
    boost resume after. NOT changed here: the user is playing with the
    booster daily and has not reported a seam. The tracer's other 12 failures
    are its scripted-sweep monotonicity assumption meeting free playing, not
    defects.
