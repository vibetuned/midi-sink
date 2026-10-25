# Step 86 — The per-voice chaotic modulator and the four-slot mod matrix — evidence

Date 2026-10-25. Voxo 0.26.0 (additive); the core frozen (libsumi 1.12.0 untouched). Decision entry `DECISIONS_13 #4`
(`_work/DECISIONS_13.md`); the spec draft `_work/TO_PROJECT_SPEC.md` §13.14 as shipped; the book
`site/src/content/docs/suzu/strings-and-chaos.mdx` (the modulator's section rewritten).

## What shipped

* `voxo/src/voxo.cpp`: the pendulum in `SuzuVoice` (`pend`, `mod_smooth`, `modv[8]`), kicked at the voice's strike,
  stepped by `suzu_mod_compute` in the voice's block when a slot reads it; the instance's pendulum and its bus step
  removed. The matrix's sums per target: the cutoff (×2^{3m}), the coupling (+0.5m into κ), the rotor's K (+m), the drive
  (×(1+m)), the level (the output ramp ×(1+m), floored), the pitch (the block's frequency and glide ×2^{m/12}). The legacy
  pair folded into slot 0 at `voxo_set_suzu_params`; the clamps. The inspection's `mod_out` and `mod_sum[8]`.
* `voxo/include/voxo.h`: `mod_src[4]`, `mod_dst[4]`, `mod_amt[4]`; the inspection's two fields; the docs.
* `voxo/src/suzu.h`: `svf_f_for` falls back to the plain coefficient outside the ringing regime (FOUND); `voxo.cpp`'s cutoff
  ceiling follows q (FOUND).
* The presets library: the three arrays of four (`r_farray` reads all three, the integer ones as numbers), `SCHEMA.md`, the
  helper, the preset test's round-trip, the generator's `--slot i src dst amt`; `presets/chaos_matrix.json` (the bell under
  all four slots — the storm's).
* The desktop: the *Suzu mod matrix* rows (four sources, targets and depths; the modulator's rate), the INI's three lists,
  the legacy pair folded into slot 0 and cleared; `--preset` and the main's params.
* The web: the engine's twelve scalars (`mod_src0` …), the gate script's matrix section on the Duffing (the press to the
  cutoff, the slide to the pitch, the modulator to the level, the wheel to the drive).
* `tests/voxo_suzu_tests.cpp`: gates 63–64; gate 28 reads the three arrays.

## Gates (the Release suite, `suite_release.txt`: 256 green; the Debug ctest `ctest_debug.txt`: 14/14 — the preset test
round-trips the slots)

| gate | reading |
|---|---|
| 63 per voice | A3 at 127 (tumbling) −0.425 and A4 at 30 (swinging) +0.453 after 2 s — two pendulums, bounded, apart; A3 struck again restarts its own (+0.510, off its course) and leaves A4's the same bits |
| 64 the pitch | the slide at a depth of 1 moves the cell +100.1 cents at 127 and −99.9 at 0 |
| 64 the level | the wheel at −0.5 takes the cell −6.02 dB (the sum −0.500) |
| 64 the cutoff | the press at −1 takes the plucked chain's 1–4 kHz band from −52.7 to −71.1 dB against its 50–400 Hz band (the sum −1.000) |
| 64 the legacy pair | the rotor under `mod_target` 3 / `mod_depth` 0.5 renders the same bits as the slot (1 → 3 at 0.5); K's sum +0.450 |
| 64 FOUND | the bell under a 4 kHz cutoff at zero resonance sounds (the solver's fallback; f at most 1.04 over the keyboard at q 2, 1.5, 1); under a 15 kHz cutoff at zero resonance and under the press pulling the cutoff from the ceiling it sounds within 0 dBFS; the SVF's eigenvalues at the q-following ceiling under 1 over five dampings and both rates |

The web gate (`web_and_lab_gates.txt`): every section as before, the matrix section on the Duffing within its tolerance
(−92 dB); the lab gate GREEN (6 pages). The storm (`storm.txt`): fifteen channels for twenty seconds on
`presets/chaos_matrix.json` — the bell with all four slots — 0 XRuns, the render at most 0.30 ms, eleven voices at the end
(the first run on the un-fixed ceiling ended with no voice: the breaker, 44 trips in the probe).

## The tablets (`tablets/`)

* iPad (voxo 0.26.0): the bell 0 XRuns at 0.22 ms of the 2.67 ms burst; the Toda ring 0 XRuns at 0.42 ms.
* The Tab was UNPLUGGED; the Pixel_10_Pro emulator stood in: the bell 0 XRuns at 1.15 ms of a 20 ms burst; the Toda ring
  0 XRuns at 2.11 ms.

## Found at the step (the probes `/scratchpad/bell86.cpp`, `svf86.cpp`, `svf_solver.cpp`, `mat86.cpp`, not kept)

* Step 58's modulator declared four targets and wired two: the cutoff and the coupling never routed. They do now, through
  the matrix.
* Since step 56: a cutoff under the ceiling at zero resonance silenced every voice — `svf_f_for`'s ringing compensation
  solved for a ring an overdamped filter has not and ran past stability (q 2 at 4 kHz, 96 kHz: f 3.6 for a plain 0.26;
  nan; the breaker). The solver returns the plain coefficient outside |cos θ| < 1, fq < 1.
* Since step 57: the fixed fs/6 cutoff ceiling sat past the classic SVF's stability bound f² + 2qf < 4 at low resonance
  (q 2: f < 0.83, a cutoff of 0.13 of the rate); the storm's cutoff slot tripped the breaker 44 times in ten seconds. The
  ceiling follows q, the fs/6 kept above it.
* The chain's modes compress toward the top, so a brightness gate cannot read a partial at k·f — it reads a band ratio.

## Files

`suite_release.txt`, `ctest_debug.txt`, `web_and_lab_gates.txt`, `storm.txt`, `tablets/`.
