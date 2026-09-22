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

## Step 38 — Viscous multipole burst (macOS)

28. **The derivation holds, numerically, and the literature check is on
    record before any wording.** `tools/multipole_verify.py` (the sibling of
    `stokeslet_verify.py`; its output in `docs/evidence/step38/`) checks
    every link of MEDIUM §2.3 in pure Python: the m-th multipole heat kernel
    ω_m ∝ s^{m+1}e^{−s} sin(mθ)/r^{m+2} (∂_z^m of the Gaussian) solves the
    vorticity diffusion equation; its stream function is ψ_m = (K(m−1)!/4)
    γ_m(s) sin(mθ)/r^m with the spec's cutoff γ_m(s) = 1 − e^{−s}Σ_{k<m}s^k/k!
    — the mode-m Green's function and the recurrence γ(m+1,S) + S^m e^{−S} =
    m·γ(m,S) (m = 0 is Lamb–Oseen, m = 1 the Stokeslet of DECISIONS_4 #53);
    the time integral is ∫₀ᵗ γ_m(r²/4ντ) dτ = (r²/4ν) Φ_m(S) with Φ_m(S) =
    ∫_S^∞ γ_m/s² ds, and by parts Φ_m = γ_m(S)/S + Γ(m−1,S)/(m−1)! — the
    remainder is E1 for m = 1 (the logarithmic kernel that keeps the dipole
    special) and an exponential polynomial for m ≥ 2, so Φ_m is elementary
    and equals the spec's form (1/S)[1 − e^{−S}Σ_{k≤m−2}(1 − k/(m−1))S^k/k!]
    to 10⁻¹³; Φ_2 = χ = (1 − e^{−S})/S, the Stokeslet's own χ. The blob
    kernel Ψ = A_m (a/r)^{m−2} sin(m(θ−θ₀)) [Φ_m(r²/ℓ₁²) − Φ_m(r²/ℓ₀²)] gives
    d = ∇⊥Ψ in closed form (d_r, d_θ in the shader header), divergence-free
    to 10⁻⁹, zero at the origin; near the core the quadrupole is the pure
    hyperbolic strain λ(x′, −y′) with λ = A₂(1/a² − 1/ℓ²) → 1.359 D/a — the
    pinch is its r → 0 limit, as §2.3 says — and order m is the harmonic
    polynomial Im(z^m), |d| ∝ r^{m−1}. **One precision on the spec:** the
    "cos(mθ)/r far field" is exactly the QUADRUPOLE'S diffused zone a ≪ r ≪ ℓ
    (measured slope −0.997); order m decays there as cos(mθ)/r^{m−1} and
    beyond ℓ every order falls as 1/r^{m+1} (the potential multipole times
    the age). Not a conflict — §2.3's sentence is about the quadrupole, the
    primary voice — but the page draft says it the general way. **The
    normalisation** is the lobe displacement AT r = a on the ejection axis
    (D); the roadmap's "peak lobe displacement at r = a" is read that way
    because the true peak sits at 1.07–1.45 a and is 0.4–9% above D for the
    quadrupole (1.4–1.6 a and 20–34% for m = 3): the spec's point — the
    stagnation origin makes centre-normalisation meaningless — stands, and
    r = a is the clean anchor. **The literature** (`literature.md`): the
    velocity fields of the viscous multipoles are classical (Voropayev &
    Afanasyev's Stokes-approximation multipoles; Chan & Chwang's unsteady
    2-D singularities; the Hermite modes of Gallay–Wayne and Uminsky–Wayne–
    Barbaro), the time integration is Jaffer's move (arXiv:1810.04646, m =
    0), and the displacement form for m ≥ 2 we did not find stated. The
    docs say "method after Jaffer, extended here"; never "new" or "first".

29. **Class sub-stepped; the wake's a/4 rule generalises to "peak
    displacement ≤ β_m × the pass's current core".** The criterion behind
    the wake's a/4 is |∇d| ≤ 0.25 (the inverse lookup's det ≥ 0.5). For the
    burst the API amplitude is the wrong yardstick: an aged pass of order
    m ≥ 3 acts at r ~ ℓ₀ where d ∝ r^{m−1} dwarfs the lobe at r = a —
    max|∂d| per (D/a) reaches 194 for m = 4 at ℓ₀ = 11a — while normalised
    on the pass's own peak displacement and its current core ℓ₀ the gradient
    is bounded for every order and age in the table: 1.83, 2.29, 2.39, 3.04,
    3.15, 3.58, 3.27 (d_max/ℓ₀) for m = 2..8, hence β_m = 0.137, 0.109,
    0.105, 0.082, 0.079, 0.070, 0.076, shipped with a margin as 0.13, 0.10,
    0.10, 0.08, 0.075, 0.068, 0.072 (`sumi_burst_budget`). At the budget the
    inverse-lookup det stays ≥ 0.98. The peak lies on the ejection axis
    (verified for every row), so the C side finds it by a log-spaced scan
    and a golden-section refinement (`sumi_burst_peak`); the **greedy
    march** (`sumi_burst_step`) takes the largest ℓ′ whose increment is
    within budget, by bisection in ℓ² — the peak grows monotonically with ℓ′
    because Φ_m falls with S. A quadrupole of D = 2a over age 4 marches in
    thirteen pieces, each ≤ its budget (headless test). On the GPU one
    budgeted pass reads det min 0.844 everywhere, mean 1.00000; the pair
    (+D, −D) at D = a/4 leaves max 0.76 texel, mean 0.018 — the class's
    first-order residual |∇d|·d, informational as the wake's. **expm1:**
    GLSL has none; the shader carries the equivalent — the small-S series of
    the plateau DEFICIT 1/(m−1) − Φ_m below S = 1 and the closed form above,
    the difference of two near-core values taken between deficits, never
    between two plateaus (the Lamb–Oseen small-r lesson, verbatim). The
    shader reproduces the double reference along the axis at a/2 … 4a to
    0.023 texel.

