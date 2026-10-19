# Part XII — v3.0, Phase 13: Suzu foundation and the §13.7 deferred physics (steps 78–82: the SoA SIMD engine and the ×4 oversampler, the 2D membranes, the stick-slip bow, the bore v2, the editable tables — opened 2026-10-18 at step 78)

Opened 2026-10-18, the day Phases 11 and 12 were folded (Parts X and XI); merged into `docs/DECISIONS.md` as Part XII at the phase's fold. The spec is `docs/PROJECT_SPEC.md` (v5) §13 with `_work/TO_PROJECT_SPEC.md` §13.8–§13.12 as the phase's draft (the proposals, corrected by each step). THE CORE IS FROZEN from this phase to step 92 (libsumi 1.12.0): a core change is a bug → regression test → fix. The instruments: `tests/voxo_suzu_tests.cpp`, the storm (`midi-sink --dev --voxo-source suzu --voxo-storm <s>` from the Release build), `tests/voxo_fuzz.cpp`, the sound profiles, the web gate `tools/suzu_web_gate.mjs` and the lab gate.

1. **Step 78's working rule, recorded BEFORE the refactor: the web gate's
   contract under SIMD (2026-10-18).** The gate (`DECISIONS_7 #29`) compares
   the lab's wasm against a native reference built from the same sources
   with no fused multiply-add (`voxo_nofma`, `-ffp-contract=off -O3`), bit
   for bit for every voice kind whose path calls no math-library function
   whose last bit differs between the platforms' libraries, within a
   declared tolerance for the kinds that do; and reports the wasm against
   the shipping desktop build in dB, as information. **Restated for the SoA
   SIMD engine:** the reference build is the SCALAR path — `voxo_nofma`
   compiles with `VOXO_NO_SIMD` (the one header's scalar fallback) — and the
   wasm compiles with `-msimd128`; the gate stays BIT FOR BIT for the chain
   and the lattice, because the SIMD paths perform the same operations in
   the same order per node and per mode (the Laplacian's two subtractions and
   their sum in the scalar's order, the kick's product then its add, the
   rotation's two fused-or-not multiply-adds, the per-node damping before
   the drift), horizontal sums are taken in scalar order from the stored
   lanes, and the header's `mul_add` is a plain multiply then add wherever
   `VOXO_NO_FMA` is defined (the reference and the wasm, which has no FMA)
   and a fused one elsewhere (the shipping native builds, whose scalar code
   the compiler already contracts). If the comparison turned out not bitwise
   on any kind, the gate would become a TOLERANCE gate for that kind with the
   tolerance declared here — it did not have to (#2). The shipping native
   builds keep their dB report. The strobe of step 75 is compared before and
   after on the shipping build's own records (`suzu_web_reference_desktop`,
   captured before the first line moved) and on the strobe returns the suite
   dumps — bit-identical where the compiler's contraction of the scalar
   matched the intrinsics', the last bit elsewhere, reported either way.
   The lane widths: four on NEON (arm64: the Mac, the iPad, the Tab), four
   on SSE2 (the x86-64 baseline the Windows and Linux lanes build — AVX2's
   eight lanes would need a flag the release lanes do not set or a runtime
   dispatch; not taken: the roadmap's "AVX2 on x86-64" is read as "the x86-64
   path", SSE2 being what ships) and four on WASM SIMD128; one header,
   `voxo/src/simd.h`, one source.

2. **Step 78 — the four-lane engine and the ×4 oversampler, as shipped
   (2026-10-18; Voxo 0.18.0, additive; evidence `docs/evidence/step78/`).**
   The contract of #1 held without a tolerance: the suite's twin comparison
   is 100 % bit-identical over 4800 sub-steps for the chain (80 nodes) and
   the lattice (16 modes, a fold shear, a muted mode), the web gate's
   SIMD128 wasm against the scalar no-FMA reference stays bit for bit on the
   cell, the chain, the hybrid, Duffing, the rotor and the sax (the lattice,
   the flute and the trumpet within their libm tolerances as before), and
   BOTH reference records — the no-FMA scalar and the shipping FMA NEON —
   are bit-identical before and after the refactor, as are the twelve strobe
   return sets of step 75. The fusion matched because the intrinsics follow
   clang's contraction rule (the first product of `a·b + c·d`, the product
   of `a − b·c`): `simd.h` documents it.
   * **What the chain cost.** Sixteen chains of 80 nodes at 3.3 % of the
     callback on the Release build (the gate's bar 6 %), beside sixteen cells
     with their shears at 3.5 %. THE BARS ARE THE RELEASE BUILD'S: the Debug
     `build/` left the SIMD wrappers as calls (ten times the scalar, 150 %
     of the callback for the chains) until they were forced inline, and still
     measures with checks in; the suite asserts the headroom bars under
     `NDEBUG` only and prints the figure otherwise. `build-release` (Release,
     arm64, tests on) is where the number is read; `build-universal` has no
     tests and runs the storm. The structure-of-arrays across voices the
     roadmap named was not needed: within-chain lanes clear the bar.
   * **The lane widths.** Four everywhere (NEON, SSE2, WASM SIMD128), as #1
     said; AVX2 not taken.
   * **The oversampler's scope.** `oversample` (2 default, 4) is the CELL
     FAMILY'S — the cell and the lattice with their shears, kinds 0 and 1 —
     because that is where the fold is; the chains, the bores and the chaos
     voices keep the 2× section their grids are tuned to. The ×2 section is
     untouched (the two-tap average, bit for bit); ×4 adds two cascaded 2:1
     half-band stages (`suzu::HalfBand`; 47 taps, Kaiser β 5.65, the
     stopband 64 dB down — the 39-tap design read 59 dB, a hair under the
     gate, so 47) and threads the section's rate through the strike (ε, the
     decays, the bow's rate, the release) and the block (the SVF, the
     retune, the compensation), the mode ceiling pinned to the OUTPUT
     Nyquist. The strobe's sub-steps stay the 2× section's: at ×4 a wrap
     lands in the first or second quarter with its fraction rescaled (at ×2
     the mapping is the identity, bit for bit).
   * **The alias gate as measured.** The fold at full drive at C7: the floor
     in [12 k, 19.2 k] Hz is −112 dB under the fundamental at ×4 and −90 dB
     at ×2 (the red): both far under the roadmap's −60 — the symplectic
     fold's harmonics fall fast, so the aliases were never loud; C4 reads
     −160 / −129. The gate stands as the roadmap wrote it, with the red's
     gap (≥ 10 dB) as the test of the decimator; the voices that need the
     ×4 section (the Toda ring, the billiard: Phase 14) are why it exists.
   * **Found by the lab gate, a step-75 regression:** the bus BREAKER
     (`DECISIONS_10 #12`) cuts a non-finite block inside the engine — the
     voices end, the block goes out as silence — so the worklet's own
     non-finite check never fires any more and the lab's two blow-up red
     controls (the strings' CFL bypass, the winds' naive junction) read "not
     caught" since 0.17.0; the pre-78 wasm rebuilt in a detached worktree
     failed them the same way. Fixed on the web surface: `sw_breaker_trips`
     (Voxo's `breaker_trips`) and the worklet treating a new trip as the
     blow-up it is (the restart, the page told). The lab gate is green on
     all six pages; it is not in ctest and nobody ran it at 75–77 — a
     working rule from here: every Voxo change runs the web gate AND the lab
     gate.
   * **The storm** from the Release universal build on three session presets
     (the chain at 80 nodes, the trumpet, the fold at ×4) at 0 XRuns, render
     max 0.19 ms; the fuzz a minute clean, the strobe fuzz 30 s clean; the
     prewarp helper within 1e-7 of `eps_for`.
   **Carried:** the oversampler on the other kinds (their grids would need
   their own tuning at the section's rate), AVX2's eight lanes with a runtime
   dispatch, the structure-of-arrays across voices if a future voice needs
   it. The tablets run the patches on NEON through the same header: the
   Android emulator (arm64, the Tab unplugged) ran the Bowed string, the
   Trumpet and the Flute under the spike at 0 device XRuns; the iPad's run
   is built and staged but the device was locked at the install — the
   author's to unlock, the commands in the evidence.

3. **Step 79 — the membrane, the bus plate and the gong, as shipped (2026-10-19;
   Voxo 0.19.0, additive; evidence `docs/evidence/step79/`; the spec's §13.9
   rewritten as shipped).** `voice_kind` 9 is a 2D symplectic leapfrog grid
   (`suzu::Membrane<CAP>`, four lanes a row behind `simd.h`, bit for bit on the
   web gate), and the bus plate is one `Membrane<64>` on `voxo_t` at the output
   rate. The decisions:
   * **The boundary is simply supported** (u = 0 and Δu = 0 on the ring): the
     biharmonic is the Laplacian twice through one zero ring, the mode table is
     closed-form, and the suite pins the (1,1), (2,1), (1,2) to it within a cent
     for the membrane and the plate (gate delta d). The grid is a 16 : 13
     rectangle so the degenerate pairs split.
   * **Pitch through c², the grid cut per note** under the bar at E_max; the
     proposal's "0.000 cent by scaling the grid" dropped (gate delta a). `drum_kappa`
     is the biharmonic SHARE of the (1,1)'s stiffness, so the note stays on the
     (1,1) at any share and the CFL number is ε₁₁ times a factor of the grid.
   * **The cap is exact on the plain energy** (the momentum scaled to the
     remainder after every strike) and E_max = 2H_cap/(c² + κ²λ₁₁) follows by
     arithmetic — the roadmap's 2H₀/c² tightened by the biharmonic term (gate
     delta b). `drum_alpha` is the swell the player hears (c² × (1 + α) at the
     cap), α_raw derived from it.
   * **THE GONG'S BAR IS 1.80, NOT 1.95 — measured.** A pure membrane at swell 1
     displaced to exactly E_max at λ_2D 1.95 with the top mode seeded diverged: the
     (1,1)'s stretching modulates the top mode's stiffness by the whole swell at
     twice its own rate, and that close to the edge the modulation is not
     adiabatic (the linear gate is necessary, not sufficient). 1.90 held for swells
     to 2, the κ share lowers the depth, the gong patch's real strikes held at
     every note; the bar for a swelling sheet is 1.80 (`MEMBRANE_CFL_GATE_GONG`),
     the linear sheet keeps 1.95. The reds as the roadmap wrote them: 1.2× the
     cap's stretching on a sheet tuned to 1.95 diverges within 100 steps; the ABI's
     forced 1.05 is refused, and bypassed trips the breaker within a second.
   * **The damping on the momentum at twice the rate**, so `decay_s` is the
     amplitude's T60 as everywhere else (read back 1.19 s for 1.2); `decay_bright`
     the lattice's law per (λ/λ₁₁ − 1). The release multiplies the held
     contraction (the momentum's factor squared).
   * **The plate at 1×, clamped not refused.** The bus's rate is the output's;
     the plate's share and then its pitch are clamped under the gate with a log
     line (a bus element never leaves the patch standing) — a 64² plate at 48 k
     reaches about 360 Hz and is near-membrane (the share 0.003 at 1.48; gate
     delta c: 32² on a `VOXO_MOBILE` build). The voices inject their own samples
     at their points (a DRIVE, bounded) scaled by (n/64)², measured so 32² and 64²
     read alike; the mix at 0.045 puts the plate 10 dB under the dry harp chord.
     The point is the shell's: `voxo_set_strike_point` before the note-on — the
     iPad and the Tab hand the layout cell's centre from their probes (the
     roadmap's `sumi_layout_cells` does not exist), the desktop's keyboard and a
     wire take the default map (the note along x, a golden-ratio scatter along y).
   * **The drift gate** on the leapfrog's exact invariant H̃ (the staggered
     quadratic form), not the plain energy whose staggering wobble is 0.27 dB:
     −0.00016 dB over ten minutes.
   * **Found beside it:** the bench's `--preset` applied a session preset to the
     engine alone (`dev_apply_preset`), so a storm "on a preset" played whatever
     the Sound settings held — step 78's three storm lines were the settings'
     patch, not the chain, the trumpet and the fold. Now the preset's Suzu block
     reaches the settings for the run and the file is left unsaved; the three
     storms of this step ran the drum, the gong and the soundboard (0 XRuns,
     render max 0.79 ms). And the Android DEBUG APK's engine is −O0: its spike
     read 60 ms of render and hundreds of XRuns; the release APK signed with the
     debug key reads 10 ms and 0 device XRuns on the drum and the gong, one on the
     soundboard (the emulator's audio path; the Tab is unplugged). The iPad
     (M4) runs the three at 0 XRuns, render max 0.58 / 0.65 / 0.35 ms of its
     2.667 ms buffer — a CSV copied while the device was locked was a stale
     run's: a devicectl copy answers even when the launch was refused.
   **Cost:** sixteen 16 × 13 membranes 16.5 % of the Release callback, the 64²
   plate 16.3 %, 32² 4.5 %. **The patch table** holds fourteen: *Drum*, *Gong*,
   *Harp on a soundboard*, pinned to their JSON files by gate 28. **Carried:** the
   Tab's own run, the author's choice of the drum and gong references the profiles are
   read against (the profiles are the patches' own), the lab's membrane page
   (step 92), a strike profile from the layout cell (step 80), the AVX2 lanes.

4. **Step 80 — the stick-slip bow and the strike position from the cell, as shipped
   (2026-10-19; Voxo 0.20.0, additive; evidence `docs/evidence/step80/`; the spec's
   §13.10 rewritten as shipped).** `bow_mode` 1 is `suzu::StickSlip`: the hyperbolic
   friction curve solved implicitly against the point's mobility, bounded by the
   sticking limit, on three bodies — the lattice's bowing point, the Verlet
   chain's node and the hybrid string's loop (two taps against Z = 1: the textbook
   waveguide bow). The decisions and the findings:
   * **The hysteresis is part of the model.** Without the stick/slip branch
     (McIntyre–Woodhouse) Newton from the free velocity picks the slip root every
     step and every body whistles; with it the chain sticks 70 % of the period at
     the default force on the note, the waveguide 48 % rising to 68 % at twice
     the force. The branch is the voice's state (`bow_stuck`).
   * **The bow acts between the kick and the drift.** Applied after the drift a
     strong bow pinned the chain's node into a static deflection (the string's
     kick cancelled the bow's velocity before the node moved: the tone read as a
     tone a sixth low with a buzz). `VerletString::kick` / `drift` split the
     step; the fused `step` is bit for bit.
   * **The clean Helmholtz lives on the delay line.** The chain's lattice
     dispersion (its partials as sin of the mode number) smears the corner: its
     stuck share climbs gradually and its pitch wanders a dozen cents with the
     force. The hybrid's loop is dispersionless; the bow there is two taps on the
     ring — the wave about to pass the bow and, inverted, the wave from the
     bridge side — and the loop gains exactly f·v a sub-step. The roadmap's
     "the string's or the mode's impedance" is read as both the node (G = 1) and
     the waveguide's 2Z (G = ½). The patch *Bowed string (stick-slip)* is the
     waveguide; *Bowed chain* the chain.
   * **The ledger's gain is read at the synchronized velocity.** The leapfrog
     invariant's change under a kick at the half-step is Δp times the mean of the
     half-step velocities around it; read at the post-kick velocity the chain's
     gain summed to −5.3 against 4.1 of work with the energy flat — impossible.
     With every loss zeroed the balance is now exact per quarter second on both
     bodies (`gain_probe.cpp`).
   * **The red is the raw Newton, not the explicit force.** A converged implicit
     root never crosses the bow, and the explicit force's excess is friction
     heat; neither crosses the roadmap's line E_held ≤ ∫F·v_bow, which sits a
     hundred times above the energy held. The tight line is per step — what the
     string takes, ∫f·v, against the bow's work, ∫f·v_bow — and the three
     Newton steps taken raw on the folding flank cross it: 0.156 against 1.46 on
     the chain, 7.0e4 against 6330 on the waveguide. Both lines are the gate.
   * **The controls:** the mouth is the speed, the press and the velocity the
     force (×2 at a full press, ×½ at a soft touch) — §13.3's "press as a
     sustained drive" is this. The scales measured: KV 2.3 / 1 / 0.45 and KF
     6 / 1 / 15 (the chain / the lattice / the waveguide), v₀ a tenth of the
     speed, μ_s 0.8, μ_d 0.3. `bow_position` is the bowing point on the strings
     and the profile on the lattice.
   * **The web gate drops the lattice under the bow:** the branch is a
     discontinuity, and the lattice's per-sample sinf flips one at a sub-step,
     moving the snapshot's decimated trace past any tolerance while the audio
     holds within −80 dB; the chain and the waveguide under the bow are bit for
     bit. The first twelve sections' audio and traces are bit-identical to the
     step-79 reference (the inspection records carry the new k slots).
   * **The strike position from the cell** (`voxo_set_strike_pluck`; the
     tablets' touch-begins map the distance from the cell's centre, the centre
     the middle, the rim the bridge) replaces `pluck` on the chain, the plucked
     lattice and the membrane; the bar keeps its fitted 1/m.
   **Gates:** the suite's 39–41 green (the stuck share rising with the force on
   both strings, the ledger's two lines, the red on the chain, the lattice on the
   note, the servo bit for bit under the new knobs); the web and lab gates green;
   the storms on both patches at 0 XRuns (render max 0.26 ms); the fuzz clean;
   ctest 14/14; the chart `bow_force` and the two profiles. The emulator's
   release APK ran both patches at 0 device XRuns (the spike has no press: the
   bow lifts; the load's evidence); the iPad took the build and locked before the
   launches. **Carried:** bowing under the finger on both tablets is the
   author's; "a struck bar's" strike profile (the VCSL fits would move); the
   lattice's pitch pull under the bow; the raucous pin on the chain above the
   playable range; the bowed lattice in the web gate.

5. **Step 81 — the acoustic bore v2, as shipped (2026-10-19; Voxo 0.21.0,
   additive; the core frozen).** The spec draft's §13.11 is rewritten as
   shipped; the proposal's four items each took a correction on the probe.
   * **The toneholes are branch ports, and the pickup had to move.** A hole
     is an inertance kicked by its node's pressure at the half step (as u),
     radiating through a one-pole resistance at twice the bore's corner, its
     flow in the node's continuity; closed, a wall. The first build read the
     bore at the far end as before, and the holed flute's fundamental sat
     40 dB under its fifth partial — the oscillation inside was right (the
     pitch landed), the far end sits past the lattice where the fundamental
     is evanescent. The output is now what leaves the bore: the end's
     velocity plus the open holes' flows. Found on the way: a flat hole
     resistance ate the fundamental (the hole's R was above its reactance at
     the pitch — the one-pole form of the ends' fixes it); the hole's size
     must be physical (1/L_h = S/8 at the node: a flute's hole is eight
     cells' inertance; at S/1 the scheme grew at 0.99 of the bound and the
     holes cut nothing cleanly); the hole mode takes room under the CFL
     bound (Gershgorin, λ ≤ λ_bound/√(1 + h/4): at 0.999 of the bound the
     third register's bores grew); a cone's holes count the apex (the sax's
     corrections saturated at +300 cents until they did); a hole sits at a
     fractional cell, split over two nodes (the top register's semitone is a
     cell and a half); the vent is a real register key (90 cells' inertance
     at an eighth of the length — a quarter-hole at a third detuned the
     second register by 230 cents), its detune calibrated per register.
   * **The calibration is the woodwind maker's, per register, and the sax's
     is blown.** Each fingering pulsed on the register's bore, the first peak
     read, the hole moved by the miss, twice; on the sax the passive peak and
     the reed's note differ by up to a quarter-tone on the hole bores (not
     the plain bore's pull), so the second pass BLOWS each fingering (0.4 s:
     the upper register takes a third of a second to settle; a 0.25 s blow
     left the second register 50 cents flat). The cost at patch load: 228 ms
     (the flute), 770 ms (the sax), cached per patch slot by the holes'
     parameters. The holes serve two registers (D4..C♯6); the third is past
     the lattice's cutoff — a hole's impedance is the bore's and it no
     longer cuts (measured: −60…−100 cents however the holes were moved) —
     and takes the v1 cut, as does any note the keys disagree with.
   * **The quasi-periodic A3 was the bore's slow mode, not the reed's
     inertia.** The proposal's L_r·dq/dt ships as proposed (dflt 0) but
     barely touches the ±16 Hz sidebands (L_r 8 cleans breath 70 only, 30
     degrades). The one-pole radiation port leaves the bore's slow modes
     undamped; a resistance on the bell's slow flow (a one-pole at 40 Hz,
     z_l 0.4, positive-real) takes them: −62…−63 dB at breath 50/70/100
     (the red: +13 dB). Its reactance is a negative end correction the cut
     counts (uncounted: the flute +40 cents sharp). AND THE LEAK IS THE SAX'S
     ALONE: on the flute's two open ends and the trumpet's bell it cost the
     bass (22 dB at the flute's C2, 7 at the trumpet's) — the leak at 65 Hz
     is a hundred times the radiation loss there. The flute's profile is
     back within 7.8 dB over C2..C7. And a leaky bell takes room under the
     CFL bound (0.96): the sax at G7 — thirteen cells, λ 0.989 — blew up
     under z_l 0.4, a full-scale spike at one note of the profile that no
     gate reads (the sound profile does, again).
   * **The shock is dissipative by a flux limiter, and the local-CFL story
     was not the ringing's cause.** The local Courant number on both sweeps
     (on the momentum sweep alone the steepening was too weak), saturated at
     20 P_REF with the base λ leaving room; the Lapidus flux limiter's loss
     logged in the ledger (`k[10]`). Measured at C4/127: the blare +10 dB
     over the v1 bore in the 6–20 kHz band. THE RED moved under the probe:
     with the leak at the trumpet's bell the limiter off had rung 23 dB above
     the limited in the output's high band; without the leak (where it ends)
     the ringing stays in the bore — its profile 1.46 rough against 1.01
     limited, the bell keeping it out of the output — and at twice the
     steepening the unlimited shock is refused by the valve gate at patch
     load (the junction makes energy with the losses zeroed, 1.1× the
     mouth's work: the 58c gate caught it) while the limited one is
     admitted and holds. The gate reads both. The trumpet's λ sits at 0.28
     (its cells capped), so the ringing is the dispersion at a shock, not a
     Courant crossing; the saturation stays as the safeguard. The roughness
     is a relative measure (a linear bore reads 0.70; read against the
     limited bore's at the same note). Under the limiter more steepening
     dulls (shock 2: −32 dB, 4: −37): the blare peaks near κ, the default.
   * **The shock is the trumpet's; the sax keeps the v1 bore.** Built on
     both valve winds, the shock cost the sax 80 % of its render for no
     claim the spec makes of a cone. And the trumpet's storm threw XRuns at
     thirteen voices (render 2.0 ms of 2.67): the limiter's passes vectorise
     but a running sum for the loss serialised them (790 µs a block against
     the v1 bore's 380); the terms are written beside the updates and summed
     after (615 µs, the loss identical). And every wind strike ran the
     bore's power iteration twice (inside the build on the fresh 64-cell
     bore, again on the cut one), 40–60 µs each: a block carrying fifteen
     strikes cost the trumpet 1.5 ms. The profile's μ_max is taken once per
     patch (`WindCal.mu64`), the flute's once per engine. The storms
     pass.
   * **The two-mode lips ship as a knob** (`lip_shear`, dflt 0): the
     registers hold at share 0.3; at 0 the one-mode lips render bit for bit
     under the ratio. The author's ear decides the trumpet's default.
   * **The web gate: the sax joins the libm class** (−80 dB) — its
     calibration's log2f/exp2f on the pull; the leak moved the pull to
     values whose last bit differs (measured −102 dB).
   **Gates:** the suite's 42–45 green with their reds (the sax's sidebands,
   the shock's ledger and its Gibbs red, the lips, the holes on both
   woodwinds: the chart's twelve fingerings, the registers, the disagreeing
   keys); the web and lab gates green on the final wasm; the fuzz clean (the
   strobe 0 blocks differ); ctest; the storms on the three winds from the
   Release universal build (the trumpet 0.97 ms, 0 XRuns); the three
   profiles; the emulator's release APK on the three winds at 0 device
   XRuns; the iPad's three runs at 0 XRuns (render max 0.42 / 0.41 /
   0.70 ms); the Tab at its 4 ms low-latency burst throws one to four XRuns
   on the winds — and did at step 80 too (the trumpet's render max 4.0 →
   4.3 ms there): twelve wind voices at 4 ms were at the Tab's edge before
   the step. **Carried:** the hole mode's third register; the sax's hole
   intonation to ±10 cents; the lab's shock-limiter and leak reds on the
   winds page; the tablets' key panels driving the holes (the Boehm
   layout's keys reach the engine already; the mode is a patch knob); the
   Tab's wind headroom at 192 frames (a mobile cap on the voice count, or
   the spike's burst, is the author's call).

6. **Step 82 — the editable modal table, the Scala tuning and the unison pair,
   as shipped (2026-10-19; Voxo 0.22.0, additive; the preset schema 2; the core
   frozen).** The spec draft's §13.12 is rewritten as shipped.
   * **The table is `modal_preset` 5** with three arrays beside the knobs
     (ratio, t60, weight; a zero defers to the formula), read by the one
     table builder every call site already used; `voxo_suzu_table` hands any
     preset's resolved table out so a shell edits from the formula's values.
     The compensation becomes a flag (`modal_comp`): a unison pair has no
     separate targets for the solve — the load gate's spectral radius ran
     the solve on the raw ratios, diverged (16.6 against 4) and refused the
     patch until the raw bound was its own; and under the raw lattice THE
     SWIRL ADDS NOTHING to the coupling, for the same reason the
     compensation exists (0.5·κ would detune every mode).
   * **The split is the chain's, not the pair's.** The proposal's figure
     assumed √(1 + 2κ); the first pair's two normal modes sit at
     +κ(3 − √5)/4 and +κ(3 + √5)/4 of the frequency, one mode having one
     neighbour and the other two — measured +3.25 / +22.50 cents at κ 0.01
     against +3.30 / +22.37. The patch's κ 0.006 beats 11.6 cents apart.
   * **The schema is 2.** The suzu block's arrays and `modal_comp` ride as
     JSON arrays; a schema-1 reader keeps what it knows. The preset test's
     three schema-1 assertions moved. `sumi_preset_suzu_to_voxo` is the one
     file-to-params mapping with every step's sentinel, in the presets
     library (types only); the gate pinning the patch table to its files
     now reads the arrays element for element and skips the serializer's
     own keys (the trace routes, `oversample`) — the unison file is the
     first one written by the serializer itself.
   * **The tuning is one site.** `voice_retune` was already the only place
     a note became a frequency, for Suzu and the sampler alike; the Scala
     table sits under it, double-buffered, the bend an equal-tempered offset
     on the tuned note. The parser takes the files' text (the tablets read
     through their pickers) or their paths; it refuses with a reason. 19-EDO
     reads 0.0001 cent off the formula over the keyboard, the cell 0.0003
     at the oscillator; Pythagoras's fifth 1.5000000 under a .kbm.
   * **The tablets expose the table and the file as files**: a Suzu patch
     .json through the document picker (the table inside it) applied over
     the table's patch, and a Scala .scl, both copied beside the instruments
     and persisted; a sixteen-row editor stays the desktop's.
   **Gates:** the suite's 46–48 green (the table resolved / inspected / at
   the oscillator with its decays; the Scala table, oscillator, mapping and
   refusal; the unison's two lines and its deaf swirl); `preset_tests`
   40/40 (the round trip whole); the patch table's seventeen pinned to
   their files; the web and lab gates green; ctest; the storms; the unison's
   profile; both tablets under the spike on the unison. **Carried:** a table
   editor on the tablets; the tuning inside the session file; the lab's
   table; §14.6's microtonal mapping (the core's, as the roadmap says).
