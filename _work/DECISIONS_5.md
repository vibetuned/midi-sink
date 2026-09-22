# DECISIONS_5 — Phase 6: Medium (the Anod operators, the ABI event, palettes and presets)

Ambiguities resolved during Phase 6 (steps 35–46 of `_work/ROADMAP_5.md`).
Prior history: `docs/DECISIONS.md` (Parts I–IV; references written as
`DECISIONS_4 #n` mean Part IV). This file merges into that document as Part V
when the phase ships. The specs are the `specs/` set (`MEDIUM §n`,
`INSTRUMENT §n`, `SOUND §n`, `QOL §n`); where an entry here and a spec
conflict, the entry is the record of what shipped — and the conflict is
flagged to the author, who owns the specs.

## Step 35 — Phase opening & the conservation gate (macOS)

1. **The medium is named Anod.** The author's decision of 2026-09-21, closing
   the `MEDIUM §1` iterate point (candidates were Anod, Biri, Ichifu).
   Ichisuke Fujioka is honoured in the documentation's acknowledgments
   regardless of the name — an homage sits better there than carried by a
   product name (the spec's own suggestion). The ABI value is
   `SUMI_MEDIUM_ANOD = 1`, landing in step 41.

2. **Four phases, one public release, version 2.0.0.** The `specs/` set is
   four programs (medium, sound, instruments, publish) and the author split
   them into Phases 6–9 in that order; only Phase 9 releases to the public
   channels. Each earlier phase ends with a pre-release tag
   (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a
   pre-release and keeps the lanes proven) that the author installs on every
   device; Phase 9 runs `v2.0.0-rc.N`. Nothing reaches a stable channel
   before step 66.

3. **The ABI break is one step (41) and carries the probe's state argument
   two phases early.** `sumi_layout_probe` gains INSTRUMENT §1's
   `const sumi_layout_state_t*` and `sumi_cell_info_t` gains `flags` in the
   same bump that adds `medium` and `sumi_set_palette`, although no stateful
   layout uses them until Phase 8 (shells pass zeros). One break, then
   additive growth only — `libsumi` is **1.0.0** from that step, the settled
   ABI the deferred SDK needs.

4. **Operator families for the Anod set.** Wave torsion is a third vortex
   PROFILE in the core (the vortex passes already rotate by a profile of r)
   with its own page in the operator book — the `MEDIUM §2.1` iterate point
   closed as "profile in the core, standalone in the book". Chladni and the
   spark shear are passes of the ripple's shear family at both insertion
   points (live and bake). The burst lives in the wake's sub-stepped family
   (`core/src/displacement.cpp`).

5. **Fingering CCs (Phase 8, decided now so the chart and the specs can
   settle).** Valves CC 110 / 111 / 112 (≥ 64 = pressed) on the MASTER
   channel — global state, recorded where a DAW records the mod wheel. The
   slide is 7-bit **CC 113 with normalizer smoothing**, not a 14-bit pair:
   the pair's MSB would land in CC 0–31, the Airwave's block (DECISIONS_4
   #50), and 128 steps over six semitones is ≈ 4.7 cents per step, under the
   ~5-cent just-noticeable difference — the arithmetic is the justification.
   CC 102–119 are undefined in the MIDI specification and already carry
   midi-sink's own controls (102/103 ripple).

6. **Presets share one host-side serializer** (pure C, beside `hostmpe`, step
   43), so the four shells cannot drift; the core stays stateless about
   files.

7. **Per-device default presets are OFFERED, never auto-applied** (`QOL §6`
   iterate point).

8. **Replay files record gestures too** (`QOL §5` iterate point: yes), so pen
   performances replay complete, and they carry FRAME BOUNDARIES: playback
   drives the scripted clock through them (step 61). Re-bucketing recorded
   bytes by wall time is the documented anti-pattern and the negative test.

9. **New-feature documentation lands in step 63 and merges with the release
   tag.** The live `/marble/` is the newest stable tag (DECISIONS_4 #82), so
   an operator page merged early would embed a scene the released wasm does
   not know. Guide fixes ship from `main` at any time; pages for new
   operators, layouts and Voxo are drafted in the step's evidence folder.

10. **The palette curve is fixed per medium in 2.0** (`QOL §1`: the
    "advanced fold" is deferred); **the theremin is a `flags` bit** on the
    cell info, not a radius sentinel (`INSTRUMENT §5`).

11. **The fretboard generalises to `SUMI_LAYOUT_STRINGS`** with three FIXED
    tuning presets — standard guitar, a whole-tone tap grid, all-fourths — as
    a params enum over fixed arrays; user-editable tunings stay deferred with
    microtonal. Wicki–Hayden stays its own layout (not a string layout).
    **"Harpejji" is Marcodi's trademark:** the docs may say "inspired by
    tapping instruments such as the Harpejji"; the word never enters an enum,
    a setting label or a product name.

12. **The phase invariant.** `tests/fixtures/field_512_metal.bin` stays
    bitwise on Metal from step 35 to step 66: new operators add passes, media
    change the composite, layouts change the probe — none touches an existing
    pass. A step that believes it must change the fixture stops and records
    the decision first.

13. **The four-part gate is one `--dev` command per operator, and its (b)
    is measured per class.** `midi-sink --dev --soak <operator|all>` (in
    `desktop/src/dev_tools.cpp`, beside the step-19 pinch soak it generalises)
    drives the nine v1.0 passes through their REAL routes on the fixed 512²
    scripted clock — the glide tine by note bend, the two pinches by CC 74
    under `slide_mode = 1`, the wakes as stylus segments of ≤ a/4 per frame,
    the ripple bake by its amplitude CC, the swirl by channel pressure under
    `press_mode = 1`, the two vortex profiles by the mod wheel — with one
    stream shape for all (a 0.5 Hz wobble at 120 Hz, at most one pass per
    frame) and prints four verdicts plus a machine-readable `SUMMARY` line
    that `tools/soak_report.py` tabulates. **(a)** is the class declaration
    (`MEDIUM §2`) with its one-line det J = 1 argument. **(b) for EXACT
    operators:** 500 strong (+k, −k) gesture pairs at one centre hold ink
    mass (Σ phase) within **±5%** AND return the pre-image to within **4
    texels** mean displacement — mass is the coarse guard (a broken pair
    loses everything: the negative control reads −99.7%) and the pre-image
    return is the sharp one, the observable that tells a non-inverting pair
    from two different area-preserving passes (the negative control leaves it
    204 texels away; the v1 references sit at 1.5–2.7). **(b) for SUB-STEPPED fields:** the class
    promises first-order area preservation per step, so that is measured —
    one a/4 sub-step on an identity field, pre-image Jacobian det > 0.5
    everywhere outside the swept capsule and mean within 2·10⁻³ of 1 (the
    Stokeslet test's own bar, DECISIONS_4 #53; the capsule is excluded as in
    the flick test because the doublet's slip surface is a genuine tangential
    discontinuity, not a fold — measured across it the doublet reads −1.44,
    outside it 0.73). The ±D pair drift is printed beside it, not gated:
    one-sub-step pairs already lose 4.7% over 500 pairs because every step
    resamples the slip surface, and a long stroke per pair would be forty
    passes against the exact operators' two. **(c)** the stream never grows
    mass by more than 0.5%. **(d)** per-pass mass loss ≤ 2× the glide-tine
    control under the identical stream shape, from a FRESH copy of the same
    scene (erosion is front-loaded — sharp structure fades fastest — so a
    control run on the field the operator already worked would inflate the
    baseline), gated only at ≥ 3000 passes (default 6000, the #33 window)
    because of #15 below; a shorter run prints (d) as information.

14. **The (c) negative control uses the v1 tine's clamp legacy, not a core
    switch.** The roadmap asked for "the pinch soak with the ingress mask
    disabled"; the mask lives in the core and the core is frozen, and a debug
    switch would be a core change for a test. The pinch's pre-ingress
    fabrication (+9.5% over 12 000 passes) stays history's red run (#33); the
    gate's own red uses the mechanism #33 kept fixture-pinned in the v1
    operators: ink laid on the left edge and tines dragging it inward
    duplicate the clamped edge column every pass — **+14.0% in 300 passes**.
    The other two reds: a (+k, −k) pinch pair whose −k sits at an offset
    centre (204 texels, −99.7%); an over-stepped wake stream — a stroke of
    fifteen tip radii every frame, fifteen internal sub-steps, over the full
    6000 window — for (d), which is the class rule (≤ a/4 PER FRAME at
    gesture rate) stated as a failure. A chaotic pinch schedule was tried
    first and dropped: its erosion is front-loaded and dilutes below 2× over
    any window long enough for the control to be meaningful (1.4× over 3000
    passes at k = 0.3). `--soak-negative` asserts that each part fails.
    Flagged against the roadmap's literal wording.

15. **The resampled medium is not mass-neutral, in both directions, and the
    (b) mass bar is ±5%, not #33's ±0.5%.** Measured while calibrating, on
    Metal: the incumbent tine's strong ±pairs (z = 0.05, ~25 texels at the
    line) GAIN mass steadily after the first ~200 pairs, +0.76% at 500 — all
    of it interior (the 16-texel edge band contributes exactly zero, so this
    is not the clamp legacy): 6 900 units leave the initially inked texels
    and 7 550 appear in the water around them, a net gain of 650 at the
    ink/water boundaries under bilinear gather. The gain scales with the
    boundary length a pass moves: the ripple bake, an exact whole-canvas
    shear of only 12.8 texels at full amplitude (it carries the ingress rule,
    so this is not clamp either), gains +2.69% over the same 500 pairs while
    its pre-image returns to 2.6 texels — every ring edge on the canvas moves
    on every pass. The pinch pairs, exact by construction (#33), drift
    −0.43%/+0.15% with a 2.3-texel pre-image wander; the swirl and both
    vortex profiles stay within ±0.5% (1.5–2.7 texels). A FRESHLY laid drop gains mass over its
    first few hundred glide passes (−1.2·10⁻⁵/pass "erosion" at 300 passes,
    i.e. growth) before the steady fade of ~1.2·10⁻⁵/pass sets in; #33's
    control never saw this because it ran on a field aged by 6000 pinch
    passes. So: the exact-class mass window is ±5% — it has to clear a
    whole-canvas exact shear, and Chladni, the spark and Chirikov are all of
    that family (the references: tine +0.76%, ripple +2.69%, a broken pair
    −99.7%); the pre-image bar is 4 texels (references at 1.5–2.7) and is
    the criterion that actually decides; (d) is gated only in the steady
    window. The
    mechanism behind the gain is not established here — the gather of a
    det = 1 map conserves Σ in exact arithmetic, so it is in the filtering
    precision or the half-float representation of the phase — and it is
    recorded as a Phase-6 question, not chased in this step: the gate needs
    the medium's floor, not its explanation. Flagged against the roadmap's
    ±0.5%.

16. **The ripple soaks through its CC route.** The bend-driven bake drifts
    the ripple phase a little per pass ON PURPOSE (#36 permanence: an
    excursion never retraces exactly, so vibrato leaves a mark), which makes
    bend-driven ±pairs non-inverting by design. The amplitude CC (102,
    `bend_mode = 0`, `ripple_bake = 1`) keeps φ fixed — the composing-back
    group the operator's exactness claim is about — so both the pairs and the
    stream run through it. A future operator whose route drifts a parameter
    by design has the same choice to make, and this entry is the precedent.

17. **The crossed-tine pinch is exact per pass but NOT sign-reversible
    through its ABI, and it fabricates under torture — recorded red in the
    baseline table, flagged as a Phase-6 fix candidate.** The gate's first
    full run: `pinch-cross` (b) leaves the pre-image 92 texels adrift and
    GAINS 25% of its mass over 500 ±k pairs, while the saddle passes. The
    reason is in `sumi_deform_crossed_pinch` (`core/src/displacement.cpp`):
    the variant is a composition of two perpendicular infinite-line tines
    T₁∘T₀, and a negative k reverses BOTH drags but keeps the ORDER — the
    inverse of T₁∘T₀ is T₀⁻¹∘T₁⁻¹, and crossed shears do not commute, so the
    sign flip leaves a second-order kick-drift residual every pair (the same
    order-matters fact `MEDIUM §2.2` states for Chladni). That residual
    walks ink to the canvas edges, and the tines carry the v1 edge-clamp
    legacy (#33 kept it fixture-pinned) — hence the fabrication. Two
    consequences: DECISIONS_4 #69's "a squeeze-and-release nets out in exact
    math" holds for the saddle only; and the fix — emit the tines in reversed
    order for k < 0 — is a one-line core change that would make release
    retrace exactly, which the author must weigh against the look (#69 also
    calls what the release does not retrace "marbling"). Not touched in this
    step (core frozen; the gate needs the baseline, not the fix); the (c)
    and (d) columns are green for the variant. The gate now also asserts that
    the stream MOVED the field (> 100 texels changed) inside (c), the
    step-19 soak's own sanity check, so a mis-wired route can never pass
    trivially.

## Step 36 — Wave torsion, the proof brick (macOS)

18. **Torsion is the third vortex profile, and the pass grew without moving
    the fixture.** `SUMI_VORTEX_TORSION = 3` — value 2 stays the gesture-only
    Lamb–Oseen, which `sumi_add_vortex` reroutes to the swirl pass, so the
    CC-routed profiles are 0, 1 and 3 and every picker skips 2. The vortex
    shader's uniform block gained `k` and `phase` and its branch tests
    torsion FIRST (`profile > 2.5`), then Rankine, then the exponential
    default, so the exponential path's expression is byte-identical to
    v0.9's; `tests/fixtures/field_512_metal.bin` stays bitwise on Metal
    (max|d| 0) and the web tier reads max 9.8·10⁻⁴ / mean 7.9·10⁻⁹, inside
    its documented tolerance. The queue payload `sumi_deform_vortex_t` gained
    `k, phase` (zero for the other profiles; the §4.6 field script sets them
    explicitly so the uniforms stay bit-identical). `sumi_version` → 0.10.0:
    additive — the enum value, two ctl dims, one params field.

19. **k and φ are flavour controls, the ripple's precedent.**
    `SUMI_CTL_TORSION_K = 14` maps 0..1 onto 2π·4 … 2π·40 radians per canvas
    height (4 to 40 rings across the sheet; rests at 0.5) and
    `SUMI_CTL_TORSION_PHASE = 15` onto 0 … 2π (rests at 0); `SUMI_CTL_COUNT`
    is 16. Unmapped in the core like the ripple's dims; the desktop's stock
    map adds CC 104/105 as its handles (map version 4 — an INI still carrying
    the version-3 stock map upgrades, #71's mechanism) and the CC-map editor
    lists the two names. Every vortex route reads the mapper's SMOOTHED
    values at emit time — the gesture route (`sumi_add_vortex`) through
    `sumi_voice_mapper_torsion_kphi`, the CC-routed vortex and the sweep in
    the mapper itself — so a slider move never jumps the pattern.

20. **The engine's first episode: the note-on torsion sweep, opt-in.**
    `params.torsion_sweep` (default 0) arms, on every VoiceBegin, a per-voice
    time-driven emitter: φ = φ_ctl + ω·t with ω = 2π·1.5 rad/s, amplitude
    RATE·e^(−t/τ) with RATE = 1.2 rad/s and τ = 0.6 s (the integral, 0.72 rad
    at the crests), reach 3× the strike radius floored at 0.05, over after 4τ.
    Every frame emits that frame's rotation INCREMENT (rate · envelope · dt)
    as a torsion pass at the voice's current centre(s) — the delta rule, so
    two strikes add and a frame the budget refuses merges its increment into
    the next. The episode runs whether or not the note is still held (a
    discharge dies on its own clock; the loop handles it before the
    `active` check) and a new note in the slot re-arms it. Measured
    (`--torsion-test`): a marker 0.12 from the strike swings 0.087 rad, the
    largest 10-frame step is 0.038 rad against a 0.15 bound (a whole pattern
    at once would read ~0.7), the angle at 3 s equals the angle at 4 s to
    10⁻⁴ although the note was released at 2 s, and a second strike re-arms
    with the same bounded steps. Test-design note recorded: a second strike's
    own drop pushes an outside marker RADIALLY (√(r² + R²)), so its net
    rotation is not comparable to the first's — the re-arm is checked by
    swing and step, not by accumulation. The strike still lays its drop; the
    sweep rides on top until the Anod binding table (step 42) makes it the
    strike.

21. **The gate learned two things from its first new operator.** (i) Its
    (c) criterion — "mass never grows past 0.5% over 6000 passes" — was met by
    every v1 operator only because their erosion outran the medium's
    boundary gain (#15). The torsion at its default wavelength gains
    3.8·10⁻⁶ per pass (+2.26% over the window), ALL of it interior (edge
    band exactly 0), an order below edge-clamp duplication (4.7·10⁻⁴/pass in
    the negative control): the medium at high spatial frequency, not
    fabrication. (c) is now "growth ≤ 0.5% over the window, OR a growth rate
    ≤ 5·10⁻⁵ per pass", and the line prints the edge/interior split so the
    reader sees where the mass appeared; the negative control trips both
    forms. (ii) Pair magnitudes now follow one convention — about 25 texels
    of displacement at the ink (tine z = 0.05, pinch k = 0.3, one a/4 wake
    step, rotations ~1 rad at R = 0.25, torsion 0.5 rad on a 39-texel
    wavelength at R = 0.5, its k set through CC 104 before the scene) — a
    pair at a pathological scale (a radian across a 23-texel wavelength,
    7.8 texels of drift) measures the resampler, not the operator. Even so
    an oscillatory exact field wanders more than a smooth one (5.45 texels
    against 1.5–2.7 while its markers return to 3·10⁻⁴ rad), so the exact-
    class pre-image bar moves from 4 to **8 texels**, 25× under the 204 of a
    non-inverting pair. Chladni, the spark shear and Chirikov are all
    oscillatory whole-canvas shears: expect the same numbers, and treat a
    smooth operator that reads above 3 as a question.

22. **The `torsion` scene ships in the marble app and the web gate's sweep,
    not yet in the docs' check.** `web/site/scenes.js` gained the scene (A,
    k, φ, R, the sweep flag and the pace), the web host a `torsion_sweep`
    parameter id, `tools/web_gate.mjs` the name (12/12 scenes run clean on
    the new wasm). `site/scripts/check.mjs` requires every scene it lists to
    be embedded by a page, and the page is drafted in this step's evidence
    (`torsion.mdx`) to land at step 63 (#9) — listing the scene now would
    break the docs build. The web host's own settings panel (the profile
    picker) is step 46's; the scene calls the profile by value.

## Step 37 — Chladni lattice (macOS)

23. **The Chladni operator is the Taylor–Green cellular flow on the layout's
    cell lattice, split into two exact diagonal shears.** The step arrived
    here through four designs in review, the first of them committed: (1) a
    separable kick-drift x₁ = x + a·cos(k_y·y), y₁ = y + b·cos(k_x·x₁) with
    its ratio from the interval between the two lowest voices — the roadmap's
    text; (2) the same with every cell centre a node; (3) a channel profile
    with every cell a still island; (4) this. The author's objection to (1)
    and (2) was legibility ("the layout will be hard to see with only the
    waves"), to (3) that the breathing "looks kind of bad" — a quadrature of
    shears wobbles, it does not form a figure — and, decisively, the physics:
    with det J = 1 Liouville forbids any change of density, so no operator of
    this engine can GATHER ink the way sand gathers (the author's derivation
    of the ponderomotive potential V = ¼mω²W² with the nodal lines as its
    minima is in `chladni.md`; settling there is dissipative — grains
    oscillate across the trough forever without friction — and a sheet with
    positions only cannot carry the momentum the conservative version needs).
    What the engine CAN do is stretch: iterate an area-preserving flow whose
    separatrices are the lines wanted, and the ink is drawn out along them
    (Aref & Ottino's chaotic advection). For a plate mode W = cos(k_x x)·
    cos(k_y y) the nodal lines W = 0 are the cell boundaries, and the
    Taylor–Green stream function ψ ∝ W — an exact Navier–Stokes solution,
    the founding rule's welcome guest — has an eddy in every cell (neighbours
    counter-rotating) and its separatrices exactly on W = 0: the fluid
    streams the ink along the lines where the sand would settle. ψ = cos u·
    cos v is not separable, but ½[cos(u−v) + cos(u+v)] is a sum of two waves
    each depending on one DIAGONAL coordinate, and the flow of such a term is
    a pure shear along the direction where that coordinate is constant —
    (k_y, k_x) and (−k_y, k_x) — with magnitude ½Ψ·sin(·): each is exact, so
    one step of the flow is two exact passes (`SUMI_DEFORM_CHLADNI` with a
    `stage`), det J = 1 at any amplitude, the kick-drift splitting of a
    symplectic integrator. CLASS EXACT. The exact inverse of a step is both
    shears negated in REVERSED order (#17); `sumi_add_chladni(psi, balance,
    s_x, x_0, s_y, y_0)` takes a negative psi as that inverse, and the mapper
    and the gesture share `sumi_chladni_emit_step`. The "simultaneous" form
    stays the headless NEGATIVE (`test_chladni_kick_drift_order`). On the
    GPU one step and its inverse leave the interior pre-image within a
    fraction of a texel of where it was (the whole-field figure is larger
    only by the ingress bands, fresh water by design).

24. **Bake only, steadily driven — no live path, no quadrature.** The
    author's call: "remove the live and bake and only do bake". The flow
    writes into the field while the stir control is up, the vortex's pattern
    — rate × dt every frame, never an absolute, one step = two passes under
    the budget — with Ψ = rate/(k_x·k_y) so that `SUMI_CHLADNI_RATE` (1.5
    rad/s at ctl 1) is the cells' rotation rate. Nothing breathes: the
    quadratures of the earlier designs (cos ωt, sin ωt) only wobbled the
    sheet, and their delta-driven bake left residue by construction. The
    composite lost its Chladni block (the live path of design 3) and the
    print path has nothing to zero; `params.chladni_bake` and
    `chladni_channel` are gone. `sumi_version` stays 0.11.0: none of it had
    shipped.

25. **The layout is the plate — with a cell-size knob, and no Faraday.**
    `sumi_layout_cell_lattice` (`core/src/layouts.cpp`, internal) reports
    each playable layout's cell pitch and first centre — the Jankó's stagger
    and the piano grid's accidentals sit at half-cell offsets, so those two
    report the HALF pitch along x and every cell is an eddy; the layout lives
    in normalized space and the pass in aspect-corrected space, so x converts
    through the aspect normalize() last saw. Each note's drop, at its cell
    centre, spins in place (the elliptic point; the Rankine core's look); the
    cell corners are the saddles; the ink between is stretched along the
    boundaries into the figure that outlines the grid. `params.chladni_cell`
    (0.5..1.5, default 1) scales the lattice pitch about the layout's first
    cell centre — one eddy per cell at 1, four at 0.5, a cell and a half at
    1.5 — the author's "cell size" slider. The layouts without drawn cells
    take, at the author's instruction, the largest IMAGINARY square cell that
    does not touch a neighbour's: the circle of fifths its octave-ring
    spacing (0.032 canvas heights — pitch classes on a ring are 0.52 r apart,
    at least 0.052 on the innermost ring, so the rings bind), centred on the
    circle; the rolls one semitone of their pitch axis (0.88 of the canvas
    over 128 notes = 0.0069, three and a half texels at 512 — the eddies are
    at the texel scale there and the flow reads as fine shear; the octave
    would be the legible alternative, the author's to pick), anchored on the
    note positions and the now-line. A `chladni_faraday`
    switch (the lattice half a cell over: eddies on the corners, the
    figure's lines through the cells) was built and measured — the fixed
    points swap type exactly — and then REMOVED at the author's request on
    closing the step: the real inverse Chladni effect is boundary-layer
    acoustic streaming, which the author wants to think through properly on
    another day rather than approximate with a phase shift; recorded here as
    the author's deferred idea, with the half-cell shift in this entry's
    history as the cheap version it is not.
    `SUMI_CTL_CHLADNI_A` (16) is the stirring rate and `_B` (17) the balance
    between the two diagonal waves (weight 1 − 2B on the second: 0 the cells,
    ½ a single diagonal wave, 1 the cells reversed); CC 106/107 on the
    desktop, stock map v5. Measured on the chromatic grid through the public
    probe after ninety stirred frames: the cell centres and corners stay
    fixed while the boundary midpoints move; a ring round a centre TURNS
    and a ring round a corner STRETCHES; the same holds on a 16:9 field; at
    cell size 1.5 the mapper's pitch is 1.5× the probe's and the probe's
    centre cell is still a lattice centre; at 1 the mapper's pitch equals
    the probe's to 10⁻⁴ and its centres fall on the probe's to 10⁻⁷ of a
    pitch (the numbers are in the step's evidence).
    Flagged against the roadmap's step-37 text (the separable pair at both
    insertion points, the interval ratio), superseded by the author's
    decisions in review; the author's `chladni.md` records the physics.

26. **Test observables that survived the first run, recorded for the next
    operators.** (i) A pair's residual is measured over the INTERIOR (a margin
    of the displacement plus a texel) — the ingress bands are fresh water by
    design and dominate a whole-field mean. (ii) A lattice's wavenumber is
    read by the dominant Fourier component of a displacement profile, not by
    zero-crossing counts, which the distortion of many composed passes can
    push off by one (the row read 7 crossings for 3 waves). (iii) A test
    that compares two renditions of a scene must start both from a fresh
    sheet — the first run's dip check laid its first scene over the previous
    part's rings. (iv) The quadrature's phase is the mapper's clock since the
    instance was created, so a bake test cannot assume which shear is strong;
    it checks whichever carries more than two texels. (v) In a two-dimensional
    kick-drift only the cell CENTRES — where both shears vanish — are fixed
    points of every pass; along a row's centre line the perpendicular shear
    still moves things and the composed passes bend the rest, so nodes are
    measured at points (the 84 cell centres against the 66 corners), never
    along lines — the first measurement along lines read a ratio of 0.5 where
    the point measurement reads 0.013. (vi) Node spacing is HALF a
    wavelength, so an inversion is a quarter-wavelength phase shift. (vii)
    A flow's fixed points are classified by what they do to a RING of texels
    around them — an eddy rotates it, a saddle stretches it (mean |log r'/r|)
    with no net rotation — never by "tangential versus radial": after a
    radian of turning a ring point's chord has a large radial component, and
    the first classifier read 1.4 : 1 where the rotation reads 0.33 rad
    against 0.00. (viii) Neighbouring eddies COUNTER-rotate, so a rotation
    averaged with its sign over the lattice is zero by construction — the
    second classifier read exactly 0.00 before the per-ring absolute value.

27. **The `chladni` scene ships in the marble app and the gate's sweep, not
    yet in the docs' check (#22's rule).** A chord on the chromatic grid —
    its drops are the eddies' centres — stirred for a chosen number of frames
    at a stir, a balance and a cell size; the web host gained one parameter
    id and the `chladni` cwrap (a lattice of the gesture's own); the
    settings-panel controls are step 46's. The desktop settings window
    gained a "Chladni" section: stir and balance as CC sliders on the routes
    and the cell-size slider.
