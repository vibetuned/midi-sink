import sys, time, json
exec(open(sys.argv[2]).read())
from PIL import Image; import numpy as np
def ink(png):
    a = np.asarray(Image.open(png).convert("L")); m = a < 120
    if m.sum() == 0: return (0, None)
    ys, xs = np.where(m); return (int(m.sum()), [round(float(xs.mean()) / a.shape[1], 3), round(float(ys.mean()) / a.shape[0], 3)])
def focus(wid):
    win = dpy.create_resource_object("window", int(wid, 16)); win.set_input_focus(X.RevertToParent, X.CurrentTime); dpy.sync(); time.sleep(0.3)
R = {}
# A2 again with the #73 settle: F11 twice, twice over (two round trips)
ini_set(settings_open=0, fullscreen=0, layout=0, input_mode=1)
log = f"{OUT}/a2_f11_fixed.log"; p = launch([], 20, log); wid, ax, ay, w, h = canvas(); time.sleep(1.5)
g0 = geom(wid); move(ax + w // 2, ay + h // 2); focus(wid); trips = []
for i in range(2):
    key("F11", True); key("F11", False); time.sleep(3.0); g1 = geom(wid); fs1 = ini_get("fullscreen")
    if i == 0: shot(wid, "a2_fullscreen_fixed")
    key("F11", True); key("F11", False); time.sleep(3.0); g2 = geom(wid); fs2 = ini_get("fullscreen")
    trips.append({"fullscreen": g1, "ini": fs1, "back": g2, "ini_back": fs2, "exact": g2 == g0})
shot(wid, "a2_back_windowed_fixed"); p.wait()
R["a2_f11_fixed"] = {"windowed": g0, "trips": trips, "log": tail(log, r"\[window\]|renderer")}
# A5-2 again: press INSIDE the fed drop's rim so the swirl shows on the boundary
ini_set(settings_open=0, layout=0, input_mode=1, vortex_profile=1, wake_profile=1, wake_spread=3, fullscreen=0)
log = f"{OUT}/a5_swirl.log"; p = launch([], 14, log); wid, ax, ay, w, h = canvas(); time.sleep(1.2)
cx, cy = ax + w // 2, ay + h // 2; move(cx, cy); focus(wid)
hold(3, cx - 300, cy, 2.5, shift=True); time.sleep(0.4); shot(wid, "a5_swirl_0_fed_drop")
drag(3, cx - 300 + 130, cy - 90, cx - 300 + 130, cy + 100, 2.5, shift=True); time.sleep(0.5); shot(wid, "a5_swirl_1_after_pull")
p.wait(); R["a5_swirl"] = {"before": ink(f"{OUT}/a5_swirl_0_fed_drop.png"), "after": ink(f"{OUT}/a5_swirl_1_after_pull.png")}
json.dump(R, open(f"{OUT}/results3.json", "w"), indent=1); print(json.dumps(R, indent=1))
