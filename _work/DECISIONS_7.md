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

## Step 58c — The reed & the lips: the saxophone and the trumpet (the Mac; Voxo 0.12.0)

20. **The valve is one kick–drift cell with a pressure force and a stop; its
    junction with the bore is solved implicitly, the Bernoulli quadratic
    against the bore's one-step impedance, and both the valve's swept volume
    and its energy are counted — the mouth-power ledger holds by arithmetic,
    and the naive junction is the red control.** SYNTH §2.12 as built
    (`suzu::Valve`): a 1-DOF mass–spring, v −= ω²y, v += ±g·Δp, y += v,
    v ×= damp (Q declared), inward-striking (the reed, closing with
    Δp = P_mouth − p[0]) or outward (the lips, opening with it); the opening
    h = h₀ + y clamped at 0 and the aperture A·h; the force gain g =
    h₀ω²/P_close so the rest opening closes at the declared pressure. THE
    JUNCTION: the flow through the aperture is Bernoulli, q = A·h·√|Δp|·sgn
    — but Δp is the pressure AFTER the flow has entered the end node, and
    the bore's closed-end update (58b's mirror, p[0] −= 2λ(u₀ − q_b)/S₀)
    makes that a quadratic: q = A h √(P_mouth − p_ahead − Z q) with p_ahead
    the end pressure the step would produce without the flow and Z = 2λ/S₀
    its one-step impedance (`end_pressure_ahead`, `end_impedance`,
    `flow_implicit` — the closed form). The naive form (q from the previous
    step's Δp) is the spec's "explicit coupling that can blow up though the
    continuous system is passive": on the primitives it goes non-finite
    within the test's phrase (a swell, a note change, a release) and stays
    finite on a steady blow at every pressure — its instability is kicked
    by transients — so the load-time probe blows twice (soft, 0.6 P_ref,
    where the reed's local gain is highest; hard, 2.5) with the bore's
    declared losses zeroed AND a retune a fourth up halfway (the state
    kept), and asserts E_bore + E_valve ≤ 1.05·∫P_mouth·Q after 30 ms
    (the two staggered energy forms wobble against a per-step work sum; the
    reed beating hard reads 1.02 with the losses at zero). The naive form
    reads 1e300× and is rejected with the message; the ABI's red control
    (the gate bypassed, a bend of four semitones) goes non-finite within the
    second. FOUND, three things the ledger caught before the ear could: (1)
    the valve's displacement moves air — the swept volume ±S_r·v into the
    bore in the force's direction (the reed closing pushes air INTO the
    chamber: −S_r·v; a first form carried the wrong sign and the ledger
    read the valve doing work on the mouth); its mass is S_r/g so the
    valve's energy is in the flow's units; (2) the valve's energy in the
    kick–drift's own conserved form, (S_r/g)·½(v² + ω²·y·y_prev) — the
    symmetric ½ω²y² wobbles at the valve's frequency and tripped the probe;
    (3) an undamped explicit valve step diverges even where the continuous
    loop is passive, so the probe zeroes the BORE's losses, not the valve's
    damping — the lip on the reed is structural. THE STOP (the reed's
    only): the reed meets the lay and rests closed at h = 0 — an inelastic
    collision, the velocity dies, the potential falls (a loss: the ledger
    only gains margin). Without it the reed swung on through into a closed
    phase clocked by its own resonance and the first register let go above
    C4. The lips keep their swing-through: they meet each other softly, and
    the trumpet's registers were tuned on it (with the stop the lips
    reopened at once and the tuning went). THE NOISE: the breath's
    turbulence is σ·P_mouth·noise on Δp, the perturbation a static reed
    grows from (a reed at rest is a fixed point); the reed's is low-passed
    at 2 kHz (white noise at the sub-rate seeded its upper registers), the
    lips' white. ζ: with A 0.6 the reed's coupling ζ = Z_c·A·h₀·√(2/P_M) was
    4.3 — ten times a real reed's — and the bore's returning wave ran to
    eight times the mouth pressure; at 0.14 it is 0.8 and the mouthpiece
    pressure swings about ±P_mouth as the textbook's does.

