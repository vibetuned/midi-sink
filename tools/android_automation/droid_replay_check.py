# Startup replay of the persisted ripple CCs (#77): set the prefs, cold start,
# FLUSH the byte log (settings open + close), then read the first CC 29/28
# lines with source 2 — they must carry the persisted values.
import sys, os, json, time, subprocess, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from droid import *
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
R = {}
sh("am", "force-stop", PKG); time.sleep(0.5)
R["prefs_write"] = prefs_write(prefs_set(prefs_xml(), rippleAmount=40, rippleWavelength=50, ccMap=""))
R["prefs_now"] = [l.strip() for l in prefs_xml().splitlines() if "ripple" in l]
start(layout=0, playMode="0"); time.sleep(1.0); open_settings(); close_settings(); time.sleep(1.2)
log = sh("run-as", PKG, "cat", "files/midi_log.csv"); open(f"{OUT}/midi_log_startup.csv", "w").write(log)
lines = [l for l in log.splitlines() if l.count(",") == 4 and l.split(",")[1] == "176" and l.split(",")[2] in ("29", "28")]
R["first_cc29_28_lines"] = lines[:4]; R["all_src2_cc29_28"] = [l for l in lines if l.endswith(",2")][:6]
R["log_head"] = log.splitlines()[:3]
json.dump(R, open(f"{OUT}/replay_check.json", "w"), indent=1); print(json.dumps(R, indent=1))
