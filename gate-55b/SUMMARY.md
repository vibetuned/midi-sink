# Step 55b on the Linux box — the GL tier re-measured under the displacement payload

Box: Ubuntu, kernel 6.17, GNOME on Wayland, NVIDIA RTX 5090 (GL driver
610.43), GCC 15.2, Release (Ninja). Tree: `6d1e034` ("Step 55B apple and
android"), `midi-sink 2.0.0-alpha.1-14-g6d1e034 (libsumi 1.2.0)`. Every
command from the repo root; the harness's PNGs went to a scratch cwd.

| item | result | evidence |
|---|---|---|
| 1. build + ctest | clean (the same three benign warnings as step 55); **9/9** (`abi_c_compile` at 1.2.0) | — |
| 2. field gate, GL, the new fixture | **dx 2.44e-4, dy 3.66e-4, ink 3.91e-3, aux 0; mean 2.26e-6** — PASS at 1e-2 / 1e-4, the negative control red. The old payload sat at max 1.5e-2 / mean 6e-4 on this box (DECISIONS_5 #44, #83): the mean fell ~270×, the max 4× | `field_gate_gl.txt` |
| 3. composite gate, GL | max channel diff 1 (the GL tier of one 8-bit step), 25 672 of 1 048 576 channel samples differ; the negative control red (65) | `composite_gate_gl.txt` |
| 4. palette test | first run **4/5**: the Sumi four `(= recorded)` untouched, the Anod four `(!= recorded)` as predicted; their hashes copied into the `gl` column (`cases[]` rows 5–8, `desktop/src/dev_tools.cpp`); rebuilt; second run **5/5** | `palette_test.txt`, `palette_hashes_gl.txt`, `palette_test_after.txt` |
| 5. the seventeen self-tests | wake flick rankine pressure swirl ripple-group ripple-dip ripple-permanence stokeslet torsion chladni burst spark chirikov anod print gesture: every exit 0, every `^FAIL` count 0 | `<test>.txt` |
| 6. the soak | see below | `soak_*.txt`, `soak_table.md` |

## The GL column's Anod four

| case | was (1.1.0 payload, step 45a) | now (displacement, 55b) |
|---|---|---|
| medium 1 palette 0 morph 0 | `50c793eccae0acf1` | `7bc658dd87238d03` |
| medium 1 palette 1 morph 0 | `095dfa67dbc075a4` | `b3121824077f8525` |
| medium 1 palette 2 morph 0 | `a2135a24e751e900` | `bc0fc6df5b03f797` |
| medium 1 palette 1 morph 38 | `6d33c733939b8926` | `eb3e16793e8608db` |

## The soak

`soak_table.md` (16 operators, 3 negative controls): **47 of 48 checks**,
the one red the crossed pinch's pair (73.10 texels; DECISIONS_5 #17's known
non-inverse — the Mac: 72.91), every exact operator's (b) between **8.11
(ripple-bake) and 21.98 (chladni) texels** under the 32-texel tier, fifteen
operators PASS · PASS · PASS; `soak_negative.txt`: the three negative
controls red as required (the offset pairs 198.47 texels > 32, the
edge-clamp tines +13.99 %, the wake stream ×5.8). Chirikov reports here
(17.97 texels) where the Mac's table has a `?`.

Beside the Mac (`docs/evidence/step55b/soak_final_tier32.md`): tine 11.84
vs 11.87, pinch-saddle 13.16 vs 13.21, wake-doublet 12.97 vs 13.25,
wake-stokeslet 14.18 vs 14.44, ripple-bake 8.11 vs 9.11, vortex-exp 14.10
vs 14.18, vortex-rankine 15.91 vs 16.10, torsion 16.36 vs 16.42, chladni
21.98 vs 22.10, chladni-field 6.63 vs 6.67, burst 20.04 vs 20.13,
spark-shear 20.21 vs 20.33, spark 35.83 vs 36.23 — GL sits a few tenths of
a texel under Metal everywhere. The ripple-bake's 8.11 is under the
handoff's "9 to 23" by 0.9 texel (lower is the better side of that range).

Beside this box's own Phase-6 soak (step 45a, commit `da750be`): the
ink-mass pairs are **bitwise** for burst (−16.88 % / +0.00 %), chladni
(−0.10 % / +1.92 %) and chladni-field (−0.37 % / +0.26 %); the spark's
moved, −8.66 % / +0.00 % → −8.12 % / +0.00 %, its (c) growth +0.00 → +0.19 %
— the spark's emission floor is one of the three DECISIONS_7 #2 re-derived
(5e-4 → 1.22e-4 canvas heights), so its episode emits on a different
schedule; the burst's floor moved too and its pairs stayed. The pre-image
deviations rose from the old tier's 0.75–5.15 texels to 6.63–21.98 as #3
describes (the resampler's smoothing the coordinates froze).

How the runs went: `--soak all` was cut by my one-hour `timeout` after
seven operators (the soak steps frames through the bench window's swaps
and ran at the paced display rate, ~7 min per operator); `soak_six.txt` is
its first six operators, the other ten ran as `--soak <op>` (the same code
path; `soak_report.py` merges the logs). Five of those were started in
parallel: on Wayland an occluded bench window gets no frame callbacks and
its swap blocks, so the second such batch sat at 0 CPU and was restarted
one at a time. The `.clean` copies are the logs with the stderr lines that
had landed mid-way through block-buffered stdout lines removed (the later
runs used `stdbuf -oL` and needed none); the report read the `.clean` ones.
The two `.rgba` and `.bin` files are the gates' dumps.

## Tree

`desktop/src/dev_tools.cpp`: the `gl` column's Anod four (rows 5–8 of
`cases[]`) and the comment above them. Nothing else. `DECISIONS_7 #6`
records the row.
