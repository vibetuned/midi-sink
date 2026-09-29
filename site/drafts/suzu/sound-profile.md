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

## The modal voice (step 57): the five presets

*Eight modes, coupling 0.05, the defaults — `--voxo-suzu-preset <n>`.*

![the harmonic string](profile_modal_harmonic.png)

The harmonic string: partials at 1, 2, 3 … with weights 1/k, the highs
decaying first. The second sits 8 dB under the fundamental, the third 14.
Above C7 the upper partials cross the 24 kHz ceiling and are muted — fewer
partials, a little less held energy, and the level line bends by a
decibel.

![the stiff bar](profile_modal_bar.png)

The free bar's ratios, 1, 2.78, 5.44, 9, 13.4 … : by C4 most of them are
past the ceiling. The 5 dB spread across the keyboard is the instrument's
own — a bar's high notes have fewer partials.

![the bell](profile_modal_bell.png)

The bell: the hum an octave under the prime, the tierce, the quint, the
nominal and the cluster above (0.5, 1, 1.2, 1.5, 2, 2.5, 2.667, 3). What the
Goertzel calls "harmonic 2" here is the nominal, exactly an octave up. The
peak line's sawtooth is the measurement's: a 0.4 s window catches the
prime–tierce beat (a fifth of the pitch) at a different phase on each note;
the RMS underneath is flat to 0.8 dB.

![glass](profile_modal_glass.png)

Glass (1, 2.32, 4.25, 6.63 …, weights 1/m²): nearly a sine with a faint
inharmonic ring.

![the plucked string](profile_modal_plucked.png)

The plucked string is Karplus–Strong in modal form: partials at
k·√(1 + Bk²), fed by the pluck position in closed form — sin(kπp)/k², the
triangular pluck's spectrum. At p = 0.28 the comb is what you see; pluck at
the middle and the even partials vanish.

## The breath bow

![the breath bow](profile_bowed.png)

*The harmonic string with breath 0.63 held through every strike.* An
energy servo per mode pulls the orbit toward the breath's target and holds
it: a flat singing line across the keyboard, the second partial level with
the first at bow position 0.3. No breath, no bow — the declared decays
alone, and silence is gated rather than hoped.

## Mode splitting

![mode splitting](mode_splitting.png)

Two cells at the same pitch, coupled, split into a close pair — the joint
map's normal modes — and the sum beats at their difference. The measured
beat sits on the analytic line to a hundredth of a percent from κ = 0.0125
to 0.5. In a voice with placed ratios the coupling's detune is compensated
so the partials stay where the preset put them; what remains of the
coupling is the exchange of energy between partials — the swirl's job.
