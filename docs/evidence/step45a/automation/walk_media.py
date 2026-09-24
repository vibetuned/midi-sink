import sys, time, os, json; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from x11ui import *
p = launch([], OUT + "/walk_media.log", 150); c = canvas(); s = settings(); time.sleep(2)
cx, cy = c["x"] + c["w"] // 2, c["y"] + c["h"] // 2
def gestures(tag):
    focus(c)
    click(cx - 380, cy - 120); time.sleep(0.6); shot(c, f"m_{tag}_1_click")
    click(cx - 60, cy - 120); time.sleep(0.4)
    drag(1, cx - 110, cy - 120, cx - 10, cy - 60, 0.8, shift=True); time.sleep(0.8); shot(c, f"m_{tag}_2_shift_drag")
    click(cx + 300, cy - 120); time.sleep(0.4)
    drag(3, cx + 240, cy - 170, cx + 360, cy - 70, 0.8); time.sleep(0.8); shot(c, f"m_{tag}_3_right_drag")
    move(cx - 150, cy + 170); time.sleep(0.1); key("Shift_L", True); btn(3, True); time.sleep(1.0)
    for i in range(25): move(cx - 150, cy + 170 - 4 * i); time.sleep(0.05)
    time.sleep(1.0); shot(c, f"m_{tag}_4a_press_push")
    for i in range(40): move(cx - 150, cy + 70 + 5 * i); time.sleep(0.05)
    time.sleep(1.0); shot(c, f"m_{tag}_4b_press_pull"); btn(3, False); key("Shift_L", False); time.sleep(0.8); shot(c, f"m_{tag}_4c_release")
def sections(tag):
    for name in ("Substrate", "Palette", "Medium"):
        pt, _ = seek(s, name, f"m_{tag}_seek_{name}")
        if pt: wheel(s["x"] + 280, s["y"] + 300, 2); shot(s, f"m_{tag}_sec_{name}")
gestures("sumi"); sections("sumi")
pt, w = seek(s, "Anod (strain", "m_seek_anod")
if pt: click(pt[0] - 70, pt[1]); time.sleep(1.0)
w = top_w = ocr(shot(s, "m_after_anod"))
wheel(s["x"] + 280, s["y"] + 300, -60); focus(c); click(cx + 500, cy + 250); time.sleep(0.3)
tap("9"); time.sleep(1.2)   # a fresh sheet (lab key) so the Anod gestures start on clear glass
gestures("anod"); sections("anod")
p.terminate(); p.wait(); print("done")
