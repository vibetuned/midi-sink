# Step 81 — The acoustic bore v2: the toneholes, the two-mode lips, the brass shock and the leak (2026-10-19)

Voxo **0.21.0**, additive (`DECISIONS_12 #5`; `_work/ROADMAP_6.md` step 81;
`_work/TO_PROJECT_SPEC.md` §13.11 as shipped). The core is frozen: nothing
under `core/` moved.

## What shipped

* **The toneholes** (`tonehole_mode` 1; the flute and the sax): branch ports
  at the bore's pressure nodes — an inertance kicked at the half step as u
  is, radiating through a one-pole resistance at twice the bore's corner,
  its flow in the node's continuity, a closed hole a wall; `Bore::holes_set
  / holes_open / holes_energy`, `HOLES_MAX` 24 (eleven holes and the vent,
  each over two nodes: a fractional place). A hole is a flute's hole
  (1/L_h = S/8 per `hole_size`), the vent a register key (90 cells', at an
  eighth of the length, open from the second register). The keys are the
  core's nine (CC 110–112 and 114–119 on the master channel into
  `voxo_t::keys`), the chart the core's Boehm rule copied
  (`boehm_note_of`); the bore cut for the register's lowest note, hole j
  where the j-th semitone would end it (a cone counting its apex), the
  holes 1..j open. **The pickup is what leaves the bore**: the far end's
  velocity plus the open holes' flows. The calibration at patch load, per
  register, cached per patch slot: pulsed on the flute (221 ms), pulsed and
  then blown twice on the sax (`suzu_blow_offset`, 777 ms). Two registers;
  the third and a disagreeing key take the v1 cut. The inspection's
  `k[11..13]`.
* **The leak** (`bore_leak`, the sax's bell's; 0.3): a one-pole resistance
  at 40 Hz on the bell's slow flow, positive-real; its reactance a negative
  end correction the cut counts (`Bore::leak_correction`). The sax's
  quasi-periodic A3 cured. The flute and the trumpet take none (it cost
  their bass). `reed_inertance` as proposed (0 dflt; partial).
* **The brass shock** (`brass_shock` 1, the trumpet's; `shock_limiter` the
  lab's): the local Courant number λ(1 + κ·p̄) on both sweeps, saturated at
  20 P_REF with the base λ leaving room (`Bore::shock`, `shock_room`); the
  Lapidus flux limiter, its loss `shock_loss` → `k[10]` in the ledger,
  summed after the vectorised passes (a running sum had serialised them:
  the storm's XRuns). The sax's cone keeps the v1 bore.
* **The two-mode lips** (`lip_shear` 0, `lip_shear_ratio` 1.5): the
  shearing mode as an inward-striking swing beside the outward one
  (`Valve::setup_shear`), its mass in the valve's energy.
* **The plumbing**: the web table's nine, the preset library's eight fields
  (`hole_size` 0 = the defaults stand), the desktop's rows (*Suzu toneholes*
  / *hole size* / *hole loss*, *reed inertance*, *bore leak* on the sax,
  *brass shock*, *lip shear* / *ratio*), `main.cpp`, `dev_tools.cpp`, the
  suite's field table and gates 42–45, `voxo_c_compile.c` 0.21.0, the web
  gate's sax in the libm class, the three profiles re-rendered into the
  book, the winds chapter's new section, the settings reference's row.

## Findings (the decision entry has them in full)

* The holed bore must be read where it leaves: at the far end, past the
  lattice, the fundamental is evanescent (the flute's sat 40 dB under its
  fifth partial while the oscillation inside was right).
* A flat hole resistance eats the fundamental; a cell-sized hole cuts
  nothing cleanly and breaks the bound; the hole mode needs CFL room
  (Gershgorin); a cone's holes count the apex; the vent must be a vent.
* On the sax the passive peak and the reed's note differ by a quarter-tone
  on the hole bores: the second calibration pass blows.
* The quasi-periodic A3 was the bore's undamped slow mode, not the reed's
  inertia; the leak takes it, and its reactance must be counted (the flute
  read +40 cents before it was).
* The leak at the flute's open ends and the trumpet's bell cost their bass
  (22 dB at the flute's C2): the leak is the sax's alone. Only the profiles
  showed it — as they showed the sax's G7 blowing up at λ 0.989 under the
  leak's reactance (a leaky bell now takes 0.96 of the bound).
* The shock built on the sax cost it 80 % of its render for nothing the
  spec claims of a cone: it is the trumpet's. The limiter's loss reduction
  serialised its vectorised passes (the trumpet 790 µs a block for thirteen
  voices, the storm's XRuns): the terms are summed after the passes (615). And every wind strike ran
  the bore's power iteration twice (40–60 µs each; fifteen strikes in a
  block cost 1.5 ms): the profile's μ is cached per patch (`WindCal.mu64`),
  the flute's once per engine.
* The shock's red moved with the leak's removal: the unlimited steepening
  rings in the bore (its profile 1.46 rough against 1.01) while the bell
  keeps it out of the output; at 2κ the valve gate refuses it at load.

## Gates

| Gate | Result | File |
|---|---|---|
| the sax at A3, breath 50 / 70 / 100: the sidebands (bore_leak 0.4) | −63.2 / −62.8 / −61.8 dB under the harmonics (< −45); the pitch +1 / 0 / −1 cents | `suite_step81_blocks.txt` |
| RED: `bore_leak` 0 | the worst sideband +13.8 dB (above the harmonic): the quasi-periodic A3 of DECISIONS_7 #33 | same |
| the trumpet at C4, breath 127, under the shock | the ledger holds (0.117 of 42 of work); the limiter's loss 1.29 (0.083 at breath 60); the 6–20 kHz band −22.5 dB, +9.8 dB over the v1 bore's −32.3; the cost 620 µs a block for thirteen voices against the v1 bore's 396 (`probes/wind_cost.txt`) | same |
| RED: the limiter off | at κ the bore's profile 1.46 rough against 1.01 (the output's high band −20.4 against −22.5: the bell keeps the ringing in); at 2κ the valve gate refuses the patch at load (the junction makes energy: 1.1× the mouth's work with the losses zeroed), the limited 2κ admitted | same |
| the two-mode lips at share 0.3 | the registers −806 / +19 / +1284 cents | same |
| the one-mode lips read none of the shear's knobs | bit for bit under the ratio | same |
| the flute with the toneholes: the twelve fingerings D4..C♯5 | +16 +7 +6 +8 +9 +9 +8 +6 +8 +10 +10 +10 cents (the +16 the plain cut's own at breath 70); the inspection's semitone and holes right; the level the plain cut's | same |
| the registers and the chart | D5 with the D4 keys = the second register, the vent open (+32); G4 with the D4 keys = the v1 cut (j −1, +21); D6 past the two registers | same |
| the sax with the toneholes: F4, A4, B4 | −6 / −13 / +3 cents | same |
| the calibration's cost at patch load | the flute 205 ms, the sax 559 ms | same |
| the Release suite | all 199 green | `suite_release.txt` |
| the web gate | GREEN — the sax joins the libm class (it read −102.6 dB under the leaky-bell cut; the final build bit-identical); the flute −91.7, the trumpet −704.7 | `gates/suzu_web_gate.txt` |
| the lab gate (six pages) | GREEN | `gates/suzu_lab_gate.txt` |
| the fuzz (Release) | 60 s clean; the strobe 30 s: 0 blocks differ, 0 non-finite | `gates/voxo_fuzz.txt` |
| ctest (Debug) | 14/14 | `gates/ctest.txt` |
| the storms from the Release universal build | the flute, the sax and the trumpet: 0 XRuns, render max 0.66 / 0.68 / 0.97 ms (the trumpet had thrown 1–60 XRuns at 2.0 ms before the limiter's loss sum and the strike's μ cache) | `gates/storm_*.txt` |
| the profiles | the flute C2..C7 within 7.8 dB at breath 0.44; the sax within 6.5 at 0.55 (no note above −28 dBFS: the G7 spike gone); the trumpet C3..C6 within 2.2 at 0.55 (C2..C7 27.9: its range starts at C3) | `profiles/` |
| the leak against the bass (the probe) | the flute at C2 / G2 / C3: −22 / −16 / −12 dB under the leak; the trumpet at C2 −7 | `probes/leak_bass_probe.txt` |
| the hole lattice's stability (the probes) | the bare bore at 0.99 of the bound holds up to 1/L_h = S/2, grows at S; at 0.999 the register bores grow with 1/8 | `probes/bore_hole_stab.cpp` |
| the holed bore's admittance (the probes) | ten open holes read at the far end: no fundamental at the mouth drive; the standing wave inside right | `probes/bore_hole_sweep.cpp`, `bore_resp.cpp` |

## The tablets

* **Android (the Pixel 10 Pro emulator from the release APK signed with the
  debug key)**: the Flute, Saxophone and Trumpet patches under the spike —
  5 286 / 5 275 / 5 275 callbacks, 0 proxy XRuns, 0 device XRuns (AAudio),
  render max 1.13 / 1.22 / 1.17 ms of a 20 ms burst (2.0 / 1.9 / 1.9 before
  the strike's μ cache). `tablets/android_emulator_patch{2,3,4}.txt`.
* **Android (the Galaxy Tab S8 Ultra, the same release APK)**, at its
  low-latency 192-frame burst (4 ms): the flute 2 device XRuns, render max
  3.12 ms; the sax 0 / 0, 2.70 ms; the trumpet 3 proxy + 1 device XRuns,
  4.27 ms. **The step-80 APK on the same Tab, the same runs**: the flute
  1 device XRun at 2.56 ms, the sax 1 at 2.92 ms, the trumpet 1 proxy XRun
  at 4.01 ms — the Tab's twelve wind voices at 4 ms were at the edge before
  the step; the shock's net cost on the trumpet after the caches is 4.0 →
  4.3 ms there, the sax is cheaper (no shock), the flute's difference is
  the run's. `tablets/android_tab_patch{2,3,4}.txt`,
  `android_tab_step80_patch{2,3,4}.txt`. The spike plays the plain cut (no
  fingering keys): the load's evidence.
* **iOS (the iPad Air M4)**: `build-ios` rebuilt, the app built on the
  existing project and installed, the three runs launched and their CSVs
  copied out fresh (Voxo 0.21.0's header): 8 003 / 7 962 / 7 932 callbacks,
  0 XRuns, render max 0.42 / 0.41 / 0.70 ms of a 2.67 ms burst.
  `tablets/ipad_voxo_spike_patch{2,3,4}.csv`.

## Carried

* The hole mode's third register (past the lattice's cutoff the holes
  cannot cut; a player vents the altissimo).
* The sax's hole intonation to ±10 cents (the blown pass measures a
  jittering reed).
* The lab's shock-limiter and leak red controls on the winds page.
* The tablets' key panels driving the holes (the Boehm layout's keys reach
  the engine; the mode is a patch knob).
* Both tablets under the finger with the fingering keys (the spike plays the
  plain cut); the Tab's twelve wind voices at its 4 ms burst (at the edge
  before the step).
