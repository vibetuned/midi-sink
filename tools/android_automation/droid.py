# adb UI driver for the Step-33 Android checks (uiautomator dumps + input taps).
import subprocess, re, time, sys, os, json
PKG = "com.vibetuned.midisink"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "android"); os.makedirs(OUT, exist_ok=True)
def sh(*a, **k): return subprocess.run(["adb", "shell"] + list(a), capture_output=True, text=True, **k).stdout
def dump():
    sh("uiautomator", "dump", "/sdcard/ui.xml"); x = sh("cat", "/sdcard/ui.xml"); out = []
    for m in re.finditer(r'text="([^"]*)"[^>]*clickable="(true|false)"[^>]*bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"', x):
        t, c, x0, y0, x1, y1 = m.groups(); out.append((t.replace("&#10;", "\n"), c == "true", (int(x0) + int(x1)) // 2, (int(y0) + int(y1)) // 2, int(y0), int(y1)))
    return out
def texts(): return [n[0] for n in dump() if n[0].strip()]
def tap(x, y, wait=0.7): sh("input", "tap", str(x), str(y)); time.sleep(wait)
def swipe(x0, y0, x1, y1, ms=400, wait=0.7): sh("input", "swipe", str(x0), str(y0), str(x1), str(y1), str(ms)); time.sleep(wait)
def shot(name): subprocess.run(["adb", "exec-out", "screencap", "-p"], stdout=open(f"{OUT}/{name}.png", "wb")); return f"{OUT}/{name}.png"
def find(sub, nodes=None):
    for n in (nodes or dump()):
        if sub in n[0]: return n
    return None
def screen():
    ps = re.search(r"Physical size: (\d+)x(\d+)", sh("wm", "size")); w, h = int(ps.group(1)), int(ps.group(2))
    rot = re.search(r"rotation=(\d)", sh("dumpsys", "window", "displays")); r = int(rot.group(1)) if rot else 0
    return (h, w) if r in (1, 3) else (w, h)
def scroll_sheet(up=True, px=None):
    W, H = screen(); px = px or int(H * 0.5); x = W // 2; y0 = int(H * 0.72) if up else int(H * 0.25)
    swipe(x, y0, x, y0 - px if up else y0 + px, 300)
def scroll_to(sub, max_scrolls=16):
    W, H = screen()
    def ok(n): return n and 120 < n[3] < H - 160
    for _ in range(max_scrolls):
        n = find(sub)
        if ok(n): return n
        scroll_sheet(True, int(H * 0.3))
    for _ in range(16): scroll_sheet(False, int(H * 0.45))
    for _ in range(max_scrolls):
        n = find(sub)
        if ok(n): return n
        scroll_sheet(True, int(H * 0.3))
    return find(sub)
def sheet_open(): return len([n for n in dump() if n[0].strip()]) > 4     # the main screen has only the gear
def open_settings():
    if not sheet_open():
        g = find("⚙"); W, H = screen(); tap(g[2], g[3]) if g else tap(W - 68, 134); time.sleep(0.8)
    for _ in range(14):
        if find("LAYOUT"): break
        scroll_sheet(False)
def close_settings(): sh("input", "keyevent", "KEYCODE_BACK"); time.sleep(0.6)
def sheet_texts():
    seen = []; 
    for _ in range(12):
        scroll_sheet(False)
    for _ in range(18):
        for t in texts():
            if t not in seen: seen.append(t)
        before = len(seen); scroll_sheet(True)
        if len(seen) == before and find("ABOUT"): break
    return seen
def relaunch():
    sh("am", "force-stop", PKG); time.sleep(0.8); sh("am", "start", "-n", f"{PKG}/.MainActivity"); time.sleep(5)
def logcat(pat): return [l for l in subprocess.run(["adb", "logcat", "-d"], capture_output=True, text=True).stdout.splitlines() if re.search(pat, l)]
