# Step 74 — The chord-wave lattice, route (a) (2026-10-14)

libsumi **1.10.0**, additive (`DECISIONS_10 #11`; `_work/ROADMAP_6.md` step 74;
`_work/TO_PROJECT_SPEC.md` §10.8.2 marked with what shipped; `KESSHO §2.3`).

## What shipped

* The ripple pass given a window along its second coordinate (`win_c`, `win_w`
  in `sumi_deform_ripple_t` and `ripple_fs`; 0 = the v2 ripple, bitwise). A
  shear by a function of s₂ alone, windowed or not: exact. No new pass type,
  entry point or params.
* Under Kessho a held note's poly pressure (0xA0) is the amplitude of a
  ripple-tine through its drop: the axis by the pitch class on the circle of
  fifths (15° a step), the wavenumber by the octave (4 periods a canvas at
  octave 3, doubling up, 2..32), the window 2.5 drop radii, a crest at the
  note, 0.02 canvas at full pressure, delta-driven as the ripple bake (held
  adds nothing, easing off takes it back, the wave stays baked when the note
  lifts); one band a voice. The voices compose as separate passes.
* The book's Chord wave page, the index row, the settings reference's note;
  the bench's `--chord-wave-test`, `--soak chord_wave`, `sumi_debug_add_chord_wave`.

## Gates

| # | Gate | Result | File |
|---|---|---|---|
| 1 | the four §10.2 gates, `--soak chord_wave` | green: (b) the pairs at ±A −1.07 % / 8.45 texel; (c) +0.00 %; (d) ×0.20 the control | `soaks/soak_chord_wave.txt` |
| 2 | flatness, `--kessho-flat chord_wave` | median 2.75e-3 at ±3 — the field's floor | `soaks/flat_chord_wave.txt` |
| 3 | one note (G3) pressed | across its band the wave's displacement (the strike's field subtracted) fits A sin(ω s₂ + φ) w(s₂) with \|corr\| 0.978, amplitude 10.2 texels, 0.00 outside the band; the bare pass reads the same profile a quarter canvas along the axis to 0.47 texel | `chord_wave_test.txt`, `renders/chord_wave_one_*.png` |
| 3 | a fifth (C3 + G3) pressed | two bands at 0° and 15° crossing at (0.60, 0.39): G's component alone along G's band (13.5 texels; residue 2.0), C's alone along C's (13.1; 2.3), both at the crossing (19.9 and 19.9) — the lattice; a triad archived | `chord_wave_test.txt`, `renders/chord_wave_fifth_*.png`, `chord_wave_triad_kessho.png` |
| 4 | route (b) | deferred: the local rosettes wait on the author's viewing of these renders (`DECISIONS_10 #11`) | — |
| — | the frozen paths | the field and composite fixtures bitwise on Metal; ctest 13/13 (the records pinned headlessly); the web shell builds | `gates/`, `normalizer_chord_wave.txt` |

## Open for the author

* Route (b), the quasicrystalline rosettes: go or defer, after these renders.
* The angle step (15° a fifth), the octave's wavelength law, the band's width
  (2.5 R) and the amplitude (10 texels at full pressure).