21. **The saxophone is a cone truncated at a quarter of its length with a
    mouthpiece holding the missing apex's volume, its bell and its wall loss
    scaled with the note, a quasi-static reed, and its intonation calibrated
    at four notes when the patch loads.** THE CONE (SYNTH §2.11's S ∝ x²,
    closed at the truncation, open at the mouth) is normalized at the REED
    end (normalized at the mouth its throat impedance read 880 and the
    junction saturated). THE MOUTHPIECE: a bare truncated cone's series is
    stretched (+36 cents at the fourth peak at apex 0.1); Benade's rule — a
    chamber holding the missing apex's volume — brings it back: a cylinder
    of the throat's area and a third of the cut cone's length
    (`cone_mouthpiece`), and the pinned cone's first four peaks sit within
    0.1 cent of the integers, the ported one's within 31 (the radiation's
    end correction shrinks with frequency). THE TRUNCATION decides the
    register, and it took a direct impedance measurement to see it (a
    sinusoidal flow at the throat, |p[0]|/|q| scanned about each harmonic —
    an impulse's Goertzel had listed the peaks in the wrong order): at apex
    0.1 the cone's first peak at A3 is 3.5 throat impedances against 7.1,
    9.5 and 10.3 for the next three — the FIRST PEAK IS THE WEAKEST of a
    deeply truncated cone (the cylinder's is 71 against 18, 9, 6) — and the
    reed took the strongest, the third register with a slow modulation at
    the note's period, which every estimator read as a wandering
    fundamental; at C6 (0.9 against 5.8 for the eighth) nothing held. At
    apex 0.2 the peaks are 11, 21, 21, 16; at 0.25 (the default: an alto's
    ratio, the missing cone a fifth of the body) the first is within a
    factor two of the second and the first register holds; at 0.3 the
    fourth peak stretches 3 %; at 0.5 the chamber swallows the third and
    fourth. THE SAX SCALES WITH THE NOTE (as the flute's embouchure does,
    #19): its bell's radiation corner and its wall loss are declared at A3
    and follow the pitch — corner·f/220, T60·220/f — so every note is the
    A3 cone in miniature (measured: A3 11/21/21, C5 18/27/24, C6 27/29/25);
    with the corner fixed the high notes' peaks sat under the bell's cutoff
    and the reed took the octave from F#4. THE REED is quasi-static:
    `reed_hz` 12000 with Q 0.7 (its response settles in two sub-steps; at
    2500 the fifth harmonic of C5 met the resonance and the register hopped;
    past 20 kHz the kick–drift's ω nears 2 and the reed misbehaves), its
    stiffness the closing pressure 3 P_ref, its rest opening 0.5, its flow
    factor 0.14. THE ONSET ISLAND, measured through the ABI over 104
    onsets (C3–C6, breath 60–127): apex 0.25 with A between 0.12 and 0.15
    starts every note in its first register; 0.22 starts 17 in the third,
    A 0.18 starts 10, apex 0.3 with A 0.1 starts 20 — the defaults sit in
    the island's middle (0.25, 0.14), and the mouth pressure rises through
    an 80 ms one-pole so a hard blow on C3 does not seed the third. THE
    BREATH: P_mouth = P_ref·(1.1 + 0.55·breath) — from just under the
    reed's threshold (P_M/3 in theory, 1.15 P_ref with the losses) to 0.55
    of the closing pressure at full breath (at 0.6 two notes thinned and
    read +23 cents at the top; at 0.7 some squealed to the fourth register).
    FOUND BY THE AUTHOR: a first map started at 0.7 P_ref, so the sax said
    nothing until the breath passed 50 of 127 and then jumped to nearly
    full level — a pedal or a light press never reached it ("I hear
    nothing"), while the trumpet whispers from 5. Now a note held under a
    slow ramp speaks at 4–10 of 127 (measured through the ABI at 0.3, 2 and
    6 s ramps, by CC 11 and by pressure). THE DYNAMICS, declared: a beating
    reed's bore amplitude grows 1.7 dB from the threshold to full breath
    where the lips' grows 14, so the sax's level follows the breath itself
    (0.32 + 0.68·breath, −10 dB at the threshold, smoothed as the pressure
    is) — the player's crescendo, the physics adding the brightening on top;
    the press adds the gain stage's own (SYNTH §3's two consumers, #19).
    THE RELEASED VOICE'S MOUTH GOES TO ZERO: the pressure
    is what smooths, and a released voice's target is 0 — a first form
    smoothed the breath and mapped it, so a released sax kept blowing at
    half the reference and its tail read as the next note's fundamental a
    second and a half later (the test heard the previous note, 300 cents
    down, within 15 dB). THE INTONATION: the reed's pull is measured at
    load — C3, C4, C5, C6 blown 0.45 s at 1.4 P_ref, the period of the last
    0.2 s against the note (−13, −18, −20, −22 cents at the defaults) — and
    the bore is cut to cancel it, lerped in octaves between the four (held
    flat outside); the calibration is keyed by the embouchure's fields and
    the kind, ~150 ms, once per change, logged. THE PERIOD, not the lowest
    partial: a sax's first register carries its fundamental under the
    second harmonic, and "the lowest partial within 15 dB" read it as the
    octave and mis-cut the bore; the period is the autocorrelation's first
    maximum within 0.08 of its top (the mean removed — a DC offset flattens
    it; 0.005 read a jittery cycle as chaos), refined by a 4-cent Goertzel
    scan — in the calibration and the gates alike. MEASURED through the
    ABI at breath 70/127: C3–C6 every third semitone within 5 cents (the
    worst C6 at −5); the cylinder with the same reed, for the record: the
    textbook square wave at γ 0.47 but period-doubled (r(2T) 1.000 against
    r(T) 0.962), periodic at γ 0.37, 0.4 and 0.6 — Maganza's cascade on the
    near-lossless Raman model, a T60 of 0.1 s suppresses it; the cone at
    the defaults shows no doubling. THE LEVEL: the mouth end's velocity
    ×1.0 sits A3 at −24 dBFS at full breath (−30 at 0.55, the profile's);
    C2–C7 within 6 dB, the fundamental and the second harmonic within a
    few dB of each other at half breath and the fundamental ahead from C4
    up; below C2 the cell cap (128) thins it — the sax's range. The ramp
    chart at A3: the tone from a light breath, swelling 10 dB and
    brightening as the reed closes further — no register change, the sax's
    own spectacle is its steadiness.

22. **The trumpet is a cylinder with a Bessel flare, its pitch on the bore's
    third peak, the lips' resonance an outward valve that CC 74 bends an
    octave either way — the registers — with the intonation calibrated as
    the sax's and a declared bell shear for the brass.** THE BORE
    (`BORE_TRUMPET`): S = 1 along the tube, the flare S = (1 − t)^{−γ} from
    `bell_start` (0.6) with t reaching 1/(1 + ε), ε 0.1, γ 0.7, to the mouth;
    the radiation port scaled by the mouth's area (in tube units the flare
    blew up); the bell's brassiness a §2.4 shear in the last cell's (p, u),
    p −= brass·u³ — amplitude-driven, det 1, declared (the spec's v1 stand-in
    for the shock). THE PEAKS of a flared bore are not a harmonic series
    from its quarter-wave fundamental, so they are MEASURED at load
    (`bore_peaks`: a 200-cell reference bore, a 0.6 s pulse, a 2 % Goertzel
    scan, prominence ×3, threshold 0.2 % — a first threshold found only the
    first) and the note is placed on the `partial`th (the third: the
    trumpet's written middle register), the bore cut so that peak lands
    there. THE LIPS: outward-striking, Q 3, rest opening 0.05, closing at
    P_ref, area 0.5, at CC 74 centre their resonance 0.95 of the note (an
    outward valve plays above its resonance, on the bore's peak: the pull
    reads +89, +87, +86, +92 cents at C3–C6 and the calibration cuts the
    bore to cancel it — before it the trumpet played 151 cents sharp); CC 74
    bends the resonance ±`lip_range` octaves, so at 0 the lips sit on the
    peak below and at 127 on the peak above — the register key is the
    embouchure, as the spec's `[ITERATE]` said it would be. FOUND: at
    `lip_ratio` 0.9 the lips at full CC 74 sat between the fifth and sixth
    peaks and notes from F#3 fell to the pedal; at 0.95 they land on the
    sixth — the octave — for every note C3–C6 (measured through the ABI:
    CC 74 at 0 → −772 cents on all thirteen, at 64 → +14…+25, at 127 →
    +1276…+1296). THE BREATH: P_mouth = P_ref·(0.5 + 2.5·breath), the
    attack a 20 ms one-pole (the lips choose their register at once).
    THE LEVEL: ×4.0 sits A3 at −17 dBFS; C3–C6 within 1 dB, C7 at −26 —
    the trumpet's range; below C3 the bore wants more cells than the cap
    (its fundamental is the note's third) and the bass falls away. The
    embouchure chart at A3, CC 74 swept 0 → 127 over 16 s: the registers as
    a staircase — the second peak, the third (the note), the fourth, fifth,
    sixth — each bending within itself as the lips pull it, the pedal
    showing through where the lips cross between peaks. The trumpet held
    its gates from its first build; it was the sax that took the step.

23. **Flags for the author (the spec folds).** SYNTH §2.12: the reed's
    stop at the lay (an inelastic collision, a loss); the swept volume
    ±S_r·v and the valve's energy in the kick–drift's form; the implicit
    Bernoulli junction against the one-step impedance (the naive form the
    red control); the probe's design (two blows with a retune, the bore's
    losses zeroed, 5 % after 30 ms); the breath noise low-passed for the
    reed; ζ in a real reed's range (0.8). §2.11: the cone normalized at the
    reed end; the mouthpiece as the missing apex's volume (Benade); the
    truncation ratio decides the register — the first peak of a deeply
    truncated cone is its weakest, and an impulse's peak listing is not an
    impedance measurement; the sax's bell corner and wall loss scaled with
    the note (the flute's law again); the trumpet's flare with ε and the
    port scaled by the mouth's area; the peaks measured at load; the bell
    shear. §3: the intonation calibration at four notes per embouchure
    (the pull lerped in octaves), logged; the period as the pitch measure;
    the released mouth at zero; the sax's breath map 1.1 → 1.65 P_ref (from
    its threshold) with an 80 ms attack and its level following the breath
    (a declared 10 dB, where the lips' physics gives 14), the trumpet's 0.5
    → 3 with 20 ms; CC 74 as the trumpet's
    register key (the `[ITERATE]` resolved: lip tension); the press blows
    the reed and the lips as it does the jet and the bow (#19). Carried
    `[ITERATE]`s: the reed channel's flow inertia (a lowpass on the flow
    that would favour the first register physically, where the truncation
    does it geometrically); toneholes and a register vent; the two-mass
    lips; the physical reed at 2–3 kHz with the lip's damping (the
    quasi-static reed is v1's); the ports' discrete power at the half step
    (#19); a sax below C2 and a trumpet below C3 (the cell cap — 256 cells
    or a sub-rate for the bass). The steadiness measure (the period per
    50 ms window, its spread) is in the scratch probes, not the suite: the
    suite's gates are the pitch, the register and the ledger.

## Step 59 — The orbit trace & the phase close (the Mac; Voxo 0.13.0)

24. **The synth draws itself from its own state: each traced voice keeps the
    last 85 ms of its orbit in a lock-free ring, and a poll from any thread
    takes what is new, decimates it curvature-weighted to a handful of
    segments and hands it over unit-normalized with its amplitude.** SYNTH
    §2.7 as built, Voxo's side (`voxo_set_trace`, `voxo_trace_poll`): the
    rendering thread writes one point in eight sub-steps (12 kHz) into a
    1024-point ring per voice when the voice's kind is in the mask — one
    shared word, the write index, and the reader keeps its own cursor per
    slot, re-synced by the voice's serial (a new voice in the slot starts a
    quarter ring back at most) — so the audio path is untouched: the
    rendering with the trace on is BIT-IDENTICAL to the rendering without it
    (gated for the cell, the rotor, the hybrid string and the sax), and a
    mask of 0 costs nothing. THE PAIR per voice kind: the single cell's own
    (x, y); the lattice's (Σx, Σy) over its modes with the mix (the output
    and its quadrature); the Duffing cell's and the kicked rotor's (x, y) —
    the rotor's momentum on its torus is what scribbles; the strings and
    the winds, whose state is a chain, trace their output against its scaled
    derivative, (s, ṡ/ω) with ω the note's — the phase plane of the sound
    itself, honest where no single cell exists. THE DECIMATION: of n points
    keep at most K + 1 — the first, the last, and the points where the
    running weight (the turning angle at each point plus half the step
    length in orbit radii) crosses each K-th of its total, so the bends get
    the vertices and a straight run gets few; the polyline comes back
    divided by its peak radius over the window, the radius beside it, so
    the shell scales by amplitude × its trace scale (the spec's law).
    MEASURED: the single cell at A3, eight segments asked — nine points, all
    on the unit circle within 0.4 % (the magic circle's orbit is a circle;
    the amplitude reported 0.175, the level's); ten rotor voices polled at
    four segments answer with five points each, finite; the hybrid string's
    phase plane comes back with its note's amplitude.

25. **The gesture route is the shell's bridge, Voxo → libsumi at frame rate,
    no core change: the orbit lands at the note's cell centre from the
    layout probe's own table, as tine or wake segments, budgeted at
    twenty-four a frame over all voices with the overflow merged within each
    voice; the scope lives in the settings window as a miniature of the
    canvas.** THE PLACEMENT: the shell scans the probe once per layout (a
    96 × 54 grid of `sumi_layout_probe`, the first cell centre each note
    answers with — a multi-echo layout's first echo; rebuilt when the layout,
    the aspect or the params change), so no core query was needed; the
    rolls answer nothing and their notes stay unplaced (a scrolling sheet
    has no home for a trace: the scope draws them at the centre, the ink
    skips them). THE EMISSION: each kept vertex pair is one
    `sumi_add_tine` (the mouse's convention — alpha 0.035, the magnitude the
    segment's length in canvas heights; an exact pass) or one
    `sumi_add_wake` (tip a quarter of the trace radius, clamped 0.005–0.08;
    sub-stepped — class by inheritance under the strictest-member rule),
    placed at cx + x·r/aspect, cy + y·r with r = amplitude × scale. THE
    BUDGET: the shell's calls bypass the mapper's per-frame budget (they go
    straight to the deform queue, which holds 4096), so the bridge budgets
    itself — at most 24 segments a frame over all voices; over it, every
    voice keeps the same share of its polyline, at least one segment, its
    vertices re-sampled evenly (merged within the voice, never one voice
    culled while another draws — the echo rule's spirit); the mapper's own
    64 stay untouched by construction. MEASURED on the bench (`--trace-test`,
    ten rotor voices under a held press for 240 frames, K swept by the
    wheel): peak 20 segments a frame of the 24 (ten voices × ⌊6 × 0.4⌋),
    4448 inked over the run, 9600 merged by the budget, none unplaced; the
    bridge's own cost 0.17 ms a frame (the poll, the placement, the
    emission). THE TOGGLE IS CLEAN: the field after the run with the trace
    OFF is bit-identical to the run with no trace object at all
    (2 097 152 bytes equal); with the trace ON it differs in 1.97 million —
    the ink lands. FOUND, a core matter for the author (the core is frozen
    this phase): the same script run twice UNTRACED reads bitwise in Sumi
    (0 bytes differ) but not in Anod (1.1 million of 2 097 152 differ) —
    the strikes' episodes are not bit-reproducible run to run through the
    gesture ABI either, with a 600-frame quiet start and the dip between
    runs; the gate therefore runs in Sumi and Anod's reproducibility is
    logged here for Phase 10's core reopening. THE SCOPE: the spec's live
    composite overlay at the voice positions would be a core pass — the
    canvas window is the core's swapchain on every backend (Metal, GL,
    D3D11) and the desktop's ImGui lives in a second window — and the phase
    never touches libsumi, so the scope route ships as the Sound section's
    miniature of the canvas: the same polylines at their cells, held
    voices bright, released ones dim, unplaced ones in the centre in blue,
    with the frame's counts beneath (voices, polled, inked, merged, the
    peak against the budget); non-destructive, and outside the dip by
    construction. The on-canvas overlay is the `[ITERATE]` for a core
    reopening. THE BINDINGS: per voice KIND (bit k of `trace_kinds`), the
    kicked rotor on by default — the chaos voice scribbling its own noise
    into the water is the demo the spec asked for; `trace_scale` 0.25
    canvas heights per unit amplitude (a cell's amplitude is the level, 0.25
    at velocity 127: a full-velocity note traces a 0.06-height orbit, a
    soft one a fifth of that); 6 segments per voice per frame (4–8); the
    stroke tine by default (exact, and the comb's look), wake as the
    alternative; the two routes each their own switch; all six in the
    Sound section, the INI and the preset file. Voxo's mask is 0 when
    neither route is on — no capture at all. The trace scale and the
    rotor's default ON are the author's taste sign-off (the step's inputs).

26. **The demo, and the phase's close.** `--trace-demo <dir>`: Anod on the
    chroma grid, a chord of four rotor voices struck in turn and held under
    the press while the wheel sweeps K from 0 to 2.5 over twelve seconds,
    then released — the orbits inked as tines at their cells, eight
    segments a voice, scale 0.35 — the composite exported at 720 × 720
    whenever the last export has landed, the frames encoded with ffmpeg:
    `docs/evidence/step59/rotor_anod.mp4`, the rotor scribbling its chaos
    into Anod, the synth drawing its own phase portrait in ink (a still
    beside it). Phase 8's spec folds queue for the author in #23 and here:
    the trace's per-kind pairs (§2.7's "the voice's cell" for the chain
    voices is their phase plane), the poll and the decimation as the
    ABI, the shell-side budget (the gesture ABI bypasses the mapper's), the
    scope's home, Anod's run-to-run reproducibility. The phase end is the
    author's: the tag `v2.0.0-alpha.3` on the desktops with the synth
    played on every device, the tablets regression-checked (they compile
    Voxo 0.13.0 unchanged and stay on the sampler), and the fold of this
    file into `docs/DECISIONS.md` Part VII.

27. **The scope view is a live-composite pass in libsumi (1.3.0, additive):
    the shell hands the composite up to 128 segments a frame and it draws
    them screen-locked, over the medium or instead of it — the spec's
    on-canvas scope route, with the author's ask on top: the water hidden
    and the polylines alone in the main window.** The author, playing #25's
    scope in the settings window: "can we add a checkbox to replace the
    medium with the scope in the main window". The main window is the
    core's swapchain on every backend, so the ask is a core addition — the
    phase's rule kept libsumi untouched, and this entry records the one
    exception, chosen over a platform overlay per shell (an NSView on
    Metal, GL draws on Linux, nothing on D3D11 — rule 5: the core identical
    on every platform). THE PASS: the composite shader gained the scope's
    uniforms in the plate guide's own pattern (`dbg_cells`, DECISIONS_5): a
    mode, a count and 128 segments (x0, y0, x1, y1, normalized), drawn at
    the end of the fragment as the distance to the nearest segment in texels
    — a line a texel and a half wide with a faint halo, amber — SCOPE_OVER
    on the medium, SCOPE_REPLACE on the scope's dark glass with the medium
    not drawn (the water underneath keeps marbling and the ink route keeps
    inking; only the view changes). LIVE PATH ONLY: the renderer passes the
    scope to the swapchain composite alone — like the live ripple, the
    print, the export and the bloom's source never see it (gated: with the
    scope replacing the water for 240 frames of ten rotor voices, the field
    and the dip's print are bit-identical to the run without it, 2 097 152
    and 1 048 576 bytes). THE FIXTURES HOLD: mode 0 is the shipped composite
    — the composite gate reads bitwise on Metal (0 of 1 048 576 channel
    samples differ, the negative control red as required) and the field
    gate holds the phase invariant (no field pass changed). THE ABI:
    `sumi_set_scope(inst, points_xy, strip_lengths, strips, mode)`, the
    segments copied, mode 0 or no strips clearing it; `sumi_version` 1.3.0
    (the Step-33 minor-bump pattern: additive, nothing moved); the C compile
    test takes its address. THE SHELL: the bridge packs its placed polylines
    (sixteen voices at eight segments fill the 128) and sets the scope each
    frame the trace runs, clears it once when the trace stops; the Sound
    section's "Suzu trace on the canvas": off / over the water / the scope
    alone (the water hidden) — the INI's and the preset's `trace_canvas`;
    off by default (the author's taste). The composite's cost with the
    scope on: 128 segment distances per fragment at most, on the live
    frame only; off, one uniform compare. The shader regenerated for the
    four dialects (metal_macos, hlsl5, glsl410, glsl300es); the tablets
    compile the core unchanged in behaviour (mode 0).

## Step 59b — Suzu on the web: the engine in a worklet, the flute panel (the Mac; Voxo 0.14.0) — added by the author, 2026-10-04

28. **Suzu runs in the browser as itself: all of Voxo compiled to a
    standalone wasm with no audio device, driven by an AudioWorklet that
    owns it, talking to the page by messages — and one additive Voxo call
    reads a voice's internal state for the pages.** The author, with six
    standalone visual drafts (`visuals/`, untracked): "we can do better using
    directly a wasm version of suzu." A spike answered first: all of Voxo
    compiles to wasm UNCHANGED with a stub for the device — 110 KB, the
    sampler's parsers dropped at link because nothing reaches them — and
    renders every voice kind. The drafts each rewrote the physics in
    JavaScript, and some showed the opposite of the engine: the flute's
    register picked by pressure thresholds (Suzu's overblow is emergent,
    #19), the reed coupled explicitly (the form Suzu's load gate rejects,
    #20), strings coupled across voices (Suzu has no coupling between
    voices). The author added the steps (59b the host and the flute, 59c
    every family) and moved the phase end to 59c. AS BUILT: a `none`
    backend (`voxo/src/backend_none.cpp`: voxo_start answers false, the host
    renders — the web, node, and the gate's native reference); Voxo's CMake
    builds one source list with either backend, the web branch fetching
    pugixml, miniz and dr_libs (dead code in the lab's link) and no
    miniaudio. The lab's wasm (`web/suzu/suzu_web.c` → `build-web/suzu-
    dist/suzu.wasm`): STANDALONE (no emscripten JavaScript glue: an
    AudioWorkletGlobalScope has no fetch, timers or TextDecoder), fixed 32 MB
    memory (the worklet's views never detach; Voxo's instance is 3.5 MB),
    122 KB, ZERO imports; a flat surface — the parameters by NAME through a
    table generated from voxo.h's struct field for field (75), MIDI bytes
    in, interleaved stereo out, the trace and the inspection as documented
    float records, Voxo's log lines kept for the host — so no JavaScript
    depends on a C struct's layout. ONE ENGINE, TWO HOSTS:
    `web/suzu/site/suzu-engine.js` wraps it with nothing DOM-bound, and both
    the worklet (`suzu-worklet.js`: renders each quantum, takes MIDI and
    parameters between quanta, posts a snapshot ~60 times a second) and the
    node gate import it. Messages, no SharedArrayBuffer: GitHub Pages serves
    it without cross-origin isolation headers. THE INSPECTION
    (`voxo_suzu_inspect`, Voxo 0.14.0, additive): a copy of each sounding
    voice's state — per kind, documented in voxo.h: the cell's and the
    rotor's (x, y), the lattice's modes with their ratios and bow targets,
    the Verlet chain's shape and velocity, the hybrid's loop read around the
    ring with its bridge cells, the bores' pressure, flow and area along
    their length, the flute's jet read from its delay line at x·τ (the jet's
    shape from the flue to the labium, the engine's gain at the labium) with
    the block's mouth pressure, U₀, τ, gain and area, the valves' opening,
    displacement, velocity and flow. Read between renders (the rendering
    thread, or no device): it copies live state without a lock, and the
    rendering is bit-identical with or without it (gated). The flute's
    per-block values are written to five floats in its voice for it; nothing
    in the render reads them. The patch calibration of the sax and the
    trumpet runs on the worklet's thread when their parameters change (0.3 to
    0.8 s in wasm): the worklet mutes for the quanta after a slow apply — a
    59c concern, the flute calibrates nothing.

29. **The web gate: the wasm against a native build of the SAME flat surface
    over a Voxo built as wasm computes — bit for bit where the math library
    agrees, a declared −80 dB where it does not; the page itself proven in
    headless Chrome.** `tools/suzu_web_gate.mjs` renders
    `tests/fixtures/suzu_web_script.txt` (every voice kind: two notes on two
    MPE channels under breath, a bend, a release, the trace and the
    inspection snapped three times) through the engine module in node, and
    `tests/suzu_web_reference.c` renders it natively through
    `web/suzu/suzu_web.c` — the lab's own surface — linked against
    `voxo_nofma`: the same sources with no device, `-ffp-contract=off` (wasm
    has no fused multiply-add) and `-O3` whatever the tree's build type.
    FOUND, twice: (1) with the desktop's default contraction the output
    differs from the first millisecond; without it seven of nine kinds are
    BIT-IDENTICAL to the wasm, the chaotic rotor included; (2) a Debug
    reference read the lattice differently — at -O1 and up the compiler
    rewrites some math-library calls (pow(2, x) as exp2(x)), so the
    reference is optimized as the wasm is. The remaining differences are the
    math libraries' last bits — Apple's libm against emscripten's musl —
    where a voice's path calls them on arguments that differ: the
    lattice's sinf per sample under a glide (its first difference falls on
    the script's bend, block 150), the flute's tanhf from the first block,
    the trumpet's powf. THE VERDICT: bit for bit on the audio, the trace and
    the inspection for the cell, Verlet, the hybrid, Duffing, the rotor and
    the sax; within −80 dB of each record's peak for the lattice (measured
    −113), the flute (−92) and the trumpet (−728: one ulp); the negative
    control (one reference sample moved by 0.25) red. THE REPORT against the
    SHIPPING desktop build (contraction on): −68 to −111 dB for every kind
    but the rotor, which diverges as chaos does with a last-bit difference —
    the statistics, not the samples. Making the web and the desktops render
    bit-identically (Voxo without contraction everywhere, Suzu's own tanh
    and pow) is the author's input on the step; not done (the default). THE
    PAGE GATE (`tools/suzu_lab_gate.mjs`): the page's `?gate=overblow`
    renders a breath ramp — A4 held, the breath 0 → 127 over 12 s, then 3 s
    at the top — OFFLINE through the same worklet (OfflineAudioContext: the
    script rides in processorOptions, deterministic), reads the pitch with
    its own estimator every quarter second and posts it; GREEN in Chrome
    154: the audio finite; mid-ramp on the note (closest 0.4 cents); the
    jump at breath 116/127 with nothing programmed; the last second on the
    octave (+4.4 cents); soft blowing flattens (−46 cents at breath 32
    against +3 at 56) — the 58b chart's spectacle, in a browser. The cost: 15
    s rendered offline in 227 ms; one flute voice 38–61 µs a quantum in the
    live worklet (1.3–2.1 % of its time), 72 µs for two voices in node; the
    marble's own web gate stays green on the rebuilt tree.

30. **The flute panel: the bore's standing wave and its envelope, the jet
    swinging across the labium, the phase plane, the spectrum with the
    note's harmonics, and the sounding pitch read from the audio — the page
    holds no physics.** `web/suzu/site/` (`index.html`, `lab.js`,
    `lab.css`; the site's paper in the light, a scope's glass in the dark;
    a phone stacks the panels). THE BORE: the engine's pressure node by node
    inside the tube drawn from its area, the flow dashed, and the ENVELOPE —
    an RMS per node the worklet accumulates on every quantum over a quarter
    second, drawn as √2·RMS: one half-sine on the first register, two humps
    with a node in the middle on the octave, the overblow made visible.
    FOUND by the drawing, measured in node: (1) a single snapshot is jagged
    though the field is not chaotic — the odd harmonics sit 13 dB under the
    fundamental (harmonics 1–8 hold 88 % of the bore's pressure energy), so a
    peak-hold envelope caught every crest; the RMS at 375 samples a second
    shows the mode; (2) the grid's own shortest waves — near the staggered
    grid's cutoff, slow by its dispersion — hold 6 % of the bore's pressure
    energy at breath 70 and 21 % at full breath, driven by the jet's sharp
    switching (the same with the jet's noise at zero), inaudible at the mouth
    end, loud in a snapshot: the drawing averages along the bore ([1, 2, 1]/4
    twice: the 8th harmonic of a 94-cell bore kept at 96 %) and its caption
    says so; the engine is unchanged (a gentle grid-scale damping in the
    bore is an `[ITERATE]`). THE JET: the delay line read along it, scaled to
    its displacement at the labium (where the engine applies the gain),
    compressed past three jet widths so the swing stays on the page; the
    labium's edge at its offset y₀; the share entering the bore,
    ½(1 − tanh(η − y₀)). THE PHASE PLANE: the sound against its slope,
    (s, ṡ/ω), the last two periods read from the audio at the device's rate
    — the flute's orbit trace is this very pair (#24), but its poll
    decimates ~16 ms (seven periods of A4) to 16 segments, which aliases the
    orbit into a streak: the trace serves the ink route, the scope reads the
    sound. THE PITCH: the autocorrelation's first lag within 0.08 of its
    maximum, refined by a parabola (the desktop calibration's measure, #21),
    the register as the nearest multiple of the note, the last 12 s strip-
    charted against the breath. PLAY: a keyboard C4–C7 (the flute's range;
    the computer's A…K row from C5), the breath slider on CC 2, the breath
    ramp (12 s, the overblow), four embouchure knobs (the jet's delay and
    gain, the breath range, the wall loss) sent as parameters, Web MIDI in
    (a breath controller plays it). Served locally by `tools/web_serve.py
    --dist build-web/suzu-dist` (localhost is a secure context); deployed at
    the docs step with the rest (the documentation-timing rule). The author's
    drafts are superseded by the engine for the flute; their other ideas —
    the A/B switches, the bore beside the reed's portrait — are 59c's.

## Step 59c — The Suzu lab: every voice family (the Mac; Voxo 0.15.0) — added by the author, 2026-10-04

31. **One host, six panels: the pages share a core and hold no physics; the
    red controls are switches on the engine's own lab parameters, and the
    worklet, not the page, keeps them off the speakers.** `web/suzu/site/`:
    `lab-core.js` (the audio graph and the MIDI a controller would send, the
    keyboard, the pitch read from the sound — 59b's estimator — the
    spectrum, a portrait with persistence, a strip chart, the bore view, the
    red controls' banner, the navigation, and the offline runner every
    page's browser check renders through), a page per family (`cell`,
    `modal`, `strings`, `chaos`, `flute` — 59b's page moved onto the core —
    and `winds`) and a front page (`index.html`: a card per panel, how
    faithful the lab is, what the red controls are). The panels: THE CELL —
    its orbit from the ring at full density, the conserved form over whole
    turns, the shears combing harmonics in; red: the naive update, the plain
    retune. THE MODAL VOICE — a bar per mode at its ratio with the bow's
    target and where the servo settles (#33), the servo strip, the
    lattice's orbit, the presets, the coupling against the load gate (the
    refusal shown as the engine words it); red: the gate bypassed. THE
    STRINGS — the Verlet chain's shape, the hybrid's ring and bridge, the
    modal pluck, A/B by one switch (the author's drafts asked for it); red:
    the CFL number forced over with the gate bypassed. THE CHAOS VOICES — the
    rotor's Chirikov section (θ, p) at each kick from the ring's aux
    channels, K on the mod wheel, the Duffing cell's stroboscopic section
    and its clang; the page links the canvas's Chirikov operator page and
    the operator draft links back (one theorem, two senses; `DOCS_ROOT` in
    `lab-core.js` — the lab's address is the docs step's, step 67). THE
    WINDS — the bore with the valve drawn at its blown end, the valve's
    portrait (its displacement against its velocity, from the aux
    channels), the ledger as a live meter and strip (#32), CC 74's register
    staircase for the trumpet; red: the naive junction on a lossless bore,
    the valve gate bypassed, a bend to trip it. THE SAFETY (the worklet):
    FOUND with the naive cell — Voxo clips its mix at full scale, so a
    runaway voice never leaves the range: it sits at ±1, a full-scale wave,
    while its state reaches 10¹² and beyond. So a quantum that reaches
    |x| ≥ 0.98 (the lab's voices sit under 0.5) goes silent at once, and a
    non-finite sample or twelve such quanta in a row is a BLOW-UP: the page
    is told (the banner says which gate's reason it was) and the engine
    restarts from the staged parameters. Nothing a page offers reaches the
    speakers at full scale. THE CHECKS: each page's `?gate=<name>` renders a
    scripted demonstration offline through the same worklet and measures it
    with the page's own code (`tools/suzu_lab_gate.mjs` runs them all;
    `--shots page[:arg][:light][:phone]` captures the live pages through
    the DevTools protocol, the colour scheme and a phone emulated). GREEN in
    Chrome 154, every page (the evidence's `suzu_lab_gate.txt`).

32. **Voxo 0.15.0 (additive): the trace's density and its recent points with
    two aux channels per kind; the winds' mouth-power ledger in the
    inspection.** The 59b poll merges ~16 ms into segments for the ink
    route; a portrait wants every point. `voxo_set_trace_decimation`
    (1..64 sub-steps a point, default 8) and `voxo_trace_recent` (the last
    ≤ 1023 points of one sounding voice at full density, oldest first:
    x, y and the kind's aux pair — the lattice's mode 0, the hybrid's
    bridge cell, Duffing's drive phase and amplitude for a stroboscopic
    section, the rotor's momentum and K, the flute's η at the labium and
    the flow in, the winds' valve displacement and velocity; voxo.h
    documents each). THE LEDGER (SYNTH §5): the winds' inspection carries
    k[8], the mouth's work ∫P_mouth·Q since the strike (accumulated in the
    render per sub-step in double, only while the mouth blows), and k[9],
    the energy the bore and the valve hold — k[9] ≤ k[8] always, the losses
    take the difference. Gate 26 in `voxo_suzu_tests`: the cell's 1023
    recent points keep x² + y² − εxy within 7e-5 of itself (the orbit
    undecimated); the rotor's aux shows one kick a period (4 expected, 4
    seen) at the wheel's K; the sax's and the trumpet's ledgers read every
    block for a second hold (the held energy at worst 0.52 and 0.14 of the
    work); a render read at density 2 every block is bit-identical to the
    unread one. The node gate's script now sets density 2 and its snapshots
    carry the recent points: every kind's recent trace bit for bit or
    within the kind's declared −80 dB (the lattice −106.5, the flute
    −106.0). `suzu.wasm` 124 024 bytes.

33. **What the panels measured, each page's check: the engine as shipped
    behaves as its gates say, and four drawings had to learn the physics
    first.** THE BOW is a proportional servo, not an integral one: each
    mode settles where the bow's push balances its own decay, E/E_t = 1 −
    γ_k·τ (γ_k = ln 1000 / T60 + the stiffness term, τ the bow's onset
    0.15 s) — the first check against the target failed by 2.6 dB, the
    check against this balance holds within 0.38 dB mode by mode (the
    harmonic string, A3, breath 80, T60 3 s); with no decay and no coupling
    the fundamental sits ON its target (0.031 dB). The bar view draws both
    ticks. The bell at κ 0.4 stays in tune (its first four peaks at +0
    cents of 0.5, 1, 1.2, 1.5); at κ 1 the load gate refuses it, saying
    why. THE HYBRID's body rings loud beside its string (the bridge modes at
    341 and 472 Hz, −3 and +2 dB against the note at A3 — #14's design),
    so a period estimator mixes them: the page reads the spectral peak at
    the note (+0.0 cents). The Verlet chain −1.3 cents, the modal pluck
    +0.1; the CFL gate refuses k·dt² = 1.05 and the bypassed chain blows up
    in its first quantum. THE ROTOR below K_c: the tori confine the
    momentum to the primary island (|p| ≤ 1.07 rad at K ≈ 0.3, the
    island's half-width 2√K ≈ 1.1); past it (K = 2.5) the momentum spreads
    into the sea
    (1.57 rad against a uniform sea's π/√3 = 1.81 and 0.62 confined) — a
    first check asking for the whole circle failed because the period-1
    island about θ = π stays stable to K = 4, and the sea surrounds it.
    DUFFING struck hard at β 8: +226 cents in the first 80 ms, +0.0 at 2 s.
    THE NAIVE CELL grows 26 dB in 0.38 s at A3 before the lab catches it;
    the leapfrog's conserved form drifts 7e-6 dB over 2.5 s. THE NAIVE
    JUNCTION: the valve gate's probe measures the bore + valve holding
    5.6e7× the mouth's work and refuses it; bypassed, the bend trips it at
    0.30 s. THE CONE'S DRAWING: along a cone the pressure grows toward the
    apex as one over the distance, so the narrow end took the scale and the
    sax's bore looked still; it is p·r that stands as a sine, and the bore
    view draws the cone's pressure times the local radius (the caption
    says so; the trumpet and the flute are drawn as they are). THE VALVE'S
    OPENING is averaged over the snapshots (one catches the lips shut).
    FOUND, an engine `[ITERATE]` for the author (58c, not changed here):
    THE SAX at A3, breath 70, sounds +18 to +19 cents by the period
    estimator, natively and in wasm alike, at every block size 64–512;
    its spectrum is QUASI-PERIODIC — the fundamental on the note (0
    cents), the second harmonic split into 440 Hz (−16 dB) and +18 cents
    (0 dB), the third into +12 and +24 cents: a ~4.6 Hz sideband family.
    The sweep is bistable near the calibration's pressure — breath 50: +1,
    60 … 110: +19 … +14; the calibration's step blow lands in the lower
    regime, the voice's 80 ms onset in the upper (59b's suite log read −3
    cents for A3 within its own sequence: the regime follows the history).
    A reading, not measured on the bore: the cone's second resonance sits
    sharp of twice the first and the reed's oscillation shares itself
    between them. The winds check's bound (the first register within 40
    cents) holds; the cone's resonances (the truncation's correction) are
    where a fix would go.

34. **Flags for the author.** (1) SYNTH §7 still lists the web build among
    the deferrals; steps 59b–59c brought it forward at the author's word
    (the roadmap says so) — the spec's line is the author's to strike.
    (2) The roadmap's "the mode splitting" on the modal panel: the shipped
    voice has no unison pair (every preset places distinct ratios; the
    splitting was measured at 57 with two bare cells in the suite, #12), so
    the panel shows the coupling's exchange of energy and keeps the
    splitting chart as the draft's figure; a live splitting needs a unison
    preset — an engine change, not this step's. (3) Which red controls the
    public page exposes, and the panels' taste: the author's sign-off (the
    roadmap's author input). (4) The older Suzu drafts say "for the docs,
    step 63"; the roadmap's docs step is 67. (5) Safari and Firefox were not
    run (the browser gate is Chrome's). (6) The lab's address: the drafts
    and the Chirikov operator draft link `/suzu/…`, beside `/marble/`; the
    docs step decides.
