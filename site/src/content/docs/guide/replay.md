---
title: Replay
description: Record a session and play it back frame for frame — on the device that recorded it, on another, or in the browser — the water redrawn from the same bytes and gestures at the same frame boundaries, and the sound re-sounded on the native shells.
---

A performance on midi-sink is MIDI, a handful of gesture calls and a
session; a **recording** writes all three the way the engine saw them —
frame by frame, with each frame's time step and the bytes it drained — and
a **replay** drives the engine through those frames again. On the device
that recorded it the water comes back **bit for bit**; on another device it
comes back within the cross-backend tolerance the release gates hold the
field to, which the eye does not see. The web plays the same file.

## Recording

* **Desktop** — Settings → *Replay* → **Record**; the window title shows
  the frames and seconds; **Stop recording** writes
  `<config>/replays/<timestamp>.sumireplay`.
* **iPad and Android** — *Replay* on the sheet: **Record** / **Stop
  recording**; the file lands in the *Recordings* list, with *Export* to
  Files (or the system picker) and **Import a recording…** for one from
  another device.

Recording starts with the sheet kept **and dipped** — the dip is the
first event, so a replay starts from plain water — and with the session
config and the strip's announce re-sent, so frame 0 carries the MPE
configuration and the fingering. Everything after is what the engine
received: device bytes, the play surface's bytes, the pen's and the
fingers' gestures, the orbit trace's segments, settings changes, resizes
and dips. A recording is capped at four million events; the file is text.

## Playing

* **Desktop** — *Replay* → pick a recording, **Play**, or **Play file** for
  one from elsewhere; a progress bar, the source device in the title, and
  **Stop replay**.
* **Tablets** — tap a recording in the list; a banner at the top names the
  source (device, platform, backend, app version, date) with the elapsed
  time and **Stop**.
* **Web** — the panel's **Replay a recording…** opens a local file, or
  `/marble/?replay=<url>` fetches one: the gallery's "watch it again" link.

While a replay plays, **live input is muted** — the keyboard, the fingers,
the pen, Web MIDI — and the settings apply only the palette: the physics is
the recording's, the look is yours. The replayed sheet is drawn at **your
canvas size and palette**, not the recording's, so you can dip and print it
at your resolution; the recording's size is used only by the gates. On the
desktop and the tablets the replay **re-sounds**: the same bytes reach the
sound engine as they reached it when played. The web has no sound core, so
a replay there is the picture alone.

A 120 Hz recording plays on a 60 Hz display two recorded frames per
display frame (the desktop turns display sync off meanwhile), never more
than a quarter second behind; the sound follows the frames. At the end —
or at Stop — your session is re-applied and the sheet is left as the
replay left it.

## What a file carries

Plain UTF-8 text, extension `.sumireplay`, documented in the tree as
`replay/FORMAT.md`: a header (platform, backend, device, app, engine
version, date, size), the session as a [preset](../presets/) writes it,
then the events. The same schema rule as the presets': unknown lines are
skipped, a newer schema or a truncated file refused.

## The same file on another device

A recording made on the iPad replays on the Mac, the Tab and the phone,
and in Chrome. Measured: the Mac replaying its own recording, and the
iPad its own, are bitwise; a Pixel's recording on the Mac is within the
mobile tier the release gates hold the field to; the iPad's on the Mac
differs by a few percent of displacement — the ink bands identical to the
half-float's last place, the deformation under them a little different,
accrued while the passes ran — which the author's eye called almost
imperceptible. That gap is recorded as an open question in the
[design notes](../../notes/decisions/part-8/) with the experiments that
would close it.

## For the gates

The desktop bench records a canonical performance through the real
recorder and replays it against its own field dump; the negative test
re-buckets the bytes by wall time on another cadence and must diverge —
proof that the frame boundaries matter. The web gate runs the same two
checks in headless Chrome. [Architecture → Replay](../../architecture/replay/)
