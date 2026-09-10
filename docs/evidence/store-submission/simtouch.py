#!/usr/bin/env python3
"""Inject mouse events into the iOS Simulator window as single-finger touches.
Window bounds are re-read on every call, so moving/rotating the window is safe."""
import Quartz, time, subprocess, random

TITLE = 25   # simulator title-bar height, px

DEVICE = "iPad Pro 13-inch (M5)"      # set by use()

def use(name, cal_x=None, cal_y=None):
    """Point every helper at a different booted simulator."""
    global DEVICE, CAL_X, CAL_Y
    DEVICE = name
    if cal_x: CAL_X = cal_x
    if cal_y: CAL_Y = cal_y

def win():
    wl = Quartz.CGWindowListCopyWindowInfo(
        Quartz.kCGWindowListOptionOnScreenOnly | Quartz.kCGWindowListExcludeDesktopElements,
        Quartz.kCGNullWindowID)
    for w in wl:
        if ('Simulator' in (w.get('kCGWindowOwnerName') or '')
                and DEVICE in (w.get('kCGWindowName') or '')):
            b = w['kCGWindowBounds']
            return b['X'], b['Y'], b['Width'], b['Height']
    raise SystemExit(f'Simulator window for {DEVICE!r} not found')

# Empirical affine from device-normalised coords to window fraction. The
# Simulator window is ~11% larger than the device screen (rounded-corner bezel
# plus title bar), so assuming the screen fills the window puts edge taps off
# the display entirely. Measured with calib.py; re-run it if the window is
# resized or the device changed.
CAL_X = (1.1096, -0.0548)   # observed_device = a*window_fraction + b
CAL_Y = (1.1104, -0.0704)

def pt(dx, dy):
    """device-normalised (0..1, 0..1) on the device screen -> global screen px"""
    X, Y, W, H = win()
    fx = (dx - CAL_X[1]) / CAL_X[0]
    fy = (dy - CAL_Y[1]) / CAL_Y[0]
    return (X + fx * W, Y + TITLE + fy * (H - TITLE))

def post(kind, x, y):
    Quartz.CGEventPost(Quartz.kCGHIDEventTap,
        Quartz.CGEventCreateMouseEvent(None, kind, (x, y), Quartz.kCGMouseButtonLeft))

def activate():
    subprocess.run(['osascript', '-e', 'tell application "Simulator" to activate'],
                   capture_output=True)
    time.sleep(0.6)

def tap(dx, dy, hold=0.05):
    x, y = pt(dx, dy)
    post(Quartz.kCGEventMouseMoved, x, y); time.sleep(0.02)
    post(Quartz.kCGEventLeftMouseDown, x, y); time.sleep(hold)
    post(Quartz.kCGEventLeftMouseUp, x, y); time.sleep(0.06)

def stroke(a, b, dur=0.5):
    x0, y0 = pt(*a); x1, y1 = pt(*b)
    post(Quartz.kCGEventMouseMoved, x0, y0); time.sleep(0.03)
    post(Quartz.kCGEventLeftMouseDown, x0, y0); time.sleep(0.05)
    n = max(4, int(dur / 0.012))
    for s in range(n):
        t = (s + 1) / n
        post(Quartz.kCGEventLeftMouseDragged, x0 + (x1-x0)*t, y0 + (y1-y0)*t)
        time.sleep(0.012)
    post(Quartz.kCGEventLeftMouseUp, x1, y1); time.sleep(0.12)

def hold_down(dx, dy):
    x, y = pt(dx, dy)
    post(Quartz.kCGEventMouseMoved, x, y); time.sleep(0.02)
    post(Quartz.kCGEventLeftMouseDown, x, y); time.sleep(0.05)

def release(dx, dy):
    x, y = pt(dx, dy)
    post(Quartz.kCGEventLeftMouseUp, x, y); time.sleep(0.06)

def shot(path, dev=None):
    subprocess.run(['xcrun', 'simctl', 'io', dev or DEVICE, 'screenshot', path], capture_output=True)
    return path

def relaunch(dev=None, bid="com.vibetuned.midi-sink"):
    dev = dev or DEVICE
    subprocess.run(['xcrun', 'simctl', 'terminate', dev, bid], capture_output=True)
    time.sleep(1)
    subprocess.run(['xcrun', 'simctl', 'launch', dev, bid], capture_output=True)
    time.sleep(6); activate()

def compose(seed=3):
    """Portrait marbling: drop stacks, then short alternating combs."""
    random.seed(seed)
    for cx, cy, n in [(.30,.20,5),(.62,.16,6),(.24,.36,7),(.70,.34,5),
                      (.46,.50,8),(.20,.60,5),(.72,.58,6),(.40,.74,6),(.66,.82,5)]:
        for _ in range(n):
            tap(cx + random.uniform(-.02,.02), cy + random.uniform(-.012,.012))
    time.sleep(0.5)
    for i, y in enumerate((.14,.28,.42,.56,.70,.84)):
        if i % 2 == 0: stroke((.16, y), (.60, y), 0.45)
        else:          stroke((.84, y), (.40, y), 0.45)
    time.sleep(0.3)
    for i, x in enumerate((.32,.52,.72)):
        if i % 2 == 0: stroke((x, .22), (x, .58), 0.42)
        else:          stroke((x, .78), (x, .42), 0.42)
    time.sleep(0.8)

def compose_light(seed=5):
    """Fewer, smaller drop stacks and only three combs — leaves paper showing."""
    random.seed(seed)
    for cx, cy, n in [(.34,.26,4),(.66,.34,3),(.28,.55,4),(.62,.66,3),(.46,.42,5)]:
        for _ in range(n):
            tap(cx + random.uniform(-.015,.015), cy + random.uniform(-.010,.010))
    time.sleep(0.4)
    for i, y in enumerate((.30,.50,.70)):
        if i % 2 == 0: stroke((.22, y), (.58, y), 0.40)
        else:          stroke((.78, y), (.42, y), 0.40)
    time.sleep(0.3)
    stroke((.50, .30), (.50, .62), 0.40)
    time.sleep(0.8)

def compose_phone(seed=4):
    """Tall, narrow sheet: stacks down the column, combs kept well inside."""
    random.seed(seed)
    for cx, cy, n in [(.40,.22,4),(.64,.32,3),(.32,.44,4),(.60,.56,4),(.42,.68,3),(.58,.80,3)]:
        for _ in range(n):
            tap(cx + random.uniform(-.020,.020), cy + random.uniform(-.008,.008))
    time.sleep(0.4)
    for i, y in enumerate((.28,.42,.56,.70)):
        if i % 2 == 0: stroke((.24, y), (.68, y), 0.40)
        else:          stroke((.74, y), (.30, y), 0.40)
    time.sleep(0.3)
    stroke((.50, .30), (.50, .66), 0.45)
    time.sleep(0.8)

def compose_phone2(seed=9):
    """Keeps the top eighth clear of the Dynamic Island."""
    random.seed(seed)
    for cx, cy, n in [(.38,.34,4),(.64,.44,3),(.32,.56,4),(.62,.66,4),(.44,.78,3)]:
        for _ in range(n):
            tap(cx + random.uniform(-.020,.020), cy + random.uniform(-.008,.008))
    time.sleep(0.4)
    for i, y in enumerate((.38,.52,.66,.80)):
        if i % 2 == 0: stroke((.24, y), (.66, y), 0.40)
        else:          stroke((.74, y), (.32, y), 0.40)
    time.sleep(0.3)
    stroke((.50, .44), (.50, .74), 0.45)
    time.sleep(0.8)
