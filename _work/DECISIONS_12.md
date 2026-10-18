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
