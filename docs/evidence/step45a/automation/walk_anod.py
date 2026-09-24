import sys, time, os, json, glob; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from x11ui import *
R = {}; EXP = OUT + "/exported-desktop-anod.json"
if os.path.exists(EXP): os.remove(EXP)
pics_before = set(glob.glob(os.path.expanduser("~/Pictures/midi-sink-print-*.png")))
p = launch([], OUT + "/walk_anod.log", 170); c = canvas(); s = settings(); time.sleep(2)
def at(h): return (s["x"] + h[0], s["y"] + h[1])
def top():
    wheel(s["x"] + 280, s["y"] + 300, -60); return ocr(shot(s, "cur"))
# -- 7. presets: Load, name, export path, Export
w = top(); h = phrase(w, "Load"); R["load"] = h
click(*at(h)); time.sleep(1.5); shot(c, "a2_after_load_canvas")
w = top(); sa = phrase(w, "Save as"); R["save_as"] = sa           # the name box is the field left of "Save as"
click(s["x"] + sa[0] - 130, s["y"] + sa[1]); paste("desktop-anod")
w = top(); im = phrase(w, "Import"); R["import"] = im              # Export sits left of Import on the path row
hs = [x for x in [phrase(w, "Export", k) for k in range(4)] if x]; ex = min(hs, key=lambda e: abs(e[1] - im[1])); R["export_btn"] = ex
click(s["x"] + ex[0] - 170, s["y"] + ex[1]); paste(EXP); shot(s, "a3_filled")
click(*at(ex)); time.sleep(1.0); shot(s, "a4_after_export"); R["exported"] = os.path.exists(EXP)
# -- 8. gestures in Anod
focus(c); cx, cy = c["x"] + c["w"] // 2, c["y"] + c["h"] // 2
click(cx - 400, cy - 150); time.sleep(0.8); shot(c, "g_anod_1_click_strike")
drag(1, cx - 100, cy - 200, cx + 60, cy - 120, 0.8, shift=True); time.sleep(0.8); shot(c, "g_anod_2_shift_drag_burst")
drag(3, cx + 250, cy - 150, cx + 400, cy - 20, 0.8); time.sleep(0.8); shot(c, "g_anod_3_right_drag_torsion")
move(cx - 250, cy + 150); time.sleep(0.1); key("Shift_L", True); btn(3, True); time.sleep(1.2)
for i in range(20): move(cx - 250, cy + 150 - 4 * i); time.sleep(0.05)
time.sleep(0.8)
for i in range(40): move(cx - 250, cy + 70 + 5 * i); time.sleep(0.05)
time.sleep(1.0); btn(3, False); key("Shift_L", False); time.sleep(0.8); shot(c, "g_anod_4_shift_right_press")
# -- 6. prints through the Canvas button, then the ledger
w = top(); dip = phrase(w, "Paper dip"); R["dip_btn"] = dip
click(*at(dip)); time.sleep(2.0)
for k in range(4): click(cx - 300 + 180 * k, cy + 40 * (k % 2)); time.sleep(0.4)
time.sleep(1.2); w = top(); click(*at(phrase(w, "Paper dip"))); time.sleep(2.5)
w = top(); shot(s, "p1_prints_top")
pt, w = seek(s, "Export PNG", "p2_seek", top_first=True); R["export_png"] = pt
if pt:
    shot(s, "p3_ledger"); click(*pt); time.sleep(5.0)                    # 4K wide
    w = top(); al = phrase(w, "over alpha"); R["alpha_label"] = al
    if al: click(s["x"] + al[0] - 80, s["y"] + al[1]); time.sleep(0.5)   # its checkbox, left of the label
    pt2, w = seek(s, "Export PNG", "p4_seek", top_first=True)
    if pt2: click(*pt2); time.sleep(5.0)
    shot(s, "p5_after_exports")
time.sleep(1.0); p.terminate(); p.wait()
R["new_pngs"] = sorted(set(glob.glob(os.path.expanduser("~/Pictures/midi-sink-print-*.png"))) - pics_before)
R["log"] = [l.strip() for l in open(OUT + "/walk_anod.log") if any(k in l.lower() for k in ("[print", "preset", "export", "ledger", "error", "warn", "dip"))][:30]
json.dump(R, open(OUT + "/walk_anod.json", "w"), indent=1); print(json.dumps(R, indent=1))
