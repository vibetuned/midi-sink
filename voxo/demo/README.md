# The demo instrument's slot

`demo.dspreset` and `Samples/` are what a shell loads on a first launch so the
app makes a sound before any library is chosen (SOUND §3; DECISIONS_6 #21–#22).
The instrument is the **Dan Tranh**, a Vietnamese zither, from the Versilian
Community Sample Library (VCSL, CC0 1.0 — `LICENSE.txt`), the author's pick:
its "Normal" articulation's f layer, sixteen samples converted to 32 kHz
16-bit mono, trimmed to three seconds, normalised, stretched across the whole
keyboard (about 3 MB), by `tools/fetch_dan_tranh.py` — which also downloads the
full three-layer articulation as a test library outside the tree. Replace the
files, keep the names, and every shell's bundle picks them up: the desktop
copies this folder into the bundle's `Resources/demo` (macOS) or beside the
executable (Windows, Linux); the iOS app carries it as a folder reference; the
Android step does the same for its assets. Nothing else is ever bundled: users
load their own libraries.
