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

## Device evidence (iPad Air 11" M4)

* `device_pen_legato.txt` — pen strokes reconstructed from a LIVE session's
  byte log (`midi_log.csv`, pulled with `devicectl device copy from
  --domain-type appDataContainer`). A real 10-note ascending glissando
  (ch 2, 1.85 s) shows the sounding pitch (note + bend) advancing in ~1.0
  semitone steps with NO jump at any crossing; a vertical sweep (ch 3)
  steps octaves the same way; a boundary wobble (ch 5) crosses back and
  forth continuously — the #39 hysteresis holding. The file ends with the
  raw bytes of one crossing: bend -> Note On(new) -> Note Off(old), all on
  one channel, exactly the legato-overlap idiom.
* `tools/midi_asserts.py device midi_log.csv` on that session: MCM present
  and ordered, RPN 0 = 48 on 15/15 members, 300 Note Ons each preceded by a
  bend (268 strikes centered + 32 legato retriggers carrying their cell
  offset), every Note Off preceded by pressure 0 or a retrigger, strip
  traffic on the master channel only, 0 open voices at the end.
* `ipad_play_01/03/05.png` — live screen captures from the device (settings
  -> Evidence -> "Capture screen", pulled with devicectl): the floating
  strip with **Sus lit by a Pencil Pro squeeze** (#62), the piano-grid
  lattice in its two-tone form (paper halo under each ring, accidentals on
  top — #41), and the pen's glissando painted across the water as a chain
  of legato drops deformed into one ink stroke.
* `pen_trace_ipad.txt` — `tools/pen_trace.py` (the Android-side analyser)
  reading the iPad's log natively now that stylus bytes carry src 4 (#63):
  **10 pen strokes, ASSERTS PASS** — every crossing a same-channel legato
  retrigger, monotone strokes, no overshoot, every stroke released.
* `midi_asserts_ipad.txt` — the shared device asserts on a post-guard
  session: **ALL ASSERTS PASS** (13,581 messages; 535 Note Ons each
  bend-preceded — 134 centered strikes + 401 legato retriggers carrying
  their cell offset; every Note Off preceded by pressure 0 or a retrigger;
  **pen CC74 = 1,237 with zero finger CC74**, the §3.3 stylus-only rule now
  provable thanks to the src-4 tag; strip on master only; sustain 6 presses,
  never stuck; zero open voices). `midi_asserts_ipad_before_echo_guard.txt`
  is the same run BEFORE #66 — 40 "externally-held channel" violations and
  14.6 k self-consumed messages — the guard's before/after proof. The
  post-guard log carries **no src-0 rows at all**.
* `pen_trace_ipad_freeplay.txt` — the same session through the pen tracer
  (115 strokes). Its failures are documented in the file's header: 12
  "non-monotone" are free playing (the assert assumes a scripted sweep) and
  13 "overshoot" are the #40 booster deepening the bend at a crossing
  (#68).
* `ipad_sustain_released.png` — the same scene one second later with the Sus
  pad dark: the squeeze's release, the pair to the lit capture above.

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
