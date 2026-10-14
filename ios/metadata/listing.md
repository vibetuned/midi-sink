# App Store listing — midi-sink (iPadOS)

Prepared for the beta wave (Phase 5 §4), rewritten for 2.0 at the Phase-10
fold (2026-10-13, `DECISIONS_9 #18`); localized later if ever. Paste into
App Store Connect → App Information / the version page. Character limits
are ASC's; `android/metadata/release-notes-README.md` has the count command.

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
Ink marbling on water, played from your MIDI instrument, your fingers or the
Pencil — now with sound inside: a sampler, the Suzu synth, and a second medium.

**Description:**
midi-sink models a tray of water with ink floating on it. Every gesture is an
exact, area-preserving transformation of the whole sheet — Aubrey Jaffer's
mathematical marbling — so the rings stay crisp after a thousand strokes.
Since 2.0 the tray also sounds, and it can be read two ways.

MARBLE MODE is the tray under your hands: tap for a drop, drag to comb, twist
two fingers for a vortex, pinch to fold, draw with the Pencil and a wake
follows the tip. Plug in a MIDI instrument and it plays into the same water:
an MPE controller's strike, pressure, glide and slide become drop, feed, comb
and hue; a wind controller draws a single calligraphic line; any keyboard
plays drops on the circle of fifths.

TWO MEDIA. Sumi is ink on washi paper. Anod reads the same water as an
electric discharge: every strike a spark, the charge glowing by how far it has
been stretched, the water drawing its own field lines. Switch between them
live — the sheet is kept.

PLAY MODE turns the iPad into an MPE instrument. Thirteen layouts: the
chromatic grid, Jankó and the piano grid; a trumpet and a trombone whose
partials sit under your fingers while three valve pads or a slide retune every
held note; the Wicki–Hayden button field, a fretboard in three tunings, and a
theremin that is the whole screen. Every touch is a joystick (sideways bends
in semitones, push to feed ink, pull back to stir). The Apple Pencil plays
legato across the keys with real force as velocity, its barrel roll deepens
vibrato and the Pencil Pro's squeeze is the sustain pedal. A floating strip
carries pitch, mod, two assignable wheels, sustain and a panic button. The
stream goes out over a virtual CoreMIDI source (also to a USB-tethered Mac),
the MIDI network session and Bluetooth LE MIDI, so any DAW or synth records a
standard MPE performance while the water paints it.

SOUND. The app plays what it draws. A sampler loads Decent Sampler libraries —
thousands of free instruments exist in that format; bring your own, or play
the bundled Dan Tranh zither at first launch. Suzu, the built-in synth, is the
same mathematics one octave down: oscillators that cannot drift, a bowed string
that sings under your pressure, a bell, a flute, a saxophone, a trumpet whose
lips follow your slide, and six patches fitted to sampled instruments. Or let
your DAW make the sound and keep the picture.

AND THE REST. Palettes you can edit, with colour-blind-safe presets. Presets
that carry a whole session between the iPad, the desktop and the web. A prints
ledger: every paper dip is kept and re-exports at any size, up to 8K. Session
replay: record a performance and play it back frame for frame — here, on
another device, or in the browser.

Made for MPE controllers — ROLI Piano and Seaboard, Expressive E Osmose — the
ROLI Airwave's gesture CCs, wind controllers (Roland Aerophone, Odisei Travel
Sax) and classic keyboards.

Free software (AGPL-3.0). Collects nothing.

**Keywords (100):**
MPE,MIDI,marbling,suminagashi,controller,synth,sampler,ROLI,Osmose,Apple Pencil,DAW

**What's New (4000):** `whats-new-2.0.0-en-US.md` beside this file — the
2.0 section of `docs/CHANGELOG.md` condensed to what a player notices; the
Play text (`android/metadata/release-notes-2.0.0-en-US.txt`) is the same
list at 500 characters.

**TestFlight — Test Information (beta description):**
midi-sink 2.0 is an ink-marbling visualizer and MPE instrument with its own
sound. Please tell us (GitHub Discussions, link in Settings → Support): what
you played it with (device/controller), whether the sound's latency felt
right, any library that failed to load (the text of its compat report), which
Suzu patch you kept, and what confused you in the first five minutes. The
user guide is at https://midi-sink.vibetuned.com/.
**Feedback e-mail:** info@vibetuned.com

**App Review notes:**
The app needs no account, and no hardware to review. Marble mode is the
visualizer: a connected MIDI instrument plays into the water — an MPE
controller's strike, pressure, glide and slide become drop, feed, comb and
hue — and touch and the Apple Pencil drive that same water, so the mode can
be exercised in full with nothing plugged in. Play mode generates MIDI to
other apps/devices (a MIDI destination such as a synth app shows the output).
The app also plays audio of its own, in the foreground only: an internal
sound engine — a sampler with a bundled public-domain demo instrument, and a
synth — sounds what is played, and stops when the app leaves the foreground.
Bluetooth is used only for Bluetooth MIDI instruments; local network only for
the MIDI network session (RTP-MIDI). The `audio` background mode is required
by iOS for `MIDISourceCreate` (kMIDINotPermitted without it); no audio plays
in the background.
