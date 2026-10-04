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

[Open the flute →](/suzu/flute.html)

## The cell and the shears

One cell is one rotation in phase space, and its orbit is the circle you
see. The engine steps it so that the orbit keeps its size for ever: the
strip under it tracks x² + y² − εxy, the quantity the step conserves, and
with the decay at zero it does not move. The shear pushes x by a function of
y on every step; the circle bends, and the bend is the harmonics the
spectrum shows arriving.

In the lab mode the naive update moves x and y together from the same old
values. It looks innocent and it multiplies the orbit's area by 1 + ε² on
every step: at A3 the orbit grows twenty-thousandfold a second. The lab
mutes the sound and restarts the engine when it runs away. That is why the
engine ships the other update.

[Open the cell →](/suzu/cell.html)

## The modal voice and the bow

A lattice of coupled cells, one per partial: a string, a bar, a bell, a
glass. Each bar is a partial's energy, placed at its frequency. A strike
sets them ringing and each fades at its own rate. Breathe, and the bow
feeds each partial toward its target and no further. It is a proportional
servo, so each partial settles where the bow's push balances its own decay:
just under the target, closer the longer the decay (the dashed tick), and on
it when nothing else takes energy. The servo strip shows the lattice's
energy converging. Raise the coupling and the partials exchange energy but
stay in tune: the engine compensates the detune when the patch loads, and
it refuses a coupling it cannot compensate, saying why.

[Open the modal voice →](/suzu/modal.html)

## The strings

Three strings built three ways, side by side: a chain of masses stepped by
Verlet, a delay line closed by a bridge with a body of its own (the hybrid),
and a sum of modes plucked at a point. Each one's shape is drawn from the
engine's own state. The hybrid's body rings beside the string: its bridge
modes are part of its tone. In the lab mode the chain's stiffness can be
pushed past the stability bound. The engine refuses the patch; with the
refusal bypassed the chain blows up at once.

[Open the strings →](/suzu/strings.html)

## Duffing and the kicked rotor

The kicked rotor is the standard map played as an oscillator: one dot per
kick, the angle across and the momentum up. Turn the mod wheel. Below the
threshold the dots lie on curves and the momentum stays inside its island;
past it they fill a chaotic sea and the pitch wanders with them. The same
map stirs the water on the canvas as the Chirikov operator: one theorem, two
senses. The Duffing cell is a spring that stiffens as it swings: struck
hard, it clangs sharp and settles onto the note.

[Open the chaos voices →](/suzu/chaos.html) · [the Chirikov operator](../operators/chirikov/)

## The saxophone and the trumpet

A reed on a cone and a pair of lips on a flared bore. The tube shows the
standing wave. For the cone it draws the pressure times the distance from
the apex, the part that stands as a sine. At the left, the valve opens and
closes; beside it, its portrait is the reed's displacement against its
velocity. The ledger is the energy budget the engine keeps for the reader:
the mouth's work against the energy the bore and the valve hold. The held
energy never exceeds the work. On the trumpet, the embouchure (CC 74)
climbs the bore's peaks: loose lips fall to the peak below, tight ones
climb to the octave.

In the lab mode the junction can be made the naive way. The engine refuses
it, because with the losses zeroed the bore and the reed would make energy
the mouth never gave. Bypass the refusal, bend the note, and the ledger's
line is crossed until the samples are no longer numbers. The lab catches
it.

[Open the winds →](/suzu/winds.html)

---

*(The links assume the lab at `/suzu/` beside `/marble/`; the docs step,
step 67, decides the address and builds the lab from the tag.)*
