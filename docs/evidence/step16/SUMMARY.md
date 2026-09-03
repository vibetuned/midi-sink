# Step 16 — hostmpe allocator + loopback Play mode (iOS, fingers): DONE evidence

Specs: `_work/PHASE4_SPEC.md` §3 (rev: Y→pressure upward-only fingers), §4,
§5.1, §5.2; `_work/ROADMAP_3.md` Step 16 (rev). Decisions: `_work/
DECISIONS_3.md` #10–#21. Device: iPad Air 11" (M4); ROLI Piano over Bluetooth
MIDI for the hardware tests. The byte log (`midi_log.csv`) is the tap at the
§5.2 merge point: every message that crossed `sumi_push_midi`, tagged
external / touch / session-config.

| DONE criterion | Result | Evidence |
|---|---|---|
| hostmpe unit suite passes headlessly | **PASS — 324 checks**: LRU-by-release round-robin (release 7 then 9 → next allocs are 7 then 9; most-recently-released never reused while another is free), saturation = silent drop (no messages, never steal), external-occupancy masking with 30 s activity-refreshed timeout, **one grid column = exactly ±171 bend counts** (and 2 columns = 341 — absolute tracking), Y→pressure upward-only (0 at touch-down, 0 downward, monotonic through the floored knee, 127 at full-radius up), **no CC74 from fingers ever**, change-only emission, session config (MCM + RPN 0 = 48 × 15 members) | `tests/hostmpe_tests.cpp`, ctest 4/4 |
| Loopback conformance (working rule) | **PASS** — hostmpe bytes through OUR normalizer: MCM flips MPE + 15 members deterministically; touch → VoiceBegin at the probed cell center; one-column drag decodes back as **exactly 1.0 semitone** (full MCM/RPN0/±48 round trip) with pressure decoded from the upward component; lift → VoiceEnd with the lift velocity | `tests/normalizer_tests.cpp` `test_hostmpe_loopback_conformance` (12,485-check suite) |
| Emit order: center-bend-before-Note-On, pressure-0-before-Note-Off | **PASS in unit tests AND in the live byte log**: all 36 real touch Note Ons immediately preceded by a center bend on their channel; all 36 Note Offs preceded by pressure 0; byte-log assert script green | `midi_log.csv` + assert run (below) |
| Channel-steal test with the ROLI | **PASS** — live session: 2,766 external (ROLI) messages with held chords across member channels 1–15 while 36 touches played: **zero touch allocations on externally-held channels**, ROLI voices never truncated, both performances painted simultaneously (user-confirmed) | `midi_log.csv`: 5,036 messages total; assert: 0 steal violations |
| In-tune feel: drops spawn at the touched cell with centered bend; one-column drags = clean semitone glides on canvas | **PASS** — byte log shows centered bend before every strike; the glide cap was LIFTED for the playable lattices (DECISIONS_3 #20) so drops visibly travel the true lattice step under the finger; user-confirmed feel after the #15→#19 pressure revision (Y-up growth) and #16 deadband floor | user feel pass + `midi_log.csv` |
| Touch-down → visible drop ≤ 2 frames | **PASS** — in-app measurement (touch-down timestamp → completion of the first `sumi_render` that could show the drop): 36 samples, **min 0.29 / median 0.39 / max 0.55 ms** — the loopback enqueue always beats the same frame's update drain; ≤ 2 frames (33 ms) by two orders of magnitude. os_signpost intervals emitted for Instruments corroboration | `latency_log.csv` |
| 10-finger chords allocate distinct channels; clean release; no stuck voices; dropped = 0 | **PASS** — touch voices used **all 15 distinct member channels** across the session (LRU rotation visible in the log); 36 on / 36 off, **0 open voices at session end**; ~6-minute session at a flat 60 fps, thermal nominal; status line showed dropped 0 throughout (user-observed; the byte volume, ~5 k messages, is 4 orders below the stress-proven ring rate) | `midi_log.csv`, `session_log.csv` |

Byte-log assert script (run on the pulled log): center-bend-before-NoteOn per
channel, pressure-0-before-NoteOff per channel, zero CC74 with src=touch,
zero touch NoteOns on channels with an open external note, on/off balance,
distinct-channel census. Result: **ALL ASSERTS PASS**.

Feel iterations during the step, all logged in DECISIONS_3:
- #10 knee = deadband not travel limit (the ±171 exactness proof);
- #15→#19 pressure: majorRadius abandoned by spec revision — **Y-up is
  pressure** (0 at touch-down/downward, knee-monotonic, 127 full-up), fingers
  emit no CC74 (timbre belongs to the Step-18 stylus);
- #16 absolute deadband floor (Jankó cells smaller than a finger);
- #17→#18 bend direction: Jankó's semitone axis is HORIZONTAL in the core
  (pitch lives on x; rows are echoes) — glides read the same on both
  playable layouts, one semitone per half column;
- #20 true-step glides on the lattices (the far-bend "stray white drop" was
  the §3.4 lift ring at the old capped position);
- #21 the lift ring lands at the note's home cell ("the tablet is the
  instrument" — the release mark belongs to the note, not the bend).
