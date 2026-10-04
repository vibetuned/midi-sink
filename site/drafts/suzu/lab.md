# The Suzu lab — the synth in your browser (draft for the docs step)

Every Suzu page has a live panel, and the panel is not a drawing of the
synth: it is the synth. Voxo's Suzu, the same C++ the desktop plays, is
compiled to WebAssembly and runs in your browser's audio thread. The page
sends it the MIDI a controller would, receives its sound, and reads the
state of every voice about sixty times a second: the pressure along a bore,
the shape of a string, the opening of a reed, the energy in each mode. What
you see is what you hear, drawn from the same numbers.

How faithful is it? The web build and the desktop build render the same
script and are compared sample by sample. For most voices they agree to the
last bit. Where they differ, it is because the two platforms' math libraries
round a sine or a power differently in the last place, and the difference
stays more than 80 decibels under the sound. The chaotic voices are the
exception that proves the rule: a last-bit difference grows, as chaos does,
and only their statistics can be compared.

## The flute

Hold a note and raise the breath. The tube is the flute's bore, open at
both ends, and its pressure swings inside a half-sine envelope: the first
register, the note. The jet leaves the flue at the left and swings across
the labium's edge, letting air into the bore and out of it in turn. Blow
harder and the jet reaches the edge sooner. Past a point the bore's second
mode takes over, the envelope grows a node in the middle, and the flute
sounds the octave. Nothing in the program picks the register: the overblow
is the physics.

The phase plane shows the sound against its own slope; the spectrum marks
the note's harmonics; the strip chart follows the sounding pitch against
the breath, and soft blowing visibly flattens it.

*(The panels for the other voices, the cell, the modal lattice and the bow,
the strings, the chaotic voices, the saxophone and the trumpet, follow in
step 59c.)*
