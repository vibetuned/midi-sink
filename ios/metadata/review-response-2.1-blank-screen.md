# App Review — Guideline 2.1(a) "launched to a blank screen" (submission 65ac6adf)

Review of `2.0 (133)` on an iPad Air 11-inch (M3), iPadOS 27.0.1, 15 Oct 2026.
The reviewer saw the first screen — the sheet of marbling paper with the gear
at the top right — and read it as a blank screen. Nothing was tapped. This is
the app's working state: there is no splash, no menu, no sign-in; the picture
appears when something is played into the water, or when the paper is touched.
The reply below says so politely and walks the reviewer through every feature
with the exact row names of the 2.0 settings sheet. The notes stored under *App
Review Information → Notes* (`listing.md`) already describe this; the reply
repeats the steps in full because the notes were evidently not consulted.

---

## 1. Before replying — check these

* **Is the screenshot the reviewer's?** If Apple attached it, the paper texture
  in it is the Metal canvas drawing correctly on iPadOS 27.0.1 — the app
  rendered, the reviewer did not touch it. If the screenshot is ours, say
  nothing about their device in the reply.
* **The reviewer is on iPadOS 27.0.1; the author's iPad Air (M4) is on
  26.6.1** (`devicectl device info details`, 21 Oct 2026). A real
  first-frame failure on 27 cannot be ruled out from our device alone. If the
  iPad can be updated to 27.0.1, launch the TestFlight copy of build 133 once
  before sending the reply; that one check turns "works as designed" into
  "verified on the reviewer's OS".
* **Attach a screen recording** — launch from the Home screen, a tap and a
  drag on the paper, the gear, Pitch layout → "Chromatic grid (playable)",
  Mode → "Play", a few notes sounding from the speaker, "Dip the paper — keep
  the print". 60–90 s from the TestFlight build of 133 on the iPad Air. The
  1.0 round showed a recording is what moves a review along
  (`review-response-2.1.md`).
* A second reviewer may stop at the same screen. Worth deciding for 2.0.1: a
  one-time first-launch line on the paper ("Tap the paper, or the gear at the
  top right") that fades on the first touch. Not promised in the reply below;
  add a sentence if it is going in.

---

## 2. Paste-ready reply to App Store Connect

> Hello,
>
> Thank you for the review. The screen reported as blank is the app's first
> screen working as designed, and I would like to explain what it shows and
> how each feature is reached, since the review appears to have stopped
> there.
>
> midi-sink is a music visualizer and a MIDI instrument. It launches straight
> into Marble mode: a sheet of marbling paper — the pale, lightly textured
> surface in the screenshot — waiting for music. The picture appears when
> something is played into the water, or when the paper is touched. There is
> intentionally no splash screen, no menu and no sign-in. The only control
> drawn on the surface is the settings gear at the top right corner, kept
> faint so it does not disturb the picture.
>
> Everything below can be verified with no hardware and no other app:
>
> **1. Touch the paper.** A tap drops a ring of ink; dragging a finger combs
> it; a two-finger twist stirs a vortex; a pinch folds the sheet. Apple
> Pencil draws a line with a wake behind the tip. This is the visualizer
> answering to touch in place of a MIDI instrument.
>
> **2. Tap the gear** at the top right to open the settings sheet. Its
> sections are Canvas, Sound, Medium & look, Layout, Mode, Input and the
> expression routing.
>
> **3. Play it as an instrument, with sound.** In *Layout*, set *Pitch
> layout* to "Chromatic grid (playable)" — or Piano grid, Jankó,
> Wicki-Hayden, Strings, Trumpet, Trombone, Theremin, Boehm flute, each
> marked "(playable)". In *Mode*, choose "Play", then swipe the sheet down.
> The playing surface appears on the paper: fingers play notes, a finger
> moved sideways bends the pitch, pressing deeper feeds ink, and each note
> also paints the water. The app has its own sound engine, on by default
> with the bundled Dan Tranh instrument, so the notes are heard from the
> iPad's speaker (device volume up; the engine stops when the app leaves the
> foreground). The same performance is sent out as MPE MIDI to other apps
> and devices, which is optional.
>
> **4. Sound.** The *Sound* section holds the engine: "Internal sound
> (Voxo)" on, *Source* "Sampler" (the bundled demo instrument), "Suzu" (the
> built-in synthesizer, with a list of patches) or "Both".
>
> **5. Save a picture.** *Canvas* → "Dip the paper — keep the print" writes
> the picture to Photos, the only thing the app writes outside itself;
> "Clear the canvas — discard" starts a fresh sheet.
>
> **6. With a MIDI instrument** (optional): connected by USB, by Bluetooth
> (*Input* → "Pair Bluetooth MIDI instrument…") or over the MIDI network
> session, it plays into the water automatically — an MPE controller's
> strike, pressure, glide and slide become a drop of ink, its feed, a comb
> and a hue shift.
>
> The same steps are in App Review Information → Notes. A screen recording
> of this flow, made on an iPad Air 11-inch from the build under review, is
> attached.
>
> Please let me know if anything further would help.
>
> Best regards,
> Pedro Fillastre

---

## 3. One line to add to the stored Notes

The stored notes (`listing.md`, "App Review notes") begin with the hardware
and account facts. Put this sentence first, so the next reviewer reads it
before anything else:

> THE FIRST SCREEN IS NOT BLANK: the app opens on a sheet of marbling paper
> waiting for music. Tap the paper to drop ink, or tap the faint gear at the
> top right for the settings; Mode → "Play" with a "(playable)" Pitch layout
> turns the iPad into an instrument that sounds from its own speaker.
