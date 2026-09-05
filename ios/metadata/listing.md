# App Store listing — midi-sink (iPadOS)

Prepared once for the beta wave (Phase 5 §4); localized later if ever. Paste
into App Store Connect → App Information / the version page.

**Name:** midi-sink
**Subtitle (30):** Ink marbling, played with MIDI
**Primary category:** Music · **Secondary:** Graphics & Design
**Age rating:** 4+ (no objectionable content, no web access, no purchases)
**Privacy policy URL:** https://midi-sink.vibetuned.com/privacy/
**Support URL:** https://midi-sink.vibetuned.com/support/
**Marketing URL:** https://midi-sink.vibetuned.com/
**App Privacy (ASC questionnaire):** Data Not Collected.
**Copyright:** Pedro Fillastre · AGPL-3.0

**Promotional text (170):**
Suminagashi ink marbling on water, driven by your MIDI instrument — or by
your fingers and Apple Pencil, as a 15-voice MPE controller for your DAW.

**Description:**
midi-sink models a tray of water with ink floating on it. Every gesture is an
exact, area-preserving transformation of the whole sheet — Aubrey Jaffer's
mathematical marbling — so the rings stay crisp after a thousand strokes.

Two modes.

MARBLE MODE is the tray under your hands: tap for a drop, drag to comb, twist
two fingers for a vortex, pinch to fold, draw with the Pencil and a wake
follows the tip. Plug in a MIDI instrument and it plays into the same water:
an MPE controller's strike, pressure, glide and slide become drop, feed, comb
and hue; a wind controller draws a single calligraphic line; any keyboard
plays drops on the circle of fifths.

PLAY MODE turns the iPad into an MPE instrument. A pitch lattice — chromatic
grid, Jankó or piano grid — sits under your fingers; every touch is a
joystick (sideways bends in semitones, push to feed ink, pull back to stir).
The Apple Pencil plays legato across the keys with real force as velocity,
its barrel roll deepens vibrato and the Pencil Pro's squeeze is the sustain
pedal. A floating strip carries pitch, mod, two assignable wheels and sustain
on the MPE master channel. The stream goes out over a virtual CoreMIDI source
(also to a USB-tethered Mac), the MIDI network session and Bluetooth LE MIDI,
so any DAW or synth records a standard MPE performance while the water paints
it.

Made for MPE controllers — ROLI Piano and Seaboard, Expressive E Osmose — the
ROLI Airwave's gesture CCs, wind controllers (Roland Aerophone, Odisei Travel
Sax) and classic keyboards. midi-sink makes no sound of its own: it is a
visualizer and a controller.

Free software (AGPL-3.0). Collects nothing.

**Keywords (100):**
MPE,MIDI,marbling,suminagashi,controller,ROLI,Osmose,Apple Pencil,DAW,ink

**What's New:** the tag's section of `docs/CHANGELOG.md`, condensed to the
user-visible lines.

**TestFlight — Test Information (beta description):**
midi-sink is an ink-marbling visualizer and MPE instrument. Please tell us
three things (GitHub Discussions, link in Settings → Support): what you played
it with (device/controller), what confused you in the first five minutes, and
what you expected Play mode to do that it didn't. The user guide is at
https://midi-sink.vibetuned.com/.
**Feedback e-mail:** info@vibetuned.com

**App Review notes:**
The app needs no account. Marble mode works with touch alone; Play mode
generates MIDI to other apps/devices (a MIDI destination such as a synth app
shows the output). Bluetooth is used only for Bluetooth MIDI instruments;
local network only for the MIDI network session (RTP-MIDI).
