# DECISIONS_7 — Phase 8: Suzu, the synth (continuing Part VII)

Ambiguities resolved during Phase 8 from step 56 on (steps 56–59 of
`_work/ROADMAP_5.md`). Step 55b's entries #1–#7 were merged into
`docs/DECISIONS.md` as Part VII at the 55b fold (2026-09-29, the author's
call); this file CONTINUES that part — its numbering runs on from #8 — and
merges into it at the phase's end, as the earlier phases' files did. References
written as `DECISIONS_7 #n` mean Part VII, wherever the entry sits. The spec is
`specs/SYNTH_SPEC.md` (`SYNTH §n`); where an entry here and the spec conflict,
the entry is the record of what shipped — and the conflict is flagged to the
author, who owns the spec.

## Step 56 — Suzu cells (the Mac; Voxo only)

8. **Suzu is the synth's name, and the cell is the leapfrog with its orbit
   re-based on retune, its filter the Chamberlin core tuned for the damped
   ring, both in a 2× section; every element declares its class.** The
   author's working name from SYNTH_SPEC — 鈴, the small bell — stands
   unless they override it here (the roadmap points this confirmation at
   "`DECISIONS_7 #1`", which 55b's entries had taken; it is this entry).
   Suzu is a SOURCE beside the sampler (`voxo_set_source`, `VOXO_SOURCE_*`;
   Voxo 0.8.0, additive): the same callback, the same normalizer-fed voice
   model (a `SuzuVoice` inside each `Voice`), the same bus and master gain;
   switching ends every voice as an instrument swap does; the desktop's
   Sound section grew the source row (INI `sound_source`), the tablets
   consume it in their shell steps. THE CELL (`voxo/src/suzu.h`, the class
   table at its top): x −= ε·y; y += ε·x with the UPDATED x — the magic
   circle, det = 1 — at ε = 2 sin(π f / fs) so the pitch is exact (the
   oscillator measured within 0.000 cent across MIDI 21–108 at 44.1 k and
   48 k, the struck note through the ABI within 0.001); the strike a DRIVE
   that sets the orbit (x = A, y = εA/2, the point where x peaks on the
   invariant); the release a DECLARED conformal contraction per sub-step
   (a T60 in the patch — `contraction_for`); the output the cell's x
   through an attack ramp (a mixer stage, not a map on the cell) and the
   pressure's level. THE MPE CORRECTION, measured (SYNTH §2.1's
   `[ITERATE]`): the invariant of the update is x² + y² − ε·x·y, on which x
   peaks at A = sqrt(E/(1 − ε²/4)); a retune with the state kept changes E
   by −Δε·x·y, so the SHIPPED form re-bases the state onto the orbit of the
   same amplitude under the new ε (`Cell::retune`, one rsqrt per retuned
   sample, none while ε holds) — what the spec called the Gordon–Smith
   form is this recurrence (Gordon & Smith's own is the staggered update
   itself); the plain form is the lab's `retune_mode` 1, and 2 steps the
   pitch once per block without the ramp, SYNTH §2.1's feared case. THE
   NUMBERS: a scripted −48 → +48-semitone glide over 2 s ripples 0.097 dB
   re-based (the gate 0.5) — and 0.057 plain under Voxo's per-sample pitch
   ramp, 0.062 plain and stepped: at a sweep's rate Δε per step is tiny and
   x·y averages out, so the feared modulation does not appear; it appears
   at a JUMP — −48 → +48 in one block, eight phases of the orbit: 0.082 dB
   re-based (the decimator's roll-off alone) against 0.630 dB plain, the
   invariant's −Δε·x·y at whatever phase the jump lands. The re-base ships
   because it is exact at any rate of change; the ripple that remains is
   the 2× section's decimator, a 2-tap average whose gain is cos(θ/2) —
   −0.08 dB at C8 — the `[ITERATE: budget]` of §2.4 carried (a half-band
   FIR when the budget says). THE DRIFT TEST: the bare cell at A = 0.5 for
   ten minutes at 48 k — amplitude +0.0001 dB, frequency +0.000 cent; the
   negative control, the "simultaneous" update x′ = x − εy, y′ = y + εx
   (det 1 + ε²), spirals past the 0.1 dB bound within the first second and
   reaches +690 dB by the tenth minute — proven red. THE FILTER (§2.2's
   `[ITERATE]`, decided by the tuning test): the Chamberlin SVF at the
   oversampled rate, its (low, band) core the magic circle, q = 2 −
   1.9·resonance the declared dissipation, CC 74 scaling the cutoff by
   2^((t − 0.5)·6) as the sampler's default timbre map; the exact tuning
   f = 2 sin(π fc / fs) holds for the UNDAMPED core only — with damping the
   update is [[1, f], [−f, 1 − fq − f²]] and its ring sits sharp of fc by
   cos θ = (2 − fq − f²)/(2√(1 − fq)): measured 2.3–2.5 cents at C8 for
   Q = 50, over the 2-cent gate. So the coefficient is solved for the
   damped ringing frequency per block (`svf_f_for`: the same equation in
   its cancellation-free form through 4 sin²(θ/2) and fq/(1 + √(1 − fq)),
   in double, three passes of a quadratic — the first form drowned at 35
   Hz in float): the ring lands within 0.008 cent across MIDI 21–108 at
   both rates; the thematic form kept, the trapezoidal escape hatch
   unused; the classic form's fs/6 ceiling sits at fs/3 of the output
   rate, the cutoff clamped under it. THE SHEARS (§2.4): x += g(y), cubic
   (g·y³) or a triangle fold, in the 2× section, the gain clamped to 1 —
   the fold's ×4 waits on the budget. FTZ/DAZ (§2.4, §4): set on the
   rendering thread at its first block (`suzu.cpp`: MXCSR on x86, FPCR.FZ
   on arm64 — verified by a read-back, reported in `voxo_stats.ftz`), the
   64-voice decay-tail stress flat to the end (48 ms of render per second
   of tail, first and worst alike); the control with FTZ off shows no
   cliff on this Apple CPU — the cliff is the x86 boxes' to show, the gate
   is the flat line. HEADROOM: sixteen Suzu voices with the SVF and a
   shear, 128 frames at 48 k — 0.06 ms mean, 0.09 worst, 3 % of the
   callback. The suite is `tests/voxo_suzu_tests.cpp` (ctest, 10 suites
   now); the storm on the real output (`--voxo-source suzu --voxo-storm`)
   is the evidence's other half. Carried: the quadrature y as a stereo
   width, the drift's 10-minute figure re-run per box, the spec's
   companions (`SOUND_SPEC.md`, `MEDIUM_SPEC.md`) which are in git history
   and `specs/TO_PROJECT_SPEC.md` §10/§12 now (the spec itself moved to
   `specs/SYNTH_SPEC.md`, where the roadmap points). Evidence:
   `docs/evidence/step56/`.

9. **Every synth ships with its sound profile; the shears act on the
   normalized orbit and calibrate their own detune; the bass is flat and
   the ear is not.** The author, with step 56 on the desktop: "I like the
   sound; the bass notes have a strange amplitude — we will need a graph
   of the sound profile for every synth in the evidence and in the docs."
   THE PROFILE: the bench's `--voxo-profile <dir>` (with `--voxo-source`,
   `--voxo-preset`, `--voxo-suzu-shear`) strikes every MIDI note 21–108 at
   velocity 100 offline — held 0.5 s, released 0.3 s — and writes the run
   (`profile.wav`) with a row per note (`profile.csv`: the held window's
   peak and RMS in dBFS, the first three harmonics by a Goertzel at f, 2f,
   3f); `tools/sound_profile.py` draws the level across the keyboard, the
   harmonics per note and a spectrogram of the run. From here every source
   and instrument has its figure in the step's evidence and, drafted for
   step 63, in the docs (`site/drafts/suzu/`). THE BASS, measured: Suzu's
   level is flat to 0.09 dB from A0 to C8 (−24.27 → −24.36 dBFS peak), the
   sampler's sine to 0.00 — the signal does not vary; the ear and the
   speaker do, and a pure sine at 30–60 Hz at −24 dBFS sits where the
   equal-loudness curve and a laptop's cone give little back and unevenly.
   The remedy is timbre: the desktop's Sound section gained the patch's
   knobs (level, release, shear and its kind, cutoff, resonance — INI
   `suzu_*`) so the author can hear the cells with harmonics; the modal
   voice's partials (57) are the instrument's own answer. THE SHEARS,
   fixed twice on the way: as first written, x += g·y³ was a fixed kick
   per sub-step — at low pitch it dwarfed the tiny rotation per step and
   the orbit escaped (every note below C3 rendered NaN at g = 0.3, the
   profile's first run); a shear is a FORCE, so it scales with ε
   (`Cell::step_sheared`: x −= ε·(y + s(y)), the hardening sense —
   confined: the worst peak across the keyboard at full gain and velocity
   127 is 0.088 against the bare cell's 0.25, every sample finite, both
   shapes). Scaled, it detunes: the cubic hardens the spring by
   sqrt(1 + ¾g) to first order and the note went 110 cents sharp at full
   gain under that compensation, the fold — not sine-like — 555; so the
   cell CALIBRATES ITSELF at create (`shear_period_ratio`: the sheared cell
   at unit amplitude, 80 cycles at a reference ε per gain of a 17-point
   table per shape, its period against the bare cell's — cubic 1.081 /
   1.144 / 1.241 at g = ¼ / ½ / 1, the fold 1.088 / 1.244 / 1.651) and the
   voice scales ε by the interpolated ratio: C2, C4 and C7 within 0.19
   cent (cubic) and 0.02 (fold) at full gain — a gate now. And each shape
   acts on the orbit NORMALIZED by its amplitude (u = y/A, s = g·A·shape(u)):
   a fixed-scale shape either never reached its fold at A = 0.25 (the fold
   was a pure detune there) or swamped a soft orbit; normalized, a soft
   note is sheared as a loud one. What the shear buys, measured: a
   symplectic cubic is a gentle waveshaper — the third harmonic 33 dB under
   the fundamental at g = 0.3 and 23 dB under at g = 1 (−47 dBFS at C2),
   the fold 29 dB under at g = 1; the hard timbres are the kicked rotor's
   and the Duffing cell's (58). Odd shapes make odd harmonics (the suite's
   witness is the third). Evidence: `docs/evidence/step56/profile/`, the
   suite's log.

