# X11 (Xwayland) automation for the Step-33 desktop checks: launch the canvas
# on X11, inject pointer/keyboard through XTest, play MIDI files into the ALSA
# Through port (the app subscribes to it), and grab the canvas with `import`.
import os, re, subprocess, sys, time, json
from Xlib import X, XK, display
from Xlib.ext import xtest
ROOT = "/home/flux/projects/midi-sink"; APP = ROOT + "/build/desktop/midi-sink"
INI = os.path.expanduser("~/.config/midi-sink/settings.ini")
OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
dpy = display.Display(":0")

def ini_set(**kv):
    lines = open(INI).read().splitlines(); seen = set()
    for i, l in enumerate(lines):
        k = l.split("=", 1)[0]
        if k in kv: lines[i] = f"{k}={kv[k]}"; seen.add(k)
    for k, v in kv.items():
        if k not in seen: lines.append(f"{k}={v}")
    open(INI, "w").write("\n".join(lines) + "\n")
def ini_get(k):
    for l in open(INI).read().splitlines():
        if l.startswith(k + "="): return l.split("=", 1)[1]
def launch(flags, exit_after, log):
    env = dict(os.environ); env.pop("WAYLAND_DISPLAY", None); env["XDG_SESSION_TYPE"] = "x11"; env["DISPLAY"] = ":0"
    return subprocess.Popen([APP, "--dev", "--exit-after", str(exit_after)] + flags, env=env,
                            stdout=open(log, "w"), stderr=subprocess.STDOUT)
def canvas():
    for _ in range(40):
        t = subprocess.run(["xwininfo", "-root", "-tree"], capture_output=True, text=True).stdout
        m = re.search(r'(0x[0-9a-f]+) "midi-sink": \("midi-sink" "midi-sink"\)\s+(\d+)x(\d+)\+(-?\d+)\+(-?\d+)\s+\+(-?\d+)\+(-?\d+)', t)
        if m:
            wid = m.group(1); w, h = int(m.group(2)), int(m.group(3)); ax, ay = int(m.group(6)), int(m.group(7))
            return wid, ax, ay, w, h
        time.sleep(0.25)
    raise SystemExit("no canvas window")
def shot(wid, name):
    subprocess.run(["import", "-display", ":0", "-window", wid, f"{OUT}/{name}.png"], check=False)
def play(midi):
    return subprocess.Popen(["aplaymidi", "-p", "14:0", midi], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def move(x, y):
    xtest.fake_input(dpy, X.MotionNotify, x=int(x), y=int(y)); dpy.sync()
def button(b, down):
    xtest.fake_input(dpy, X.ButtonPress if down else X.ButtonRelease, b); dpy.sync()
def key(name, down):
    kc = dpy.keysym_to_keycode(XK.string_to_keysym(name))
    xtest.fake_input(dpy, X.KeyPress if down else X.KeyRelease, kc); dpy.sync()
def drag(b, x0, y0, x1, y1, secs, shift=False, steps=40):
    move(x0, y0); time.sleep(0.1)
    if shift: key("Shift_L", True); time.sleep(0.05)
    button(b, True); time.sleep(0.05)
    for i in range(1, steps + 1):
        move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps); time.sleep(secs / steps)
    button(b, False)
    if shift: key("Shift_L", False)
    dpy.sync()
def hold(b, x, y, secs, shift=False):
    move(x, y); time.sleep(0.1)
    if shift: key("Shift_L", True); time.sleep(0.05)
    button(b, True); time.sleep(secs); button(b, False)
    if shift: key("Shift_L", False)
    dpy.sync()
def geom(wid):
    t = subprocess.run(["xwininfo", "-id", wid], capture_output=True, text=True).stdout
    g = {k: int(re.search(rf"{k}:\s+(-?\d+)", t).group(1)) for k in ("Absolute upper-left X", "Absolute upper-left Y", "Width", "Height")}
    return g["Absolute upper-left X"], g["Absolute upper-left Y"], g["Width"], g["Height"]
def tail(log, pats):
    return [l for l in open(log).read().splitlines() if re.search(pats, l) and "glfw" not in l]
