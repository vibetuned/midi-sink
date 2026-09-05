# midi-sink v9.9.9

> **DRAFT** — `CHANGELOG.md` has no section for v9.9.9 yet; these are the latest notes (`v0.4.0 — Phase 4: Touch & Stylus MPE Play Surface (steps 15–22)`). Add the section before publishing.

The tablets become MPE instruments: every touch is a joystick on a pitch
lattice, the pencil plays legato, a floating control strip rides the master
channel, and the generated stream feeds both the loopback visualizer and
real DAWs over USB, virtual, network and BLE transports. All host-side MPE
generation lives in one shared pure-C library, `hostmpe/`, consumed by Swift
(module map) and Kotlin (JNI) — one implementation, 1,569 headless checks,
zero drift between platforms. The core grew four deformation operators
(v0.4) and the layout probe; otherwise it stayed frozen.

### Close-out (after step 22)
Pencil Pro **squeeze = sustain pedal**, the S-Pen barrel button's twin
(#62): `UIPencilInteraction` drives the same `hostmpe_strip_t` engine as the
palette's pad, transition-only, Play-mode only; Pencil 2 double-tap latches.
iOS byte log adopts Android's `src` taxonomy (pen = 4, #63) so
`tools/pen_trace.py` and `tools/midi_asserts.py` read both platforms'
logs — the §3.3 stylus-only CC74 rule became provable (1,237 pen / 0 finger
CC74 in a live session). In-app **evidence capture** (timed full-screen PNG
burst through the render-server path, which carries Metal content) and a
log flush, pulled with `devicectl device copy from` (#64). **Echo
suppression** in hostmpe (#66): a live iPad log showed 99.5% of "external"
MIDI was our own output mirrored back (median 0.3 ms), marking our own
channels externally held and painting every note twice; every delivered
byte is now recorded at the single transport emit point and matching input
inside 300 ms is consumed — the post-guard session carries zero external
rows and passes every assert. **The sustain pedal no longer wipes the
canvas** (#67): `CC 64 → paper dip` is honoured only outside MPE mode
(§2.4's classic-keyboard mapping survives); the dip became a deliberate
settings control. Flagged, unchanged: with the bend booster engaged, a cell
crossing is no longer pitch-continuous (#68 — 69% of a free-play session's
crossings carried boosted bends; expressive by design, options recorded).

### Step 22 — Android port (Galaxy Tab S8 Ultra, on the Linux box)
Full parity: `sumi_play.cpp` hosts `hostmpe_t`, the strip engine, one
limiter per transport and the byte log on the **AMidi poller thread** (the
§5.2 single producer, #46); a params snapshot lets the instance-free probe
answer on the UI thread (#47). Kotlin `PlayOverlayView` (probe-swept
two-tone lattice, joystick indicators, fingers, S-Pen, hover), floating
`ControlStripView` with the long-press CC editor, `MidiOutputs` (**USB
gadget as primary sink**, `MidiDeviceService` virtual device, a hand-rolled
BLE-MIDI GATT peripheral — MidiManager is central-only, #44), Marble pinch.
Measured on the Linux side: gadget appears in `amidi -l` within ~150 ms of
the USB mode flip; MCM/RPN 0 = 48 on 15/15 members in wire order on USB and
BLE simultaneously; **BLE − USB latency +29.8 ms median**; the full hostmpe
(1,559) + normalizer (14,997) suites pass on-device; GLES3 field dump
reproduces the Metal fixture numbers exactly; a **10-minute mixed session**
(three sinks + 10-voice storm + scripted play) — 834,018 wire messages,
1,149/1,149 notes released, worst slot 92/s under the 100 Hz policy, +1 ms
cumulative lag, 0 dropped, 120 fps, PSS flat at 253 MB, 0 EGL errors. An
independent review of the diff fixed real bugs (#54): an inverted §5.4
teardown block, a lost command-queue wakeup, an `AMidiDevice` double-free,
duplicate ingestion after relaunch, BLE flow-control stalls, and two false
greens in the analysers (now negative-tested). Late additions: the piano
grid's accidental knob made aspect-invariant in the core (#57), the S-Pen
barrel button as sustain (#58), a natural's drawn cell re-centred on its
octave pair (#60), the black-key glissando corridor (#61); a black-key
glissando experiment rolled back in full (#59).

### Step 21 — Stylus legato, wake & pinch (iOS)
The pen redesigned same-day into **per-cell legato retriggers** (#39): the
shell probes the cell under the tip each move, a crossing emits
bend→Note On(live-force velocity)→old Note Off on ONE channel (the legato
overlap idiom — DAWs record real notes, mono/MPE synths glide), the in-cell
offset is the bend, ±0.65 st boundary hysteresis keeps vibrato from
machine-gunning at cell edges; the ±47 re-anchor and piano retune ramps were
retired. Velocity in UIKit force units (baseline tap = 96, force 3 = 127,
#38). Barrel controls became derivative-only **gestures** (#40, four
iterations): the Pencil Pro roll multiplies the in-cell bend ×1→×3 with a
0.4 s decay; azimuth tail-stir is the fallback; tilt→CC1 and every
orientation→vortex path removed after repeated leaks. `sumi_add_wake` on
every stroke (physical, never MIDI); `slide_mode = 1` drives the pinch with
the fold axis from azimuth, CC74 then outbound-only. Polish batch (#41):
Marble two-finger pinch on iOS, piano accidentals narrowed to 0.6 keys with
white-key tops between them (natural→natural glissandi), the two-tone
lattice (paper halo under each ring, accidentals drawn on top), and the
§3.4 lift ring removed. Goldens: a 12-cell scripted glissando tracks pitch
< 1 cent across every crossing; device evidence: a live 10-note glissando
reconstructed from the byte log, `pen_trace.py` PASS on 10 iPad strokes,
screen captures of the strip lit by the squeeze.

### Step 20 — Lamb–Oseen swirl & bipolar press (core + iOS)
`SUMI_DEFORM_SWIRL` (θ = S/(2πr²)·(1−e^(−r²/r_c²)), series guard below
x < 1e-3), 0xA0 decoded as poly pressure → the *swirl* dimension keyed by
the voice's note, `press_mode` arbitration of 0xD0 (feed or swirl, one
consumer), r_c = the voice's boundary, band-parity sign, echo fan-out (#37).
hostmpe bipolar Y: one radial knee, up → 0xD0 feed, down → 0xA0 swirl on the
voice's channel, lift releases the engaged half before Note Off. 19/19
scripted checks: core coherence (ring sharpness inside 0.7 r_c retained
exactly through ~4 rad while the annulus moved 29 k texels), counter-rotation
of adjacent notes, small-r stability (half-float ULP freeze documented),
full step-19 battery and fixture bitwise.

### Step 19 — v0.4 deformation operator batch (core, desktop harness)
`sumi_version()` → 0.4.0. Vortex **Rankine profile** (rigid core, 1/r²
exterior; 20 rotations leave the interior unblurred: mean |Δink| 0.00000 vs
0.758 for the exponential control); **dipolar wake** `sumi_add_wake`
(lab-frame doublet, sign corrected from the draft, rigid tip body, ≤ a/4
sub-steps because a/2 is the fold threshold; a one-frame 8×a flick keeps a
positive Jacobian everywhere); **Hamiltonian pinch** `sumi_add_pinch`
(streamline-windowed saddle, det = 1) plus the crossed-tine variant the
user kept behind `pinch_variant` (#34), also reachable from CC74 deltas via
`slide_mode = 1`; **sine ripple** live (composite-time view displacement,
prints un-rippled, amp = 0 bit-identical to v0.3) and baked (delta-driven
passes). `bend_mode` (#35/#36): the PER-NOTE bend either glides the drop or
raises the ripple by its distance from center, permanently (phase drift per
vibrato cycle), stilling on re-center/release/mode flip — unit-tested as
glide-XOR-ripple. Pinch soak (#33): ink MASS is the observable — 500
reversible pairs hold ±0.5%, zero fabrication after the §3.4 ingress rule
was extended to every new pass (the edge clamp had fabricated +9.5%), per-pass
erosion 1.9× the glide tine's. Field-dump fixture bitwise, all stress
regressions clean.

### Step 18 — Performance control strip (iOS)
`hostmpe_strip_t`: spring wheel (master bend ±2, ~50 ms return, guaranteed
exact-center final message), latch wheels (Mod CC 1 + two assignables,
relative accumulation so a regrasp never jumps, protocol CCs refused),
momentary/toggle sustain, announce after every MCM re-sync; 128 master-channel
CC slots joined the limiters so wheels are policed while buttons ride the
never-dropped class (#30). 494 checks. First device test moved the strip
from a docked band — which displaced every drop from its touched cell — to a
compact **floating palette** top-left over the full-canvas lattice, and
fixed the long-press recognizer cancelling a held sustain (#31); sustain
stays momentary by default.

### Piano-grid layout (between steps 17 and 18)
`SUMI_LAYOUT_PIANO_GRID = 5` (#29): the chroma grid's frame with each octave
drawn as a two-row keyboard — accidentals above at white-key units
{1, 2, 4, 5, 6}, naturals below; position + inverse probe, true-step glides,
goldens over all 84 notes at two aspects. R_max revised after the first
device test to half of min(key width, octave-pair height) so the knobs
match the chroma grid's feel.

### Step 17 — Outbound transports & per-pipe rate limiting (iOS)
Dispatcher fan-out: loopback full-rate; outbound change-only + per-voice
per-dimension latest-wins decimation (≤ 100 Hz), or a token budget with
round-robin fairness for BLE (~300 msg/s); Note On/Off and center bends
exempt. Sinks: virtual CoreMIDI source (which also sends explicitly to the
`AppleIDAMDriver` destination — a virtual source is NOT bridged to a tethered
Mac, #27), rtpMIDI network session, BLE peripheral (`MIDISend` to the
Bluetooth destination; the local mirror duplicated delivery, #25), with
MCM/RPN 0 = 48 re-sent on every sink appearance (#28) and a "Re-sync DAW"
button. Wire-order captures on every transport: handshake ordered, 100% of
Note Ons bend-preceded, worst per-slot 70/s under the 100 Hz policy, BLE
315/s at the sender, no cumulative lag over 60 s. Found by instrumenting:
`MIDISourceCreate` needs `UIBackgroundModes: [audio]` (#24); an analyser
that re-sorted equal-timestamp records had faked an rtpMIDI reordering
(#22 — delivery order IS the data). Panic + per-sink silence primitives
(#26).

### Step 16 — hostmpe allocator & loopback Play mode (iOS, fingers)
LRU-by-release round-robin allocator with external-occupancy masking (30 s
timeout), saturation = silent drop + HUD blink; joystick → MIDI with the
semitone-exact bend (one grid column = exactly ±171 counts at ±48; the knee
is a deadband, not a travel limit — identity beyond the circle, #10;
absolute deadband floor for finger-sized jitter, #16); Y-up is pressure,
fingers emit no CC74 (#19); Jankó's semitone axis is horizontal in the core
(#18); true-step glides on the lattices (#20). Live on the iPad with a ROLI
Piano held over Bluetooth: 2,766 external messages, zero touch allocations
on held channels, touch-down → visible drop **0.39 ms median**, all 15
member channels rotated, 0 stuck voices.

### Step 15 — Layout probe ABI & Play-mode overlay (iOS)
`sumi_layout_probe` + `sumi_cell_info_t` (#6): an instance-free pure
function callable from any thread — the shells' hit-test — sharing the
semitone-step derivation with the glide axis (#7). Goldens round-trip every
cell center on CHROMA_GRID and all three Jankó echo rows (suite 8,934 →
12,480 checks); lattice/drop alignment ≤ 0.0004 canvas (½ px). `hostmpe/`
seeded with the §3.2 soft knee behind a pure-C header (#8). iOS Marble/Play
toggle and `PlayOverlayView` — a faint lattice built by sweeping the probe
(#9), per-touch joystick indicators, zero MIDI yet. Version 0.3.0.

---
Notes generated from `docs/CHANGELOG.md` by the release workflow.
