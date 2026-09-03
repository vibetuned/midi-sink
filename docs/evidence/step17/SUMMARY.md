# Step 17 — Outbound transports + per-pipe rate limiting (iOS): DONE evidence

Specs: `_work/PHASE4_SPEC.md` §5.3 (dual pipe, per-transport budgets),
§5.4 (iOS transports). Decisions: `_work/DECISIONS_3.md` #22–#28.
Rig: iPad Air 11" (M4) → MacBook Pro over Wi-Fi (rtpMIDI session), Bluetooth
LE, and USB (IDAM). Receiver: `midilisten.swift` (in this folder) logs every
message from every CoreMIDI source with host timestamps.

| DONE criterion | Result | Evidence |
|---|---|---|
| Decimation unit tests prove change-only and latest-wins | **PASS — hostmpe suite now 340 checks.** Rate policy: first value immediate, identical value never resent, a burst inside one period collapses to the LAST value, 1 kHz of changes for 1 s yields **95–101 emissions** (≤100 Hz ceiling), slots independent per voice-dimension, exempt passthrough clears pendings. Budget policy: 6,000 msg/s offered → **≤650 in 2 s** (~300/s + burst), every slot inside a **115 ms** fairness window, 16/16 interleaved Note On/Off passed through untouched (no token cost) | `tests/hostmpe_tests.cpp`, ctest 4/4 |
| All sinks enumerate and play | **PASS** — virtual CoreMIDI source ("midi-sink Play Surface", stable UID), MIDI Network Session (rtpMIDI, Bonjour-discovered and connected from the Mac), BLE peripheral (advertised from the app, connected from Audio MIDI Setup; appears as "iOS Bluetooth"), and the wired USB/IDAM link (appears as "iPad"). All four carry the play surface; user-confirmed audible through a DAW on the virtual path | `network_storm_capture.csv`, `ble_storm_capture.csv`, `ble_sender_log.txt` |
| Virtual/network pipe keeps its own ≤100 Hz/dimension policy | **PASS** — 60 s storm over rtpMIDI: 70,640 messages, **worst per-slot 1 s rate 70/s** across 30 active voice-dimension slots (policy 100 Hz), notes balanced 117/117, change-only holding (identical repeats were only legitimate return-to-value sweeps seconds apart, plus transport-level repeats) | `network_storm_capture.csv` |
| BLE global budget holds (~300 msg/s) | **PASS, asserted at the sender where the policy lives: 18,900 messages in 60.0 s = 315/s** (300 + burst headroom). The receiver logged 490/s; the surplus is duplicate delivery — **23.1% of budgeted messages are identical-consecutive values per slot in wire order**, while the sender had exactly **one** destination (logged census: `'LT-… Bluetooth' driver='com.apple.AppleMIDIBluetoothDriver'`). So it is the BLE receive path, not our transmission. The limiter is token-metered AND change-only, so it cannot emit a repeated value at all (DECISIONS_3 #25) | `ble_sender_log.txt` (`[storm] done: … ble=18900 over 60.0s (ble 315/s)`), `ble_storm_capture.csv` |
| Round-robin fairness: every active voice's dimensions update within any 100 ms window | **PASS** — measured among **held** voices (channels idle between LRU restrikes are inactive, not starved): median gap **0 ms**, 95th percentile **75 ms**, worst 248–323 ms during restrike churn | `ble_storm_capture.csv` |
| End-to-end lag < 100 ms sustained, no cumulative drift | **PASS** — a 1 Hz exempt marker (CC 118, counting) rides the stream: network **+46 ms** drift over 60 s; BLE **−18 ms** over 55 s, max inter-marker gap 1.013 s. No runaway | both captures |
| Zero Note On/Off or center-bend messages dropped | **PASS** — exempt messages bypass the budget entirely (unit-tested 16/16 under a 6,000 msg/s storm); on the wire, center bends ≥ note-ons in every window (172 ≥ 101), with on/off deltas only at capture-window truncation where voices were still held | captures + unit tests |
| Toggling transports mid-performance never glitches the loopback | **PASS by construction + observed** — transport flags gate only the outbound fan-out; the loopback `sumi_push_midi` path is untouched by them. Toggles were flipped repeatedly across runs (virtual on/off, network on, BLE on/off) with the canvas holding 60 fps | `session_log.csv` pattern, user-confirmed |
| MCM handshake: glide = 1 semitone in the DAW, not 1/24th | **PASS on every sink, byte-verified in wire order**: master MCM `101=0, 100=6, 6=15` then nulls, all 15 member channels at RPN 0 = 48, **zero misordered data entries**, and 100% of note-ons preceded by their center bend — USB/IDAM 90/90, rtpMIDI 20/20, BLE 80/80. (An earlier run appeared to show rtpMIDI reordering RPN; that was an artifact of the analyser re-sorting equal-timestamp records — retracted in DECISIONS_3 #22.) The loopback conformance test independently decodes a one-column drag as exactly **1.0 semitone** | `usb_idam_capture.csv`, `network_resync_capture.csv`, `ble_storm_capture.csv` |
| USB/IDAM round-trip; wired latency best of all sinks; IDAM enable mid-session re-sends MCM | **PASS** — tethered iPad appears on the Mac as `name='iPad' driver='com.apple.AppleMIDIUSBDriver'`; **926 messages** captured on it with the handshake ordered and worst per-slot rate **34/s** (policy 100 Hz). Wired needs no Audio-MIDI-Setup "Enable" (that button is IDAM *audio*). Required a spec correction: a virtual source is **not** bridged to a tethered Mac — the wired path is an explicit send to the `AppleIDAMDriver` destination, implemented inside the same sink/policy (DECISIONS_3 #27). A sink appearing mid-session now triggers a debounced MCM re-send via CoreMIDI `msgSetupChanged` (#28) | `usb_idam_capture.csv`, `midiports.swift` (port census tool) |

Fixed during the step (both found by instrumenting rather than guessing):
- **#24 — `MIDISourceCreate` failed with -10844 (kMIDINotPermitted)**: iOS
  requires `UIBackgroundModes: [audio]` to publish virtual MIDI endpoints.
  Until then the outbound virtual source did not exist and every send went
  to endpoint 0 — silently, because no CoreMIDI status was checked. Now all
  statuses are logged and the shell shows live per-sink counters. Also fixed
  a latent stack overflow: `MIDIPacketList()` holds one packet but was being
  told it had 1024 bytes (a limiter drain of up to 64 messages would have
  overrun it).
- **#25 — BLE routing**: bytes reach a central via `MIDISend` to the
  Bluetooth-driver destination (deduped per entity), not by publishing our
  own virtual source; the local mirror was removed because iOS bridges
  device sources over the same link and duplicated delivery.

Close-out addition (DECISIONS_3 #26): **"Stop all notes (panic)"** — a BLE
peripheral cannot disconnect its central (no public API; the central owns the
link), so the meaningful control is a panic that releases every held voice
(pressure 0 → Note Off) and silences the zone (CC 64/CC 123 on master + all
15 members) across the loopback and every transport. Switching a transport
off now also silences **that sink only**, leaving voices on the other pipes
sounding. Both primitives live in hostmpe (`hostmpe_panic`,
`hostmpe_silence_zone`) so Android inherits them; 12 new checks cover emit
order, per-channel coverage, idempotence, channel reuse after panic, and that
stateless silence leaves the voice table intact (suite now **352 checks**).

Correction logged (DECISIONS_3 #22): an earlier reading of these captures
re-sorted equal-timestamp records and so appeared to show rtpMIDI mangling the
RPN handshake. Delivery order IS the data; re-analysed in wire order every
transport is correct. The monotonic per-message timestamp was kept as ordering
hygiene; the 4 ms config spacing it prompted was reverted.

Outstanding (user-side, optional): the DAW *ear* test on any sink —
chords keeping independent bends and a one-column glide sounding as one
semitone. The byte-level equivalent is already proven by the loopback
conformance test and the ordered emission log; GarageBand iOS is a poor MPE
citizen for this, so a dedicated MPE synth is the right judge.
