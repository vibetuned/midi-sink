import sys, time, subprocess, os, json
exec(open(sys.argv[2]).read())          # the helper (x11auto.py), OUT = sys.argv[1]
from PIL import Image
import numpy as np
M = sys.argv[3]                          # midi dir
R = {}                                    # results
def ink(png):
    a = np.asarray(Image.open(png).convert("L")); m = a < 120
    if m.sum() == 0: return (0, None)
    ys, xs = np.where(m); return (int(m.sum()), (float(xs.mean()) / a.shape[1], float(ys.mean()) / a.shape[0]))

# ---- A2: F11 fullscreen and back ---------------------------------------------------
log = f"{OUT}/a2_f11.log"; p = launch([], 14, log); wid, ax, ay, w, h = canvas(); time.sleep(1.5)
g0 = geom(wid); move(ax + w // 2, ay + h // 2); time.sleep(0.2)
key("F11", True); key("F11", False); time.sleep(2.5); g1 = geom(wid); ini_fs1 = ini_get("fullscreen"); shot(wid, "a2_fullscreen")
key("F11", True); key("F11", False); time.sleep(2.5); g2 = geom(wid); ini_fs2 = ini_get("fullscreen")
p.wait(); R["a2_f11"] = {"windowed": g0, "fullscreen": g1, "back": g2, "ini_after_first_F11": ini_fs1, "ini_after_second_F11": ini_fs2, "log": tail(log, r"\[window\]|renderer")}

# ---- A3: input modes with MIDI through the ALSA Through port ----------------------------
for mode, name, midi in ((1, "mpe", "mpe_ch1_chord_cc64.mid"), (2, "classic", "classic_chord_bend_cc64.mid"), (3, "wind", "wind_breath_legato_cc64.mid")):
    ini_set(input_mode=mode, layout=0, vortex_profile=0, wake_profile=0)
    log = f"{OUT}/a3_{name}.log"; p = launch([], 12, log); wid, ax, ay, w, h = canvas(); time.sleep(1.0)
    shot(wid, f"a3_{name}_0_blank"); pl = play(f"{M}/{midi}"); time.sleep(1.2); shot(wid, f"a3_{name}_1_notes")
    time.sleep(2.3); shot(wid, f"a3_{name}_2_after_cc64_or_bend"); pl.wait(); time.sleep(1.0); shot(wid, f"a3_{name}_3_end"); p.wait()
    R[f"a3_{name}"] = {"ink": {k: ink(f"{OUT}/a3_{name}_{k}.png") for k in ("0_blank", "1_notes", "2_after_cc64_or_bend", "3_end")},
                       "log": tail(log, r"input|mode|dip|override|wind|mpe|classic")}

# ---- A4: the four rolls reload from the INI (#68) and drift away from the now-line --------
for lay in (3, 4, 6, 7):
    ini_set(layout=lay, input_mode=1)
    log = f"{OUT}/a4_layout{lay}.log"; p = launch([], 9, log); wid, ax, ay, w, h = canvas(); time.sleep(1.0)
    pl = play(f"{M}/mpe_ch1_chord_cc64.mid"); time.sleep(0.8); shot(wid, f"a4_layout{lay}_t1"); time.sleep(3.0); shot(wid, f"a4_layout{lay}_t2")
    pl.wait(); p.wait()
    R[f"a4_layout{lay}"] = {"ini_layout_after_run": ini_get("layout"), "ink_t1": ink(f"{OUT}/a4_layout{lay}_t1.png"), "ink_t2": ink(f"{OUT}/a4_layout{lay}_t2.png"),
                            "log": tail(log, r"layout")}

# ---- A5: gestures — Shift+right (press: hold = feed, pull = swirl), middle drag viscous wake, right drag Rankine ----
ini_set(layout=0, input_mode=1, vortex_profile=1, wake_profile=1, wake_spread=3)
log = f"{OUT}/a5_gestures.log"; p = launch([], 22, log); wid, ax, ay, w, h = canvas(); time.sleep(1.0)
cx, cy = ax + w // 2, ay + h // 2
hold(3, cx - 300, cy, 2.5, shift=True); time.sleep(0.5); shot(wid, "a5_1_shift_right_hold_feed")
drag(3, cx + 250, cy - 60, cx + 250, cy + 110, 2.0, shift=True); time.sleep(0.5); shot(wid, "a5_2_shift_right_pull_swirl")
drag(2, cx - 500, cy + 160, cx + 500, cy + 160, 1.2); time.sleep(0.6); shot(wid, "a5_3_middle_drag_viscous_wake")
drag(3, cx - 300, cy - 40, cx - 300 + 90, cy + 90, 1.0); time.sleep(0.6); shot(wid, "a5_4_right_drag_rankine")
p.wait(); R["a5"] = {"ink": {k: ink(f"{OUT}/{k}.png") for k in ("a5_1_shift_right_hold_feed", "a5_2_shift_right_pull_swirl", "a5_3_middle_drag_viscous_wake", "a5_4_right_drag_rankine")},
                     "log": tail(log, r"press|swirl|wake|vortex|feed|gesture")}
json.dump(R, open(f"{OUT}/results.json", "w"), indent=1, default=str); print(json.dumps(R, indent=1, default=str))
