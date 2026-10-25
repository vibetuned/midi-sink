# The preset file — schema 2

Written and read by `sumi_presets` (`presets/include/sumi_preset.h`), the one
serializer every shell links. JSON, UTF-8, one object. Numbers are JSON
numbers (`%.9g` for floats, so a float round-trips exactly). Order does not
matter.

## The rule

* A file carries `midi_sink_preset` (this schema, 2 since v3.0 step 82; 1 before) and `sumi_version` (the
  `sumi_version()` that wrote it, as `[major, minor, patch]`).
* A reader **ignores keys it does not know** and **keeps its defaults for keys
  that are missing**. Arrays with more elements than the reader knows drop the
  extras; shorter arrays leave the rest as they were.
* The core validates on apply: `sumi_set_params` and `sumi_set_palette` clamp
  out-of-range values, so a hand-edited file cannot wound the engine.
* Reading fails, leaving the target untouched, only when the text is not a
  JSON object.

## Keys

| Key | Type | Meaning |
|---|---|---|
| `midi_sink_preset` | integer | the schema, 2 (step 82: the suzu block's arrays; a schema-1 reader keeps what it knows) |
| `sumi_version` | `[maj, min, patch]` | the library version that wrote the file |
| `name` | string (≤ 63 bytes) | the preset's name |
| `input_mode` | integer | `sumi_input_mode_t`: 1 MPE, 2 classic, 3 wind |
| `params` | object | every `sumi_params_t` field by its C name; arrays for `burst_order_by_class` (12) and `paper_tint` (3); from libsumi 1.6.0 (v3.0 step 71) also `polarizer_mode`, `braid_mode`, `kessho_retard`, `kessho_backlight`, `kessho_grain`, `kessho_tint`, `kessho_caustic`, and from libsumi 1.7.0 (step 71b) `drop_habit`, `habit_roundness`, `habit_blades`, `tine_profile` (the mineral's mechanical half) — the schema stays 1 (a reader keeps its defaults for keys it does not know, and these have defaults); from 1.11.0 (step 76) `anod_glow_mode` (0 plasma, 1 spark), `spark_afterglow` (seconds) |
| `palette` | object | the custom slot: `stops` as `[[r, g, b, position], …]` (2–8, linear RGB, ascending), `depth_gamma`, `depth_floor`, `hue_drift`, `accent_rgb` `[r, g, b]`, `clear_rgb` `[r, g, b]` |
| `cc_map` | `[[channel, cc, target], …]` | the CC routes; channel 255 = any; target a `sumi_ctl_t`, or from 1000 one of Voxo's bus targets (`VOXO_CTL_*`: 1000 reverb amount, 1001 room, 1002 damping, 1003 delay amount, 1004 delay time, 1005 delay feedback — Phase 7 step 53; 1006 chorus amount, 1007 chorus rate, 1008 chorus depth — v3.0 step 88); a shell without Voxo ignores those |
| `controls` | `[[ctl, value], …]` | the values (0–127) the shell sends on its routed controls — the desktop harness's ripple, Chladni, spark and Chirikov CCs, a tablet's strip values |
| `strip` | `{assign_a, assign_b}` | the control strip's latch-wheel CCs (`hostmpe_strip_assign`); 0 = unset |
| `layout_state` | `{buttons, slider}` | `sumi_layout_state_t` defaults for the stateful layouts (Phase 8) |
| `mineral` | object, optional | the mineral (libsumi 1.7.0, step 71b, `KESSHO §4`): `id` (the library's `SUMI_MINERAL_*` 0–5, 6 = edited by hand), then `sumi_mineral_t` field for field — `optic_class` (0 isotropic, 1 uniaxial, 2 biaxial), `retardation` (nm at unit strain), `retardation_bias` (nm, the sensitive-tint plate), `two_v` (rad), `walkoff` (texels), `dispersion`, `film_n`, `film_r`, `conical_ring` (texels), `body_tint` `[r, g, b]` (linear), `bloom_gain`, `lut_id` (0 the Michel-Lévy chart, 1 the custom palette as a chart); written only by a shell that carries one, kept as read (`mineral_present`); applied through `sumi_set_mineral` (the core clamps). The six shipped minerals are `presets/kessho_<mineral>.json`, each carrying both halves |
| `suzu` | object, optional | Suzu's patch (Phase 8 step 57): `source` (0 the sampler, 1 Suzu), then `voxo_suzu_params_t` field for field — `level`, `attack_s`, `release_s`, `cutoff_hz`, `resonance`, `shear`, `shear_kind`, `voice_kind`, `modal_preset`, `modes`, `coupling`, `decay_s`, `decay_bright`, `stiffness`, `pluck`, `bow_onset_s`, `bow_position`, `breath_cc`, from step 78 `oversample` (2 or 4: the cell family's section), and from step 58 `string_nodes`, `string_decay_s`, `pickup`, `bridge_hz`, `bridge_cells`, `bridge_coupling`, `bridge_decay_s`, `loop_loss`, `duffing_beta`, `drive`, `drive_ratio`, `rotor_k`, `mod_target`, `mod_depth`, `mod_rate`, and from step 58b `bore_nodes`, `bore_loss`, `bore_corner_hz`, `jet_gain`, `jet_drive`, `jet_tau`, `jet_q`, `jet_noise`, `breath_ref`, `breath_range`, `bore_wall_s`, `press_blows`, and from step 58c `reed_hz`, `reed_q`, `reed_open`, `reed_close`, `reed_area`, `reed_noise`, `cone_apex`, `lip_ratio`, `lip_q`, `lip_open`, `lip_close`, `lip_area`, `lip_range`, `partial`, `bell_start`, `bell_gamma`, `brass`, and from step 59 `trace_scope`, `trace_ink`, `trace_kinds` (a bitmask of voice kinds), `trace_segments`, `trace_stroke` (0 tine, 1 wake), `trace_scale`, `trace_canvas` (0 off, 1 the orbits over the water, 2 the scope instead of the water), and from step 75 `trace_mode` (0 continuous, 1 the Poincaré strobe on the bass phasor, 2 the strobe on each voice's own drive clock), and from step 79 `drum_size`, `drum_kappa`, `drum_alpha`, `drum_cap` (the membrane) and `plate_on`, `plate_size`, `plate_hz`, `plate_kappa`, `plate_decay_s`, `plate_bright`, `plate_gain` (the bus plate), and from step 80 `bow_mode` (0 the servo, 1 the stick-slip), `bow_force`, `bow_speed`, and step 81's `reed_inertance`, `bore_leak`, `brass_shock`, `lip_shear`, `lip_shear_ratio`, `tonehole_mode` (0 the bore cut per note, 1 the holes), `hole_size`, `hole_loss`, and step 82's `modal_comp` (1 the coupling compensated, 0 the raw lattice) with the three arrays `modal_ratio`, `modal_t60`, `modal_weight` (sixteen numbers each, the editable table under `modal_preset` 5; extras dropped, a shorter array leaves the rest; `modal_ratio[0]` 0 = a file from before, the defaults stand), and step 83's `toda_nodes`, `toda_b`, `toda_cap`, `toda_width` (the Toda ring, voice kind 10), and step 84's `boris_b`, `boris_b_rest` (the Boris gyroscope, voice kind 11; `boris_b` 0 = a file from before, the defaults stand), and step 85's `dirac_masses`, `dirac_bridge_hz`, `dirac_bridge_mass`, `dirac_stop`, `dirac_stop_loss`, `dirac_bridge_decay` (the Dirac network, voice kind 12; `dirac_masses` 0 = the defaults stand) and `bill_drive`, `bill_aspect`, `bill_length`, `bill_length_rest` (the billiard, voice kind 13; `bill_drive` 0 = the defaults stand), and step 86's mod matrix `mod_src`, `mod_dst`, `mod_amt` — three arrays of four, the slots' sources (0 none, 1 the voice's chaotic modulator, 2 the press, 3 the swirl, 4 the slide, 5 the wheel, 6 the breath, 7 the velocity), targets (0 none, 1 the cutoff, 2 the coupling, 3 the rotor's K, 4 the drive, 5 the level, 6 the pitch) and depths (−1..1); the legacy `mod_target`/`mod_depth` pair is slot 0 when that slot is empty (`source` 2 = both layered; `voice_kind` 0 cell, 1 lattice, 2 Verlet string, 3 hybrid string, 4 Duffing cell, 5 kicked rotor, 6 flute, 7 saxophone, 8 trumpet, 9 membrane, 10 Toda ring, 11 Boris gyroscope, 12 Dirac network, 13 billiard); written only by a shell that carries one, kept as read (`suzu_present`); a shell without Voxo ignores it |

## What the shells do with it

`sumi_preset_apply` pushes `params`, `input_mode`, `palette`, `cc_map` and, when
carried, `mineral` into the instance. `controls`, `strip`, `layout_state` and `suzu` live host-side: the shell
sends the control values through its MIDI producer, assigns its strip and
keeps the layout state beside its params snapshot.

The desktop keeps the last session as `<config>/last_session.json` and named
presets as `<config>/presets/<name>.json`; a tablet keeps them in its
documents folder and offers Files export/import; the web keeps them in
`localStorage` and offers a download. All of them are this file.

## Example

```json
{
  "midi_sink_preset": 1,
  "sumi_version": [1, 1, 0],
  "name": "Indigo evening",
  "input_mode": 1,
  "params": { "fluid_viscosity": 0.5, "active_palette_id": 1, "medium": 0, "paper_tint": [0.9, 0.868, 0.79] },
  "palette": { "stops": [[0.6, 0.05, 0.02, 0], [0.01, 0.01, 0.01, 1]], "depth_gamma": 1, "depth_floor": 0,
               "hue_drift": 0.3, "accent_rgb": [0.3, 0.06, 0.02], "clear_rgb": [0.85, 0.8, 0.78] },
  "cc_map": [[255, 1, 5], [255, 74, 8]],
  "controls": [[9, 32]],
  "strip": {"assign_a": 23, "assign_b": 24},
  "layout_state": {"buttons": 0, "slider": 0}
}
```
