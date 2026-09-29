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
