import json, sys
# usage: uinput_gen.py <order A|B> <fx> <fy> <px> <py> <out.json>
# screen (landscape 2960x1848) -> the virtual devices' raw space (ABS_X 0..2959 along the portrait long side)
order, fx, fy, px, py, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
def raw(sx, sy): return (round((1848 - sy) / 1848 * 2959), round(sx / 2960 * 1847))
fxr, fyr = raw(fx, fy); pxr, pyr = raw(px, py)
absinfo = lambda code, mx: {"code": code, "info": {"value": 0, "minimum": 0, "maximum": mx, "fuzz": 0, "flat": 0, "resolution": 0}}
touch = {"id": 1, "command": "register", "name": "Sumi Virtual Touch", "vid": 6353, "pid": 43981, "bus": "usb",
         "configuration": [{"type": 100, "data": [1, 3]}, {"type": 101, "data": [330, 325]}, {"type": 103, "data": [47, 48, 53, 54, 57]}, {"type": 110, "data": [1]}],
         "abs_info": [absinfo(47, 9), absinfo(48, 255), absinfo(53, 2959), absinfo(54, 1847), absinfo(57, 65535)]}
pen = {"id": 2, "command": "register", "name": "Sumi Virtual Pen", "vid": 6353, "pid": 43982, "bus": "usb",
       "configuration": [{"type": 100, "data": [1, 3]}, {"type": 101, "data": [320, 330, 331]}, {"type": 103, "data": [0, 1, 24]}, {"type": 110, "data": [1]}],
       "abs_info": [absinfo(0, 2959), absinfo(1, 1847), absinfo(24, 4095)]}
f_down = [3,47,0, 3,57,100, 3,53,fxr, 3,54,fyr, 3,48,30, 1,330,1, 0,0,0]
f_up   = [3,47,0, 3,57,-1, 1,330,0, 0,0,0]
p_in   = [1,320,1, 3,0,pxr, 3,1,pyr, 3,24,0, 0,0,0]
p_down = [1,330,1, 3,24,1500, 0,0,0]
p_up   = [1,330,0, 3,24,0, 0,0,0, 1,320,0, 0,0,0]
D = lambda i, ms: {"id": i, "command": "delay", "duration": ms}
I = lambda i, ev: {"id": i, "command": "inject", "events": ev}
if order == "A":   # finger holds 1.5-2.9 s, pen in at 1.95, down 2.0-2.5 s, out at 3.2
    seq = [touch, pen, D(1,1500), I(1,f_down), D(1,1400), I(1,f_up), D(1,300),
           D(2,1950), I(2,p_in), D(2,50), I(2,p_down), D(2,500), I(2,p_up), D(2,700)]
elif order == "B": # pen in at 1.45, down 1.5-2.9 s; finger presses 2.0-2.5 s
    seq = [touch, pen, D(2,1450), I(2,p_in), D(2,50), I(2,p_down), D(2,1400), I(2,p_up), D(2,300),
           D(1,2000), I(1,f_down), D(1,500), I(1,f_up), D(1,700)]
elif order == "C": # finger holds 1.5-4.0 s; pen taps twice during it (2.0-2.4, 2.8-3.2), out at 3.5; a second finger press 4.5-4.9 on the same pad
    seq = [touch, pen, D(1,1500), I(1,f_down), D(1,2500), I(1,f_up), D(1,500), I(1,f_down), D(1,400), I(1,f_up), D(1,300),
           D(2,1950), I(2,p_in), D(2,50), I(2,p_down), D(2,400), I(2,p_up[:12]), D(2,400), I(2,p_down), D(2,400), I(2,p_up), D(2,1500)]
with open(out, "w") as f:
    for c in seq: f.write(json.dumps(c) + "\n")
print(out, "finger raw", (fxr, fyr), "pen raw", (pxr, pyr))
