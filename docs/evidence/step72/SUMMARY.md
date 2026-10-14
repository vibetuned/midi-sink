# Step 72 — The Arnol'd tongue lock (2026-10-14)

libsumi **1.8.0**, additive (`DECISIONS_10 #9`; `_work/ROADMAP_6.md` step 72;
`_work/TO_PROJECT_SPEC.md` §10.8.1 marked with what shipped; `KESSHO §2.2`).

## What shipped

* `SUMI_DEFORM_ARNOLD` (`deform.glsl`'s `arnold_fs`): one iteration a frame of
  M = twist ∘ kick about a voice's drop in the action-angle frame — the kick
  from the generating function F₂ = θρ₁ + (K/q)W(ρ₁)cos q(θ − φ), the
  Lamb–Oseen twist with its core at the ring turning Ω₀ turns a frame,
  windowed to zero by 2.5 R; the lookup is M⁻¹ in one pass (the twist undone,
  one Newton on θ — five iterations — ρ explicit); `backward` = 1 the exact
  inverse (one Newton on ρ₁). The CPU reference and the pure parts in
  `core/src/displacement.cpp` (`sumi_arnold_tongue`, `_kmax`, `_window`, `_eval`).
* `sumi_add_arnold` (the gesture), press_mode 3 live — Kessho's press default —
  the press is K, the interval against the lowest held note sets Ω₀ and the
  tongue's q (the continued-fraction convergents of order 2..6), the kick's
  frame precesses by 2πδ a frame (the beat); the bass keeps the swirl;
  `SUMI_CTL_ARNOLD_K` the CC alternative; a tongue change crossfades K
  through zero over 50 ms.
* The settings' *Arnol'd tongue lock* press choice; the book's Arnol'd page,
  the index row, the settings reference's row.
* Corrections to the proposal (the entry): K_max = 0.8/max|W′| (no factor q),
  the window zero at the centre, five Newton iterations, the twist's cutoff,
  the trivial convergents excluded, the integer ratio a full turn.

## Gates

| # | Gate | Result | File |
|---|---|---|---|
| 1 | the four §10.2 gates, `--soak arnold` | green: (b) through the exact inverse pass −0.13 % / 6.13 texel; (c) +0.00 %; (d) ×0.09 the control | `soaks/soak_arnold.txt` |
| 2 | flatness, `--kessho-flat arnold` | medians 8.3e-3 / 4.3e-3 / 3.1e-3 at ±1/2/3 — the field's floor (`DECISIONS_10 #4`) | `soaks/flat_arnold.txt` |
| 3 | the Newton (headless) | residual 9e-12 at K_max over 3721 points, M⁻¹(M(P)) = P to 1e-12, \|det J − 1\| 2e-6, the identity beyond the window exact; RED at 1.3 K_max: 1e4 | `normalizer_arnold.txt` |
| 3 | the pass against the closed form (GPU) | 40 texels to 0.121 texel; the kick then its inverse pass 0.376 texel from the identity (with the half-turn twist 1.16: two resamplings of a 150-texel move) | `arnold_test.txt` |
| 4 | the islands stationary (exact Ω₀ = 1/2 and 1/4, 600 iterations) | the q-fold pattern's angle within its island: excursion 0.298 rad (q 2), 0.138 rad (q 4); the ink librates, the islands stand. The MIDI fifth (2^(7/12), two cents under 3:2) precesses at −0.0018 turns a frame (δ = −0.00169): the temperament's beat | `arnold_test.txt`, `renders/arnold_lock_q*_sumi.png` |
| 5 | the precession (+10 cents on the tempered fifth) | 0.00694 turns a frame against δ = 0.00695 (0.2 %), with the twist's sense | `arnold_test.txt`, `renders/arnold_precess_sumi.png` |
| 6 | RED, the twist's role | without the twist no islands: the 4-fold harmonic 0.293 over the 2-fold 0.010 (the ink on the kick's nodal lines); with it the two islands, 0.084 | `renders/arnold_notwist_red_sumi.png`, `arnold_twist_sumi.png` |
| 7 | the tongue crossing | headless: across a two-semitone bend (q 2 → 5 → 3) \|ΔK\| ≤ 0.26 K_max a frame, every q change through a frame at K = 0, \|Δφ − 2πδ\| ≤ 2e-7, \|ΔΩ₀\| ≤ 0.0014; the bench's band flicker mean 0.09, max 0.17, 0.145 at the first change | `normalizer_arnold.txt`, `arnold_test.txt`, `renders/arnold_crossing_sumi.png` |
| — | the frozen paths | the field and composite fixtures bitwise on Metal; ctest 13/13; `--kessho-test` 17/17 and `--habit-test` 9/9 under the new press default; the web shell builds | `gates/` |

`renders/arnold_{fifth,third}_locked_*.png`, `arnold_fifth_10cents_*.png`,
`arnold_second_offtongue_*.png`: the lock in Sumi and Kessho (the upper voice
struck first — the first drop is ink, the second clear); `renders/probe_k*`:
the kick regimes 0.1 .. 1 of K_max through the gesture.

## Open for the author

* The stroboscopic rate: one iteration a frame at Ω₀ turns is the spec's; the
  half-turn-a-frame twist of a fifth shears the drop's interior to grey within
  a second while the islands stand (the spec's own `[ITERATE]`: a slow-motion
  factor).
* The 50 ms crossfade; the Kessho look of the lock (the cool disc, the
  crescents); the tempered beat as a feature.