## Step 57 — The modal voice & the breath bow (the Mac; Voxo 0.9.0)

10. **The voice is a chain of cells coupled by a shared-potential kick,
    its partials placed by preset and KEPT on their ratios by an exact
    compensation solved at patch load; the load gate has two conditions,
    and the compensation's binds first.** THE LATTICE (SYNTH §2.5): N
    magic-circle cells (`voxo_suzu_params_t.modes`, 1–16, default 8), each
    at f₀·r_k with its declared contraction, and per sub-step (1) the
    coupling kick `y_k += (κ·ε₀²/ε_k)·(L·x)_k` from the PRE-update
    positions (L the chain's Laplacian: nearest neighbours — the
    `[ITERATE]` topology, resolved as the chain), (2) each cell's own
    drift and kick, (3) the declared damping. κ is relative to the LOWEST
    mode's stiffness (ε₀ its ε), not the nominal fundamental's, so the
    bell's hum (r = 0.5) keeps a spring of its own; the order is the
    standard drift–kick leapfrog with the full S = diag(ε_k²) + κε₀²L up to
    a half-step shift (two kicks in a row commute and merge), so the joint
    stability bound is exactly λ_max(S) < 4. A mode's CEILING is the output
    Nyquist (`MODE_CEILING` = ¼ of the oversampled rate — above it a mode
    folds back through the decimator): muted at the strike and the moment
    a glide crosses it; the chain is the alive prefix; the alive modes'
    weights normalize the mix, so the strike peaks at A on every note
    however many modes the ceiling left (the bar's profile still spreads
    4.8 dB across the keyboard — fewer partials, less held energy).
    The coupling's stiffness κ·ε₀² is CAPPED at C8's ε₀: a bend reaches
    MIDI 156 and the chain must not stiffen past what the gate saw.
    THE DETUNE, FOUND AND COMPENSATED: a spring between neighbours
    stiffens each mode it touches — the first lattice played C4's
    fundamental 149 cents sharp through the ABI at the default κ with the
    swirl's share (a harmonic chain's fundamental goes sharp by κ/4 to
    first order) — so κ would have been a pitch control before a timbre
    control. Not in the spec; decided here: at patch load the normal modes
    of M(κ) = diag(ρ_k²) + κL (ρ_k = r_k/r₀) are solved (a cyclic Jacobi in
    double, n ≤ 16, with its vectors) and each mode's own stiffness d_k is
    moved by Newton on the eigen-derived Jacobian (∂s_k/∂d_j = V_jk², a
    plain fixed point stalls near the edge) until the k-th normal mode
    lands on ρ_k²; the voice runs ε_k·c_k, c_k = sqrt(d_k)/ρ_k, from a
    129-point table over κ ∈ [0, 4] (c² stored, the root after the lerp;
    a 33-point grid missed by 3.6 cents where the curve bends toward the
    edge, 129 leaves 0.2), interpolated per block as the swirl moves, and
    cached on the fields that define the ratios (preset, modes, stiffness:
    2.4 ms to build at 8 modes, 13.4 at 16; a knob that keeps them, κ
    among them, 0.8–1.9 ms). The table is solved for the LINEAR ratios
    ρ_k = r_k/r₀, and a mode's ε bends below linear toward the ceiling
    (ε = 2 sin(θ/2): C8's third partial at 12.5 kHz sits 5 % under, and
    read 5 cents off with the swirl at full), so the table keeps J⁻¹ per
    κ point as well (264 KB per instance) and the voice corrects the
    solved stiffness to first order at its pitch, δd = J⁻¹·(ρ'² − ρ²) with
    ρ'_k = ε_k/ε₀ the true ratios — n² multiply-adds per voice per block,
    the residual second order in the bend. Muted modes are WALLS, not
    kicked and held at zero, so the top alive partial keeps both springs
    (the first cut ended the chain at the first muted mode and the top
    partial lost a neighbour's stiffness). Measured: the lowest three
    partials of the harmonic, bell and plucked presets at C2, C4, C6 and
    C8 within 0.08 cent at κ = 0.05 and with the swirl at full; C8's five
    alive partials within 0.00 cent but the fifth, at 20.9 kHz against the
    wall, +1.0; at the compensation's far end (κ = 1.483) C4's fundamental
    within 0.76 cent; the solve's own residual 5e-8. Mode SPLITTING stays what
    §2.5 names: two cells at the same pitch split into f₊ − f₋ of the joint
    map — measured against the analytic split within 0.01 % across κ =
    0.0125 … 0.5 (2.7 … 91 Hz at A3) and charted once
    (`tools/mode_splitting_chart.py`); with placed ratios the coupling's
    exchange of energy between partials is the feature, their pitches are
    the preset's. THE LOAD GATE, two conditions in `voxo_set_suzu_params`
    at κ_patch + the swirl's 0.5: (i) the compensation is feasible — every
    d_k keeps at least 1 % of the mode's own stiffness (else "a mode's own
    stiffness would be gone"); (ii) λ_max(S) of the alive, compensated
    chain — as the voice runs it, corrected per pitch, the walls in — stays
    under 4 at every semitone of MIDI 21–156 (power iteration, the sweep
    under a millisecond). A rejected patch returns false with
    the message on the log callback (the desktop prints it) and the live
    patch stands; `voxo_suzu_coupling_bound` bisects the PATCH's bound
    (the swirl's share reserved). FOUND: for every shipped preset within
    κ ≤ 4 the compensation's condition binds first — the harmonic 16-mode
    chain's bound is κ = 1.036 (total 1.536, where the SECOND partial's
    own spring is gone) while its λ_max never leaves ~1.96 — so the
    sampling bound's red control lives on the primitive: two cells at
    23.04 kHz / 96 k, κ* = 0.567 — 5 % over (λ_max 4.106) blows up at
    once, 5 % under (3.894) stays bounded; the compensation's red control
    through the ABI: the 1.05× patch with `lattice_gate = 0` (the lab's
    bypass, the table frozen at its last feasible row) plays C4's
    fundamental 21 cents off — the partials cannot be placed. THE PRESETS
    (`suzu::modal_table`): harmonic string (r = m·sqrt(1 + B·m²), w = 1/m),
    stiff bar ((2m+1)²/9: 1, 2.78, 5.44, 9 …, w = 1/m), bell (0.5, 1, 1.2,
    1.5, 2, 2.5, 2.667, 3 …, cast weights, the prime loudest), glass (1,
    2.32, 4.25, 6.63 …, w = 1/m²) and the plucked string — Karplus–Strong
    in modal form, w = sin(mπp)/m² (at p = ½ the even partials are not
    fed: gated). The decays: γ_k = α + β·(r_k² − 1) with α = ln 1000 /
    `decay_s` — so the fundamental's T60 IS the declared `decay_s` and an
    inharmonic preset's highs die by their frequency; the spec writes
    α + βk² (FLAG: the shipped form keeps the fundamental exact and reads
    r, not k); `decay_s` = 0 declares no decay (the lab's ledger). The
    ENERGY LEDGER: the bell, 8 modes, κ = 0.3, every decay zero, struck
    once, ten minutes through the ABI — RMS second 1 → second 600: −0.0026
    dB (the envelope's spread 0.034 dB, the inharmonic beats); decays
    restored (2 s, β 0.3), every mode's T60 within 0.1 % of the declared,
    at κ = 0 and unchanged at κ = 0.05. The swirl (0xA0) adds up to 0.5 to
    κ per block through the 20 ms one-pole (the `[ITERATE]` binding, built
    as the spec's thematic one; the author signs by ear). Patches ride the
    QoL preset file as a `suzu` object (`presets/SCHEMA.md`: the source and
    `voxo_suzu_params_t` field for field, kept when absent; the desktop
    writes and reads it). THE COST: the first cut computed a sine per mode
    per frame and 64 released voices cost 513 ms per second of audio (ten
    times step 56's); the retune now runs only when the pitch moves (the
    block's first frame, then each frame of a glide) — 60 ms/s for the
    64-voice tail, sixteen voices 3.5 % of the callback (0.09 ms worst of
    2.67). Voxo 0.9.0: `voice_kind`, `modal_preset`, `modes`, `coupling`,
    `decay_s`, `decay_bright`, `stiffness`, `pluck`, `bow_onset_s`,
    `bow_position`, `breath_cc`, `lattice_gate`; `voxo_set_suzu_params`
    returns bool; `voxo_suzu_coupling_bound`; `voxo_stats_t.suzu_modes`.
    The storm on the MacBook's speakers, twice: 0 XRuns, 0 dropped, render
    max 0.83 ms, 96 fps. The profiles of the five presets and the bowed
    voice are the evidence's and the drafts'.

11. **The bow is an energy servo per mode toward the breath's target,
    bowed only while breath is held — and it can hold only the partials
    whose declared decay is slower than its own onset.** SYNTH §2.6 as
    built: per sub-step each bowed mode's state is scaled by
    1 + g·u, u = clamp((E_t − E)/E_t, −4, +1), g = 1/(τ·rate) with τ =
    `bow_onset_s` (0.15 s default; 0 removes the bow) — anti-damped under
    the target, damped over it, the servo's own rate bounded by g per
    step: a limit cycle by construction. E_t per mode is (A·b_k)² with
    A = level·(0.35 + 0.65·breath)·breath (the breath twice: a soft breath
    sings softly) and b_k = |sin(kπ·pos)| the bow's profile
    (`bow_position`: 0 the fundamental alone, 1 every mode evenly — the
    profile-from-CC `[ITERATE]` stays fixed per patch). Breath is the
    patch's CC (`breath_cc`, default 2) and CC 11 as its alias, per zone
    through the normalizer; a voice is bowed only while HELD with breath
    > 0, so no breath means no bow (silence is gated: the declared decays
    alone) and a released voice ends by amplitude as any other. A bowable
    mode at rest (E < 1e-10) is seeded at −20 dB of its target — the bow's
    first grip on a resting string, a declared DRIVE. THE REACH, found:
    the servo's injection tops at 1/τ (u ≤ 1), so a mode whose declared
    decay rate γ_k exceeds it cannot be held; the first cut re-seeded such
    modes each time they died (a −40 dB tick every 0.3 s on the eighth
    partial) — now they are not bowed at all: they ring from the strike and
    die. At the defaults (τ 0.15, decay 3 s, β 0.3) the first three
    partials of the harmonic string sing. The steady state sits UNDER the
    target by the decay's share, E = E_t·(1 − γτ): measured A_ss 0.2022
    against 0.2023 predicted under a 2 s declared decay at τ = 0.1
    (`[ITERATE]`: the target corrected for the decay; μ from the breath's
    attack rate, the spec's). THE GATES (§5), on the cell: from silence
    (the seed) the orbit settles within 5 % in 4.9 τ, from 2× in 1.7 τ,
    and the two orbits agree to 0.00 %; zero breath — the servo off, the
    declared 2 s decay alone — falls monotonically to −66 dB in 2.2 s; over
    a second of steady state the injected and the extracted energy (the
    decay's and the bow's slips) balance within 0.000 % (the rotation
    itself injects nothing: symplectic); the RED control — the servo that
    gives and never takes, from 2× — is at 1000× the target's energy in
    0.43 s and 8e11× at 2 s. Through the ABI: breath 80/127 on a
    velocity-1 note sings at −39.4 dBFS, stable to 0.05 dB over the last
    second; the same tone from velocity 127 within 0.3 dB; the breath
    withdrawn, the held note is 84 dB down after 3 s. The bowed profile
    (harmonic string, breath 0.63 held under velocity-100 strikes) is a
    flat singing line across the keyboard (peak spread 1 dB; the second
    partial level with the first at bow position 0.3). The Brisa's singing
    tone on the actual hardware is the author's judgement.

12. **The binding table as built (SYNTH §3), for the author to sign by
    ear.** Through the normalizer, every dimension one consumer:

    | dimension (MPE) | Suzu's consumer |
    |---|---|
    | strike (velocity) | the kick across the modes by the preset's profile — the plucked string's `pluck` position in closed form, sin(kπp)/k² |
    | press (channel pressure) | the output gain 0.35 + 0.65·p (step 56's stage; §3's "sustained small drive into damped modes" is NOT built — `[ITERATE]`, the bow being the press consumer where there is breath) |
    | breath (CC 2, the patch's `breath_cc`; CC 11 as alias) | the bow's E_target per mode (#11) |
    | glide (pitch bend) | every mode's ε retuned per frame, the orbits re-based, each partial on its ratio (the cell's amplitude refreshed from its state first: the coupling moved energy the cell's own `amp` did not see) |
    | slide (CC 74) | the SVF's cutoff, ±3 octaves about the patch's |
    | swirl (0xA0, poly pressure) | κ += 0.5·swirl per block (#10) |
    | lift (note-off) | the release's declared contraction, the tighter of the patch's release and the mode's own |

    Not built, flagged: the strike-position profile from the layout's
    cell (the `[ITERATE]` of §3 — a struck bar) and the bow profile from a
    CC. The desktop's Sound section carries every knob when the source is
    Suzu and the voice the lattice (INI `suzu_*`); the roadmap's "patch
    names" are the author's — the five presets are named by their physics.

## Step 58 — Strings & chaos (the Mac; Voxo 0.10.0)

13. **The Verlet chain tunes itself under the CFL bound by shedding nodes,
    and the bound is a gate with a red control.** SYNTH §2.8 as built
    (`suzu::VerletString`): M interior nodes between fixed ends, per
    sub-step every acceleration from the pre-update u, then v = v·d + k·Δu,
    u += v (symplectic Euler, a conformal per-node damping d from the
    patch's `string_decay_s` as the fundamental's T60; 0 = none). In
    dimensionless units the one number is s² = k·dt², and the m-th mode
    rotates by 2·asin(s·sin(mπ/(2(M+1)))) per step — the 1–2–1 Laplacian's
    eigenvalues, the cell's ε² = λ — so THE CFL BOUND s ≤ 1 keeps the top
    mode under Nyquist and past it the explicit scheme is unstable. Tuning
    couples M, k and dt: a note f₀ needs s = sin(πf₀/rate)/sin(π/(2(M+1)))
    ≤ 1, so the chain is REDUCED per note to floor(rate/(2f₀) − 1) nodes
    (48 hold to C6; 44 at C6, 21 at C7, 10 at C8 on the 96 kHz section;
    above rate/6 no chain is representable and the note is silent) and s
    re-derived — the pitch is exact by construction: the sweep MIDI 21–108
    through the ABI reads 0.000 cent. The gate: `voxo_set_suzu_params`
    rejects a forced k·dt² over 1 (the lab's `string_cfl`; the derived one
    never exceeds it) with its message; the red control — 1.05 with
    `cfl_gate` bypassed — goes non-finite within a second at C4 (25 samples
    on the primitive), 0.95 rings bounded. The pluck is a triangle ADDED to
    the state (re-plucking a ringing string, §2.8's ask), released from
    rest; the pickup reads a node at `pickup` (0.25). A glide re-derives s
    per frame and saturates at the bound (the pitch stops rising where the
    chain cannot follow) — the chain has no re-base, so a glide ripples as
    a retensioned string does (not gated; the cell's glide gate is the
    cell's). The cost is honest: ten strings at 80 nodes with the SVF on
    take 20.8 % of the callback (0.55 ms worst of 2.67) — `[ITERATE]` the
    structure-of-arrays SIMD of §4 when the budget asks. The chain's spectrum
    is the discrete string's (its highs compress toward the top mode): the
    body the modal preset's spectrum lacks.

14. **The hybrid string's junction is a scattering junction on the cell's
    synchronized velocity, discretized by the midpoint rule so its balance
    is exact; the bridge's reflection phase is solved in closed form and
    folded into the delay; the passive bound is a point.** SYNTH §2.9 as
    built (`suzu::HybridString`): a ring of N samples read N behind the
    write (the history stays, so a glide's integer steps read real samples)
    plus a first-order Thiran allpass for the fraction (|H| = 1), a
    one-zero loss y = (1 − a)x + a·x[n−1] with a = `loop_loss`/2 (the KS
    averager as a declared conformal loss, |H| ≤ 1) and a round-trip factor
    from `string_decay_s` (a T60 at the note's period), into a BRIDGE of
    1–3 magic-circle cells at `bridge_hz` × {1, 1.618, 2.618} with their
    declared decays. THE JUNCTION, three forms on the way: the spec's
    explicit "kick the bridge by c·s, reflect −s + c_back·v" pumped energy
    through its c·F² term — the first probe went to infinity at any
    coupling; the physical scattering form (the incident wave's force on
    the bridge is 2s − v_b, the −v_b share the bridge's own motion radiating
    BACK into the string, the reflection v_b − s) with the midpoint rule on
    y also grew, because the staggered cell's y sits half a step ahead of
    x and a kick on it changes the invariant by −ε·x·δ beyond the kinetic
    energy; on the synchronized velocity ỹ = y − ε·x/2 the balance holds
    EXACTLY per sample: F = (2s − Σỹ)/(1 + Σc/2), each mode kicked by
    c_m·F, the reflection s − g·v̄ with v̄ the midpoint velocity (the nut's
    inversion folded in on the way back), g = 1 the conserving junction.
    Measured: every declared damping zeroed, the loop holds 60 s within
    0.004 dB at any coupling, and the ten-minute soak through the ABI at A2
    reads −0.018 dB (the spread 0.037: the allpass's one sample of state).
    THE BRIDGE'S SCALE: c is the string's impedance over the bridge's mass
    per sample, and a real bridge is HEAVY — at c = 0.08 the reflection's
    phase swung the loop by a semitone at every note (−120 cents saturating
    the search); shipped c = 0.002 (range 0..0.02), at which the pull is a
    few cents far from the bridge's resonance and up to π at it (the wolf).
    THE PHASE, compensated: the junction is linear, so its per-sample map
    z' = A·z + B·s, r = Cᵣ·z + D·s is read off by pushing unit vectors
    through one sample and its response H(ω) = Cᵣ(e^{iω}I − A)⁻¹B + D is a
    2·nb complex solve at tune time; the loop resonates where its whole
    phase is 2π, so arg H/ω is folded into the fractional delay with the
    one-zero's phase delay — the fundamental stays on the note (the sweep
    MIDI 21–108 within 0.144 cent; 0.07 on the primitive at c = 0.002,
    0.58 at 0.008) while the partials keep the bridge's pull: the body. The
    hybrid retunes once per block (a glide steps at most a few cents per
    block). THE GATE, twice (§5): the load-time probe — the closed loop at
    C6 for 300 ms (three hundred round trips), every declared damping
    zeroed, plucked; the energy's peak over the start: 1.013 at g = 1 (the
    allpass's wobble), 1.07 at 1.005, 1.15 at 1.01, 3.7 at 1.05 — rejects
    over 1.03 with its message; `voxo_suzu_passive_bound` searches above 1
    and reads 1.0016; and the soak. The red control: g = 1.05 with the
    probe bypassed goes non-finite in 31 s at A2. FOUND: the conserving
    junction is a passive POINT, not a half-line — under 1 the bridge still
    takes the full force 2s − v̄ while the string sees less of v̄ come back,
    the cross term (1 − g)·v̄·(2s − (1 + g)v̄) is sign-indefinite, and the
    probe admits 0.95 only because it does not grow at C6 (the spec's
    "coupling gain 1.05× the passive bound" reads, as built, "5 % over the
    conserving point"); a lossy bridge would need a resistor the junction
    has none of — the declared decays are the bridge cells' own. The output
    is the string at the pickup (both directions, half) plus the bridge's
    velocity in the string's energy units (half): the body speaks — the
    profile shows its response, up to 11 dB louder around the bridge's two
    modes with the wolves as dips exactly at 220 and 356 Hz, the modes
    ringing across every note. The `[ITERATE]` wave-digital escape hatch
    stays unused: no patch failed the probe on its merits.

15. **The Duffing cell is the cubic on the position; the rotor is the
    standard map with its momentum as the pitch on the torus.** SYNTH §2.10:
    `suzu::Duffing` puts the hardening spring in the kick, y += ε·(x + βx³)
    — a shear of y by a function of x, symplectic across any swing (the
    spec writes "g(y) = −βy³·dt", a shear of y by itself, which is not a
    map of det 1 — FLAG; step 56's cubic shear on the drift is the same cell
    with the coordinates swapped, normalized and detune-calibrated, which is
    exactly what the Duffing cell must NOT be: the clang IS the detune). The
    settled pitch is exact with no compensation (the cubic vanishes with the
    orbit): C4 at velocity 127 with β 8 clangs +258 cents sharp and settles
    to +0.00 by 2.5 s under a 2 s declared decay (`chart/duffing_clang.png`,
    the pitch against time over the amplitude's straight decibel line). The
    drive: a sinusoid at `drive_ratio` × the note, its amplitude
    `drive` × the press (channel pressure), added as a force in the kick;
    swept 0 → 1 over 24 s on A3 (β 8, a 0.3 s decay) the spectrum holds one
    partial and its harmonics until 0.87 and there bifurcates into a comb of
    new partials (`chart/duffing_drive.png`: the second chaos voice's order
    → chaos, as a period-multiplying window rather than broadband at these
    settings — `[ITERATE]` the drive ratio and β for the Ueda-like storm).
    `suzu::Rotor` (§2.3): the cell carries the angle; once per NOMINAL
    cycle — a fixed clock at the note's period, Chirikov's kick period — the
    momentum takes p += K·sin θ with sin θ read from the cell's quadrature,
    p is wrapped to (−π, π] (the map on its torus) and the cell is re-based
    onto the pitch f₀·(1 + p/2π): the momentum IS the pitch, within the
    octave about the note, and the kick is a re-based retune (phase- and
    amplitude-continuous), never a jump. K = `rotor_k` + (2.5 −
    `rotor_k`)·wheel (CC 1, per zone), delta-smoothed by the 20 ms one-pole
    per block; the chaotic modulator may add to it. Gated: K = 0 is the
    pure tone (0.000 cent); K = 0.3 undamped for ten minutes holds the
    orbit's peak within 0.0000 dB and |p| ≤ 1.108 (the island's libration,
    2√K); past K_c the fundamental's share of the power falls (97 % → 2 % →
    20 % → 52 % → 36 % at K 0 / 0.47 / 0.98 / 1.5 / 2.5 — the small-K figure
    is the wide slow libration of the island, ±2√K in p: ±3 semitones at K
    0.3, the map's own scale). The K sweep 0 → 2.5 over 24 s on A3 is
    `chart/rotor_sweep.png`: the tone, its sidebands, the band widening
    toward the octave about the note — the visual Chirikov's sibling, one
    theorem, two senses. FLAG for the author's ear: whether K's musical
    range wants a gentler mapping of the wheel (the island already swings
    three semitones at 0.3).

16. **The chaotic modulator is a double pendulum under RK4 with its energy
    projected, one per patch.** SYNTH §2.10 says a leapfrog double pendulum;
    the double pendulum's Hamiltonian is not separable, so a leapfrog
    (velocity Verlet) is not symplectic for it — measured over ten minutes
    at dt 0.02: E drifted from −2.0 to +1.4 and a hard kick went to NaN.
    Shipped (`suzu::DoublePendulum`): classical RK4 in double at a fixed
    sub-step of 0.005 (RK4 alone holds 6.0000 → 5.9998 over ten minutes),
    with the kinetic energy PROJECTED onto the trigger's value after each
    control step — the velocities rescaled — a declared correction that
    measures a no-op (within 9e-8 of 1) and bounds by construction; the
    class-table row says so (FLAG: the spec's word). The energy is set at
    the trigger from the velocity (a kick from rest at the bottom of
    0.5 + 2.5·vel, E from −2.75 to 6: below the flip energy it swings,
    above it tumbles), the output sin θ₂ (bounded in [−1, 1]) is smoothed
    20 ms per block, a unit of pendulum time is 50 ms / `mod_rate`, and the
    value goes to one smoothed target — the cutoff (±3 octaves × depth),
    the coupling (+0.5 × depth), the rotor's K (+1 × depth) or the drive —
    the `[ITERATE]` mod-matrix corner left as one row. One pendulum per
    patch (the instance's), re-energized by every strike; per voice is the
    `[ITERATE]`.

17. **The sampler and Suzu can sound together — the layered source — and the
    combined stress holds.** The roadmap's DONE asks for the Osmose storm +
    a heavy sampler preset + ten synth voices at once; Voxo's source was
    exclusive (step 56: the switch ends every voice), so `VOXO_SOURCE_LAYERED`
    (2) is added: every note strikes a sampler voice AND a Suzu body in the
    same Voice, both release together, the voice ends when both have, the
    mixer sums them (SYNTH §4: the synth and the sampler share the mixer and
    the bus); switching to or from it still ends every voice. The desktop's
    Source row gains "Both"; the preset file's `source` carries 2. Gated: one
    note on the layered source is one voice whose RMS exceeds either body's
    alone, and it ends when both have. THE COMBINED STRESS on the MacBook's
    speakers (`--voxo-storm 8 --voxo-source layered --voxo-preset <the
    author's Bösendorfer 280VC library> --voxo-suzu-voice 2`): the
    fifteen-channel MPE storm with the sampler's heavy library under the
    Verlet chain (48 nodes), twice — 0 XRuns, 0 dropped, render max 0.80 /
    0.77 ms of 2.67, 96.1 / 94.5 fps, twelve layered voices at the end; and
    under the hybrid string — 0 XRuns, render max 0.39 ms, 94.7 fps. The
    boxes' re-run of the suite (Windows, Linux) is the author's fan-out, as
    at 55b. The bench's `--voxo-suzu-voice <kind>` picks Suzu's voice for a
    run (the storm's and the profile's) and `--voxo-chart <dir>` writes the
    chaos charts' material (`tools/chaos_chart.py` draws it; the chart's
    scripted run renders one settled frame first — the core's Metal shutdown
    waits on a frame semaphore only a committed frame arms, and the first
    chart run hung there).

## Step 58b — The bore & the jet: the flute (the Mac; Voxo 0.11.0)

18. **The bore is the chain in acoustic variables on a staggered grid, its
    pitch its length, its ends declared ports whose radiation rises with
    frequency and whose reactance is an end correction the tuning counts.**
    SYNTH §2.11 as built (`suzu::Bore`): Webster's system on the Yee grid —
    p at the integer nodes, u (the volume velocity) at the half nodes, both
    in the units where the bore's characteristic impedance at its mouth is
    1, S(x) a per-node weight normalized at the mouth — u −= λ·S·Δp then
    p −= λ·Δu/S, symplectic Euler on the wave equation; profiles cylinder,
    cone S ∝ (x₀ + x)² (closed at the truncated apex) and the Bessel flare
    S ∝ (1 − x/x₁)^{−γ} (58c's). THE CFL: λ²·μ_max(L_S) < 4 with μ_max the
    weighted Laplacian's top eigenvalue by power iteration (3.996 on the
    uniform grid, 4.015 on the cone's apex cells) — the bound λ ≤ 2/√μ_max,
    derived, never trusted; the lab's `bore_cfl` forces a multiple of it and
    the gate rejects over 1 with its message; the red control (1.05×) goes
    non-finite in 57 sub-steps on the primitive and within a second through
    the ABI, 0.95× rings bounded. PITCH IS BORE LENGTH: n = ⌊λ_max·rate/(2f₀)
    − ends⌋ cells (open–open or the cone; a quarter-wave closed–open takes
    half) and λ = 2f₀(n + ends)/rate ≤ λ_max absorbs the fraction — the
    KS-delay tuning move — so a note's dispersion stays under a cell's
    worth; above the cell cap λ falls instead (A0 at 128 cells runs at λ
    0.07, stable and near enough to harmonic for m ≪ n). THE SERIES,
    measured on the primitive at A4: the closed–open cylinder's first four
    peaks on the odd series within 0.0 cent, the open–open cylinder's on the
    integers within 0.0, the cone's (apex 5 % of its length, counted to the
    apex — without it the whole series read 84 cents flat, exactly 1/1.05)
    on the integers within 8.7 cent, stretched +0.7 / +2.5 / +5.1 / +8.7 as
    a truncated cone is (the mouthpiece's compensation is 58c's to find).
    THE ENERGY, in the staggered form the leapfrog conserves exactly —
    ½Σ S·p_n·p_{n+1} + ½Σ u²/S — holds the closed lossless bore within
    0.0003 dB for ten minutes (the symmetric p² form wobbled a quarter of a
    decibel at λ ≈ 1, and a first staggered form paired the wrong levels).
    THE ENDS are declared ports: closed is the mirror (a half cell), open is
    p = ∓z·(u − lp(u)) with lp a one-pole at `bore_corner_hz` — the
    radiation resistance of an unflanged pipe grows as (ka)², and this
    port's response z·iω/(ω_c + iω) has a non-negative real part at every
    frequency, so it only absorbs: the fundamental sees little of z, the
    upper modes all of it (with z 0.3 and the corner at 1500 Hz the first
    mode's T60 at A4 is 0.19 s, the third's 0.077) — the bore's own
    selectivity between its registers, without which the jet's flat gain
    locked the sixth mode. FOUND: the port's reactance is an inertance
    z·ω_c/(ω_c² + ω²), i.e. λ·that many cells of extra bore — 2.8 per end at
    A4 — and the flute read 81 cents flat until the tuning counted it
    (`end_correction`): the pinned-end theory is not the bore's. A glide
    changes the cells and λ per block; the wave keeps circulating (the cells
    past the new end are dropped). Cost: cells × rate × voices, as the chain.

19. **The jet is a bandpass amplifier whose centre rises with the breath;
    its drive is the labium's dipole, power-limited by the mouth per sample;
    the flute overblows by itself and flattens when blown softly.** SYNTH
    §2.13 as built (`suzu::Jet`), the loop found in five probes: (1) the
    acoustic DISPLACEMENT at the flue — a leaky integral of the bore's
    velocity there, DC-blocked at 10 Hz — not the velocity, carries the jet:
    with the velocity the phase condition landed a quarter period off; (2)
    the jet's travel time τ = jet_tau·T·√(P_ref/P_mouth) — d/(αU₀) with the
    embouchure following the note (the delay in periods of the note at the
    reference breath, a v1 choice: the physical jet with a fixed distance,
    where the player blows harder for high notes, is the `[ITERATE]`),
    Hermite-interpolated in a 4096-sample ring (A0's period and a half:
    a 1024 ring left the low half of the keyboard silent); (3) the
    RECEPTIVITY BAND: the sinuous instability grows fastest at one Strouhal
    number f·d/U₀ = f·τ, so a second-order bandpass (the Chamberlin core,
    `jet_q`) on the displacement centred at ½/τ amplifies best around the
    fundamental at the reference breath and around the octave at four times
    it — the register jump IS the band crossing the modes, and without it a
    full-period delay locked every frequency at once into a broadband
    saturation (the very soft end); (4) the labium's tanh partition Q_in =
    (Q₀/2)(1 − tanh(η − y₀)), Q₀ = jet_area·U₀, η = −G·(band)(t − τ) + the
    stochastic vector σ·U₀·white (chiff, filtered by the bore), an inward
    displacement carrying the jet in (more flow); (5) THE DRIVE: a flow
    alone does no work at a pinned open end, so the source is the jet
    drive's dipole across the labium, a pressure port at node 0 ∝ dQ_in/dt —
    p_src = −jet_drive·(Q − Q_prev) per sub-step, O(1): a first form carried
    an arbitrary ×T/2π that let the acoustic injection exceed the mouth's
    work sixfold (the ledger caught it). The jet's gain G = e^{μd} at A4
    (`jet_gain` 560) follows the note as f² because the bore's radiation
    loss does (the embouchure follows the note again). THE POWER-LIMITED
    PORT: the dipole never does more work on the bore in a sample than the
    mouth does on the jet, P_mouth·Q_in — a declared limiter (it acts on
    0.5 % of the samples of the test's phrase), so §1's self-excited row for
    the winds holds by arithmetic: over a 4 s phrase (a swell, a note change
    A4 → C5, a release) the stored energy never exceeds 0.06 % of
    ∫P_mouth·Q_in. THE BREATH: P_mouth = P_ref·r with r from 1/√range to
    √range across the breath about `breath_ref` (0.44 → the reference, the
    note in tune; `breath_range` 12); no breath, no tone. THE EMBOUCHURE
    FOLLOWS THE NOTE, three laws in v1: the jet's delay in periods (above),
    its gain rising as f/440 (the bore's losses rise with the pitch) and its
    area — the flow — falling as √(440/f): the drive is a derivative, so its
    saturated amplitude climbs 6 dB an octave otherwise (C2 read 27 dB
    under A4; with the laws C2–C7 sit within 8 dB, the sub-contra octave
    below the flute's range quieter still); the physical jet with one
    distance and one width, the player blowing harder for height, is the
    `[ITERATE]`. THE WALL: the radiation alone left the low notes nearly
    lossless and seconds slow to speak, so the bore carries a declared
    per-node damping, `bore_wall_s` (a T60 of 1 s) — the class table's
    "string's per-node damping" row in acoustic clothes. THE OUTPUT is the
    mouth end's volume velocity (the standing wave's amplitude there; the
    port's own pressure falls as f² toward the bass and read 60 dB down at
    C2), scaled to sit near the cell's −24 dBFS at A4; the labium's offset
    y₀ (0.3 jet widths) is the partition's asymmetry, where the even
    harmonics come from (the profile still reads the odd ones 20 dB ahead —
    the jet's asymmetry is an `[ITERATE]` for the ear). THE PRESS BLOWS: the
    author, at the desk, heard nothing — the winds and the bow sound only
    under breath (CC 2 or 11), and the controller in hand had none. So
    `press_blows` (default on; the desktop's "the press blows" switch): the
    mouth is the larger of the channel's breath and the voice's pressure,
    for the flute and the bow alike — an Osmose or an aftertouch keyboard
    plays them; a pure breath player switches it off. FLAG: the press then
    has two consumers on those voices (the gain stage and the mouth), SYNTH
    §3's one-consumer rule bent where the alternative was silence; a
    keyboard without pressure or breath still has nothing to blow with (a
    velocity floor is the `[ITERATE]`). Gated: A4 under channel pressure
    alone sounds; with the switch off it stays silent. MEASURED, the
    delay scan at A4: the first register sustains for τ between 0.35 and 0.6
    periods — +39 cents at 0.3, +3 at 0.5, −37 at 0.7 — the octave below
    0.3, silence past 0.8 (the second hydrodynamic mode a whisper near 1.2);
    the breath ramp through the ABI, A4, 0 → 127 over 12 s, no other
    change: −283, −147, −59, −23, −3, +9, +21, +37, +77, +157, +265 cents
    second by second, then +1193 — THE OVERBLOW, the octave within 7 cents,
    autonomous; soft blowing flattens 32 cents at breath 32/127 against
    53/127 — the τ-phase lag, free (`chart/flute_ramp.png`, the
    spectacle). The bore's own arithmetic with centred port products does
    not close (injected 0.049 against radiated 0.057 on the phrase): the
    ports' discrete power at the half step is an `[ITERATE]`; the exact
    conservation is the closed bore's gate. The Brisa's breathing flute is
    the author's judgement.
