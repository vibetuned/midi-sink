# Step 73 — Kirchhoff chord braiding (2026-10-14)

libsumi **1.9.0**, additive (`DECISIONS_10 #10`; `_work/ROADMAP_6.md` step 73;
`_work/TO_PROJECT_SPEC.md` §10.8.3 marked with what shipped; `KESSHO §2.4`).

## What shipped

* bend_mode 5 live, Kessho's bend default: every sounding voice a Lamb–Oseen
  vortex at its drop whose circulation is its consonance against the lowest
  held note (`sumi_consonance`, Sethares over six partials on three anchors —
  the unison +1, the tritone 0, the minor second −1 — the register floored at
  C4), Γ_k = sign(C)·Γ_vel·(0.3 + 0.7|C|) with the floor made continuous
  through C = 0; the ink advected by the chord's superposed flow as the exact
  swirl passes in Strang order, 2N a frame. No new pass, no new entry point.
* `params.braid_mode`: pinned (the vortices at the keys) or free — Kirchhoff's
  point vortices under the N-vortex Hamiltonian by the implicit midpoint rule
  in double (`sumi_braid_step` / `_invariants`), re-anchored at the strike,
  fading over the release, clamped 5 % off the edges. `SUMI_CTL_BRAID_COUPLE`
  live. The voice's bend kept in every mode (`bend_s`): the lock reads it too.
* The settings' *Braid consonance* (bend) and *Braid vortices* (pinned / free);
  the book's Braid page, the index row, the settings reference's rows.

## Gates

| # | Gate | Result | File |
|---|---|---|---|
| 1 | the passes and the frame time at fifteen voices | 30 swirl records a frame headlessly (2N; the bench's chromatic cluster 28: its exact tritone carries no vortex); 8.3 ms a frame at 512² and 1024² (60 fps) | `normalizer_braid.txt`, `braid_test.txt` |
| 2 | flatness, `--kessho-flat braid` | median 8.5e-3 at ±3 — the field's floor (`DECISIONS_10 #4`) | `soaks/flat_braid.txt` |
| 3 | a fifth's dyad: the saddle between the cells | after 300 frames the stagnation point at 54 % of the way from the bass, its material moved 1.8 texel, Δ there +0.51; the cores −3.6 and −2.6 (elliptic); the figure in Kessho | `braid_test.txt`, `braid_fifth_*.png` |
| 4 | a minor second's dyad: the jet | the ink's centroid 8.4 texels across the key line in 120 frames (0.7 along it) | `braid_test.txt`, `braid_second_sumi.png` |
| 5 | the second bent to the fifth | the material moved at most 0.25 texel a frame; the jet's least-moved material 22 texels upstream, the fifth's 0.4 — a stagnation point appears, elliptic at the second's geometry (the cores overlap: the cells 0.070 apart, the cores 0.087); the records' sign flips with \|ΔS\| ≤ 5 % of the bass's a frame (headless) | `braid_test.txt`, `normalizer_braid.txt`, `braid_bend_sumi.png` |
| 6 | the free vortices' invariants, ten minutes | \|ΔH/H\| 3e-9, the centre of vorticity 4e-16, the angular impulse 6e-16; the fixed point in ≤ 6 rounds | `normalizer_braid.txt` |
| 7 | A/B B-1, `--ab braid` | a triad (the bass, its fifth, its minor second) 600 frames, pinned and free, Sumi and Kessho — the author's viewing | `braid_ab/` |
| — | the four-part gate, `--soak braid` | (b) the two-vortex Strang pair at ±Γ −0.70 % / 32.3 texel against the bar scaled for a four-pass pair (52); (c) +0.00 %; (d) ×10.9 the control — INFORMATIONAL: 2N full-sheet resamplings a frame are the composition's nature (`#10` lists the mitigations) | `soaks/soak_braid.txt` |
| — | the frozen paths | the field and composite fixtures bitwise on Metal; ctest 13/13; the web shell builds | `gates/` |

## Open for the author

* B-1: pinned stays the default until the viewing (`braid_ab/`).
* The erosion of a held chord — the Strang composition's 2N full-sheet
  resamplings a frame: alternate the order frame by frame (half), window the
  tails as the lock's twist is, or slow Γ_vel (0.036 canvas²/s at full velocity).
* The consonance curve's shape and the floor's ramp; the release (0.25 s); the
  5 % clamp.
* In Kessho a held chord fringes the whole sheet within seconds — the rate
  question of `#9` again.
