# Evidence — Step 21: Stylus legato, wake & pinch (iOS)

PHASE4 §7 (legato per layout, re-anchor, wake-not-in-MIDI, pen pinch), §4
(stylus truth-table rows), §3.3 (CC74 stylus-only). Decisions: DECISIONS_3
#38. No core changes (working rule: the v0.4 operators landed in steps
19–20; this step consumes them).

## What landed

**hostmpe (pure-C, headless — Android inherits verbatim):** the pen legato
engine, REDESIGNED same-day to per-cell retriggers (#39 — the finger already
bends continuously, so the pen's job is real note changes):
`hostmpe_pen_begin` (center-bend→Note-On; velocity calibrated to UIKit force
units, baseline tap = finger default 96, force 3 = 127 — #38 corrected) and
`hostmpe_pen_glide` (cell under the pen + offset from its center: crossing =
bend→Note-On(live-force velocity)→old-Note-Off, the legato overlap idiom on
ONE channel; inside a cell the offset is the bend). The ±47 re-anchor and
the piano retune ramp are retired; dead zones sustain by construction; one
behavior on all three playable lattices. `pen_slide` (§3.3 CC74) and
`pen_pressure` (true force → 0xD0) unchanged.

**iOS (PlayOverlayView + SumiCanvasView):** pencil touches abandon the
joystick — absolute-position play off the probe (anchor cell's axis/step);
`sumi_add_wake` on EVERY stroke segment (tip radius from force — physical,
never MIDI); piano-grid cell tracking → retunes; CC74 from knee-shaped Δy
about the strike; `slide_mode = 1` → smoothed CC74 deltas drive
`sumi_add_pinch` at the pen position with the fold axis from AZIMUTH (CC74
then outbound-only — #38); tilt → master CC 1 (vortex by default); hover
ghost cursor (M2); re-anchor batches sent whole as strike class; pen ramps
ride the frame drain. Fingers unchanged; palm rejection untouched.

## Verification

* **Golden legato traces** (hostmpe_tests — 1552 checks total):
  - 12-cell scripted glissando: **12 retriggers, 12 old-note Offs**, all on
    one channel, every attack in tune (bend precedes each Note On), Off
    always after On (the legato overlap), retrigger velocity = the live
    force value, and the sounding pitch (note + bend) tracks the pen
    **< 1 cent everywhere including across every crossing**, monotone.
  - Same cell + same offset = silence (change-only); in-cell offset = bend
    (vibrato without retrigger); Note Off releases the CURRENT note after
    the sweep.
  - CC74 law: 64 center, +0.5 → 96, clamps 1..127, change-only; force →
    0xD0 change-only.
* Full regression with the step in the build: ctest 4/4; the entire
  step-19/20 scripted battery re-passes (19 ok / 0 fail); field-dump fixture
  bitwise.
* On-device gates for the user: a pen sweep in a DAW records a run of real
  same-channel legato notes (a mono/MPE synth glides through them); the
  piano-grid sweep sustains across the E–F gap; wake-not-in-MIDI
  demonstrated by replaying a DAW recording through the loopback; pinch
  folds following azimuth; 5-minute pencil+fingers session (no palm
  misfires, zero stuck notes).
