# Step 75 — The Poincaré strobe, the trace's second mode (2026-10-14)

Voxo **0.17.0** and `trace/`; **no core change** — the core is frozen again
from here to step 91 (`DECISIONS_10 #12`; `_work/ROADMAP_6.md` step 75;
`_work/TO_PROJECT_SPEC.md` §10.9 marked with what shipped; `KESSHO §2.5`).

## What shipped

* **Voxo:** the reference phasor at the bass — the lowest held voice's
  frequency, bend included (the lowest sounding one when none is held),
  advanced per sub-step in double along the block's glide line, the block's
  wraps precomputed (≤ 64 a block); at each wrap every traced voice's pair,
  interpolated between the two sub-steps the wrap falls between, into a
  per-voice strobe ring of 256 with the kind's aux pair. `voxo_set_strobe`
  (0 off, 1 the bass phasor, 2 each voice's own drive — the Duffing's phase
  wrap, the rotor's kick: Chirikov's section), `voxo_strobe_poll`
  (`voxo_strobe_t`), `voxo_strobe_hz`. A reader: bit-identical audio, no
  allocation. Found by the step's fuzz and fixed: a patch of another voice
  kind over sounding notes ends them (the old bodies were NaN); the flute's
  jet and bore are built for the pitch up to rate2/16 (a +47 semitone bend
  on B5 was NaN); the bus's breaker (`voxo_stats_t.breaker_trips`).
* **`trace/`:** `sumi_trace_config_t.mode` (continuous / Poincaré on the
  bass / Poincaré on the own drive); the bridge's `poincare_frame` —
  `OrbitDust` per voice (the newest 128 returns) for the settings miniature
  and the canvas scope (dashes, round-robin to 128); the ink route stamps a
  micro-drop (Sumi), a charge speck (Anod — the continuous trace too, one
  along the orbit at the same rate; the author's choice) or a micro-swirl
  (Kessho) through the gesture hook
  at about 24 returns a second per voice by a fractional stride (every
  return floods; an integer stride aliases a 2-cycle), the frame budget of
  24 shared fairly.
* **Desktop:** the *Suzu trace mode* combo, `suzu_trace_mode` (INI),
  `trace_mode` (preset, `SCHEMA.md`); the miniature's dust. The bench's
  `--strobe-test`; `tools/strobe_chart.py`; the book's strobe section and the
  settings reference row.

## Gates

| # | Gate | Result | File |
|---|---|---|---|
| 1 | 1:1 → one stationary point, < 0.5 texel | the undamped cell at A2 strobed at its own frequency, 2 s: 219 returns within **0.001 texel**; on the live core (A3, the ink route through the hook) 95 stamps within 0.001 texel | `suzu_tests.txt`, `strobe_test.txt` |
| 2 | 3:2 → two points | E3 bent to the just fifth (+3 units; the residual 0.20 cents): 109 returns alternating between two points π apart, each class within **0.120 texel** over every tenth of a second; the equal-tempered fifth precesses its pair 1.132 rad/s (1.128 from 1.955 cents); on the live core 47 + 46 stamps within 0.011 rad of the pair's axis, precessing 0.0011 rad a return | `suzu_tests.txt`, `strobe_test.txt`, `strobe_csv/strobe_sections.png` |
| 3 | +50 cent bass bend shifts the strobe's rate by the ratio | the bass bent +49.8 cents: the phasor at 113.2105 Hz against 110 (ratio 1.02919 = 2^(49.8/1200) to 1e-5); 1132 returns in 10 s against 1099 | `suzu_tests.txt` |
| 4 | the rotor sweep K 0.3 → 2.5: a closed KAM ring → dust, charted | by its own kicks, undamped, 12 s each: over the first 440 kicks 10 / 12 / 20 / 18 / 47 / 22 % of a 24 × 24 (θ, p) grid, \|p\| ≤ 1.11 at K 0.3 (the separatrix's eye from the strike's hyperbolic point); **the strobe's finding:** at K 2.5 the rotor sinks into a period-4 cycle within ~10 s (thirds 22 → 7 → 6 %) — its section is the standard map only approximately (the retune's re-basing); Phase 12's | `strobe_csv/`, `strobe_sections.png` |
| 5 | bit-identical and zero-allocation | kinds 0, 1, 4, 5, 7, three bent voices, polled every block: equal to the unstrobed rendering bit for bit; 0 global allocations, the malloc zone's blocks unchanged; the fuzz's strobe run 120 s: 0 blocks differing from the unstrobed twin | `suzu_tests.txt`, `fuzz_strobe_120.txt` |
| — | the tritone (√2) and the Duffing's own drive | the dotted ring (sixteen bins, radius within 0.5 %); the Duffing 219 returns in 2 s by its drive, a three-point cycle at drive 0.6 | `strobe_sections.png` |
| — | the live core (`--strobe-test`, 8 checks) | the scope route alone bit-identical to the untraced run; the ink lands under Sumi and Kessho, and under Anod as charge specks in both modes; the rotor's dust inked; the budget held (peak 3 a frame) | `strobe_test.txt`, `renders/` |
| — | the Voxo faults the fuzz found | the kind change ends the voices (finite, the next note sounds as the trumpet); the flute at B5 +46.9 semitones finite and sounding, the breaker untouched; the breaker trips on the lab's forced-unstable chain and the next note sounds; the suite's four red controls read the breaker's trip | `suzu_tests.txt`, `winds_bend_probe.txt` |
| — | the frozen paths | the field and composite fixtures bitwise on Metal; the Suzu web gate GREEN; the preset round trip 40/40; ctest 14/14 (`voxo_fuzz_strobe` is the fourteenth; `voxo_c_compile` pins 0.17.0) | `gates/` |

## Open for the author

* The ink rate (23.7 a second a voice) and the stamps' sizes (a texel and a
  half; the micro-swirl's strength 0.06 at a core of 3 r) — `[ITERATE]`.
* The Anod charge specks' look, by eye (`renders/strobe_anod.png`, `trace_anod.png`).
* The rotor's symplecticity in (θ, p) — the strobe is Phase 12's instrument.
* The tablets' *Synth trace* mode rows (their own sub-steps).
