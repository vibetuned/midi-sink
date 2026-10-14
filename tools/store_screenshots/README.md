# Store screenshot scripts (v1.0.0 submission prep)

From `docs/evidence/store-submission/` (git history). For 2.0 (2026-10-14,
`DECISIONS_9 #18`): `perform.py` plays a deterministic MPE performance into
a MIDI output (the Tab's USB port, or a virtual source for the simulators)
and takes a capture mid-way; `assemble20.py` turns the raw captures into the
stores' files (the Tab's 16:9 crop, the phone emulator's centre crop, the
simulators' portrait sizes as they are), numbered after what each folder
holds. `simtouch.py` injects
touches into the iOS Simulator window through synthesised CGEvents,
`marble.py` composes a session on the Tab over adb, `crop169.py` crops
16:10 tablet shots to Play's 16:9, `feature.py` builds the Play feature
graphic. What they produced lives under `ios/metadata/` and
`android/metadata/`; the findings are condensed in `docs/CHANGELOG.md`
(v1.0.0, "Store submission prep").
