# X11 (Xwayland) driver for the step-45a UI walk: launch on X11, XTest input,
# `import` grabs, tesseract to find ImGui labels in the settings window.
import os, re, subprocess, time
from Xlib import X, XK, display
from Xlib.ext import xtest
APP = "/home/flux/projects/midi-sink/build/desktop/midi-sink"
OUT = os.path.dirname(os.path.abspath(__file__)); dpy = display.Display(":0")
def launch(args, log, exit_after=120):
    env = dict(os.environ); env.pop("WAYLAND_DISPLAY", None); env["XDG_SESSION_TYPE"] = "x11"; env["DISPLAY"] = ":0"
    return subprocess.Popen([APP, "--dev", "--exit-after", str(exit_after)] + args, env=env, stdout=open(log, "w"), stderr=subprocess.STDOUT)
def win(title):
    for _ in range(60):
        t = subprocess.run(["xwininfo", "-root", "-tree"], capture_output=True, text=True).stdout
        m = re.search(r'(0x[0-9a-f]+) "' + re.escape(title) + r'": \("midi-sink" "midi-sink"\)\s+(\d+)x(\d+)\+(-?\d+)\+(-?\d+)\s+\+(-?\d+)\+(-?\d+)', t)
        if m: return dict(id=m.group(1), w=int(m.group(2)), h=int(m.group(3)), x=int(m.group(6)), y=int(m.group(7)))
        time.sleep(0.25)
    raise SystemExit("no window " + title)
def canvas(): return win("midi-sink")
def settings(): return win("midi-sink — Settings")
def shot(w, name):
    p = f"{OUT}/{name}.png"; subprocess.run(["import", "-display", ":0", "-window", w["id"], p], check=False); return p
def focus(w):
    o = dpy.create_resource_object("window", int(w["id"], 16)); o.set_input_focus(X.RevertToParent, X.CurrentTime); dpy.sync(); time.sleep(0.25)
def move(x, y): xtest.fake_input(dpy, X.MotionNotify, x=int(x), y=int(y)); dpy.sync()
def btn(b, down): xtest.fake_input(dpy, X.ButtonPress if down else X.ButtonRelease, b); dpy.sync()
def key(name, down):
    kc = dpy.keysym_to_keycode(XK.string_to_keysym(name)); xtest.fake_input(dpy, X.KeyPress if down else X.KeyRelease, kc); dpy.sync()
def tap(name): key(name, True); time.sleep(0.04); key(name, False); time.sleep(0.04)
def click(x, y, b=1, hold=0.08):
    move(x, y); time.sleep(0.12); btn(b, True); time.sleep(hold); btn(b, False); time.sleep(0.25)
def drag(b, x0, y0, x1, y1, secs=1.0, shift=False, steps=40):
    move(x0, y0); time.sleep(0.1)
    if shift: key("Shift_L", True); time.sleep(0.05)
    btn(b, True); time.sleep(0.05)
    for i in range(1, steps + 1): move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps); time.sleep(secs / steps)
    btn(b, False)
    if shift: key("Shift_L", False)
    time.sleep(0.2)
def wheel(x, y, n):   # n > 0 scrolls down
    move(x, y); time.sleep(0.1)
    for _ in range(abs(n)): b = 5 if n > 0 else 4; btn(b, True); btn(b, False); time.sleep(0.03)
    time.sleep(0.3)
CHARS = {"-": "minus", "/": "slash", ".": "period", "_": "underscore", " ": "space", ":": "colon"}
def typetext(s):
    for ch in s:
        if ch.isupper(): key("Shift_L", True); tap(ch.lower()); key("Shift_L", False)
        elif ch == "_": key("Shift_L", True); tap("minus"); key("Shift_L", False)
        elif ch == ":": key("Shift_L", True); tap("semicolon"); key("Shift_L", False)
        else: tap(CHARS.get(ch, ch))
def ocr(png):
    """[(text, cx, cy, x0, y0, w, h)] in image coordinates, words merged into lines."""
    from PIL import Image
    im = Image.open(png).convert("L"); big = im.resize((im.width * 2, im.height * 2))
    big.save(png + ".ocr.png")
    tsv = subprocess.run(["tesseract", png + ".ocr.png", "-", "--psm", "11", "tsv"], capture_output=True, text=True).stdout
    out = []
    for line in tsv.splitlines()[1:]:
        f = line.split("\t")
        if len(f) == 12 and f[11].strip() and float(f[10]) > 30:
            x, y, w, h = (int(f[6]) // 2, int(f[7]) // 2, int(f[8]) // 2, int(f[9]) // 2)
            out.append((f[11].strip(), x + w // 2, y + h // 2, x, y, w, h))
    return out
def find(words, text, nth=0):
    hits = [w for w in words if w[0].lower().startswith(text.lower())]
    return hits[nth] if len(hits) > nth else None
def phrase(words, text, nth=0):
    toks = text.lower().split(); hits = []
    for i, w in enumerate(words):
        if not w[0].lower().startswith(toks[0]): continue
        j, ok, last = i, True, w
        for t in toks[1:]:
            j += 1
            if j >= len(words) or abs(words[j][2] - w[2]) > 8 or not words[j][0].lower().startswith(t): ok = False; break
            last = words[j]
        if ok: hits.append(((w[3] + last[3] + last[5]) // 2, w[2]))
    return hits[nth] if len(hits) > nth else None
def seek(s, text, name, max_wheel=40, step=3, nth=0, top_first=True):
    """Scroll the settings window until `text` (a phrase) is visible; return screen coords."""
    if top_first: wheel(s["x"] + s["w"] // 2, s["y"] + 200, -60)
    for k in range(max_wheel // step + 1):
        w = ocr(shot(s, name)); h = phrase(w, text, nth)
        if h and 20 < h[1] < s["h"] - 20: return (s["x"] + h[0], s["y"] + h[1]), w
        wheel(s["x"] + s["w"] // 2, s["y"] + s["h"] // 2, step)
    return None, None
def paste(s):
    """Type into the focused ImGui field through the clipboard (the X core keymap is US, the XKB layout French)."""
    subprocess.run(["xclip", "-selection", "clipboard"], input=s, text=True, env=dict(os.environ, DISPLAY=":0"))
    time.sleep(0.2); key("Control_L", True); tap("v"); key("Control_L", False); time.sleep(0.3)
