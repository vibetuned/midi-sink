# Step 49 — Voice dispatch & pitch — "it glides" (macOS) — DONE evidence

Decisions `DECISIONS_6 #10` (Hermite and the block-linear pitch ramp, the
measurement), `#11` (the sample, its swap, the desktop's WAV reader), `#12`
(Local Control, the strip's volume, the two switches).

## What shipped

- **Voxo 0.3.0**: the voice is a sample player — `voxo_set_sample` (interleaved
  float, mono/stereo, any rate, fractional root), `voxo_clear_sample` (the sine
  returns), `voxo_set_interpolation` (0 Hermite, 1 linear: the lab's
  comparison), `voxo_set_local_control` and CC 122 tracking; the read ratio
  2^((note − root + bend)/12) × rates, recomputed per block and ramped linearly
  per sample, 4-point Hermite; a sample plays once, ends the voice, restarts
  on retrigger; the swap protocol (pending → callback → retired → shell frees)
  keeps the callback allocation-free (the counting-allocator test covers a
  swap and a stereo 44.1 kHz read).
- **Desktop**: `wav_io` (reader for the Sound section's "Sample (WAV)" row with
  its root note, `sound_sample` / `sound_root` in the INI; writer for the
  bounce); `--dev --voxo-bounce <dir>` renders the check's material offline.
- **`tools/voxo_glide_check.py`** (numpy through `uv run`): the spectral check.

## DONE checks

| check | result |
|---|---|
| ctest (7 suites; voxo_tests now 38 checks: ratio at root / +12 / −48, bend, the sample's end, clear, Hermite vs linear read error, local control, the swap under the counting allocator) | 7/7 — `voxo_tests.log` |
| the ±48 glide bounced to WAV, no aliasing, Hermite vs linear side by side | **PASS** — `glide_check.txt`: holds −61.8 / −62.4 dB (linear −47.0 / −48.1), sweep worst −59.9 dB, median −62.3 (linear −47.0): 12.8–14.8 dB apart; bar −55 dB and a 10 dB margin |
| the reason recorded | `DECISIONS_6 #10`, with the limits (0.44-of-Nyquist content: 7 dB apart; the corners; the inherent up-glide fold) |
| the desktop storm on the sample-player build | `--voxo-storm 10`: 0 XRuns at 128 frames |
| the ROLI glides a piano sample across four octaves on the Mac | **the author's**: Settings → Sound → Internal sound, a WAV path in "Sample (WAV)", its root note, then the ROLI |

## How to run

```
build/desktop/midi-sink.app/Contents/MacOS/midi-sink --dev --voxo-bounce <dir>
uv run tools/voxo_glide_check.py --dir <dir>
```

## Notes for the record

- The Hermite-vs-linear test in `voxo_tests` reads a 1 kHz table at −7
  semitones and fits the true sine: Hermite's residual is under −60 dB and at
  least 20 dB below linear's.
- The first bounce's test tone at 48 kHz showed no difference between the
  reads (content at 0.03 of its Nyquist): the tone's own rate is the
  variable that makes the check meaningful, recorded in #10.
- The scripted-mode process must render one frame before exiting (the core's
  Metal shutdown waits on a frame semaphore); the bounce path does.
