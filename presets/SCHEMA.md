# The preset file — schema 1

Written and read by `sumi_presets` (`presets/include/sumi_preset.h`), the one
serializer every shell links. JSON, UTF-8, one object. Numbers are JSON
numbers (`%.9g` for floats, so a float round-trips exactly). Order does not
matter.

## The rule

* A file carries `midi_sink_preset` (this schema, 1) and `sumi_version` (the
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
| `midi_sink_preset` | integer | the schema, 1 |
| `sumi_version` | `[maj, min, patch]` | the library version that wrote the file |
| `name` | string (≤ 63 bytes) | the preset's name |
| `input_mode` | integer | `sumi_input_mode_t`: 1 MPE, 2 classic, 3 wind |
| `params` | object | every `sumi_params_t` field by its C name; arrays for `burst_order_by_class` (12) and `paper_tint` (3) |
| `palette` | object | the custom slot: `stops` as `[[r, g, b, position], …]` (2–8, linear RGB, ascending), `depth_gamma`, `depth_floor`, `hue_drift`, `accent_rgb` `[r, g, b]`, `clear_rgb` `[r, g, b]` |
| `cc_map` | `[[channel, cc, target], …]` | the CC routes; channel 255 = any; target a `sumi_ctl_t`, or from 1000 one of Voxo's bus targets (`VOXO_CTL_*`: 1000 reverb amount, 1001 room, 1002 damping, 1003 delay amount, 1004 delay time, 1005 delay feedback — Phase 7 step 53); a shell without Voxo ignores those |
| `controls` | `[[ctl, value], …]` | the values (0–127) the shell sends on its routed controls — the desktop harness's ripple, Chladni, spark and Chirikov CCs, a tablet's strip values |
| `strip` | `{assign_a, assign_b}` | the control strip's latch-wheel CCs (`hostmpe_strip_assign`); 0 = unset |
| `layout_state` | `{buttons, slider}` | `sumi_layout_state_t` defaults for the stateful layouts (Phase 8) |

## What the shells do with it

`sumi_preset_apply` pushes `params`, `input_mode`, `palette` and `cc_map` into
the instance. `controls`, `strip` and `layout_state` live host-side: the shell
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
