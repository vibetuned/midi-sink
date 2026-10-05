# The replay file — schema 1

Written and read by `sumi_replay` (`replay/include/sumi_replay.h`), the
library every native shell links (Phase 9 step 65, QOL §1, `DECISIONS_8
#22`). Plain UTF-8 text, one event per line, extension `.sumireplay`. The
same file replays on the desktop, the iPad and the Tab (the web's playback is
step 66's).

## The idea

Everything musical the engine sees is MIDI, the marble gestures are a handful
of calls, and the look is the session preset. A recording is the session at
the start, then, FRAME BY FRAME, the frame's dt and wall time, the bytes the
core drained in that frame, the gesture calls made before its update, the
state changes, the resizes and the dips. The frame boundary is the drain
point: the engine coalesces continuous dimensions per `sumi_update`, so a
replay reconstructs the field only when every byte lands in the frame that
drained it and every update runs with its recorded dt. The recorder makes
that exact by construction — the shell's MIDI producer stages its bytes and
the render thread hands them to the core at the start of the frame, stamping
them — and the player drives the scripted clock through the recorded
boundaries. Re-bucketing the bytes by wall time on another cadence is the
negative test (`sumi_replay_rebucket`): it must diverge.

## The lines

```
#sumi-replay 1                 the magic and the schema
platform ios                   macos | linux | windows | ios | android | web
backend metal                  metal | gl | d3d11 | gles | webgpu
device iPad16,8                the machine or model name
app 2.0.0-alpha.3+12           the app version
sumi 1.5.0                     sumi_version() of the writer
recorded 2026-10-05T21:23:39Z  ISO 8601, UTC
size 640 445 1                 the instance's size at the start: width height pixel_ratio
frames 1223                    hints (a reader pre-allocates; it never trusts them)
events 133
seconds 20.449131
#session                       the session as the preset serializer writes it (presets/SCHEMA.md), until the next marker —
{ ... }                        the params AS THE CORE HELD THEM (sim_scale included), the input dialect, the CC map, the palette
#events
F <dt> <t>                     a frame boundary: dt at %.17g (bit for bit), its wall time in seconds since the start
M <t> <status> <d1> <d2> <src> a byte the core drained in this frame; src 0 an external device, 1 the shell, 2 session config
G <kind> <n> <args…>           a gesture call before this frame's update: kind (below), n args at %.9g
S                              a state change: the session as applied, a JSON block until the line `#end`
R <w> <h> <ratio>              a resize
D                              a paper dip
#eof                           the end marker: a file without it is truncated and refused
```

Events follow the `F` line of their frame. Gesture kinds: 0 tap (x y r),
1 pinch (x y k angle span), 2 twist (x y strength radius profile), 3 press
(x y R up down dt), 4 press end, 5 tine (x0 y0 x1 y1 alpha magnitude), 6 wake
(x0 y0 x1 y1 tip), 7 drop (x y radius layer), 8 vortex (x y strength radius
profile), 9 raw pinch (x y k angle) — the core's calls, in `sumi_core.h`.

## The rules

* A reader IGNORES header lines, event lines and gesture kinds it does not
  know (the presets' schema rule); it refuses a schema newer than its own, a
  file without `#eof`, and a `#session` or `S` block without its end.
* Wall times are relative to the first frame; `M` carries the moment the
  producer staged the byte. They pace nothing on playback (the frames do);
  they are the material of the negative test and of the tools.
* A replay applies the session's params, input dialect and CC map; the palette
  and the size are the viewer's unless asked (`SUMI_REPLAY_APPLY_PALETTE`,
  `SUMI_REPLAY_APPLY_SIZE` — the bench's gate asks for both). The paper is
  not dipped by the player: a recording starts with its own dip (the shells
  dip when they start recording), the first event of frame 0.
* The sound's bus routes in the CC map (targets ≥ 1000) are the shell's; the
  player maps the core's routes only.
