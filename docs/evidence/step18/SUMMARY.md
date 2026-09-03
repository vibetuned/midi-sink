# Evidence — Step 18: Performance control strip (iOS)

PHASE4 §8: a dockable master-channel strip of wheels and buttons built from
the joystick primitive. Decisions logged as DECISIONS_3 #30. Also in this
step's tree: the three piano-grid spec amendments (PHASE4 §1 playable-set
sentence, PROJECT_SPEC_NEW §3.4 bullet + enum copy) reinstated after the
user's §7/§8 spec rewrite dropped them while §7 still referenced
PIANO_GRID/#29.

## What landed

**hostmpe (shared, pure-C header, headlessly tested — Android inherits in
Step 21):**
* `hostmpe_strip_t` widget value engines: spring wheel (master bend ±2 by the
  MPE master default — never sends RPN 0 on ch 1; ~50 ms linear return ramp,
  change-only, guaranteed exact-center final message from `hostmpe_strip_tick`
  even under one sparse late tick), latch wheels (Mod CC 1 + two assignable;
  relative float accumulation, no absolute-set entry point so regrasp cannot
  jump, change-only on the rounded 7-bit value), sustain button (momentary /
  toggle; a mode switch while ON emits the OFF), `hostmpe_strip_announce`
  (full latched state in 5 master-channel messages, for post-MCM re-sync).
* Assignable wheels refuse the protocol CCs (1, 6, 38, 64, 98–101, 120–127);
  defaults CC 23 / CC 24 (loopback: viscosity / roughness — visible today,
  rebound to ripple controls in Step 19).
* Limiter: 128 new MASTER-channel CC slots (#30) — strip wheels are policed
  continuous dimensions on every transport (change-only, decimation/budget,
  round-robin fairness); before this, generic CCs bypassed the policies
  entirely. CC 64 / buttons join the never-dropped class via the existing
  exempt path.

**iOS shell:**
* `ControlStripView` (UIKit sibling of PlayOverlayView): five slots — Pitch /
  Mod / two assignables / Sustain; the touch anchors its origin and
  `hostmpe_joystick_eff` shapes Δy (§3.2, travel bound 60 pt); long-press on
  an assignable opens the CC editor; display mirrors only, all value state in
  the engine.
* `SumiCanvasView`: strip engine lifecycle on the midiQueue (values persist
  across mode/layout switches by construction); `stripDispatch` = loopback
  full-rate + outbound per class (buttons exempt, wheels policed); the frame
  drain drives `hostmpe_strip_tick` (spring ramp); `sendSessionConfig` now
  appends the strip announce (exempt) — covers Play-mode entry, mid-session
  sink appearance, and the "Re-sync DAW" button; byte log gains src = 3
  (strip) so "all strip traffic on ch 1" is assertable from `midi_log.csv`.
* §8 lattice displacement: overlay resized to the play area remaining after
  the docked band (≤ 15% height, min 96 pt; top dock respects the safe
  area) — probe coordinates remap shell-side via bounds normalization, core
  untouched. Consequence flagged in #30: drops land at full-canvas positions,
  so cell↔drop alignment is vertically offset by the strip height while
  docked.
* Storm test rider: a CC64 transition every 2 s during the 60 s BLE storm,
  exempt + byte-logged, for the receiver-side never-dropped assertion.
* Settings: Control strip section (dock Top/Bottom, sustain toggle mode) —
  wheel values session-persistent in the engine; CC assignments deliberately
  not persisted (roadmap: deferred).

## Verification (headless + build)

* `hostmpe_tests`: **494 checks passed** (`hostmpe_tests_output.txt`) — new
  suites: `test_strip_spring` (exact-center DONE gate, dense + sparse ticks,
  regrab cancels ramp), `test_strip_latch_and_assign` (regrasp DONE gate,
  sub-unit accumulation, rails, reassign keeps value + speaks on the new CC,
  protocol-CC refusals), `test_strip_sustain` (momentary/toggle/mode-switch
  OFF), `test_strip_announce_and_channel_discipline` (every widget message on
  the master channel; announce = 5 messages restating latched state),
  `test_limiter_strip_classes` (10-voice 1 kHz storm through the 300 msg/s
  budget: every CC64 transition emitted immediately, zero dropped; CC1 wheel
  policed by budget and by the 100 Hz rate policy with latest-wins drain and
  change-only).
* Full ctest: 4/4 suites pass. `hostmpe_c_compile` (C11) covers the new API.
* iOS: build-ios + xcodegen + xcodebuild clean; installed on the iPad.

## Revision after first device test (DECISIONS_3 #31)

* **Docked band → compact floating palette top-left** (~300×86 pt,
  translucent, over the FULL-canvas lattice). The band's overlay resize
  offset every drop from its touched cell by the strip height — "breaks the
  feeling of the controller". The overlay keeps full bounds again (the §6
  cell↔drop alignment property holds everywhere); the dock setting is gone.
  PHASE4 §8's displacement bullet amended accordingly.
* **Held sustain released itself after 0.5 s**: the CC-editor long-press
  recognizer cancels the view's touches when it fires (UIKit default). Its
  delegate now restricts it to the two assignable wheels; every widget touch
  (sustain included) is tracked in the grab table so releases resolve by grab
  record, not lift location. Sustain stays momentary by default (press-and-
  hold pedal feel, per the user); the latch toggle remains a setting.

## Remaining DONE gates (on-device / DAW — user validation)

* DAW pad patch with sustain + mod while chording (subjective feel gate).
* Byte log from a strip session: all src=3 rows on ch 1 (status & 0x0F == 0).
* BLE storm capture: every CC64 transition arrives, in order, undelayed.
* Mod wheel simultaneously stirs the loopback vortex (CC 1 → vortex strength,
  the default map) and modulates the DAW synth.
* Values survive layout/mode switches; re-announce after "Re-sync DAW".
