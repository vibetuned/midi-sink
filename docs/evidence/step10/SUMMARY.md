# Step 10 — Piano-roll layouts & BPM scroll: DONE evidence

All runs: 2560×1440, Metal, `--layout 3` (ROLL_H), bpm=120. Evidence images
are paper-dip print exports (the composited print the engine itself reads
back), not window screenshots.

**Default change mid-step (DECISIONS_2.md #10 resolved):** the spec author
confirmed the DONE phrasing was the intent and fixed §3.4 — default
roll_speed is now **0.0625** (1/16 canvas per beat; 16 beats = 4 bars of 4/4
span the canvas). The metronome/comet/smear/soak runs below were captured at
the earlier default 0.25 (0.5 canvas/s, residence ≈1.76 s) — still a legal
param value, and the mechanics they demonstrate are speed-independent. The
new default is verified by `roll_default_speed.png` and the updated
scripted-clock unit test.

| DONE criterion | Result | Evidence |
|---|---|---|
| Scripted-clock unit test at the default: 1/16 canvas per beat, ¼ after 4 beats, 1.0 after 16 | `test_roll_field_motion_clock` green (part of the 8,934-check suite, `ctest` 2/2 passed); also asserts the mirrored default roll_speed = 0.0625 and live bpm/roll_speed rescale | `tests/normalizer_tests.cpp` |
| Field speed at the shipped default (0.0625) | Half notes at 120 BPM (= 2 beats apart): six drops at x = 0.277 / 0.412 / 0.541 / 0.670 / 0.798 / 0.925 → gaps **0.135, 0.130, 0.129, 0.128, 0.127** ≈ 0.125 canvas (2 × 1/16; +3% = feeder sleep overshoot). | `roll_default_speed.png`, `run_default_speed.log` |
| Metronome: quarter notes at 120 BPM come out evenly spaced (run at roll_speed 0.25 → expected 0.25/beat) | Drop centers measured at x = 0.318 / 0.577 / 0.835 → gaps **0.259, 0.258** (uniform to 0.1%; the +3% vs ideal 0.25 is feeder `Thread.sleep` overshoot — the exact formula is pinned by the unit test). Parity alternation visible (dark/pale/dark). | `roll_metronome.png`, `run_metronome.log` |
| Sustained pressure stretches a drop into a comet trail | Held note (3.5 s, channel pressure 90): now-line feed + scroll drew a continuous comet from the now-line off the right edge; note-off lift ring visible at the head. | `roll_comet.png`, `run_comet.log` |
| Tines/vortices smear ink downstream while washi grain stays screen-locked | Mod-wheel vortex during residence sheared the drops into curled comma shapes; grain crosshatch inside and around the ink is undistorted. Numeric lock check: same paper region sampled across `roll_metronome` / `roll_comet` / `soak_final` prints (20,488 samples) → **max pixel diff 0** — grain bit-identical across different scroll phases. | `roll_smear.png`, `run_smear.log` |
| Clean entry edge — no streaked or wrapped ink at ingress | After the soak feeder stopped, the field scrolled ~6 canvas-lengths before the dip: the print is 100% ingress-written fresh water — pure washi, zero streaks/wrap/drift artifacts anywhere. (Ingress is an explicit shader branch writing `(st, 0, 0)`, `deform.glsl` scroll_fs.) | `soak_final.png` |
| 10-minute soak on a roll layout: stable memory, no leaks | RSS flat **91,184 KB → 91,344 KB** (+160 KB over 10 min, one early-warmup step, then flat). 60,661 frames in 618 s (**98.2 fps avg**), frame min/max 0.41/42.44 ms, dip window worst 14.33 ms, **0 dropped MIDI**. | `soak_memory.log`, `run_soak.log` |

Other checks in the suite: golden positions for all 5 layouts × 128 notes × 2
aspects (rolls: fixed now-line 0.12, pitch across the 0.06-inset cross axis,
low = bottom/left), scroll emitted first/once/outside the deformation budget,
ROLL_V mirrors to dy, ABI test exercises bpm/roll_speed at version 0.2.0.


Harness additions: `B` / `Shift-B` = bpm ±5, layouts 3/4 in the `8`/`L` cycle.
