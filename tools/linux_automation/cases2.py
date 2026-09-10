import sys, time, json
exec(open(sys.argv[2]).read())
from PIL import Image; import numpy as np
def ink(png):
    a = np.asarray(Image.open(png).convert("L")); m = a < 120
    if m.sum() == 0: return (0, None)
    ys, xs = np.where(m); return (int(m.sum()), [round(float(xs.mean()) / a.shape[1], 3), round(float(ys.mean()) / a.shape[0], 3)])
def focus(wid):
    win = dpy.create_resource_object("window", int(wid, 16)); win.set_input_focus(X.RevertToParent, X.CurrentTime); dpy.sync(); time.sleep(0.3)
    f = dpy.get_input_focus().focus; return hex(f.id) if hasattr(f, "id") else str(f)
R = {}
# ---- A2: F11 fullscreen and back (canvas focused; settings window closed for the run) ----
ini_set(settings_open=0, fullscreen=0, layout=0, input_mode=1)
log = f"{OUT}/a2_f11.log"; p = launch([], 16, log); wid, ax, ay, w, h = canvas(); time.sleep(1.5)
g0 = geom(wid); move(ax + w // 2, ay + h // 2); f = focus(wid)
key("F11", True); key("F11", False); time.sleep(3.0); g1 = geom(wid); fs1 = ini_get("fullscreen"); shot(wid, "a2_fullscreen")
key("F11", True); key("F11", False); time.sleep(3.0); g2 = geom(wid); fs2 = ini_get("fullscreen"); shot(wid, "a2_back_windowed")
p.wait(); R["a2_f11"] = {"focus": f, "canvas": wid, "windowed": g0, "fullscreen": g1, "back": g2, "ini_after_F11": fs1, "ini_after_second_F11": fs2, "log": tail(log, r"\[window\]|renderer")}
# ---- A5: gestures ----
ini_set(settings_open=0, layout=0, input_mode=1, vortex_profile=1, wake_profile=1, wake_spread=3, fullscreen=0)
log = f"{OUT}/a5_gestures.log"; p = launch([], 26, log); wid, ax, ay, w, h = canvas(); time.sleep(1.2)
cx, cy = ax + w // 2, ay + h // 2; move(cx, cy); f = focus(wid)
hold(3, cx - 300, cy, 3.0, shift=True); time.sleep(0.5); shot(wid, "a5_1_shift_right_hold_feed")
drag(3, cx + 250, cy - 70, cx + 250, cy + 110, 2.0, shift=True); time.sleep(0.5); shot(wid, "a5_2_shift_right_pull_swirl")
drag(2, cx - 560, cy + 40, cx + 560, cy + 40, 1.4); time.sleep(0.6); shot(wid, "a5_3_middle_drag_viscous_wake")
drag(3, cx - 300, cy - 30, cx - 300 + 100, cy + 100, 1.2); time.sleep(0.6); shot(wid, "a5_4_right_drag_rankine")
p.wait(); R["a5"] = {"focus": f, "ink": {k: ink(f"{OUT}/{k}.png") for k in ("a5_1_shift_right_hold_feed", "a5_2_shift_right_pull_swirl", "a5_3_middle_drag_viscous_wake", "a5_4_right_drag_rankine")}}
json.dump(R, open(f"{OUT}/results2.json", "w"), indent=1); print(json.dumps(R, indent=1))
