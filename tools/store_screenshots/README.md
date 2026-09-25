# Store screenshot scripts (v1.0.0 submission prep)

From `docs/evidence/store-submission/` (git history). `simtouch.py` injects
touches into the iOS Simulator window through synthesised CGEvents,
`marble.py` composes a session on the Tab over adb, `crop169.py` crops
16:10 tablet shots to Play's 16:9, `feature.py` builds the Play feature
graphic. What they produced lives under `ios/metadata/` and
`android/metadata/`; the findings are condensed in `docs/CHANGELOG.md`
(v1.0.0, "Store submission prep").
