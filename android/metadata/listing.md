# Google Play listing — midi-sink (Android)

Prepared for the closed-testing wave (Phase 5 §4), rewritten for 2.0 at the
Phase-10 fold (2026-10-13, `DECISIONS_9 #18`); localized later if ever. Paste
into Play Console → Main store listing / App content. The count command is in
`release-notes-README.md`.

**App name (30):** midi-sink
**Short description (80):** Ink marbling played by MIDI, fingers or S-Pen — with a sampler and synth inside
**Category:** Music & Audio · **Tags:** MIDI, music production
**Content rating:** IARC questionnaire — no violence, no user interaction between users, no purchases, no data sharing → *Everyone*.
**Privacy policy URL:** https://midi-sink.vibetuned.com/privacy/
**Support (Store listing contact):** https://midi-sink.vibetuned.com/support/ · info@vibetuned.com
**Website:** https://midi-sink.vibetuned.com/
**Ads:** none. **In-app purchases:** none. **Target audience:** 18+ (not designed for children — simplest declaration; the app has no child-directed content either way).

**Data safety (App content → Data safety):**
*Does your app collect or share any of the required user data types?* — **No.**
The app collects nothing and shares nothing: no account, no analytics, no
crash reporting, no server of its own. MIDI over USB, Bluetooth LE and the
on-device virtual device is local I/O. The one network use is the **MIDI
network session** you switch on under Outbound MIDI (new in 2.0): RTP-MIDI
over your own Wi-Fi to a computer you connect from, peer to peer — it carries
your MIDI notes to your DAW and nothing to us, and the app makes no other
connection. Sample libraries you import stay in the app's private storage.
Files written (byte logs, session CSV, recordings) stay there too and are read
only by the user; a print the user chooses to save is a PNG the user's own
gallery receives (`Pictures/midi-sink`, MediaStore — no storage permission on
Android 10+).
*Is all of the user data collected by your app encrypted in transit?* — n/a (no
collection). *Do you provide a way for users to request deletion?* — n/a.

**Permissions declared and why (App content → Permissions, if asked):**
`BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT` — pairing a Bluetooth MIDI instrument
(the ROLI); `BLUETOOTH_ADVERTISE` — the tablet itself as a BLE-MIDI device for
a desktop DAW; `neverForLocation` is set on the scan permission.
`INTERNET` — the MIDI network session (RTP-MIDI over the local Wi-Fi, advertised
by mDNS; used only when you switch it on; no server, no other connection).
On Android 10–11 only (`maxSdkVersion 30`): the legacy `BLUETOOTH`,
`BLUETOOTH_ADMIN` and `ACCESS_FINE_LOCATION`, which those versions require to
scan for Bluetooth MIDI — never used for location. No camera, microphone,
contacts or storage permissions.

**Full description (4000):**
midi-sink models a tray of water with ink floating on it. Every gesture is an
exact, area-preserving transformation of the whole sheet — Aubrey Jaffer's
mathematical marbling — so the rings stay crisp after a thousand strokes.
Since 2.0 the tray also sounds, and it can be read two ways.

MARBLE MODE is the tray under your hands: tap for a drop, drag to comb, twist
two fingers for a vortex, pinch to fold, draw with the S-Pen and a wake
follows the tip. Plug in a MIDI instrument and it plays into the same water:
an MPE controller's strike, pressure, glide and slide become drop, feed, comb
and hue; a wind controller draws a single calligraphic line; any keyboard
plays drops on the circle of fifths.

TWO MEDIA. Sumi is ink on washi paper. Anod reads the same water as an
electric discharge: every strike a spark, the charge glowing by how far it has
been stretched, the water drawing its own field lines. Switch between them
live — the sheet is kept.

PLAY MODE turns the tablet into an MPE instrument. Thirteen layouts: the
chromatic grid, Jankó and the piano grid; a trumpet and a trombone whose
partials sit under your fingers while three valve pads or a slide retune every
held note; the Wicki–Hayden button field, a fretboard in three tunings, and a
theremin that is the whole screen. Every touch is a joystick (sideways bends
in semitones, push to feed ink, pull back to stir). The S-Pen plays legato
across the keys with real pressure as velocity, its tail-stir deepens vibrato,
and its button is the sustain pedal. A floating strip carries pitch, mod, two
assignable wheels, sustain and a panic button. The stream goes out over
USB-MIDI (the tablet appears to any computer as a class-compliant MIDI
device), a virtual MIDI device for on-device apps, Bluetooth LE MIDI, and — new
— a MIDI network session over Wi-Fi that a Mac or a PC connects to, so any DAW
or synth records a standard MPE performance while the water paints it.

SOUND. The app plays what it draws. A sampler loads Decent Sampler libraries —
thousands of free instruments exist in that format; bring your own, or play
the bundled Dan Tranh zither at first launch. Suzu, the built-in synth, is the
same mathematics one octave down: oscillators that cannot drift, a bowed string
that sings under your pressure, a bell, a flute, a saxophone, a trumpet whose
lips follow your slide, and six patches fitted to sampled instruments. Or let
your DAW make the sound and keep the picture.

AND THE REST. Palettes you can edit, with colour-blind-safe presets. Presets
that carry a whole session between the tablet, the desktop and the web. A
prints ledger: every paper dip is kept and re-exports at any size, up to 8K.
Session replay: record a performance and play it back frame for frame — here,
on another device, or in the browser.

Made for MPE controllers — ROLI Piano and Seaboard, Expressive E Osmose — the
ROLI Airwave's gesture CCs, wind controllers (Roland Aerophone, Odisei Travel
Sax) and classic keyboards.

Free software (AGPL-3.0). Collects nothing.

**Release notes (per release):** `release-notes-2.0.0-en-US.txt` beside this
file for 2.0 — the tag's section of `docs/CHANGELOG.md` condensed to the
user-visible lines, within Play's 500 characters; the App Store text is
`ios/metadata/whats-new-2.0.0-en-US.md`.

**Closed-testing feedback text (Testers page / opt-in message):**
midi-sink 2.0 is an ink-marbling visualizer and MPE instrument with its own
sound. Please tell us (GitHub Discussions, link in Settings → Support): what
you played it with (device/controller), whether the sound's latency felt
right, any library that failed to load (the text of its compat report), which
Suzu patch you kept, and what confused you in the first five minutes. The
user guide is at https://midi-sink.vibetuned.com/.
**Feedback e-mail:** info@vibetuned.com
