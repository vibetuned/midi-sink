import sys, os, json, time, subprocess, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from droid import *
APK = os.environ.get("MIDI_SINK_ROOT", os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))) + "/android/app/build/outputs/apk/debug/app-debug.apk"
R = {}; RJ = f"{OUT}/final2.json"
def save(): json.dump(R, open(RJ, "w"), indent=1, ensure_ascii=False)
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
def start(**extras):
    sh("am", "force-stop", PKG); time.sleep(0.6); args = ["am", "start", "-n", f"{PKG}/.MainActivity"]
    for k, v in extras.items(): args += (["--ei", k, str(v)] if isinstance(v, int) else ["--es", k, str(v)])
    sh(*args); time.sleep(4.5)
def prefs_xml(): return sh("run-as", PKG, "cat", "shared_prefs/sumi.xml")
def prefs_write(xml):
    open(f"{OUT}/sumi_prefs_tmp.xml", "w").write(xml)
    subprocess.run(["adb", "push", f"{OUT}/sumi_prefs_tmp.xml", "/data/local/tmp/sumi.xml"], capture_output=True)
    sh("chmod", "644", "/data/local/tmp/sumi.xml"); out = sh("run-as", PKG, "cp", "/data/local/tmp/sumi.xml", "shared_prefs/sumi.xml"); return out.strip()
def prefs_set(xml, **kv):
    for k, v in kv.items():
        tag = "string" if isinstance(v, str) else "boolean" if isinstance(v, bool) else "int" if isinstance(v, int) else "float"
        val = str(v).lower() if isinstance(v, bool) else str(v)
        xml = re.sub(rf'\s*<{tag} name="{k}"[^>]*?(/>|>[^<]*</{tag}>)', "", xml)
        new = f'<string name="{k}">{val}</string>' if tag == "string" else f'<{tag} name="{k}" value="{val}" />'
        xml = xml.replace("</map>", "    " + new + "\n</map>")
    return xml
def stage(name, fn):
    try: fn()
    except Exception as e:
        R[f"err_{name}"] = repr(e)[:300]; print(name, "failed:", repr(e)[:200])
        try: close_settings()
        except Exception: pass
    save()
def s0():   # the ripple-replay build; startup replay must show CC 29/28 as source 2 with the persisted values
    R["install"] = subprocess.run(["adb", "install", "-r", APK], capture_output=True, text=True).stdout.strip().splitlines()[-1]
    sh("am", "force-stop", PKG); prefs_write(prefs_set(prefs_xml(), rippleAmount=40, rippleWavelength=50, ccMap="")); start(layout=0, playMode="0"); time.sleep(1.0)
    log = sh("run-as", PKG, "cat", "files/midi_log.csv"); R["startup_ripple_replay_src2"] = [l for l in log.splitlines() if l.count(",") == 4 and l.split(",")[1] == "176" and l.split(",")[2] in ("29", "28") and l.endswith(",2")][:4]
def s1():   # rows: segment taps, spread, ripple step -> CC in the log
    open_settings(); n = scroll_to("Exponential"); tap(n[2], n[3], 0.6); R["vortex_footnote"] = [t for t in texts() if t.startswith(("Rankine:", "Exponential:"))][:1]
    n = scroll_to("Inviscid doublet"); tap(n[2], n[3], 0.6); R["stylus_row"] = [t for t in texts() if "Inviscid" in t][:1]; R["spread_row"] = row_value("Spread (l/a)")
    if R["spread_row"] is not None: step("Spread (l/a)", "» "); R["spread_after_step"] = row_value("Spread (l/a)")
    R["amount_before"] = row_value("Amount"); step("Amount", "» ", 1); R["amount_after"] = row_value("Amount"); R["ripple_footnote"] = [t for t in texts() if t.startswith("Sent as")][:1]
    n = scroll_to("Piano roll (right)"); tap(n[2], n[3], 0.6); R["tempo_rows_on_roll_right"] = [t for t in texts() if "Tempo" in t or "Roll speed" in t]
    close_settings(); time.sleep(1.0); log = sh("run-as", PKG, "cat", "files/midi_log.csv")
    R["ripple_cc_src2_tail"] = [l for l in log.splitlines() if l.count(",") == 4 and l.split(",")[1] == "176" and l.split(",")[2] in ("29", "28") and l.endswith(",2")][-3:]
def s2():   # CC map: remove CC 29 -> Amount hidden + footnote; restore -> Amount back
    open_settings(); n, row = row_nodes("CC  29  any"); x = [m for m in row if m[0] == "✕"]
    if x: tap(x[0][2], x[0][3], 0.6)
    R["cc29_after_remove"] = find("CC  29  any") is not None; scroll_to("RIPPLE"); R["amount_after_remove"] = find("Amount") is not None; R["footnote_after_remove"] = [t for t in texts() if t.startswith(("Route a CC", "Sent as"))][:1]
    n = scroll_to("Restore default map"); tap(n[2], n[3], 0.8); R["cc29_after_restore"] = scroll_to("CC  29  any") is not None
    n = scroll_to("RIPPLE"); R["amount_after_restore"] = find("Amount") is not None; R["footnote_after_restore"] = [t for t in texts() if t.startswith(("Route a CC", "Sent as"))][:1]; close_settings()
def s3():   # #71 migration: the #50 stock map stored -> today's routes shown
    sh("am", "force-stop", PKG); time.sleep(0.5)
    old50 = "255:1:0;255:2:6;255:7:6;255:11:6;255:26:0;255:24:1;255:22:2;255:29:3;255:30:4;255:31:5;255:27:7;255:28:8;255:102:7;255:103:8"
    R["prefs_write_out"] = prefs_write(prefs_set(prefs_xml(), ccMap=old50)); R["prefs_ccMap_stored"] = (re.search(r'name="ccMap">([^<]*)<', prefs_xml()) or [None, None])[1] if re.search(r'name="ccMap">([^<]*)<', prefs_xml()) else None
    start(layout=0, playMode="0"); open_settings(); n = scroll_to("CC  27  any"); rows = [(m[0], m[3]) for m in dump() if m[0].startswith("CC ") or m[0] in ("Swirl strength", "Ripple amount", "Viscosity", "Palette morph", "Pinch (saddle)", "Paper roughness", "Swirl center X", "Swirl center Y", "Pinch (crossed tines)", "Ripple wavelength")]
    R["ccmap_rows_after_migration"] = sorted(rows, key=lambda r: r[1]); close_settings()
def s4():   # tidy: defaults back, chromatic grid + Play as the author had it
    sh("am", "force-stop", PKG); time.sleep(0.5)
    prefs_write(prefs_set(prefs_xml(), vortexRankine=False, wakeViscous=False, wakeSpread=3.0, rippleAmount=0, rippleWavelength=32, palette=0, viscosity=0.7 if False else 0.5, bpm=120.0, layout=1, playMode=True, ccMap="")); start(); R["tidy"] = True
for name, fn in (("s0", s0), ("s1", s1), ("s2", s2), ("s3", s3), ("s4", s4)): stage(name, fn)
print(json.dumps(R, indent=1, ensure_ascii=False))
