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
