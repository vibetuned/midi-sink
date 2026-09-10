import sys, os, json, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from droid import *
R = {}
open_settings(); all_t = sheet_texts(); R["sheet_texts"] = all_t
want = ["Circle of fifths", "Chromatic grid (playable)", "Jankó (playable)", "Piano roll (left)", "Piano roll (top)", "Piano grid (playable)",
        "Piano roll (right)", "Piano roll (bottom)", "Palette", "Viscosity", "Ink feed (pressure)", "Paper roughness", "INPUT", "MPE", "Classic keyboard", "Wind",
        "VORTEX", "STYLUS WAKE", "RIPPLE", "Amount", "Wavelength", "Angle", "CC MAP", "Swirl strength", "Swirl center X", "Swirl center Y",
        "Pinch (saddle)", "Pinch (crossed tines)", "Ripple amount", "Ripple wavelength", "Add route", "Restore default map", "CANVAS", "Paper dip", "ABOUT", "libsumi"]
R["rows_present"] = {w: any(w in t for t in all_t) for w in want}
R["missing"] = [w for w, ok in R["rows_present"].items() if not ok]
R["cc_rows"] = [t for t in all_t if t.startswith("CC ")]
R["about"] = [t for t in all_t if t.startswith("midi-sink 0") or t.startswith("libsumi")]
print(json.dumps({k: v for k, v in R.items() if k != "sheet_texts"}, indent=1, ensure_ascii=False))
json.dump(R, open(f"{OUT}/rows.json", "w"), indent=1, ensure_ascii=False)
