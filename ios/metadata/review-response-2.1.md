# App Review — Guideline 2.1 "Information Needed" (submission 660dc855)

First submission of `1.0` for iOS, 10 Sep 2026. This is the standard request
sent to a developer account with limited review history — **not a content
rejection**. Nothing in the app or the metadata was faulted. Answer it, and
review resumes.

Apple asks for the answers **twice**: as a reply in App Store Connect, and
pasted into *App Review Information → Notes* so future submissions inherit
them. §3 below is the Notes version.

---

## 1. The screen recording — shot list

Apple's requirements: captured on a **physical device** on the **latest OS**,
**beginning with the app launch**, showing the typical user flow. The author's
iPad Air 11-inch (M4) on iPadOS 26.6.1 satisfies this. Record the **build
under review** (install it from TestFlight so what is recorded is what was
submitted), not a debug build from Xcode.

Use Control Centre → Screen Recording. midi-sink makes no sound of its own, so
system audio only matters for step 7, where the synth app is heard; iOS
captures that automatically. Aim for 90 s – 3 min.

| # | Shot | Why it is there |
|---|---|---|
| 1 | Home screen, tap the midi-sink icon, let it open | Apple requires the recording to start at launch |
| 2 | Marble mode with fingers only: tap = drop, drag = comb, two-finger twist = vortex, pinch = fold | Proves the app is fully usable with **no hardware**, which is the reviewer's situation |
| 3 | Apple Pencil drawing a stroke, the wake following the tip | Pencil support, still no hardware needed |
| 4 | Connect the ROLI Piano (or Airwave / Travel Sax) and play a phrase into the water | **The headline.** Strike, pressure, glide and slide becoming drop, feed, comb and hue |
| 5 | Settings gear → show Pitch layout (8 options), Palette (3), Mode | Shows the app's depth and where things live |
| 6 | Switch Mode to Play, Chromatic grid: fingers play, a held finger bends, the control strip reacts | Play mode as an MPE instrument |
| 7 | Switch to a synth app on the same iPad receiving from the virtual source "midi-sink", play, let it sound | **Do not skip.** Play mode's output is invisible without a destination; this is the one thing a reviewer cannot discover alone |
| 8 | Settings → Canvas → "Paper dip — save the print", then the image in Photos | The only thing written outside the app |
| 9 | Settings → About, showing the version and build | Ties the recording to the submitted build |

Attach the file to the App Store Connect reply. If it exceeds the attachment
limit, an **unlisted** YouTube or Vimeo link in the reply body is routinely
accepted — say in the message that the link is unlisted.

---

## 2. Paste-ready reply to App Store Connect

