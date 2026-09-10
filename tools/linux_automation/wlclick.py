import sys, os, time, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mutterrd import Mutter
from PIL import Image; import numpy as np
S = os.path.dirname(os.path.abspath(__file__)); OUT = S + "/wl"
APP = os.environ.get("MIDI_SINK_ROOT", os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))) + "/build/desktop/midi-sink"
m = Mutter("HDMI-1")
log = open(f"{OUT}/click.log", "w")
app = subprocess.Popen([APP, "--dev", "--exit-after", "14"], stdout=log, stderr=subprocess.STDOUT); time.sleep(3.0)
ok, _ = m.shot(f"{OUT}/c0.png"); a = np.asarray(Image.open(f"{OUT}/c0.png").convert("RGB")).astype(int)
r, g, b = a[..., 0], a[..., 1], a[..., 2]; mm = (r > 205) & (g > 195) & (b > 175) & ((r - b) > 8) & ((r - b) < 40); ys, xs = np.where(mm)
bb = [int(np.percentile(xs, 0.5)), int(np.percentile(ys, 0.5)), int(np.percentile(xs, 99.5)), int(np.percentile(ys, 99.5))]
cx, cy = (bb[0] + bb[2]) // 2, (bb[1] + bb[3]) // 2; print("canvas", bb)
m.click(cx, cy); time.sleep(0.5)                       # plain click, no motion between press and release
m.move(cx + 200, cy + 100); time.sleep(0.3); m.button("left", True); time.sleep(0.15); m.button("left", False); time.sleep(0.5)
m.move(cx - 200, cy - 100); time.sleep(0.3); m.button("left", True); time.sleep(0.3); m.move(cx - 199, cy - 100); time.sleep(0.1); m.button("left", False); time.sleep(0.8)
m.shot(f"{OUT}/c1.png")
app.wait(); m.stop()
print("\n".join(l.rstrip() for l in open(f"{OUT}/click.log") if "[mouse]" in l or "renderer" in l))
Image.open(f"{OUT}/c1.png").convert("RGB").crop((bb[0], bb[1], bb[2], bb[3])).resize((640, 360)).save(f"{OUT}/c1_small.jpg", quality=80)
