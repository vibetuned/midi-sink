# The 2.0 release round — after the Phase-10 close (`DECISIONS_9 #16`–`#17`)

Two findings of the author's on the way to the store builds (2026-10-13).

## The trumpet saturating on the Tab (#16)

Pre-knee peaks (the master gain held at 0.05 so Voxo's knee never engages;
the peak read back over 0.05), C4 at velocity 100, member channels, MPE:

| Patch | Voices | Drive | Before | After |
|---|---|---|---|---|
| Trumpet | 1 | breath 127 (CC 2), no press | 0.230 (−12.8 dBFS) | 0.230 |
| Trumpet | 1 | pressure 127, no breath | 0.656 (−3.7 dBFS) — in the knee | 0.230 (−12.8 dBFS) |
| Trumpet | 3 | pressure 127 | 1.955 (+5.8 dBFS) — deep in the knee | 0.684 (−3.3 dBFS, the knee's start) |
| Flute | 3 | pressure 127 | 0.656 — in the knee | 0.230 |
| Saxophone | 3 | pressure 127 | 0.405 | 0.142 |
| Bowed string | 3 | pressure 127 | 0.408 | 0.408 (unchanged — the bow keeps both consumers) |

The 9.1 dB between the first two rows is the output gain's 0.35 → 1.0: the
press was the mouth AND the gain. Voxo 0.16.1 holds the winds' gain at the
breath controller's when the press blows.

| Gate | Result | File |
|---|---|---|
| `voxo_suzu_tests` with gate 29 | all green (29 gates) | `voxo_suzu_tests.txt` |
| `ctest --test-dir build` | 12/12 | — |
| the level table after the fix | `levels_after.txt` (the scratch program's output) | `levels_after.txt` |
| the Tab | APK rebuilt and installed — the author's ear | — |

## The iOS `audio` background mode (#17)

| Build | Console line | File |
|---|---|---|
| without `UIBackgroundModes: audio` | `vsrc=0(-10844) ble=0(-10844)`, every send `DROPPED: virtualEnabled=false` | `ios_without_audio_mode.txt` |
| with it (restored, the iPad reinstalled) | the virtual source and the BLE source created | `ios_with_audio_mode.txt` |

The key stays; `ios/project.yml`'s comment carries the measurement.