> Hello,
>
> Thank you for the review. Answers to each point follow; I have also added
> them to the Notes field of App Review Information.
>
> **1. Screen recording**
>
> A screen recording made on a physical iPad Air 11-inch (M4) running
> iPadOS 26.6.1, using the build submitted for review, is attached. It begins
> with launching the app from the Home screen and follows a typical session:
> painting the water by touch and with Apple Pencil, playing it from a
> connected MPE MIDI controller, switching to Play mode, sending MIDI to a
> synth app on the same device, and saving a print to Photos.
>
> Regarding the specific items listed:
>
> * **Account registration, login, account deletion** — not applicable. The
>   app has no accounts and no sign-in of any kind. There is nothing to
>   register, log in to, or delete. No part of the app is gated.
> * **User-generated content** — not applicable. Nothing is shared, published
>   or transmitted. The artwork a user makes exists only on their own device;
>   the single export is a PNG the user explicitly saves to their own photo
>   library. There is no server, no feed, no other user to see anything, so
>   reporting and blocking mechanisms do not apply.
> * **Paid content or features** — not applicable. The app is free in full,
>   with no in-app purchases, no subscriptions, no advertising and no locked
>   features.
>
> **2. Purpose and target audience**
>
> midi-sink is a music visualizer and an MPE MIDI controller. It simulates a
> tray of water with ink floating on it — the Japanese craft of suminagashi —
> and every gesture is an exact, area-preserving transformation of the whole
> sheet, so the ink rings stay crisp after thousands of strokes.
>
> It does two things:
>
> * **Marble mode** turns a musical performance into a picture. A connected
>   MIDI instrument plays into the water: on an expressive MPE controller a
>   note's strike, pressure, pitch glide and timbre slide become a drop of
>   ink, its feed, a comb through the sheet and a hue shift. A wind controller
>   draws a single calligraphic line; an ordinary keyboard drops ink around
>   the circle of fifths. The same water also responds directly to fingers and
>   Apple Pencil, so it works with no instrument attached.
> * **Play mode** turns the iPad itself into a 15-voice MPE instrument. A
>   pitch lattice sits under the fingers and every touch is a joystick —
>   sideways bends pitch, pushing feeds ink, pulling back stirs it. The Apple
>   Pencil plays legato with real force as velocity. The performance leaves
>   the iPad as standard MPE MIDI over a virtual CoreMIDI source, the MIDI
>   network session, or Bluetooth LE MIDI, so any DAW or synth can record it.
>
> The audience is musicians who own expressive controllers (ROLI, Expressive E
> Osmose, Roland and Odisei wind controllers), and anyone who wants a
> responsive drawing surface driven by music. The problem it solves: expressive
> MPE performances carry far more nuance than a piano roll can show, and there
> was no tool that renders that nuance as a picture in real time while also
> being playable as an instrument in its own right.
>
> The app produces **no audio**. It is a visualizer and a MIDI controller.
>
> **3. Setting up and accessing the main features**
>
> No credentials, sample files or configuration are required. Nothing is
> gated. On first launch the app opens directly into Marble mode.
>
> * **Marble mode (default).** Tap for a drop of ink, drag to comb it, twist
>   two fingers for a vortex, pinch to fold, draw with Apple Pencil. This needs
>   no hardware at all.
> * **Playing it from an instrument.** Connect any MIDI instrument by USB, by
>   Bluetooth (Settings → "Pair Bluetooth MIDI instrument…"), or over the MIDI
>   network session. It appears automatically and plays into the water.
> * **Play mode.** Tap the gear, set Pitch layout to "Chromatic grid
>   (playable)", "Jankó (playable)" or "Piano grid (playable)", then set Mode
>   to Play. **To observe its output you need a MIDI destination**, because
>   Play mode emits MIDI rather than sound: open any synth app on the same
>   iPad and select the virtual source named "midi-sink" as its input, or
>   connect the iPad to a computer. Without a destination the app still paints
>   but appears to do nothing audible — this is expected.
> * **Saving a picture.** Settings → Canvas → "Paper dip — save the print"
>   writes a PNG to Photos and starts a clean sheet.
>
> A note on two rows in the settings sheet: "Run 60 s storm test" and "Run
> on-device suites" are diagnostic tools that exercise the MIDI pipeline and
> print a result. They are developer conveniences, harmless to run, and touch
> nothing outside the app.
>
> **4. External services, tools and platforms**
>
> **None.** The app uses no external services whatsoever. Specifically there
> are no data providers, no authentication services, no payment processors, no
> AI or machine-learning services, no analytics, no crash reporting, no
> advertising networks and no third-party SDKs or libraries of any kind.
>
> The app contains no networking code at all — no HTTP client, no web view, no
> sockets it opens itself. It links four Apple system frameworks: CoreMIDI,
> CoreAudioKit, Metal and QuartzCore. The rendering engine is our own C++,
> built from source in this repository.
>
> The only network activity possible is the CoreMIDI network session
> (RTP-MIDI), which is a peer-to-peer MIDI link on the user's own local network
> that the user must enable, and Bluetooth LE MIDI to instruments the user
> pairs. Neither sends anything to us; we operate no server.
>
> Consistent with this, the App Privacy declaration is "Data Not Collected".
>
> **5. Regional differences**
>
> There are none. The app behaves identically in every region and on every
> storefront. There is no geo-restricted content, no region-dependent feature,
> no remote configuration and no server that could vary by territory — the
> binary is self-contained and behaves the same everywhere. The interface is
> English only in this version.
>
> **6. Regulated industry and third-party material**
>
> The app is in no regulated industry — it is a music and graphics tool with
> no health, financial, gambling, medical or similar function.
>
> It contains no protected third-party material. All code and artwork are my
> own original work, published as free software under the GNU AGPL-3.0 at
> https://github.com/vibetuned/midi-sink. The app ships no audio recordings,
> no music, no fonts, images or other licensed assets.
>
> Two points of attribution, for completeness. The marbling technique is based
> on the published mathematics of area-preserving marbling by Aubrey Jaffer,
> which is publicly documented research implemented independently here and
> credited in our documentation; no code or assets were taken. Product names
> such as ROLI, Expressive E Osmose, Roland Aerophone and Odisei Travel Sax
> appear in the description solely to state which MIDI controllers the app is
> compatible with; no affiliation or endorsement is implied and none of those
> companies' materials are included.
>
> Please let me know if anything further would help.
>
> Best regards,
> Pedro Fillastre

