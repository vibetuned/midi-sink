# Suzu — the sound profile (draft for the docs, step 63)

*Drafted at step 56 (`DECISIONS_7 #9`): the graph every synth voice ships
with. The figures beside this file are the evidence's
(`docs/evidence/step56/profile/`, in git history after the fold), regenerated
at any time by `midi-sink --dev --voxo-profile <dir>` and
`tools/sound_profile.py`.*

Each figure strikes every MIDI note from A0 (21) to C8 (108) at velocity 100,
holds it half a second and releases it for three tenths, offline through the
same `voxo_render` the callback runs, and shows three things: the level across
the keyboard (the held window's peak and RMS, dBFS), the first three harmonics
per note, and the run as a spectrogram.

## Suzu, the cells (step 56), the defaults

![Suzu, defaults](profile_suzu.png)

A magic-circle cell per voice: a pure sine at the same amplitude on every
note — flat to a tenth of a decibel from A0 to C8 — with no harmonics (the
faint second and third are the measurement's own leakage). What a listener
hears vary in the bass is not in the signal: a sine at 30–60 Hz sits where the
ear's equal-loudness curve and a small speaker give little back, and unevenly.

## Suzu with the cubic shear at 0.3

![Suzu, cubic shear 0.3](profile_suzu_shear03.png)

The phase-space shear `x −= ε·(y + s(y))` — an area-preserving map — adds
odd harmonics without moving the level or the pitch: the cell calibrates the
shear's detune at start-up and the voice compensates it, so every note stays
within a fifth of a cent at full gain. A symplectic cubic is a gentle
waveshaper: the third harmonic sits 33 dB under the fundamental here and
23 dB under at full gain; the hard timbres belong to the kicked rotor and the
Duffing cell (step 58).

## The sampler's sine, for reference

![the sampler's sine](profile_sine.png)

## The Dan Tranh demo (the sampler)

![the Dan Tranh](profile_dan_tranh.png)

Sixteen samples of one articulation stretched across the keyboard: the
library's own profile — the low zones loud, the top faint, a 24 dB spread —
which is what a sampled instrument is, and why a synth's flat line is worth
showing beside it.
