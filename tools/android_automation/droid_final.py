import sys, os, json, time, subprocess, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from droid import *
from PIL import Image; import numpy as np
APK = os.environ.get("MIDI_SINK_ROOT", os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))) + "/android/app/build/outputs/apk/debug/app-debug.apk"
ROOT = os.environ.get("MIDI_SINK_ROOT", os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
R = {}; RJ = f"{OUT}/final.json"
def save(): json.dump(R, open(RJ, "w"), indent=1, ensure_ascii=False)
def screen():
    ps = re.search(r"Physical size: (\d+)x(\d+)", sh("wm", "size")); w, h = int(ps.group(1)), int(ps.group(2))
    rot = re.search(r"rotation=(\d)", sh("dumpsys", "window", "displays")); r = int(rot.group(1)) if rot else 0
    return (h, w) if r in (1, 3) else (w, h)
def ink(png):
    a = np.asarray(Image.open(png).convert("L")); H, W = a.shape; a = a[int(H * 0.12):int(H * 0.9), :]
    m = a < 120; ys, xs = np.where(m); return (int(m.sum()), None if len(xs) == 0 else [round(float(xs.mean()) / W, 3), round(float(ys.mean()) / a.shape[0], 3)])
def start(**extras):
    sh("am", "force-stop", PKG); time.sleep(0.6); args = ["am", "start", "-n", f"{PKG}/.MainActivity"]
    for k, v in extras.items(): args += (["--ei", k, str(v)] if isinstance(v, int) else ["--es", k, str(v)])
    sh(*args); time.sleep(4.5)
TOL = 30
def row_nodes(label):
    n = scroll_to(label)
    if not n: return None, []
    return n, [m for m in dump() if abs(m[3] - n[3]) < TOL]
def row_value(label):
    n, row = row_nodes(label); return [m[0] for m in row if m[0] != label and m[0].strip() not in ("«", "‹", "›", "»")] if n else None
def step(label, arrow="» ", times=1):
    for _ in range(times):
        n, row = row_nodes(label); a = [m for m in row if m[0] == arrow]
        if a: tap(a[0][2], a[0][3], 0.5)
def prefs_xml(): return sh("run-as", PKG, "cat", "shared_prefs/sumi.xml")
def prefs_write(xml):
    p = subprocess.run(["adb", "shell", "run-as", PKG, "sh", "-c", "cat > shared_prefs/sumi.xml"], input=xml, text=True, capture_output=True); return p.returncode
def prefs_set(xml, **kv):
    for k, v in kv.items():
        tag = "string" if isinstance(v, str) else "boolean" if isinstance(v, bool) else "int" if isinstance(v, int) else "float"
        val = str(v).lower() if isinstance(v, bool) else str(v)
        xml = re.sub(rf'<{tag} name="{k}"[^/]*?(/>|>[^<]*</{tag}>)', "", xml)
        new = f'<string name="{k}">{val}</string>' if tag == "string" else f'<{tag} name="{k}" value="{val}" />'
        xml = xml.replace("</map>", "    " + new + "\n</map>")
    return xml

try:
    # ---- 0. the canvas-first build ----
    R["install"] = subprocess.run(["adb", "install", "-r", APK], capture_output=True, text=True).stdout.strip(); save()
    start(layout=0, playMode="0"); R["screen"] = screen(); open_settings()
    top = sorted([(n[4], n[0]) for n in dump() if n[0].strip()]); R["sheet_top_order"] = [t for _, t in top][:8]; shot("sheet_top"); save()
except Exception as e:
    R['err_stage_0'] = repr(e)[:300]; save(); print('stage 0 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 1. segment rows + spread + ripple step ----
    n = scroll_to("Exponential"); tap(n[2], n[3], 0.6); R["vortex_footnote"] = [t for t in texts() if t.startswith(("Rankine:", "Exponential:"))][:1]
    n = scroll_to("Inviscid doublet"); tap(n[2], n[3], 0.6); R["spread_row"] = row_value("Spread (l/a)"); step("Spread (l/a)", "» "); R["spread_after_step"] = row_value("Spread (l/a)")
    R["amount_before"] = row_value("Amount"); step("Amount", "» ", 2); R["amount_after"] = row_value("Amount"); R["ripple_footnote"] = [t for t in texts() if t.startswith("Sent as")][:1]
    n = scroll_to("Piano roll (right)"); tap(n[2], n[3], 0.6); R["tempo_rows_on_roll_right"] = [t for t in texts() if "Tempo" in t or "Roll speed" in t]; save()
    close_settings(); time.sleep(1.0); log = sh("run-as", PKG, "cat", "files/midi_log.csv"); open(f"{OUT}/midi_log_ripple.csv", "w").write(log)
    R["ripple_cc_src2"] = [l for l in log.splitlines() if l.count(",") == 4 and l.split(",")[1] == "176" and l.split(",")[2] in ("29", "28") and l.endswith(",2")][-4:]; save()
except Exception as e:
    R['err_stage_1'] = repr(e)[:300]; save(); print('stage 1 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 2. persistence: prefs on disk, then a relaunch replays the ripple CCs ----
    xml = prefs_xml(); R["prefs_after_changes"] = {k: re.search(rf'name="{k}"[^>]*?(?:value="([^"]*)"|>([^<]*)<)', xml).group(0)[:80] for k in ("vortexRankine", "wakeViscous", "wakeSpread", "rippleAmount", "layout", "viscosity", "palette", "showStrip") if re.search(rf'name="{k}"', xml)}
    relaunch(); log2 = sh("run-as", PKG, "cat", "files/midi_log.csv"); R["ripple_replay_after_relaunch"] = [l for l in log2.splitlines() if l.count(",") == 4 and l.split(",")[1] == "176" and l.split(",")[2] in ("29", "28") and l.endswith(",2")][:3]
    open_settings(); R["persist_sheet"] = {"vortex": [t for t in texts() if t.startswith(("Rankine:", "Exponential:"))][:1] if scroll_to("VORTEX") else None, "spread": row_value("Spread (l/a)"), "amount": row_value("Amount")}; save()
except Exception as e:
    R['err_stage_2'] = repr(e)[:300]; save(); print('stage 2 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 3. CC map: remove CC 29 -> Amount row gone; restore -> back ----
    n, row = row_nodes("CC  29  any"); x = [m for m in row if m[0] == "✕"]
    if x: tap(x[0][2], x[0][3], 0.6)
    R["cc29_present_after_remove"] = find("CC  29  any") is not None; scroll_to("RIPPLE"); R["amount_present_after_remove"] = find("Amount") is not None; R["ripple_footnote_after_remove"] = [t for t in texts() if t.startswith(("Route a CC", "Sent as"))][:1]
    n = scroll_to("Restore default map"); tap(n[2], n[3], 0.6); R["cc29_present_after_restore"] = scroll_to("CC  29  any") is not None; scroll_to("RIPPLE"); R["amount_present_after_restore"] = find("Amount") is not None; save()
    close_settings()
except Exception as e:
    R['err_stage_3'] = repr(e)[:300]; save(); print('stage 3 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 4. #71: an install that stored the #50 stock map comes up on the #69 routes ----
    sh("am", "force-stop", PKG); time.sleep(0.5)
    old50 = "255:1:0;255:2:6;255:7:6;255:11:6;255:26:0;255:24:1;255:22:2;255:29:3;255:30:4;255:31:5;255:27:7;255:28:8;255:102:7;255:103:8"
    R["prefs_write_rc"] = prefs_write(prefs_set(prefs_xml(), ccMap=old50)); R["prefs_ccMap_now"] = re.search(r'name="ccMap">([^<]*)<', prefs_xml()).group(1)[:60]
    start(layout=0, playMode="0"); open_settings(); n = scroll_to("CC  27  any")
    rows = [(m[0], m[3]) for m in dump() if m[0].startswith("CC ") or m[0] in ("Swirl strength", "Ripple amount", "Viscosity", "Palette morph", "Pinch (saddle)", "Vortex strength", "Ink flow (breath)", "Ripple wavelength", "Swirl center X", "Swirl center Y", "Pinch (crossed tines)", "Vortex center X", "Vortex center Y", "Paper roughness")]
    R["ccmap_rows_after_migration"] = sorted(rows, key=lambda r: r[1])[:40]; save(); close_settings()
except Exception as e:
    R['err_stage_4'] = repr(e)[:300]; save(); print('stage 4 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 5. gestures in Marble mode, in the phone's current orientation ----
    W, H = screen(); cx, cy = W // 2, H // 2; R["gesture_screen"] = [W, H]
    start(layout=0, playMode="0"); R["g_blank"] = ink(shot("g0_blank"))
    sh("input", "tap", str(cx - 200), str(cy - 300)); time.sleep(0.8); R["g_tap_drop"] = ink(shot("g1_tap"))
    sh("input", "motionevent", "DOWN", str(cx), str(cy)); time.sleep(0.7); R["g_after_long_press"] = ink(shot("g2_longpress"))
    sh("input", "motionevent", "MOVE", str(cx), str(cy - 250)); time.sleep(1.3); R["g_after_push_up_hold"] = ink(shot("g3_pushup"))
    sh("input", "motionevent", "MOVE", str(cx), str(cy + 300)); time.sleep(1.3); R["g_after_pull_back"] = ink(shot("g4_pullback"))
    sh("input", "motionevent", "UP", str(cx), str(cy + 300)); time.sleep(0.8); R["g_after_lift"] = ink(shot("g5_lift"))
    sh("input", "swipe", str(cx - 350), str(cy + 500), str(cx + 350), str(cy + 500), "500"); time.sleep(0.8); R["g_after_tine"] = ink(shot("g6_tine")); save()
except Exception as e:
    R['err_stage_5'] = repr(e)[:300]; save(); print('stage 5 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 6. rolls 6 and 7 drift ----
    for lay in (6, 7):
        start(layout=lay, playMode="0"); sh("input", "tap", str(cx), str(cy)); time.sleep(0.6); R[f"roll{lay}_t1"] = ink(shot(f"r{lay}_t1")); time.sleep(3.0); R[f"roll{lay}_t2"] = ink(shot(f"r{lay}_t2")); save()
except Exception as e:
    R['err_stage_6'] = repr(e)[:300]; save(); print('stage 6 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 7. Play mode byte log -> midi_asserts device ----
    start(layout=1, playMode="1"); time.sleep(1.0)
    for i in range(6): sh("input", "tap", str(150 + (i * 130) % (W - 300)), str(int(H * 0.35) + (i * 210) % int(H * 0.5))); time.sleep(0.35)
    sh("input", "swipe", str(cx - 250), str(int(H * 0.6)), str(cx + 250), str(int(H * 0.6)), "600"); time.sleep(0.5)
    sh("input", "swipe", str(cx), str(int(H * 0.4)), str(cx), str(int(H * 0.75)), "600"); time.sleep(0.8)
    open_settings(); close_settings(); time.sleep(1.0); log = sh("run-as", PKG, "cat", "files/midi_log.csv"); open(f"{OUT}/midi_log_play.csv", "w").write(log)
    r = subprocess.run(["python3", f"{ROOT}/tools/midi_asserts.py", "device", f"{OUT}/midi_log_play.csv"], capture_output=True, text=True); R["asserts_device"] = (r.stdout + r.stderr).strip().splitlines()[-14:]; R["asserts_rc"] = r.returncode; save()
except Exception as e:
    R['err_stage_7'] = repr(e)[:300]; save(); print('stage 7 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
try:
    # ---- 8. leave the phone tidy: defaults back for what this run changed; chromatic grid, Play on ----
    sh("am", "force-stop", PKG); time.sleep(0.5)
    prefs_write(prefs_set(prefs_xml(), vortexRankine=False, wakeViscous=False, wakeSpread=3.0, rippleAmount=0, rippleWavelength=32, palette=0, viscosity=0.5, bpm=120.0, layout=1, playMode=True))
    start(); R["done"] = True; save(); print(json.dumps(R, indent=1, ensure_ascii=False))
except Exception as e:
    R['err_stage_8'] = repr(e)[:300]; save(); print('stage 8 failed:', repr(e)[:200])
    try: close_settings()
    except Exception: pass
