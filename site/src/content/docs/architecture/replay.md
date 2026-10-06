---
title: Replay — the recorder and the player
description: Why a recording is exact — the frame boundary is the drain point, so the shell's MIDI producer stages its bytes and the render thread hands them to the core at each frame start — and how playback drives the scripted clock through the recorded boundaries on every shell, the browser included.
---

Everything musical the engine sees is MIDI, the marble gestures are a
handful of calls, and the look is the session preset. A recording is the
session at the start, then, **frame by frame**, the frame's time step and
wall time, the bytes the core drained in that frame, the gesture calls made
before its update, the state changes, the resizes and the dips. The library
is `replay/` beside the presets — pure C11, libc only, the same pure-C
discipline as the core's header — and every native shell and the web page
link it.

## The frame boundary is the drain point

The engine coalesces continuous dimensions per `sumi_update` and drains its
ring only there. A byte stamped with a frame counter at push time could
land a frame off — in the window between reading the counter and the
ring's observation inside the update — and the field would amplify the
difference. So while a recording runs the shell's MIDI producer does not
push to the core at all: it **stages** its bytes in the recorder's
wait-free SPSC ring (compiler atomics; MSVC's C mode lacks `<stdatomic.h>`)
— the desktop harness through its stage hook (the sound engine's tap still
fires at once), the iPad's MIDI queue in its push, the Tab's producer under
its mutex — and the **render thread hands them to the core at the start of
each frame**, stamped with that frame, then closes the boundary with the
frame's dt (written at `%.17g`, bit for bit) and wall time. The core sees
the bytes in the update it would have seen them in. The flag that routes
the producer flips under the producer's own serialization, so no byte is in
flight across the switch, and the stop drains the stage into the core
unrecorded.

The gesture calls go through wrappers that call the core and record (the
desktop's replay host, the orbit trace's ink segments through a hook, the
iPad's and the Tab's gesture entry points); the settings' apply reports the
session as applied; the print ledger's dip and the resizes report. Record
starts with the sheet kept and dipped — the first event — and the session
config and the strip's announce re-sent, so frame 0 carries the MPE
configuration and the fingering. Caps: four million events, 64 MB of state
text, a 4096-byte stage (a render thread stalled a second drops and
counts).

## The file

Plain text, `#sumi-replay 1`, a header (platform, backend, device, app,
engine version, date, size), the session block as the one serializer
writes it — the params **as the core holds them**, the simulation scale
included, the dialect, the CC map, the palette — then `F dt t` per frame,
`M` bytes (status, data, a source tag, the moment staged), `G` gesture
calls (ten kinds, the core's arguments), `S … #end` state blocks, `R`
resizes, `D` dips, `#eof`. The schema rule is the presets': unknown lines
and kinds are skipped; a newer schema, a missing end marker or an
unterminated block is refused. The format is `replay/FORMAT.md` in the
tree.

## Playback drives the scripted clock

The player feeds a frame's events to a sink; the apply unit — the
library's one translation unit that links the core and the presets — puts
them on an instance: the bytes through the shell's push (the core and the
sound engine both, so a replay **re-sounds**), the gestures on the core,
the state through the serializer (params, dialect and the core's CC routes;
the sound's bus routes stay the shell's), the dips. The palette and the
size apply only under flags: the bench's gate asks for both, the shells
for neither — the replayed sheet stays on the viewer's canvas at the
viewer's size and look, for the ledger to dip and print.

**One recorded frame is one update and one render.** `sumi_render`
executes the passes the update queued, with that update's dt, so two
updates before a render would merge their pass groups. The shells
accumulate the display's dt and run recorded frames while the account is
positive — at most eight a frame, never more than a quarter second behind;
the sound follows the frames — and re-composite alone when none is due.
The desktop turns Metal's display sync off while a replay plays so several
presents fit one refresh (a 120 Hz recording on a 60 Hz display); the
iPad's 60 Hz link plays a 120 Hz recording two frames a tick. The harness
is muted on the desktop and the producer flag dropped on the tablets: the
render thread is the one producer of both rings meanwhile. The settings
apply only the palette during playback; the surface's gestures are
ignored; the live orbit trace stays out (the recording carries its
segments); at the end, or at Stop, the viewer's session is re-applied and
the sheet left.

**The browser** links the same library into the wasm: the page fetches
`?replay=<url>` (or opens a local file) and runs it on the scripted clock
— as many recorded frames per animation frame as the wall clock asks,
forty a frame under `?pace=0`, never every frame in one task because
WebGPU's readbacks need the task to end — with the pointer and Web MIDI
muted. The web has no sound core, so a replay there is the picture.

## The gates, and what cross-device determinism measured

The desktop bench records a canonical performance through the real
recorder (`--record-demo`: a tablet's session config in frame 0, strokes,
taps, two MPE voices with bend, pressure and CC 74 sweeps, a twist, a
press, a viscosity change, the trumpet under valve CCs, a dip, a wake, and
a held note bending every frame to the end — so that the negative test
bites) with its field dump beside it; `--replay` plays it back and dumps
the field; `tools/replay_gate.py` compares them at the tier and then
**re-buckets the bytes by wall time** on another cadence
(`sumi_replay_rebucket`, the one place wall time is used), which must
diverge. The web gate runs the same two checks in headless Chrome.

Measured: the Mac replaying its own demo, **bitwise**; re-bucketed at
60 Hz, diverged (max 1.41 in the ink). The iPad replaying its own lab
recording, bitwise. A Pixel 9 Pro's recording replayed on the Mac, within
the mobile tier. The iPad's recording on the Mac: the ink channel to the
half-float ulp, aux exact, the displacement a smooth few percent of its
amplitude off, accrued while the passes ran and frozen after — not the
clock origin, not the build type, not the dump path, not the wrap modes,
not the shader text (the same MSL on both, compiled by each OS's Metal
compiler). It is an open question with two experiments named in the
[design notes](../../notes/decisions/part-8/): the iOS simulator on the
Mac's GPU, and the phone replaying the iPad's file.