30. **The gesture IS the strike with its lifetime; the age and the release
    are parameters; D is the linearised amplitude.** `sumi_add_burst(x, y,
    a, D, θ₀, m)` registers an EPISODE in the mapper (32 slots; a full table
    replaces the episode nearest its end): the age grows as ℓ² = a² +
    (ℓ_end² − a²)·t/life — the spec's ℓ² = a² + 4νt with 4ν set by the
    release — and each frame the increment since the last emitted age goes
    out as budgeted passes, the first in the next `sumi_update`.
    `params.burst_age` (ℓ_end/a, 1.5..12, default 4: 92% of the eventual
    displacement of the quadrupole, 47% at 1.5, 99% at 12) shapes the burst
    without scaling it, since D is measured over the burst's own age;
    `params.burst_life` (0..4 s, default 0.8; 0 = at once) is the release;
    `params.burst_order` (2..8, default 2, at the author's request) is the
    order a strike takes when the gesture passes m = 0 — the desktop's U key
    and, at step 42, the strike route until the pitch-class → m table
    overrides it per note. `m` clamps to 2..8 — 8 because the budget table
    (#29) and the shader's bounded loops stop there, and the higher orders
    keep their motion within ~1.6 cores anyway (d ∝ r^{m−1} at the core);
    θ₀ is in the canvas frame (y down); D < 0 is the first-order inverse. **The emission floor is the field's quantum:** the
    coordinates live in half floats, whose spacing in the outer half of the
    canvas is 2⁻¹¹ canvas heights (4.9·10⁻⁴), and a pass that moves a
    texel's source by less than half of that rounds back to where it was.
    With a 2·10⁻⁴ floor the release's tail vanished pass by pass (the lobe
    stalled at 73% of D, measured); at one quantum (5·10⁻⁴, merged until
    the increment's peak reaches it) the tail lands within 0.35 texel of D —
    8% of a 4-texel strike lost to the medium's quantisation, recorded. One
    episode takes at most 24 passes a frame; a pass-budget refusal merges by
    construction (the age bookkeeping IS the pending accumulator). **D is
    Eulerian:** one pass applies the closed-form field exactly; a strong
    strike composed of many passes follows the FLOW, and the quadrupole's
    core strain integrates to e^λ with λ = 1.36 D/a — at D = 2a the material
    moved four times the linear prediction. Physical (the flow of a strain
    field is exponential), documented in the ABI comment, and the reason the
    tests stay at D ≤ a/4. The strike route waits for the binding tables
    (step 42); the desktop bench fires it with U (Shift+U: m = 3) at the
    cursor, its axis toward the canvas centre.

31. **Test observables for the burst family, recorded.** (i) The field
    stores each texel's SOURCE, so st − (u, v) is the displacement of the
    material now at the texel and its radial part is positive when ejected
    — the sign the spec's θ₀ promises. (ii) Read displacements on the −x /
    −y side of a centred burst: u < 0.5 there, where the half-float quantum
    is 0.125 texel at 512 (0.25 on the + side), and the residual of a
    quantised increment is halved. (iii) A pair test cannot catch a
    per-piece amplitude error in a sub-stepped operator — both signs scale
    alike and cancel regardless; the amplitude's test is one pass against
    the closed form. (iv) A far-field law is a property of ONE pass; a
    composed strong strike is the flow, not the field, and a 1/r reading
    taken on it is wrong by e^λ (the first draft of the check read 3.0 for
    a predicted 2.03 at D = 2a). (v) An episode's end is the mapper's
    count (`sumi_debug_burst_count`), not the field: the last sliver below
    the floor closes without a pass.

32. **The gate: the strike stream at gesture rate.** `--soak burst`: the
    pairs at D = a/4 (two pieces each way, at once), the (b) det on one
    budgeted pass everywhere (no body, no capsule), and a stream of one
    quadrupole strike every third frame with a three-frame release — 2000
    strikes over the 6000-frame window at about one pass a frame, the
    wake stream's density, so (d)'s per-pass fade compares like with like.
    Green: (b) det min 0.812 everywhere, mean 1.00000; (c) growth +0.00%,
    interior −5688, route alive (71 539 texels moved); (d) 9.30·10⁻⁶/pass
    against the tine's 1.15·10⁻⁵ (×0.81). The pairs, informational, fade
    −17.6% over 500 pairs of FOUR passes (two pieces each way at D = a/4)
    against the wake's −4.7% over pairs of two: per pass twice the doublet
    pair's, the first-order residual reshuffling the boundary each pair.
    Recorded, not gated — the class's (b) is the det (#13).

33. **The `burst` scene ships in the marble app and the gate's sweep, not
    yet in the docs' check (#22's rule).** Two strikes on the clusters, A
    along θ₀ and B a quarter turn on, with D (of the core), the core, θ₀,
    the order, the age and the release as sliders; the web host gained
    three parameter ids and the `burst` cwrap; the desktop settings window a
    "Burst" section (age, life and order, with the note that the strike
    route arrives at step 42). The page draft `burst.mdx` carries the lineage
    line and the author's note from the roadmap verbatim, marked for the
    author to trim and sign.

## Step 39 — Spark shear & the composed strike (macOS)

34. **"Shears invert for any profile" is now a test, on the GPU and in
    double — and so is its converse.** The spark shear pass (`deform.glsl
    spark_fs`, `SUMI_DEFORM_SPARK`) is one stage of MEDIUM §2.4's piecewise
    kick-drift: in the frame rotated by θ₀ about the strike, stage 0 slides
    each row by A·w(y)·f(y), stage 1 each column by B·w(x₁)·f(x₁), with f a
    stack of triangle waves at k, 2k, 4k, 8k (weights 1, ½, ¼, ⅛,
    normalised so |f| ≤ 1; `params.spark_stack` the depth, 1..4, default 3 —
    the spec's stack out of a magic constant) or of piecewise-linear hash
    noise (`params.spark_profile` = 1), and w a Gaussian window ACROSS the
    shear (band 0 = none). A shear x′ = x + g(y) inverts as x = x′ − g(y)
    whatever g is, so each stage is exact for any profile and its kinks are
    creases, legally. Measured (`--spark-test`): a step of 15-texel kicks on
    a 43-texel base moves the interior pre-image 7.35 texel; the step then
    its exact inverse leaves 0.212 texel (max 2.10, at the kinks — the
    resampler's), for the noise profile 0.009 (max 0.35); the same-order
    sign flip leaves 6.17 and 1.60 — NOT an inverse, the Chladni lesson
    (#17) again. Headless the same in double: the inverse to 10⁻¹², det J =
    1 to 10⁻⁴ through the kinks, both profiles. **The exact inverse of the
    step (A, B) is the step (0, −B) followed by the step (−A, 0)** — reversed
    order and negated — and the gesture makes it expressible by skipping a
    zero amplitude: no sign convention, two calls. One stage alone is a pure
    shear: no row moved in y (0.000 texel), each row slid rigidly (in-row
    spread 0.039 texel), the largest row 11.7 texel. The noise is an integer
    hash (lowbias32), bit-identical on Metal, GL and WebGPU.

35. **The wavenumber is a flavour ctl, and CC 74 is prepared to drive it.**
    `SUMI_CTL_SPARK_K` (18; COUNT 19) maps 0..1 to a BASE wavenumber of 2π·2
    .. 2π·24 per canvas height — two waves across the height (a thick
    channel) to twenty-four (fine streamers) — with the octaves stacking
    above it, so the finest wave at the top of the range is 5 texels at 512
    and the mid default a 39-texel base. The first draft ran to 2π·64: with
    three octaves the finest wave was 2 texels and the default 4 — the
    profile aliased at the harness's resolution and an exact 15-texel kick
    on it had slopes of several texels per texel. **`slide_mode` 2** routes
    a member channel's CC 74 to this ctl (the latest voice's slide wins; the
    aux modulation and the pinch stay on modes 0 and 1 — one consumer), not
    the default: the "slide-mode-style Anod default" the roadmap asks to
    prepare, for the binding tables to switch on (step 42). The desktop
    stock CC map gained the CC 108 handle (v6; v5 migrates), the settings a
    "Spark" section (shear, decay, octaves, profile, frequency as a CC
    slider on the route) and the slide radio its third option.

36. **The shear is an episode: kicks that decay as e^{−t/τ}, spent as
    kick-drift steps at the field's quantum.** `sumi_voice_mapper_add_spark`
    registers a strike (32 slots; a full table replaces the episode nearest
    its end) with A = B = `params.spark_shear`·r (default 0.6 of the strike
    radius), τ = `params.spark_tau` (default 0.25 s, over at 4τ), the window
    2r across each shear, k from the SPARK_K ctl at the strike, φ drawn per
    strike from a small LCG so consecutive strikes crease differently. Each
    frame the exact increment A(e^{−t₀/τ} − e^{−t₁/τ}) joins the pending kick,
    which goes out as one step (two exact passes) once it reaches the
    half-float quantum (#30); the last sliver flushes when the episode
    ends. Successive steps do not commute — each is exact, the composition
    is exact, but the emission granularity is part of the look, bounded
    below by the quantum and above by the frame — so the total kick is
    A(1 − e^{−4}) exactly (headless: to 10⁻⁶) while the figure it draws
    depends on how it was dealt. Emitted amplitudes decay; pairs are
    stage 0 then stage 1 with equal kicks.

37. **The composed strike, and its class by inheritance.** `sumi_add_spark
    (x, y, r, D, θ₀, layer)` is the drop (the Joule blast — radial outflow
    is divergence, so the engine's oldest exact operator does it; the layer
    as `sumi_add_drop`'s, so the soak can strike with clear water), the
    burst of core r and lobe displacement D along θ₀ (order
    `params.burst_order`, the m = 0 rule of #30), and the shear episode
    along and across θ₀. The drop lands in the gesture's frame, the two
    episodes from the next update. `sumi_add_spark_shear(x, y, band, A, B,
    k, φ, θ₀)` is one step of the shear as a gesture (the stack and the
    profile the params'). CLASS: the shear declares EXACT and soaks as
    `spark-shear`; the composition declares SUB-STEPPED BY INHERITANCE — the
    roadmap's strictest-member rule, the burst — and soaks as `spark` under
    the burst's numbers. Measured on one composed strike (clear water, r =
    0.05, D = 0.0045): the whole composition's field differs from the
    drop's alone by 1.79 texel over r..3r; the Jacobian is read with the
    shear off — the drop then the burst, the sub-stepped member inside the
    exact one's field (#38) — and stays first-order (see the summary), the
    drop alone reading 0.812 in the same region.

38. **Test observables for shears with kinks and for compositions with a
    drop, recorded.** (i) Finite differences cannot measure an EXACT
    kick-drift shear at its kinks, at ANY slope: analytically det = (1 +
    A f′·B g′) − A f′·B g′ = 1, but on the resampled field the two
    difference quotients straddle a kink unequally (one spans 2h, the other
    2h·B g′) and the stencil reads 1 ± 2·A f′·B g′ — 0.46 at a 0.45
    texel-per-texel slope, −6.7 at 2.7 — on a map whose Jacobian is
    identically one. The Jacobian read of a composition therefore switches
    the shear OFF and reads the sub-stepped member (the drop then the
    burst); the shear's exactness is the inverse test's business, which is
    exactly the strictest-member rule made operational. (ii) A drop's rim is
    a singularity of the pre-image: inside, the identity; outside,
    sqrt(d² − r²), whose slope d/sqrt(d² − r²) stays above 1.5 texel per
    texel until d = 1.34 r; and the passes that FOLLOW a drop carry the rim
    with them — a texel inside whose source lies across the rim reads the
    compressed exterior, so the jump moves inward by their displacement (det
    −4.7 at 0.87 r, the drop alone reading 1.000 there). Exclude r − 8
    texels .. 1.4 r, not ±3. (iii) A shear
    band that reaches the canvas edge has an INGRESS SEAM — fresh water
    beside a row that sheared out — which the stencil reads as a fold; keep
    an edge margin of the kick's reach. (iv) A kick-drift step's inverse
    residual concentrates at the kinks (max 2.1 texel against a mean of
    0.21 for the triangle stack): the mean is the invariant to gate, the
    max the resampler's.

39. **The gate, and what a drop does to it.** `--soak spark-shear` (exact:
    pairs of a 15-texel step and its reversed-order inverse in a 0.2 window;
    a stream of small steps whose kick wobbles in sign — chaotic advection
    under a jagged shear): green — pairs hold mass −0.59%/+0.00% with a
    pre-image dev of 3.60 texel, growth +0.00%, erosion 1.83·10⁻⁵/pass
    against the tine's 1.15·10⁻⁵ (×1.59: kinks every few texels fade
    faster than smooth shears, within the bar). `--soak spark` (sub-stepped
    by inheritance): the Jacobian read on one composed strike — the drop
    then the burst, the shear off (#38) — det min 0.750, mean 1.00004,
    inside the wake's numbers (0.734 / 0.766). **The first stream run read
    mass 0 at 500 pairs and at 6000 passes** (pre-image dev 145 texels, (d)
    ×14.5): not erosion — every clear-water strike is an exact expansion
    that pushes the ink outward, and 1500 of them at one spot push it off
    the canvas, which no mass observable can tell from loss. The gate has
    excluded drops "by nature" since #13; the composed strike inherits that
    exclusion for its blast. So `SUMI_DROP_NONE` (3) joined the drop layers
    — `sumi_add_drop` lays nothing, `sumi_add_spark` fires the burst and
    the shear episodes without the blast — and the `spark` pairs and stream
    run blast-less: composed strikes every fourth frame, the axis turning,
    a three-frame burst release and a ten-frame shear episode. Green:
    growth +0.00% (route alive, 260 893 texels moved), erosion
    1.85·10⁻⁵/pass against the tine's 1.15·10⁻⁵ (×1.61) — the same order as
    the shear's own ×1.59, the burst's alone being ×0.81: the kinks are
    fine structure and the medium fades fine structure first (#15). The
    blast-less pairs, informational, read −9.53% and 20.6 texel: a −D strike
    negates the burst only, both strikes' shear episodes add, and the
    shear's own inverse is `spark-shear`'s business. Results in
    `docs/evidence/step39/SUMMARY.md`. Scene `spark` (the marble app and
    the gate's sweep, not the docs' check — #22): A shows the composition
    up to a chosen stage — the drop, then with the burst, then the whole
    spark — and B the whole spark a quarter turn on; sliders for the
    radius, the burst's D, the shear kick, the frequency (CC 108), the
    decay, the octaves, the profile and the axis. The web host gained four
    parameter ids and two cwraps; the desktop bench the Z key (the composed
    strike at the cursor, its axis toward the centre). Page draft
    `spark.mdx` in the evidence folder, for step 63.

## Step 40 — Chirikov standard map, the boss gate (macOS)

40. **The scaled standard map is two exact shears, and its inverse undoes
    the drift first.** `SUMI_DEFORM_CHIRIKOV` (`deform.glsl chirikov_fs`)
    is one stage of MEDIUM §2.5's kick-drift: the kick y₁ = y + A·sin(k(x −
    x_c) + φ), a y-shear; the drift x₁ = x + ε·(y₁ − y_c), an x-shear. In the
    torus variables X = kx, Y = kεy this is X′ = X + Y′, Y′ = Y + K sin X
    with **K = A·k·ε** the step's chaos parameter (Greene's threshold K_c ≈
    0.9716). The ε-scaled drift is the spec's: x₁ = x + y₁ on a non-wrapping
    canvas is a canvas-scale shear; ε keeps it a shear at usable sizes, and
    the kick amplitude follows as A = K/(k·ε). CLASS EXACT (det J = 1 at any
    K); the exact inverse is the drift undone first, then the kick (reversed
    order, negated — #17). Measured (`--chirikov-test`): one step at K = 0.5
    (a 41-texel kick amplitude) moves the central band's pre-image 32.9
    texel and the step then its inverse leaves 0.042 (smooth shears: the
    resampler's floor, ten times below the spark's kinks); the pass matches
    the closed form x = P.x − ε(P.y − y_c), y = P.y − A sin(k(x − x_c) + φ)
    to 0.12 texel. Headless: the inverse to 10⁻¹⁶, det J = 1 to 10⁻¹⁰, the
    same-order sign flip a residue of 0.16. `sumi_add_chirikov(x, y, K,
    periods, ε, φ)` is one full step as a gesture (k = 2π·periods per canvas
    height along x; K < 0 the exact inverse; |K| clamped at the gesture
    ceiling `SUMI_CHIRIKOV_K_GESTURE_MAX` = 2, where the medium stops
    rendering the map at all — the sweep, #42).

41. **"Delta-driven K" resolved: a throw of δ is one step at δ²·K_max, and
    the wheel down retraces.** The spec wants K from the mod wheel / breath
    as deltas, never absolutes. A step with the kick scaled by δ and the
    drift left whole is not a delta — a wheel at rest would still shear the
    canvas every frame — so both shears scale with δ: the identity at δ = 0,
    the full map at δ = 1, and the step's chaos parameter δ²·K_max
    (`params.chirikov_kmax`, 0..2, default 1). The consequence is the
    instrument's: a wheel eased over m frames is m steps at K_max/m² — the
    kick-drift splitting of the PENDULUM flow, integrable, smooth sheets —
    while a wheel THROWN is one hard kick, chaos. The depth into chaos is
    the wheel's speed, and the smoother (`smoothing_ms`) is the first cap
    on it. The route (`SUMI_CTL_CHIRIKOV_K`, 19; COUNT 20; unmapped in the
    core, CC 109 in the desktop stock map v7, the mod wheel under the Anod
    table at step 42) keeps a delta tracker like the pinch's; a throw below
    0.02 of the range accumulates. A NEGATIVE δ applies the exact inverse
    step, so the wheel down undoes the wheel up step for step (headless: a
    one-frame throw at K_max 2 is one step at the ceiling then the
    remainder 0.17, then nothing; at K_max 0.5 one step at 0.5 and, on the
    way down, one inverse step, drift first). On the GPU a throw of the
    control moves the band 42.6 texel through the smoother's run of gentle
    steps and the control home retraces to 2.86 — the second-order residue
    of steps that commute only to first order. The map is centred where the
    vortex is (the VORTEX_X/Y ctls): the same hand steers, and the Anod
    table gives the mod wheel to the map where Sumi gave it to the vortex.
    `params.chirikov_periods` (1..8, default 2) and `params.chirikov_eps`
    (0.05..1, default 0.5) are the geometry.

42. **The boss gate: the erosion sweep, its table, and the ceiling.**
    `--soak chirikov-sweep`: one full step of the map every other frame
    for the gate's 6000-frame window (one pass a frame, the tine control's
    density), the gate's scene and voice, per K ∈ {0.25, 0.5, 0.75, 0.9716,
    1.25, 1.5, 2.0}; periods 2, ε 0.5, the centre (0.5, 0.5). **Read over
    the whole window, (d) is RED at every K, including 0.25 — and the
    checkpoints say why:** 45–60% of the scene's ink is gone by frame 1000
    at every K, then the mass settles. That is not resampling erosion. The
    torus wraps and the canvas does not (#43): every rotating orbit
    advances in x by its momentum each step and marches off the side — at
    |y − y_c| = 0.2, the scene's reach, 51 texels a step — so the drift
    flushes the scene's rotating material in the first few hundred steps,
    at ANY K, and what stays librates or sits near the centre line. **Read
    after the flush** (frame 1000 as the base), the erosion of what stays
    is the gate's business, and it is monotone in chaos above threshold:

    | K per step | flushed by 1000 | erosion/pass after the flush | × tine | (d) |
    |---|---|---|---|---|
    | 0.25 | 58% | 2.2·10⁻⁵ | ×1.88 | green — a small separatrix (Y-reach 2√K = 1): little librates, the rest still drifts slowly and leaks |
    | 0.5 | 47% | 1.2·10⁻⁵ | ×1.02 | green |
    | 0.75 | 42% | 1.2·10⁻⁵ | ×1.00 | green |
    | 0.9716 | 44% | 1.7·10⁻⁵ | ×1.50 | green — Greene's threshold |
    | 1.25 | 52% | 2.0·10⁻⁵ | ×1.78 | green |
    | 1.5 | 57% | 3.4·10⁻⁵ | ×2.97 | RED |
    | 2.0 | 69% | 3.4·10⁻⁵ | ×2.94 | RED |

    (the glide-tine control 1.15·10⁻⁵/pass; the visible ink kept in the
    central rows 67 / 96 / 109 / 102 / 87 / 56 / 29 %, the boundary length
    ×1.2–1.8 then ×0.7 at K = 2 — the filaments finer than a texel average
    into gray). **The ceiling:** `SUMI_CHIRIKOV_K_CEIL` = 1.25 — the last
    tabulated K where (d) holds after the flush, above Greene's threshold,
    so a hard throw reaches chaos; the gesture's hard limit 2.0. **The
    author's call (MEDIUM §2.5's [ITERATE]), with the numbers:** (i) keep
    1.25 — a throw crosses into chaos, the medium erodes what stays at
    under twice the tine, the flush is accepted as the operator's
    geometry; (ii) K_c = 0.9716 — the transition itself is the ceiling, ×1.5;
    (iii) 0.75 — the tine's own rate, sheets only; (iv) a drift profile
    bounded away from the centre line (an exact shear still, the standard
    map near y_c, the far rows no longer marching) would end the flush at
    the price of the textbook map's geometry — a design change for the
    author, not made here. The whole-window (d) is recorded RED for every K
    in the log, on purpose: the gate is not bent, the reading is
    explained.

43. **The torus wraps, the canvas does not — what the KAM transition looks
    like on a sheet.** Every rotating orbit of the standard map advances in
    X by its Y each step; on a torus that is motion around the cell, on a
    canvas it is a march to the side edge and out (the drift at |y − y_c| =
    0.225 — the separatrix's reach at K = 0.5 — is 58 texels a step). Only
    librating material, inside the island, stays for good. Measured after
    60 steps (`--chirikov-test`): rings centred on the hyperbolic point
    keep 53% of their ink mass at K = 0.5 — the librating half stays, the
    rotating half streams away along smooth sheets — and 16% at K = 1.5,
    where the chaotic sea flushes the inside as well; rings on the elliptic
    island keep 101% at K = 1.5. And a lesson about the separatrix itself:
    material ON the hyperbolic point stretches exponentially at ANY K
    (the point is hyperbolic below threshold too) and dissolves into gray —
    17% of the visible area left at K = 0.5, 0% at 1.5 — so "smooth
    sheets" are what lies AWAY from the separatrix, and a boundary-length
    or visible-area reading of rings on the fixed point is the separatrix's
    signature, not the transition's. The witness that survives on a canvas
    is what stays: the mass kept. The scene `chirikov` shows exactly this —
    rings at the hyperbolic point and on the island, the same step
    iterated, K on a slider through 0.9716.

44. **The scene, the bench, the settings.** Scene `chirikov` (the marble
    app and the gate's sweep, not the docs' check — #22): K per step,
    periods, ε and the iteration count; the web host gained three parameter
    ids and the `chirikov` cwrap. Desktop: `--chirikov-test` (4 checks),
    `chirikov` in the conservation gate through its delta route — green:
    500 pairs of a K = 0.5 step and its exact inverse hold mass −0.11% /
    +2.66% with a pre-image dev of 6.06 texel (the torsion's oscillatory
    family, under the 8-texel bar), the wobbling wheel's stream grows
    +0.40% (6.7·10⁻⁷/pass) and erodes nothing (−6.8·10⁻⁷/pass, the medium
    gaining): the route's deltas are gentle steps of an integrable flow, and
    chaos is reached only by throwing, each throw's step capped at the
    ceiling — `--soak chirikov-sweep` for the table, the I key (one full step at the cursor at
    K max; Shift+I its inverse), a "Chirikov" settings section (K max,
    periods, drift, and the throw as a CC slider on the CC 109 route — its
    changes ARE the throws), INI keys, the name "Chirikov throw". Page
    draft `chirikov.mdx` in the evidence folder, for step 63.

## Step 41 — The ABI event: libsumi 1.0.0 (macOS; the Mac compiles iOS in-step)

45. **The one break, and why the probe's state ships before any stateful
    layout.** `sumi_version` reads **1.0.0**. Five things changed in one
    bump, and the header carries the migration note above `sumi_version`:
    (1) `sumi_layout_probe` gained `const sumi_layout_state_t* state` after
    `aspect` — INSTRUMENT §1's struct verbatim (`buttons`, `slider`,
    `reserved[2]`, 16 bytes), NULL or zeros = stateless, which every layout
    shipping today is; (2) `sumi_cell_info_t` gained `flags` at its end
    (`SUMI_CELL_CONTINUOUS` = bit 0, the theremin's; 0 today) — the
    INSTRUMENT `[ITERATE: sentinel vs flags]` resolved as flags, as the
    roadmap fixed; (3) `sumi_params_t` gained `medium` at its end
    (`SUMI_MEDIUM_SUMI` 0, `SUMI_MEDIUM_ANOD` 1); (4) `sumi_set_palette` and
    `SUMI_PALETTE_CUSTOM` (#46); (5) `sumi_layout_t` 8..12 named and
    RESERVED — TRUMPET, TROMBONE, WICKI, FRETS, THEREMIN, the INSTRUMENT
    spec's names. The probe's state and the cell's flags land two phases
    before Phase 8 uses them because the arc allows ONE break (#3): a
    stateful layout added later would otherwise force a second signature
    change on every shell, and the cost of carrying an unused pointer and an
    unused word until then is nil. Every call site moved mechanically — the
    desktop bench, the headless suite (25 calls), the C11 ABI test, the web
    shim, the iOS overlay (`nil`), the Android JNI (`nullptr`; it compiles on
    the Linux box as the first line of step 45) — and `hostmpe` needed
    nothing (it never probes). From here on, additive growth only.

46. **The palette POD, and its first consumer.** `sumi_palette_t` is QOL
    §1's model as data: 2..8 stops of linear RGB at ascending positions
    along the ink-depth axis, a depth curve (γ and a floor: u = floor +
    (1 − floor)·depth^γ), a per-drop drift (the aux selector shifts the
    sampled position by ±drift/2 — the built-ins' hue drift, generalised),
    the clear-water band's tone, and four reserved words. `sumi_set_palette`
    validates on the way in — counts clamped, positions forced ascending,
    RGB and the curve clamped, NaNs zeroed — and stores; the composite
    reads it only when `active_palette_id` is `SUMI_PALETTE_CUSTOM` (3),
    through a branch that leaves the built-in path textually untouched:
    the same washi, the same soak, the same ink-thickness probe — the
    identity guardrail (the user chooses the hues, the medium keeps its
    character). A sumi-like two-stop palette stands in until a host sets
    one. Measured (`--palette-test`): a red-to-black palette recolours the
    inked texels and not one paper texel, and palette 0 afterwards prints
    bitwise as before. NOT done here, on purpose: QOL's "the built-ins
    become presets in the same model, one code path" — that unification
    would touch the built-in arithmetic, and the composite gate (#48) holds
    it to bitwise; it belongs with the palette editor (step 46), where the
    presets are written out in the model and the gate is the guard. The
    Anod medium will read the same POD as a glow (step 43).

47. **`medium` is inert until the Anod composite; the reserved values are
    clamped, with a warning.** `sumi_set_params` clamps `medium` above ANOD
    to SUMI, `pitch_layout` at or above TRUMPET to FIFTHS with a WARN log
    (the reserved layouts are refused by the probe as well), and
    `active_palette_id` above CUSTOM to 0. Medium 1 renders exactly as
    medium 0 until step 43 lands its composite — recorded so no one reads
    the switch as broken. The desktop persists `medium` in the INI and
    shows no switch yet (the switch is step 43's UI); the web host gained
    the parameter id and exports `_sumi_set_palette` without a JS surface
    until the editor.

48. **The composite screenshot gate — the print of the canonical script as
    a fixture.** `midi-sink --dev --composite-dump <file>` runs the §4.6
    field script on the 512² scripted clock, dips, and writes the print
    (RGBA8, top-left origin on every backend); `tools/composite_gate.py`
    compares it bitwise against `tests/fixtures/composite_512_metal.rgba`
    and proves red on a corrupted copy, as the field gate does. The fixture
    was generated from the PRE-BREAK renderer (0.14.0) before any header
    changed, two runs bitwise identical (the print has no time-dependent
    input: the dip fade is 0 on the print path, the live ripple off, the
    grain a hash of position). After the break the 1.0.0 print is bitwise
    the fixture — with the composite shader carrying the new uniforms and
    the custom branch — so "medium 0 renders as before" is proved for the
    pixels, not only the field. The roadmap's "bitwise as 0.9.0" is read as
    "as the pre-break renderer": 0.9.0 was the last version when the
    roadmap was written; steps 36–40 grew it additively to 0.14.0, and
    every one of those was gated bitwise on the field. The gate joins the
    release spine's Metal gates alongside the field gate.

49. **What the shells did in-step, and what waits.** The Mac compiled the
    iOS shell against the 1.0.0 header (`xcodebuild … BUILD SUCCEEDED`) and
    its libsumi; the wasm rebuilt and passed the web field gate and the
    16-scene sweep; the Android JNI was edited mechanically and compiles at
    step 45's first line, on device — main is never red between 41 and 45
    by that verification, as the roadmap asks. The About strings read
    `libsumi 1.0.0` everywhere through `sumi_version`.

## Step 42 — The Anod medium (macOS)

50. **The composite branches per medium, and the Sumi branch is the 1.0.0
    print, bitwise.** `params.medium` reaches the composite as a uniform;
    medium 1 takes `anod_col`, medium 0 the 1.0.0 path wrapped untouched in
    an `else` — the composite gate (#48) reads max channel diff 0 against
    the pre-break fixture with the Anod branch and its uniforms beside it.
    Anod reads the field two ways. CHARGED material (phase ≥ 1, the ink
    re-read) glows by its STRAIN: the stored source coordinates of the
    stencil's neighbours give J (one-sided differences, the smaller kept —
    #56), and for an area-preserving map ‖J‖_F² − 2 = (λ − 1/λ)² =: σ² —
    zero for the identity and for pure rotation, positive wherever the
    sheet was stretched; the glow is 0.22 + 0.78·(1 − e^{−σ/anod_glow})
    (`params.anod_glow`, 0.2..5, default 1: the strain that glows; smaller
    is hotter), the base making a fresh, unstrained strike visible as
    charge. The charge phase bands the filament between the palette's core
    and halo by parity, aux drifts the hue per event as the ink's hue drift
    did (0.45·hue_t). WATER never glows by strain — it draws the field's
    deformed grid (#57). The substrate is near-black glass (0.010, 0.010,
    0.014 linear) with the washi's own simplex grain as a phosphor speckle
    at `paper_roughness` strength, sampled at st — SPEC §4.5's screen-locked
    invariant, composite side; the darkness and grain knobs are step 43's.
    The three Anod palettes — electric blue / violet, plasma orange,
    phosphor green — sit under the SAME ids 0..2 the sumi palettes use
    ("continuing the palette ids" read as the medium re-reading them: the
    id is the player's choice, the medium its family) and morph on the same
    ring; the custom palette (3) is read as a glow, the gradient sampled by
    g. The dip's "lift the paper" flash is shared: in Anod it is the
    photograph's flash. Measured (`--anod-test`, 9/9): the identity field
    prints the substrate alone at 512² and at 1920×1080 (mean 27.5/255, max
    29.5); the §4.6 script's 41 169 charged texels sit at 111.1 and their
    luminance correlates 0.99 with the CPU's 0.22 + 0.78(1 − e^{−σ}) read
    off the same field; a lone drop's interior carries the base glow (91.0)
    and the water round it the grid. The roadmap's "every ring boundary
    glows" resolved the other way: a drop's rim compression is WATER strain
    and water shows lines, not glow — the author's call of 2026-09-22 after
    seeing both. MEDIUM §3's "live insertion point" (torsion sweeps and
    Chladni quadrature riding the composite) is superseded: neither exists
    since #24 and #20 — both bake.

51. **The binding tables, as a resolution rule, and what the medium decides
    outright.** MEDIUM §4 ships as `eff_modes`: a mode set to
    `SUMI_MODE_MEDIUM_DEFAULT` (255, the new default of `bend_mode`,
    `slide_mode`, `press_mode`) resolves to the medium's column — Sumi 0/0/0
    (the 0.x behaviour, unchanged), Anod 2/2/2 — and an explicit mode is the
    user's override, exactly as today. New mode values, additive: `bend_mode`
    2 = the torsion's wavenumber and 3 = the spark's (±1.5 semitones span
    the ctl, the ripple law's reach, #66; last writer wins), `press_mode` 2
    = the torsion sweep FEED (pressure adds to the same pending rotation the
    note-on sweep spends, 1.2 rad/s at full pressure, with its own phase
    clock; the ink feed and its episodes stay quiet). Three dimensions have
    no mode param and the MEDIUM decides them outright: the strike (Sumi the
    drop; Anod the spark composition — the drop, a burst of core = the drop
    radius with lobes along the note's pitch axis and its order from
    `params.burst_order_by_class[note % 12]` (2..8, 0 = `burst_order`;
    default naturals 2, accidentals 3 — the table the author signs by eye),
    and the spark shear episode — per echo), the poly-pressure dimension
    (Sumi the Lamb–Oseen swirl; Anod the Chladni stir: the loudest active
    voice's pressure sets `SUMI_CTL_CHLADNI_A` unless a CC is mapped to it —
    the CC map overrides) and the mod-wheel dimension (`SUMI_CTL_VORTEX_
    STRENGTH`: Sumi the vortex; Anod the Chirikov throw's source, the vortex
    quiet, the tracker re-baselined at a switch so the switch is not a
    throw; CC 109 stays the Sumi-side handle). The master bend keeps its
    shear tine in both media — the `[ITERATE: scroll-compatible shear]`
    closes as "the tine already composes with the scroll; nothing was
    needed". The press feed's `[ITERATE]` ships as the sweep feed and the
    hour of playing decides it. Headless (`test_medium_binding_tables`):
    Sumi as before; Anod: a C♯ strike lands the drop, four burst pieces of
    order 3 and the shear's first step in one frame, the bend moves
    TORSION_K by 0.5 and lays no tine, the slide sets SPARK_K to 0.945,
    forty frames of pressure spend 39 torsion passes and no drop, poly
    pressure sets the stir to 0.79 and emits 78 Chladni passes and no swirl,
    the wheel throws 2 Chirikov steps and no vortex; with the three modes
    overridden to 0 in Anod the bend, slide and pressure behave as Sumi's
    while the strike, the stir and the throw stay Anod's. The desktop's
    mode radios became combos with "Medium default" first; an INI written
    before 1.1.0 keeps its explicit 0s, which in Anod means the Sumi
    behaviour until the user picks the default — recorded, not migrated
    (a stored 0 may be a choice).

52. **The seam mask is found, not stored — and now guards the charge alone.**
    A scroll seam — fresh water beside displaced content — is a discontinuity
    of the map, and a finite-difference stencil straddling it reads a jump
    of the scroll's size as strain. Fresh water cannot be MARKED in the
    field (the ingress rule writes identity coordinates with zero phase and
    aux, and the §4.6 fixture pins those bytes), so the composite finds it
    by its class: a texel at its own identity coordinates, within one ULP,
    with no phase. A first rule for the inner seams — the staircase of N
    translated bands N scrolls leave, whose steps are not fresh — compared
    the two one-sided jumps (a translation has a side with nothing, strain
    has both) and is superseded by the estimator itself, which keeps the
    smaller one-sided difference per entry (#56) and so reads a step as no
    strain without a rule. Since water never glows by strain (#57), the mask
    matters only where a CHARGED texel meets fresh water and its stencil
    straddles the seam: such a texel reads no strain. The earlier
    seam-column measurements (27.5 against a 27.5 substrate) stand as the
    water's; the identity checks at 512² and 1920×1080 now carry the claim.

53. **Live switching is a feature.** MEDIUM §1's `[ITERATE: live switch or a
    forced dip?]` closes as live: the switch is a params write, the
    composite is a READ, and the field is bitwise across Sumi → Anod → Sumi
    → Anod (0 samples differ, measured). The evidence the roadmap asks for —
    the same recorded session re-read in both media — is `anod_reread_sumi.
    png` / `anod_reread_anod.png` in the step's folder: the §4.6 script
    printed under each medium from the same bytes. The dip stays one key
    away (9 on the bench, the settings' button) for whoever wants a fresh
    sheet between media. The desktop's "Medium" section (a radio and the glow
    scale) and the A key switch it; the web scene `anod` lays a session in
    Sumi and switches.

54. **Prints, and the questions carried.** A dip in Anod prints through the
    same readback with the medium's composite — "the photograph of the
    discharge" is the same machinery, medium-styled by construction; no
    print code changed. MEDIUM §3's `[ITERATE: long-exposure look? strain
    accumulation buffer?]` is CARRIED to the Phase-9 beta with its question:
    the field already accumulates the whole map, so a strain buffer would
    be a second history of the same thing; whether a print wants a time
    integral of the glow is a question for the eye after the hour of
    playing, not for a step. The `[ITERATE: substrate design]` resolves as
    the phosphor speckle above, its knobs at 43.

55. **What waits for the author, and what this step did not do.** The Anod
    column of the binding table is UNSIGNED until the hour on the ROLI Piano
    + Airwave (`binding_table.md` in the evidence folder lays the table and
    the ITERATE ledger out); the order-by-class table is a proposal; the
    drop-edge glow is by design until taste says otherwise; the burst's
    order "revisited by ear once Voxo lands" is Phase 7's. `sumi_version`
    → **1.1.0**, additive: `anod_glow`, `anod_pitch`, `burst_order_by_class
    [12]`, the mode values 2/3 and `SUMI_MODE_MEDIUM_DEFAULT` (and, from the
    Chladni rework before step 43, `chladni_mode` — #62). The tablet shells' ctl-name
    lists and mode pickers are still the 0.x lists — their steps (44, 45).

56. **The half-float field cannot be differentiated at screen resolution,
    and the estimator that reads the charge.** The coordinates are half
    floats: their spacing above 0.5 is 2⁻¹¹, a texel and a quarter of a
    2560-wide window, seven tenths of a texel of a 1440-high one. The 512²
    bench hides this — its identity coordinates are exactly representable —
    and the author's window did not: the right half of the identity field
    glowed in vertical stripes (the quantum beating against the texel grid,
    a plateau every four or five texels read as ∂u/∂x = 0), two sparks drew
    a giant X (the real strain of exact shear bands, which run the whole
    canvas by construction — MEDIUM 2.4 — not an artefact), and a burst's
    far field printed as concentric arcs (its stored coordinate advances by
    one quantum every few hundred texels, and a central difference across
    such a step reads a ring of false strain). What survives on charged
    material: the stencil widens with the field (2·round(H/512) texels a
    side — 2 at 512, 6 at 1440 — keeping the rounding a fixed fraction of
    the step); the fresh test tolerates one ULP; each entry of J takes the
    SMALLER of its two one-sided differences (a step, like a seam, has a
    side with nothing; strain has both; and a triangle's kink keeps its
    slope where a central difference would cancel it); and the expected
    rounding bias of ‖J‖_F² (the variance of two uniform ±ULP/2 errors per
    entry, three times for the tail) is subtracted. Measured: the identity
    at 1920×1080 prints 27.5 with its lower-right quarter at 27.5 (a first
    build read 40+ there); the charge's correlation with the CPU replica
    rose from 0.86 to 0.99. What does not survive: any reading of WATER by
    strain — a smooth displacement of a few texels over hundreds is below
    the staircase, whatever the stencil. Tried and rejected on the author's
    window, in order: water at 0.35·g and 0.12·g (arcs, stripes, the X);
    water as substrate (clean, "but it kills the far fields"); a knee under
    which nothing glows (hides the genuine faint field too); water glowing
    only within 0.06 of the charge, above a knee — the GAS (twelve taps on
    two rings; clean, liked, and "not what I was looking for": it loses the
    far field). The gas is kept as an idea for a future medium, not this
    one — the author's call of 2026-09-22. A higher-precision field
    (RGBA32F) would make strain readable on water and would break the phase
    invariant's fixture (DECISIONS_5 #12); not proposed for this phase.

57. **Water draws the field's deformed grid — the far field the author
    asked for, on purpose.** The picture the author liked in the failed
    strain builds — lines converging on a drop like a magnetic field,
    reaching the canvas edge — was the deformed grid drawn by accident: the
    half-float staircase draws the iso-lines of the source coordinates
    every quantum; on a 2560×1440 window the quantum (2048 per canvas
    height) beats against 2560 and 1440 texels into a grid of pitch ~5
    texels whose bending is the displacement amplified ~4×, negative along
    x and positive along y (2048 − 2560 < 0 < 2048 − 1440), which is why
    one family converged on the drop and the other bulged round it — with
    straight stripes wherever the water rested, the only thing the author
    disliked. Drawn deliberately: two families of iso-lines of (position +
    gain·displacement), gains −4 along x and +3.4 along y (the accident's
    signs, kept because the author chose the picture they made; both
    negative would converge both families), at a pitch of `anod_pitch`
    canvas heights at rest (#58), one to two texels wide. The displacement
    is a VALUE the field holds to a fraction of a texel — its derivative
    was the problem — and it is read from a window along each family's
    axis (3·round(H/512) texels a side: 7 taps at 512, 19 at 1440; charged
    texels and taps past the canvas edge left out), so the staircase's
    sawtooth averages out to a few percent of a quantum and the lines stay
    smooth where a raw contour of the displacement would wobble by tens of
    texels (tried: iso-lines of |d| and of its direction printed as a
    blocky wheel). Lines show only where the averaged displacement exceeds
    a texel, fading in to three, so rest is glass and the field reaches
    exactly as far as the operator does (a drop's a²/2r: a small drop's
    lines end where its displacement drops under half a texel, measured
    0.00% of the water beyond r = 0.15 lit for R = 0.02); a family fades
    where its local pitch falls under three texels (a drop's rim, a spark's
    core), so the grid never aliases. Under it: a drop's field converges,
    a burst's lobes and a spark's jagged shears draw their own, the exact
    shears' whole-canvas bands read as what they are. Measured (512², pitch
    10 texels): 42% of the script's displaced water lit, 40% of a lone
    drop's 1.5–2.4 R annulus, 38% of a small drop's 1.5–3 R. KNOWN, for the
    author: a uniform translation is a displacement, so on the rolls the
    scroll lights the whole grid as straight lines streaming at the gain
    times the scroll speed — the physics of the reading, and "Grid lines:
    Off" on a roll is the remedy until a scroll-relative reading is asked
    for.

58. **`anod_pitch`: the grid's pitch, and 0 is off.** The number of lines is
    the knob the author asked for: `params.anod_pitch`, the grid's pitch at
    rest as a fraction of the canvas height (1/256..1/8, default 1/144 — 10
    texels at 1440, the pitch the author called lovely), 0 = no grid,
    NaN and negatives landing on 0. A fraction of the height, not a texel
    count, so a print at a higher resolution shows the same lines as the
    screen. Measured: at 0 no water texel lights (0.00%) and the charge's
    mean luminance is unchanged (111.1 both ways) — the knob touches the
    water alone. The desktop shows it as "Grid lines", lines per canvas
    height on a logarithmic slider from Off to 256 (under 8 reads Off); the
    INI key is `anod_pitch`; the web host's param 32 and the `anod` scene's
    P slider carry it. The gains, the line brightness (0.14) and the alias
    pitch stay shader constants — step 43's knobs if taste asks.

59. **The plate guide, and what the lattice turned out to be.** Before step
    43 the author asked to see the layout the Chladni operator was stirring.
    A dev-only overlay (the bench's N key; `sumi_debug_set_chladni_overlay`
    behind a `dbg_lattice` uniform of the composite, 0 on every shipped path
    — the print fixture stays bitwise, measured at each build) draws the
    plate over the print. Drawn first as the lattice's nodal lines ψ = 0 and
    its eddy cores, it showed that the operator's lattice was a SECOND
    derivation of the layout — `sumi_layout_cell_lattice`, a rectangular
    pitch and phase — beside the probe the shells draw their keys from.
    Measured against those keys at the author's aspect: exact on the
    chromatic grid (12 × 7); on the Jankó every key centre on a node but two
    eddies per key across (the half-column pitch its stagger forced); on the
    piano grid the accidentals on the nodes and the naturals a tenth of a row
    off (the two families are 0.9 rows apart, #61's tenth, and no lattice
    holds both); and on the fifths and the four rolls — which draw NO keys:
    the probe declines, Play mode is meaningless there — an invented square
    grid, the fifths' 0.032 ring spacing laid as some 1730 cells over a
    radial layout of 132. The author's finding, twice over: "you are using a
    grid, not the cells", and the cells meant are the DISPLAY cells — the
    circles the shells draw, the probe's centre and cell_radius swept and
    deduped (PlayOverlayView.rebuildLatticeIfNeeded, sumi_play.cpp). The
    guide now draws exactly those, from the same enumeration the operator
    uses (#60): naturals cyan and accidentals amber as the shells colour
    them, the odd cells (#60's checkerboard) with a fuller face. A first
    draft capped the list at 128 and showed "a little more than half" of the
    Jankó's keys — its six rows of forty-two, three echo rows a note, are
    252 cells; the cap is 320 (`SUMI_LAYOUT_MAX_CELLS`).

60. **The cells are the eddies: the Chladni stir rewritten without a
    lattice.** The author's call, 2026-09-22: "remove the lattice and put an
    eddy in each circle". `sumi_layout_cells` (layouts.cpp) is the one source
    of cells for the operator and the guide: the three key layouts' from the
    probe at every note position (the circles the shells draw), the fifths
    and the rolls the largest circle at each note that touches no
    neighbour's — half an octave ring (128 discs of radius 0.016 on the
    twelve spokes between r = 0.10 and 0.42) and half a semitone (128 discs
    of radius 0.0034 on the now-line). Each cell carries its family
    (accidental) and its parity on the layout's own checkerboard (chromatic
    grid pitch class + row, Jankó column + row, piano grid white-key index +
    octave, fifths index + octave, rolls note), so neighbours differ. THE
    PASS (`SUMI_DEFORM_CELLS`, deform.glsl `cells_fs`): the display discs are
    disjoint, so one pass turns every disc about its own centre — a texel at
    ρ = r/R by θ·(4ρ²(1 − ρ²))², a RING: zero at the centre, full at ρ =
    1/√2, zero at the rim with zero slope — and a texel in no disc stays. A
    rotation preserves r, so det J = 1 inside every disc and the map is
    continuous across the touching rims: CLASS EXACT, the inverse the
    negative pass; the note's drop rests where it fell and is wound from its
    edge, the water between the keys never moves, and the ink sheared at
    each rim is the figure — it outlines the keys themselves. Which disc a
    texel lies in is an INDEX MAP the renderer rasterizes from the cell table
    at the field's resolution (RGBA16F, up to four owners a texel for #62,
    −1 for none; rebuilt on a cells change or a resize) — one texture read a
    texel whatever the layout, which is why the Jankó's 252 cells cost what
    the grid's 84 do. The ring profile came from the author's "around the
    cell, not inside" (first drafted as a core bump (1 − ρ²)²); it also
    carries the displacement where it is largest (0.727·θ·R at ρ = √(5/9)
    against the bump's 0.286), which is #61's clock. `SUMI_CTL_CHLADNI_B`
    sets the odd cells' sense — 0 neighbours counter-rotate, ½ every other
    cell rests, 1 all turn the same way (2B − 1) — and `chladni_cell` scales
    the disc about its centre, capped at 1 in this mode: above 1 two discs
    would overlap, a texel could follow only one centre, and the cut between
    them would tear the field along a chord (the other way to grow is #62).
    The two-wave gesture `sumi_add_chladni` keeps its lattice (ABI
    unchanged): its emission was generalised on the way to any Bravais basis
    — the waves are the dual basis of (a1 + a2, a1 − a2), so a rectangular
    pair is Taylor–Green's cos(kx)cos(ky) and an oblique one a staggered
    lattice — and that form stays for the gesture and the web scene. TRIED
    AND REMOVED the same day, at the call above: a lattice FITTED to the
    probe's cells (Cartesian, with the Jankó's stagger as an oblique lattice
    and the piano grid's eddies midway between its two key families) and a
    POLAR product flow for the fifths (rings × sectors, sub-stepped, since
    exactness needs the wave arguments linear in (θ, r²/2) and octave rings
    are equal in r) — both worked and neither was the cells. SPEC: MEDIUM
    §2.2 describes a kick-drift lattice whose k_x : k_y come from the chord's
    intervals; step 37's review call made the layout the plate (#17–#22);
    this entry makes the keys the eddies. The spec and the entries disagree;
    the entry is what shipped, the spec is the author's to revise. Measured
    (`--chladni-test`, 7/7, the chromatic grid at 512² unless said): after
    150 stirred frames the 84 disc cores (ρ = 0.15) moved 0.00 texel and the
    66 corners 0.00 while the discs' rings (ρ = 0.745) moved 15.95; a ring
    at ρ = 0.7 round a centre rotates 1.45 rad and stretches |log r′/r| =
    0.09, a ring round a corner rotates 0.00 and stretches 0.00; the 84
    discs are the probe's 84 keys, same centres and radius 0.0350; cell size
    0.5 halves the radius and 1.5 leaves it; on a 16:9 field cores 0.19 and
    corners 0.19 against rings 5.44, rings rotating 1.07 rad. The exact soak
    (`--soak chladni`, 3/3): 500 (+θ, −θ) pairs hold mass −0.22 %/+1.21 %
    with a 5.16-texel pre-image drift, fabrication 0.00 % over 6000 passes,
    erosion 1.56·10⁻⁷/pass — a hundredth of the tine control. The desktop's
    "Cell size" and "Balance" help, the soak table's `chladni` row and the
    binding-table test (which counts the cells pass as the stir) follow.

61. **The emission floor, a third time.** On a fresh sheet the first disc
    build turned a ring 0.07 rad in 150 frames where the rate said 1.9: a
    frame's rotation at 1.5 rad/s moved the fastest texel of a disc by a
    quarter of the coordinate quantum, every coordinate sat exactly on the
    half-float grid, and every pass rounded straight back — nothing
    accumulated (where a drop's compression had put the values off the grid
    the rounding dithered, which is why curls showed on the scripted field
    and not on the identity). The same lesson as the burst's floor (#31)
    and the Anod water's staircase (#56): the stir now BANKS its rotation
    (`cells_pending`) and emits one pass when it carries at least
    `SUMI_CELLS_MIN_EMIT` = 10⁻³ canvas heights of peak displacement on the
    smallest disc — two quanta of the top half of the coordinate range —
    θ_pass = 10⁻³/(0.727·r_min), the engine handing r_min over with the
    cells; a remainder left when the stir stops is let go (it could not be
    applied). The stir stays exact; only its clock coarsens: at full stir
    on a 1440-high canvas a pass every frame on the chromatic grid (0.024
    rad), about every 50 ms on the Jankó (0.075) and every 60 ms on the
    fifths (0.087), while the rolls' five-texel discs would need about a
    radian a pass — the stir is not meaningful there. The ring profile's
    2.5× larger reach is what brought the Jankó from 8 visible steps a
    second to 20; the only true remedy is a 32-bit coordinate field, the
    quality flag the renderer already anticipates, which would break the
    §4.6 fixture unless gated — a roadmap decision, not this step's.
    Measured with the floor: the ring at ρ = 0.7 rotates 1.45 rad, the
    cores rest (#60).

62. **`chladni_mode`: SUMI_CHLADNI_FIELD, the author's "inverse Chladni".**
    Asked for as a second effect beside the exact discs: the rings of every
    disc covering a texel SUMMED into one displacement, d = Σ θ·w·bump·
    (−rel.y, rel.x). A radial swirl is divergence-free for any profile, so
    the sum is, and the finite step applies it to first order — the
    burst's class, SUB-STEPPED — which is what lets the discs grow past their
    keys (`chladni_cell` to 1.5) and OVERLAP, the water between the keys
    stirred by both neighbours: the "burst-like" version, switchable against
    the exact one and off by default. The index map carries four owners a
    texel for it (a corner at 1.5 lies in four discs; a fifth is dropped).
    `params.chladni_mode` (0 SUMI_CHLADNI_DISCS, 1 SUMI_CHLADNI_FIELD) is
    additive to 1.1.0; the desktop's Chladni section gains a "Mode" combo
    and the INI the key, the web host param 33; `sumi_debug_add_cells_pass`
    pushes one pass for the gate. THE BUDGET: a pass of θ on a disc of
    radius R has |∇d| ≈ 4.6·θ where the ring is steepest, twice that where
    two rings overlap, so the class budget |∇d| ≤ 0.25 wants θ ≤ 0.027 —
    which the floor (#61) meets on the chromatic and piano grids at 1440
    (0.016–0.024) and exceeds two to three times on the Jankó and the
    fifths (0.075, 0.087): there the mode runs over budget, which shows as
    fabrication, not folds, and the soak is the judge. Gated (`--soak
    chladni-field`, 3/3, the chromatic grid at 1.5): one budgeted pass of
    0.025 rad has pre-image det min 0.855 and mean 1.00000; 500 (+θ, −θ)
    pairs hold mass −0.42 %/+0.17 % with a 0.75-texel drift (informational
    for the class); fabrication 2.7·10⁻⁶/pass (+1.64 % over 6000; under
    5·10⁻⁵); erosion −2.73·10⁻⁶/pass against the tine control's
    1.15·10⁻⁵ — the mass grows slightly rather than fades. Kept as a mode
    and not a medium: the Anod gas (#56) waits for a medium of its own; this
    is the same plate stirred another way.

63. **One palette path, and the built-ins bitwise through it.** Step 43's
    first brick (QOL §1, ROADMAP_5's "one code path, bitwise-checked"). The
    composite no longer carries per-id colour tables: every palette — the
    medium's three built-ins, the curated presets, the custom slot — is one
    `sumi_palette_t`, and `pal_ink_at(depth, hue_t)` = mix(gradient(depth),
    accent, drift·hue_t) is the only colour path, for both media (Anod: the
    gradient sampled by strain is the charge's core, the accent its halo,
    the charge phase banding the filament between them — the step-42 tables
    as presets). What made the built-ins EXPRESSIBLE was one change to the
    model (#46): `hue_drift` is now the built-ins' own per-drop drift — the
    aux selector blends the sampled colour toward `accent_rgb` (new, from
    three of the four reserved words; the POD's size is unchanged at 172
    bytes) by drift·hue_t — where step 41 had it shift the sampled position
    along the gradient. A built-in is then a two-stop palette of one colour
    (the 0.x literals verbatim, `palettes.cpp`) drifting 0.45 toward its
    accent, and its arithmetic through the one path is the legacy's in the
    same order: mix(ink, ink, t) is ink exactly, mix(stopA, stopB, m) is what
    mix(pal_ink(id0), pal_ink(id1), m) was, 0.45·hue_t is 0.45·hue_t. The
    ring moved to the CPU (`sumi_palette_ring`): the engine hands the
    composite two slots, A and B, and the blend, computed with the shader's
    own float operations (t = clamp(morph)·2, ⌊t⌋ capped at 1, t − ⌊t⌋), so
    a rest position is bitwise and a morph position is too. Measured
    (`--palette-test`): the §4.6 script printed under all six built-ins of
    both media at rest and under a morph of 38/127, hashed (FNV-1a 64 over
    the RGBA8 print) and compared with hashes captured from the legacy
    tables the day before the change — eight of eight equal; the composite
    gate (#48) max diff 0; the Anod test's hue check unchanged. QOL's
    `[ITERATE: curve fully fixed per medium, or an "advanced" fold?]`
    resolves as FIXED in 2.0: the depth curve (γ, floor) is the palette's,
    the washi's soak and the strain glow are the medium's and no palette
    touches them — the identity guardrail as code; the fold is deferred.
    `[ITERATE: per-drop hue drift as a palette field or global?]` resolves as
    a palette field (drift and its target), which the built-ins needed.

64. **The preset library, and the ring with a custom slot.** The library
    lives in the core — the identity statement is the medium's, not a
    shell's: `sumi_palette_preset_count(medium)` and `sumi_palette_preset(
    medium, i, out, name)`, pure and instance-free. Indices 0..2 are the
    medium's built-ins (the ids `active_palette_id` names); from 3, curated
    additions a shell offers as starting points for the custom slot — a
    PROPOSAL the author signs by eye, as the order table: "Cobalt & amber"
    (the Okabe–Ito blue/orange pair, safe under deuteranopia and
    protanopia, as ink and drift, and as charge and halo), "Viridis" and
    "Cividis" as perceptual ramps — thin ink bright to pooled ink dark, dim
    charge violet to burning charge yellow. Six a medium. `sumi_get_palette`
    returns the stored custom slot (a shell's editor round-trips through
    the core's validation; measured: all twelve presets return byte-equal).
    THE RING WITH A CUSTOM SLOT (the roadmap's open decision): the custom
    slot joins the ring only while it is active — a built-in active id
    travels the medium's three built-ins as since 0.x (no change of feel for
    a mapped morph CC), the custom slot travels custom → 0 → 1 → 2 over the
    same CC span (t = clamp(morph)·3). Headless (`test_palette_presets_and_
    ring`): the ring's pairs and blends at 0, ¼, ½, ¾, 1 for every active
    id, the twelve presets ascending and in range, indigo's literals.

65. **The palette editor on the desktop, and what waits.** The settings
    window gains a "Palette" section: the active slot (the medium's three
    built-ins and Custom), the library with "Load into custom", and — while
    the custom slot is active — the editor: the stops as sRGB pickers over
    the linear model (2..8, "+ stop" inserts before the pooled stop midway,
    "− stop" removes the one before it; the first and last positions are
    pinned at 0 and 1, the middle ones slide), the depth curve and floor,
    the hue drift and its target, the clear-water tone (Sumi). Live: every
    change writes the slot to the core through `app_settings_apply`, which
    now carries the palette beside the params; the INI persists it
    (`pal_count`, `pal_stop_i` as "r g b position" in linear RGB,
    `pal_gamma`, `pal_floor`, `pal_drift`, `pal_accent`, `pal_clear`), the
    "palette" key accepts 3, the 7 key cycles four. The custom slot starts
    as Sumi black. The web host accepts id 3 (its editor is step 46);
    `_sumi_get_palette`, `_sumi_palette_preset_count` and `_sumi_palette_
    preset` are exported for it. `sumi_version` stays 1.1.0, additive:
    `accent_rgb`, `sumi_get_palette`, the two library calls. NOT here:
    substrate (QOL §2), presets and the serializer (§3), prints and the
    ledger (§4) — the rest of step 43, next.

66. **The substrate's knobs, composite-side and bitwise at their defaults.**
    QOL §2 as four additive params: `paper_tint[3]` (the washi's base tone,
    linear RGB, default the 0.x cream 0.900/0.868/0.790 verbatim),
    `fiber_scale` (the strands' spatial frequency as a multiple of 0.x's,
    0.5..2, default 1 — it multiplies the strand and segment-mask
    frequencies, so at 1 the product is 0.x's exactly), `anod_dark` (the
    glass's darkness, 0..1, default 0.5 = the step-42 glass: the base
    0.010/0.010/0.014 times 2·(1 − dark), which at 0.5 is times 1.0) and
    `anod_grain` (the phosphor speckle's strength, 0..1, default 0.5). Each
    replaces a literal with a uniform of the same float or enters as a
    multiplication by exactly 1.0, so the Sumi composite gate stays max diff
    0 and the eight built-in hashes (#63) unchanged — measured. Two calls
    inside: the Anod speckle no longer rides `paper_roughness` and its CC —
    MEDIUM §3 lists darkness and grain as the Anod substrate's own knobs,
    and a live roughness controller had no business in the glass; and QOL's
    `[ITERATE: expose fiber angle-drift amount?]` resolves as NO — the ±20°
    drift is the washi's identity, like the soak. All of it samples in
    screen space, so §4.5's screen-locked invariant holds by construction
    (the composite is the only reader). The desktop's "Substrate" section
    shows the medium's knobs: Sumi a tint (Cream / White / Toned as
    presets — 0.955/0.950/0.935 and 0.760/0.690/0.560 linear — or a
    picker), a paper preset (Smooth 0.25 roughness × 1.4 fibers, Washi 0.5 ×
    1.0, Coarse 0.8 × 0.7) over the roughness and fiber sliders; Anod glass
    darkness and phosphor grain. The presets are value tables the shells
    copy (44–46), not core data — there is nothing to render bitwise in
    them. INI keys `paper_tint` ("r g b"), `fiber_scale`, `anod_dark`,
    `anod_grain`; web params 34..39. Measured (`--palette-test`, the
    substrate check): the identity sheet's mean luminance 236.1 under the
    cream and 245.6 under the white tint; fibers ×2 keep the mean (236.1)
    and the texture (spread 10); the Anod glass 27.5 at 0.5 and 0.0 at
    darkness 1; the speckle's spread 4 → 0 at grain 0.

67. **Presets: one serializer, pure C, beside hostmpe.** QOL §3 as
    `presets/` — `sumi_presets`, a C11 static library with no dependency
    but libc and the core's header for its struct types: `sumi_preset_t`
    (the schema and the writer's `sumi_version`, a name, the input dialect,
    `sumi_params_t` whole, the custom `sumi_palette_t`, the CC map as
    (channel, cc, target) up to 64, the routed controls' values as (ctl,
    value) up to 32, the control strip's two latch-wheel CCs, the
    `sumi_layout_state_t` defaults), `sumi_preset_init` (zero, then the
    caller's defaults), `sumi_preset_write` (JSON, snprintf's size contract,
    always NUL-terminated), `sumi_preset_read` (a single-pass recursive
    descent over the text, no allocation, no DOM) and, in its own
    translation unit so a serializer-only consumer never links the core,
    `sumi_preset_apply` (params, input mode, palette, the CC map cleared and
    remapped; the controls, the strip and the layout state are the host's
    to send). THE SCHEMA RULE (`presets/SCHEMA.md`, the document the
    shells build from): a file carries `midi_sink_preset` (1) and the
    `sumi_version` that wrote it; a reader ignores keys it does not know,
    keeps its defaults for keys it lacks, drops an array's extra elements
    and keeps the rest of a short one; it refuses — leaving the target
    untouched — only text that is not a JSON object; the core clamps on
    apply, so a hand-edited file cannot wound the engine. Params are written
    by their C names from one field table (offsetof; the table is what the
    schema document is written from), floats as %.9g so they round-trip
    exactly, `\u00XX` for control characters, a `\u` beyond ASCII decoded
    as `?`. Headless (`preset_tests`, strict C11, 30 checks): every field
    survives the round trip byte for byte; a "newer" file with unknown keys
    at every level, a longer `paper_tint` and a shorter `burst_order_by_
    class` loads with what both understand; malformed, truncated, non-object
    and empty inputs are refused with the target intact; exponents,
    negatives, whitespace, an empty map, a clamped control value; the size
    contract with a buffer too small. The desktop: the session is written as
    `<config>/last_session.json` on every save and read BEFORE the INI at
    launch (the INI keeps the app's own flags — window, print folder, the
    hint — and, on the first launch after the upgrade, still supplies the
    whole legacy settings once); named presets are `<config>/presets/
    <name>.json`; a "Presets" section lists them with Load / Save as /
    Delete and exports or imports the same file by path (the harness has no
    file dialog). The harness's routed values (ripple, Chladni, spark,
    Chirikov CCs) travel as the `controls` list; a tablet's strip values
    will use the same list, its wheel assignments the `strip` object.
    QOL's `[ITERATE: preset-next from the strip?]` resolves as NOT in 2.0 —
    a performance feature for the instrument phase; the settings switch
    presets. `[ITERATE: schema versioning rule]` is the rule above.

68. **Prints at any size, and the ledger.** QOL §4. The field is
    resolution-independent by construction — every texel stores where its
    water came from — so a print at a target size is the composite over the
    SAME field at that size: `sumi_export_begin(inst, field, fw, fh, w, h,
    flags)` renders a field (a snapshot's data, or NULL for the field as it
    stands) into an RGBA8 target of w × h through the print pipeline,
    un-rippled like a dip, with the params and palette AS THEY STAND (the
    engine rebuilds the composite's visuals before the pass, so a shell may
    restore a dip's look and export in the same frame), and reads it back on
    the swapchain's one readback slot — asynchronous like the print, since a
    browser cannot block on a GPU map: `sumi_export_poll` returns idle, in
    flight, or done-and-copied; a dip, a field read and a second export are
    refused while one is in flight. `sumi_read_field` is public now (it was
    the §4.6 debug read): the field as it stands, RGBA16F, W × H × 8 bytes —
    what a shell keeps per dip. THE BOUND, stated honestly in the UI copy:
    detail below a field texel is interpolation; the true re-dip is replay
    (Phase 8). Measured (`--print-test`, 5/5): the export at the field's own
    size IS the dip's print — 0 of 1 048 576 bytes differ; the same field at
    4096 × 4096 box-averaged 8 × 8 lands within 0.88 counts of the 512
    print on average; a field kept before the dip re-exports after the dip
    bitwise as the live field did, at 512 and at 4k — the ledger's premise;
    a 9000-wide or a zero-height export and a second export in flight are
    refused. QOL's `[ITERATE: cap? 8k?]` resolves as 8192 a side
    (`SUMI_EXPORT_MAX_DIM`: a 268 MB target — the desktop's ceiling, a
    tablet shell will offer less). `[ITERATE: TIFF-16, demand-check first]`
    resolves as NOT NOW: no demand has been voiced, PNG is what the
    composite produces (RGBA8 through the print pipeline), and a 16-bit
    export would want a wider target — deferred, the check stays open.
    ANOD OVER ALPHA (`SUMI_EXPORT_ANOD_ALPHA`): the composite gains an
    `alpha_out` uniform (in the block's padding slot, 0 on every shipped
    path — the Sumi gate max diff 0 and the eight palette hashes unchanged);
    the Anod branch hands out its lit colour and coverage beside the opaque
    result, and an alpha export writes straight colour over alpha — the
    charge's glow and the water's grid, the glass at alpha 0 (measured: the
    resting band at 0 in 768 of 768 texels, every one of 41 169 charged
    texels lit; the plain export opaque everywhere). A Sumi export is always
    opaque: paper. THE LEDGER (desktop, `print_ledger.cpp`): the product's
    dip — the settings button — keeps the field as it stands (`sumi_read_
    field`, a GPU copy of a few milliseconds), the params and the palette,
    then dips; when the dip's print lands it becomes the newest entry's
    print (the "Save last print" button's source now) and a box-averaged
    thumbnail, drawn in the settings window as a GL texture; every entry
    re-exports at Screen / 2K / 4K / 8K wide (the height by its aspect,
    capped), Anod entries optionally over alpha, to a PNG written on a
    background thread with the size in its name. Memory-capped at eight
    entries or 384 MB (a 1440-high field is 29.5 MB), the oldest evicted
    first; the newest alone keeps its full print. The bench's 9 key dips
    raw, outside the ledger. `_sumi_read_field`, `_sumi_export_begin` and
    `_sumi_export_poll` are exported for the web (step 46). ROADMAP_5's
    step-43 DONE line "the ledger re-exports a dip at 4k" is the third
    check above.
