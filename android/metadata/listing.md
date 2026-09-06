# Google Play listing — midi-sink (Android)

Prepared once for the closed-testing wave (Phase 5 §4); localized later if
ever. Paste into Play Console → Main store listing / App content.

**App name (30):** midi-sink
**Short description (80):** Suminagashi ink marbling, played with your MIDI instrument — or the S-Pen.
**Category:** Music & Audio · **Tags:** MIDI, music production
**Content rating:** IARC questionnaire — no violence, no user interaction between users, no purchases, no data sharing → *Everyone*.
**Privacy policy URL:** https://midi-sink.vibetuned.com/privacy/
**Support (Store listing contact):** https://midi-sink.vibetuned.com/support/ · info@vibetuned.com
**Website:** https://midi-sink.vibetuned.com/
**Ads:** none. **In-app purchases:** none. **Target audience:** 18+ (not designed for children — simplest declaration; the app has no child-directed content either way).

**Data safety (App content → Data safety):**
*Does your app collect or share any of the required user data types?* — **No.**
The app collects nothing and shares nothing: no account, no analytics, no
crash reporting, no network access of its own. MIDI over USB, Bluetooth LE and
the on-device virtual device is local I/O. Files written (byte logs, session
CSV) stay in the app's private storage and are read only by the user over adb;
a print the user chooses to save (Settings → Canvas → *Paper dip — save the
print*) is a PNG the user's own gallery receives (`Pictures/midi-sink`,
MediaStore — no storage permission on Android 10+).
*Is all of the user data collected by your app encrypted in transit?* — n/a (no
collection). *Do you provide a way for users to request deletion?* — n/a.

**Permissions declared and why (App content → Permissions, if asked):**
`BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT` — pairing a Bluetooth MIDI instrument
(the ROLI); `BLUETOOTH_ADVERTISE` — the tablet itself as a BLE-MIDI device
for a desktop DAW; `neverForLocation` is set on the scan permission. No
location, camera, microphone, contacts or storage permissions.

**Full description (4000):**
midi-sink models a tray of water with ink floating on it. Every gesture is an
exact, area-preserving transformation of the whole sheet — Aubrey Jaffer's
mathematical marbling — so the rings stay crisp after a thousand strokes.

Two modes.

MARBLE MODE is the tray under your hands: tap for a drop, drag to comb, twist
two fingers for a vortex, pinch to fold, draw with the S-Pen and a wake
follows the tip. Plug in a MIDI instrument and it plays into the same water:
an MPE controller's strike, pressure, glide and slide become drop, feed, comb
and hue; a wind controller draws a single calligraphic line; any keyboard
plays drops on the circle of fifths.

PLAY MODE turns the tablet into an MPE instrument. A pitch lattice — chromatic
grid, Jankó or piano grid — sits under your fingers; every touch is a
joystick (sideways bends in semitones, push to feed ink, pull back to stir).
The S-Pen plays legato across the keys with real pressure as velocity, its
tail-stir deepens vibrato, and its button is the sustain pedal. A floating
strip carries pitch, mod, two assignable wheels and sustain on the MPE master
channel. The stream goes out over USB-MIDI (the tablet appears to any
computer as a class-compliant MIDI device), a virtual MIDI device for
on-device apps, and Bluetooth LE MIDI, so any DAW or synth records a standard
MPE performance while the water paints it.

Made for MPE controllers — ROLI Piano and Seaboard, Expressive E Osmose — the
ROLI Airwave's gesture CCs, wind controllers (Roland Aerophone, Odisei Travel
Sax) and classic keyboards. midi-sink makes no sound of its own: it is a
visualizer and a controller.

Free software (AGPL-3.0). Collects nothing.

**Release notes (per release):** the tag's section of `docs/CHANGELOG.md`,
condensed to the user-visible lines — the same text as the desktop draft.

**Closed-testing feedback text (Testers page / opt-in message):**
midi-sink is an ink-marbling visualizer and MPE instrument. Please tell us
three things (GitHub Discussions, link in Settings → Support): what you played
it with (device/controller), what confused you in the first five minutes, and
what you expected Play mode to do that it didn't. The user guide is at
https://midi-sink.vibetuned.com/.
**Feedback e-mail:** info@vibetuned.com
