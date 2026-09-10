import sys, os, time, json, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mutterrd import Mutter
from PIL import Image; import numpy as np
S = os.path.dirname(os.path.abspath(__file__)); OUT = S + "/wl"; os.makedirs(OUT, exist_ok=True)
APP = os.environ.get("MIDI_SINK_ROOT", os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))) + "/build/desktop/midi-sink"
INI = os.path.expanduser("~/.config/midi-sink/settings.ini")
m = Mutter("HDMI-1")
def frame(name):
    ok, err = m.shot(f"{OUT}/{name}.png")
    if not ok: raise SystemExit("capture failed: " + err)
    return np.asarray(Image.open(f"{OUT}/{name}.png").convert("RGB")).astype(int)
def canvas_bbox(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mm = (r > 205) & (g > 195) & (b > 175) & ((r - b) > 8) & ((r - b) < 40)
    ys, xs = np.where(mm)
    if len(xs) < 5000: return None
    return [int(np.percentile(xs, 0.5)), int(np.percentile(ys, 0.5)), int(np.percentile(xs, 99.5)), int(np.percentile(ys, 99.5))]
def ink(a, bb):
    x0, y0, x1, y1 = bb; sub = a[y0:y1, x0:x1]; return int(((sub.sum(axis=2) / 3) < 120).sum())
def ini_get(k):
    for l in open(INI).read().splitlines():
        if l.startswith(k + "="): return l.split("=", 1)[1]
R = {}
log = open(f"{OUT}/app.log", "w")
app = subprocess.Popen([APP, "--dev", "--exit-after", "40"], stdout=log, stderr=subprocess.STDOUT)
time.sleep(3.0)
a = frame("0_launch"); bb = canvas_bbox(a); R["canvas_bbox_windowed"] = bb; print("canvas", bb)
if bb is None: app.terminate(); raise SystemExit("canvas not on HDMI-1")
cx, cy = (bb[0] + bb[2]) // 2, (bb[1] + bb[3]) // 2
R["ink_before"] = ink(a, bb)
m.click(cx, cy); time.sleep(0.8); a = frame("1_click"); R["ink_after_click"] = ink(a, bb)
m.click(cx - 220, cy + 120); time.sleep(0.8); a = frame("2_second_click"); R["ink_after_second_click"] = ink(a, bb)
m.drag("left", cx - 350, cy - 40, cx + 350, cy - 40, 1.0); time.sleep(0.8); a = frame("3_left_drag_tine"); R["ink_after_tine"] = ink(a, bb)
m.drag("right", cx + 100, cy + 60, cx + 220, cy + 180, 1.0); time.sleep(0.8); a = frame("4_right_drag_vortex"); R["ink_after_vortex"] = ink(a, bb)
m.hold("right", cx + 300, cy - 200, 2.0, shift=True); time.sleep(0.6); a = frame("5_shift_right_hold"); R["ink_after_shift_right_hold"] = ink(a, bb)
m.tap("F11"); time.sleep(3.0); a = frame("6_fullscreen"); R["canvas_bbox_fullscreen"] = canvas_bbox(a); R["ini_fullscreen_after_F11"] = ini_get("fullscreen")
m.tap("F11"); time.sleep(3.0); a = frame("7_back"); R["canvas_bbox_back"] = canvas_bbox(a); R["ini_fullscreen_after_second_F11"] = ini_get("fullscreen")
R["log"] = [l.rstrip() for l in open(f"{OUT}/app.log") if "glfw" not in l and ("renderer" in l or "window" in l)]
app.terminate(); app.wait(timeout=10); m.stop()
json.dump(R, open(f"{OUT}/results.json", "w"), indent=1); print(json.dumps(R, indent=1))
for n in ("0_launch", "2_second_click", "5_shift_right_hold", "6_fullscreen", "7_back"):
    Image.open(f"{OUT}/{n}.png").convert("RGB").resize((960, 540)).save(f"{OUT}/{n}_small.jpg", quality=75)
