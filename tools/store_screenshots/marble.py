#!/usr/bin/env python3
"""Compose a marbling sheet on the tablet. Short, alternating combs keep the
ink spread: a full-width swipe translates the whole (area-preserving) sheet."""
import subprocess, time, random, sys
W, H = 2960, 1848
def sh(*a): subprocess.run(["adb","shell"]+[str(x) for x in a], capture_output=True)
def tap(x,y): sh("input","tap",int(x),int(y))
def swipe(x0,y0,x1,y1,ms=400): sh("input","swipe",int(x0),int(y0),int(x1),int(y1),ms)
def shot(p): subprocess.run(["adb","exec-out","screencap","-p"],stdout=open(p,"wb"))
def fresh():
    sh("am","force-stop","com.vibetuned.midisink"); time.sleep(1)
    sh("am","start","-n","com.vibetuned.midisink/.MainActivity"); time.sleep(5)

def compose(seed=3):
    random.seed(seed)
    # 1. A field of drop stacks across the tray (stacked taps = concentric rings).
    for cx, cy, n in [(.22,.30,5),(.45,.24,7),(.70,.32,5),(.85,.55,6),
                      (.62,.58,8),(.36,.62,6),(.15,.68,5),(.50,.80,4),(.78,.78,5)]:
        for _ in range(n):
            tap(cx*W + random.uniform(-25,25), cy*H + random.uniform(-20,20))
    time.sleep(0.5)
    # 2. Short combs, alternating direction so net displacement cancels.
    for i, y in enumerate((.20,.36,.52,.68,.84)):
        if i % 2 == 0: swipe(.18*W, y*H, .58*W, y*H, 450)
        else:          swipe(.82*W, y*H, .42*W, y*H, 450)
    time.sleep(0.4)
    # 3. Cross combs, also alternating.
    for i, x in enumerate((.30,.50,.70)):
        if i % 2 == 0: swipe(x*W, .25*H, x*W, .62*H, 420)
        else:          swipe(x*W, .78*H, x*W, .40*H, 420)
    time.sleep(0.8)

if __name__ == "__main__":
    a = sys.argv
    if "--fresh" in a: fresh()
    compose(int(a[a.index("--seed")+1]) if "--seed" in a else 3)
    out = a[a.index("--out")+1] if "--out" in a else "marble.png"
    shot(out); print("wrote", out)
