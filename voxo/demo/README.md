# The demo instrument's slot

`demo.dspreset` and `Samples/` are what a shell loads on a first launch so the
app makes a sound before any library is chosen (SOUND §3; DECISIONS_6 #21).
What sits here now is a PLACEHOLDER — a synthesised music box made by
`tools/make_demo_instrument.py`, public domain by construction — until the
author records the real one (a music box or a kalimba, on brand). Replace the
files, keep the names, and every shell's bundle picks them up: the desktop
copies this folder into the bundle's `Resources/demo` (macOS) or beside the
executable (Windows, Linux); the tablets' steps do the same for theirs.
Nothing else is ever bundled: users load their own libraries.