---

## 3. Condensed version for App Review Information → Notes

Apple asked for this to be stored for future submissions. It replaces the
shorter notes currently in `listing.md`; keep the operational detail from
those, which the reviewer still needs.

> The app needs no account, and no hardware to review.
>
> PURPOSE. midi-sink is a music visualizer and an MPE MIDI controller. Marble
> mode renders a MIDI performance as Japanese suminagashi ink marbling — an
> MPE controller's strike, pressure, glide and slide become a drop of ink, its
> feed, a comb and a hue shift — and the same water responds to fingers and
> Apple Pencil, so it can be exercised in full with nothing plugged in. Play
> mode turns the iPad into a 15-voice MPE instrument that sends standard MPE
> MIDI to other apps and devices. The app produces no audio of its own.
> Audience: musicians with expressive MIDI controllers, and anyone wanting a
> music-driven drawing surface.
>
> ACCESS. No accounts, no login, no in-app purchases, no gated content, no
> user-generated content that is shared or transmitted. Nothing to set up.
> Marble mode works immediately on launch by touch alone. For Play mode: gear
> → Pitch layout = "Chromatic grid (playable)" → Mode = Play. Play mode emits
> MIDI rather than sound, so to observe its output open any synth app on the
> same device and select the virtual CoreMIDI source named "midi-sink" as its
> input; with no destination selected the app paints but is silent, which is
> expected. "Paper dip — save the print" (gear → Canvas) writes a PNG to
> Photos. The "storm test" and "on-device suites" rows in settings are
> harmless diagnostics.
>
> EXTERNAL SERVICES: none. No data providers, authentication, payment
> processors, AI services, analytics, crash reporting, advertising or
> third-party SDKs. No networking code, no web views. Links only CoreMIDI,
> CoreAudioKit, Metal and QuartzCore. Bluetooth is used only to pair Bluetooth
> MIDI instruments and to advertise the play surface as a BLE-MIDI device;
> local network only for the CoreMIDI network session (RTP-MIDI). Neither
> reaches a server of ours — there is none. App Privacy: Data Not Collected.
>
> REGIONS: no regional differences; identical behaviour on every storefront;
> English-only interface.
>
> RIGHTS: all code and artwork are the developer's own, released as free
> software under AGPL-3.0. No licensed audio, music, fonts or images ship in
> the app. The marbling mathematics derives from Aubrey Jaffer's published
> research, implemented independently and credited. Controller brand names in
> the description are compatibility statements only.
>
> TECHNICAL: the `audio` background mode is required by iOS —
> `MIDISourceCreate` returns `kMIDINotPermitted` (-10844) without it. The app
> plays no audio itself.

---

## 4. Before replying — check these

* Record and attach the video **before** sending; the reply is what restarts
  the clock, and a reply without it will only produce the same request again.
* Record from the **TestFlight build of the submitted binary**, so what Apple
  sees is what Apple has.
* Step 7 (the synth app receiving from "midi-sink") is the single most
  valuable shot. A reviewer who has never seen an MPE controller cannot
  otherwise tell that Play mode does anything.
* Paste §3 into *App Review Information → Notes* as well as replying — Apple
  asked for both, and it spares the same request on the next submission.
