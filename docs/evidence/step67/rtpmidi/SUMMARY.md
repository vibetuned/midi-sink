# Step 67, the app fixes — the Tab's network session (RTP-MIDI), `DECISIONS_9 #14`

## Built

* `rtpmidi/` — `sumi_rtpmidi.h` / `sumi_rtpmidi.cpp`: RTP-MIDI (RFC 6295)
  with Apple's session protocol, pure C surface, POSIX UDP, no threads; a
  listener and an initiator; the codec exposed. Not built on Windows.
* The Tab: `SINK_NETWORK` in `android/cpp/sumi_play.cpp` (the rate class,
  polled and written on the MIDI thread; received MIDI at the merge point as
  a device's), `nativeNetworkStart/Stop/Status`, Kotlin's mDNS advertisement
  (`_apple-midi._udp`, `MidiOutputs.startNetwork`), the sheet's *Network
  session (RTP-MIDI) over Wi-Fi* toggle with its status line, `INTERNET` in
  the manifest, the lab extra `--es transports …,net`.
* `tests/rtpmidi_tests.cpp`: the codec and two endpoints over the loopback;
  `--serve <s> [port]` and `--connect <ip> <port> <s>` for the live checks.
* `tools/rtpmidi_mac_probe.swift`: the Mac's own network session invited to
  connect to a session by address.

## Gates

| Gate | Result | File |
|---|---|---|
| `rtpmidi_tests` (ctest, this Mac) | all green: the codec's six checks, the loopback session's twelve | `rtpmidi_tests.txt` |
| `gradlew assembleDebug` | BUILD SUCCESSFUL | — |
| the library against an independent implementation (pymidi, Python) | green: both invitations accepted, 14 messages received with their keys and velocities, the 'BY' honoured | `pymidi_interop.txt` |
| the library against Apple's stack | see below | `mac_probe.txt` |

## The Tab, live over Wi-Fi (2026-10-13) — DONE

The Tab (SM-X906B, 192.168.1.14) launched with `--es transports usb,net`
and `--es fingeringDemo 1`; the Mac (192.168.1.21) browsed and resolved
the advertisement, then invited the Tab with the library's initiator
(`rtpmidi_tests --connect 192.168.1.14 5004 16`).

| What | Result | File |
|---|---|---|
| the advertisement | `dns-sd -B _apple-midi._udp` lists "midi-sink on SM-X906B"; `-L` resolves it to `Android_L7T6P3DU.local.:5004` | this summary |
| the Tab's log | the session bound 5004/5005, advertised, the participant's join re-sent the handshake (80 messages) | this summary |
| Tab → Mac | the handshake (MCM, the bend ranges, every channel) then the fingering demo's six notes with their bends, pressures and note-offs, in wire order: 128 packets, 128 messages, 0 rejected, 2 clock syncs | `tab_connect.txt` |
| Mac → Tab | the initiator's notes 60–68 (velocity 90, off 64) in the Tab's byte log as **src 0**, device input at the merge point — the loopback drew them | `tab_midi_log_external_rows.csv` |
| two hosts at once | a second and a third initiator joined (the Tab's log: "participant joined (2)"); the settings' status line read *connected: midi-sink rtpmidi_tests, midi-sink rtpmidi_tests · sent 306, received 108*; 4 and 5 clock syncs over the real network, 0 rejected | `tab_connect_second_host.txt`, `tab_connect_third_host.txt` |
| the settings page | the Outbound MIDI section with the toggle on and the status line *connected: midi-sink rtpmidi_tests · sent 128, received 28*; the session line's outbound counters name the network sink (`out u217/v0/b0/n128`) | `tab_settings_2.png` |
| the water | the demo's charges at the partials under Anod with the network session on | `tab_network_session.png` |

## The Apple-stack check — the author's terminal

The Mac's own `MIDINetworkSession` could not be enabled from this
session's shell: the MIDI server answers `enabled false, port 0` to a
process that never got the Local Network permission (no prompt reaches a
headless process). From a terminal of the author's, two ways:

```
./build/tests/rtpmidi_tests --serve 30 5010          # the library listening on this Mac
swift tools/rtpmidi_mac_probe.swift <mac-ip> 5010 12  # Apple's session invites it (a Local Network prompt once)
```

or, with the Tab's session on, Audio MIDI Setup → MIDI Network Setup: the
tablet is in the directory as "midi-sink on SM-X906B"; Connect.
