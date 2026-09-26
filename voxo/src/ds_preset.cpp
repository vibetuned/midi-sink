// ds_preset.cpp — see ds_preset.h. pugixml (no exceptions, no XPath) over the
// XML; the attribute grammar as Decent Sampler writes it (numbers with an
// optional "dB" suffix, booleans as true/false or 1/0, note ranges as "a-b"),
// defaults cascading <groups> → <group> → <sample>. Everything unknown is
// tolerated and, where it matters to the sound, noted — never fatal: a
// preset either is a Decent Sampler preset (then it loads, with notes) or it
// is not (then the reason says so).
#include "ds_preset.h"

#include "pugixml.hpp"
#include "miniz.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace voxo_ds {

namespace {

// ---- attribute grammar -------------------------------------------------------
float parse_float(const char* s, float dflt) {
    if (!s || !*s) return dflt;
    char* end = nullptr;
    const double v = std::strtod(s, &end);
    if (end == s || !std::isfinite(v)) return dflt;
    return (float)v;
}
int parse_int(const char* s, int dflt, int lo, int hi) {
    if (!s || !*s) return dflt;
    char* end = nullptr;
    const double v = std::strtod(s, &end);
    if (end == s || !std::isfinite(v)) return dflt;
    const long r = (long)std::lround(v);
    return (int)(r < lo ? lo : r > hi ? hi : r);
}
int64_t parse_i64(const char* s, int64_t dflt) {
    if (!s || !*s) return dflt;
    char* end = nullptr;
    const double v = std::strtod(s, &end);
    if (end == s || !std::isfinite(v) || v < 0 || v > 4.0e9) return dflt;
    return (int64_t)v;
}
bool parse_bool(const char* s, bool dflt) {
    if (!s || !*s) return dflt;
    if (!std::strcmp(s, "true") || !std::strcmp(s, "1") || !std::strcmp(s, "yes")) return true;
    if (!std::strcmp(s, "false") || !std::strcmp(s, "0") || !std::strcmp(s, "no")) return false;
    return dflt;
}
float parse_db(const char* s, float dflt) { return parse_float(s, dflt); }   // "-6.0dB": strtod stops at the suffix

SeqMode parse_seq(const char* s, SeqMode dflt) {
    if (!s || !*s) return dflt;
    if (!std::strcmp(s, "round_robin")) return SeqMode::RoundRobin;
    if (!std::strcmp(s, "random")) return SeqMode::Random;
    if (!std::strcmp(s, "true_random")) return SeqMode::TrueRandom;
    if (!std::strcmp(s, "always")) return SeqMode::Always;
    return dflt;
}
Trigger parse_trigger(const char* s, Trigger dflt) {
    if (!s || !*s) return dflt;
    if (!std::strcmp(s, "release")) return Trigger::Release;
    if (!std::strcmp(s, "first")) return Trigger::First;
    if (!std::strcmp(s, "legato")) return Trigger::Legato;
    if (!std::strcmp(s, "attack")) return Trigger::Attack;
    return dflt;
}

const char* attr(const pugi::xml_node& n, const char* name) {
    const pugi::xml_attribute a = n.attribute(name);
    return a ? a.value() : nullptr;
}
// The cascade: the node's own attribute, else the parents' (nearest first).
const char* attr_cascade(const pugi::xml_node& n, const char* name, int levels) {
    pugi::xml_node cur = n;
    for (int i = 0; i <= levels && cur; i++, cur = cur.parent()) {
        if (const char* v = attr(cur, name)) return v;
    }
    return nullptr;
}

// ---- effects ---------------------------------------------------------------
Effect parse_effect(const pugi::xml_node& e, Instrument* inst) {
    Effect fx;
    fx.type = attr(e, "type") ? attr(e, "type") : "";
    const std::string& t = fx.type;
    if (t == "lowpass" || t == "lowpass_4pl" || t == "lowpass_1pl" || t == "lowpass_2pl") { fx.kind = EffectKind::Lowpass; fx.supported = true; }
    else if (t == "highpass" || t == "highpass_4pl" || t == "highpass_1pl") { fx.kind = EffectKind::Highpass; inst->notes |= NOTE_OTHER_FILTERS; }
    else if (t == "bandpass") { fx.kind = EffectKind::Bandpass; inst->notes |= NOTE_OTHER_FILTERS; }
    else if (t == "peak") { fx.kind = EffectKind::Peak; inst->notes |= NOTE_OTHER_FILTERS; }
    else if (t == "notch") { fx.kind = EffectKind::Notch; inst->notes |= NOTE_OTHER_FILTERS; }
    else if (t == "gain") { fx.kind = EffectKind::Gain; fx.supported = true; }
    else if (t == "reverb") { fx.kind = EffectKind::Reverb; fx.supported = true; }
    else if (t == "delay") { fx.kind = EffectKind::Delay; fx.supported = true; }
    else if (t == "chorus") { fx.kind = EffectKind::Chorus; inst->notes |= NOTE_CHORUS; }
    else if (t == "convolution") { fx.kind = EffectKind::Convolution; inst->notes |= NOTE_CONVOLUTION; }
    else {
        fx.kind = EffectKind::Unknown;
        inst->notes |= NOTE_UNKNOWN_EFFECT;
        if (std::find(inst->unknown_effects.begin(), inst->unknown_effects.end(), t) == inst->unknown_effects.end())
            inst->unknown_effects.push_back(t.empty() ? "(untyped)" : t);
    }
    fx.frequency = parse_float(attr(e, "frequency"), fx.frequency);
    fx.resonance = parse_float(attr(e, "resonance"), fx.resonance);
    fx.q = parse_float(attr(e, "Q"), fx.q);
    fx.gain = parse_float(attr(e, "gain"), fx.gain);
    fx.room_size = parse_float(attr(e, "roomSize"), fx.room_size);
    fx.damping = parse_float(attr(e, "damping"), fx.damping);
    fx.wet_level = parse_float(attr(e, "wetLevel"), fx.wet_level);
    fx.delay_time = parse_float(attr(e, "delayTime"), fx.delay_time);
    fx.feedback = parse_float(attr(e, "feedback"), fx.feedback);
    fx.stereo_offset = parse_float(attr(e, "stereoOffset"), fx.stereo_offset);
    fx.delay_time_format = attr(e, "delayTimeFormat") ? attr(e, "delayTimeFormat") : "";
    fx.level = parse_float(attr(e, "level"), fx.level);
    return fx;
}

// ---- bindings ---------------------------------------------------------------
Binding parse_binding(const pugi::xml_node& b, Instrument* inst) {
    Binding bd;
    bd.type = attr(b, "type") ? attr(b, "type") : "";
    bd.level = attr(b, "level") ? attr(b, "level") : "";
    bd.parameter = attr(b, "parameter") ? attr(b, "parameter") : "";
    bd.position = parse_int(attr(b, "position"), -1, -1, 1000000);
    bd.group_index = parse_int(attr(b, "groupIndex"), -1, -1, 1000000);
    bd.effect_index = parse_int(attr(b, "effectIndex"), -1, -1, 1000000);
    bd.tags = attr(b, "tags") ? attr(b, "tags") : "";
    const char* tr = attr(b, "translation");
    if (tr && !std::strcmp(tr, "table")) bd.translation = Translation::Table;
    else if (tr && !std::strcmp(tr, "fixed_value")) bd.translation = Translation::FixedValue;
    else bd.translation = Translation::Linear;
    bd.out_min = parse_float(attr(b, "translationOutputMin"), 0.0f);
    bd.out_max = parse_float(attr(b, "translationOutputMax"), 1.0f);
    bd.fixed_value = parse_float(attr(b, "translationValue"), 0.0f);
    bd.factor = parse_float(attr(b, "factor"), 1.0f);
    bd.mod_amount = parse_float(attr(b, "modAmount"), 1.0f);
    bd.mod_behavior = attr(b, "modBehavior") ? attr(b, "modBehavior") : "";
    if (const char* t = attr(b, "translationTable")) {
        // "0,150;0.3,450;..." — pairs separated by ';', in and out by ','
        const char* p = t;
        while (*p) {
            char* end = nullptr;
            const double x = std::strtod(p, &end);
            if (end == p) break;
            p = end;
            if (*p != ',') break;
            p++;
            const double y = std::strtod(p, &end);
            if (end == p) break;
            p = end;
            if (std::isfinite(x) && std::isfinite(y)) bd.table.push_back({(float)x, (float)y});
            if (*p == ';') p++; else break;
            if (bd.table.size() > 256) break;
        }
        if (bd.table.empty()) bd.translation = Translation::Linear;
    }
    // As Decent Sampler writes them: the parameter targets, the UI-targeting
    // types at level="ui" (a CC moving a knob), and note_sequence — the
    // arpeggiator's, which is the sequences note, not an unknown binding.
    const bool known_type = bd.type == "amp" || bd.type == "effect" || bd.type == "general" || bd.type == "control" ||
                            bd.type == "modulator" || bd.type == "labeled_knob" || bd.type == "labeled-knob" || bd.type == "knob" ||
                            bd.type == "button" || bd.type == "menu" || bd.type == "image" || bd.type == "note_sequence";
    if (bd.type == "note_sequence") inst->notes |= NOTE_SEQUENCES;
    if (!known_type) {
        bd.known = false;
        inst->notes |= NOTE_UNKNOWN_BINDING;
        if (inst->unknown_bindings.size() < 16 &&
            std::find(inst->unknown_bindings.begin(), inst->unknown_bindings.end(), bd.type) == inst->unknown_bindings.end())
            inst->unknown_bindings.push_back(bd.type.empty() ? "(untyped)" : bd.type);
    }
    return bd;
}

void parse_bindings_into(const pugi::xml_node& parent, Source* src, Instrument* inst) {
    for (pugi::xml_node b = parent.child("binding"); b; b = b.next_sibling("binding")) {
        if (src->bindings.size() >= 64) break;
        src->bindings.push_back(parse_binding(b, inst));
    }
}

SourceKind source_kind(const char* element) {
    if (!std::strcmp(element, "cc")) return SourceKind::Cc;
    if (!std::strcmp(element, "note")) return SourceKind::Note;
    if (!std::strcmp(element, "velocity")) return SourceKind::Velocity;
    if (!std::strcmp(element, "mpePressure")) return SourceKind::MpePressure;
    if (!std::strcmp(element, "mpeTimbre")) return SourceKind::MpeTimbre;
    if (!std::strcmp(element, "aftertouch") || !std::strcmp(element, "channelPressure")) return SourceKind::Aftertouch;
    if (!std::strcmp(element, "pitchBend")) return SourceKind::PitchBend;
    if (!std::strcmp(element, "lfo")) return SourceKind::Lfo;
    if (!std::strcmp(element, "envelope")) return SourceKind::Envelope;
    if (!std::strcmp(element, "sequence")) return SourceKind::Sequence;
    if (!std::strcmp(element, "labeled-knob") || !std::strcmp(element, "control") || !std::strcmp(element, "knob") || !std::strcmp(element, "menu") || !std::strcmp(element, "button")) return SourceKind::UiControl;
    return SourceKind::Unknown;
}

void parse_source(const pugi::xml_node& n, Instrument* inst) {
    if (inst->sources.size() >= 256) return;
    Source s;
    s.element = n.name();
    s.kind = source_kind(n.name());
    s.number = parse_int(attr(n, "number"), -1, -1, 127);
    if (const char* nr = attr(n, "note")) {           // "48-96" or "60"
        char* end = nullptr;
        const long a = std::strtol(nr, &end, 10);
        s.note_lo = (int)a; s.note_hi = (int)a;
        if (end && *end == '-') s.note_hi = (int)std::strtol(end + 1, nullptr, 10);
        s.note_lo = std::clamp(s.note_lo, 0, 127); s.note_hi = std::clamp(s.note_hi, 0, 127);
    }
    s.rising_ms = parse_float(attr(n, "risingSmoothingTime"), 0.0f);
    s.falling_ms = parse_float(attr(n, "fallingSmoothingTime"), 0.0f);
    s.scope = attr(n, "scope") ? attr(n, "scope") : "";
    if (s.kind == SourceKind::UiControl) {
        const float lo = parse_float(attr(n, "minValue"), 0.0f), hi = parse_float(attr(n, "maxValue"), 1.0f);
        const float val = parse_float(attr(n, "value"), lo);
        s.value = hi > lo ? std::clamp((val - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    }
    parse_bindings_into(n, &s, inst);
    // A modulator's own <binding>s may sit under a child element in newer presets; accept those too.
    inst->sources.push_back(std::move(s));
}

void parse_ui(const pugi::xml_node& ui, Instrument* inst) {
    inst->notes |= NOTE_UI;
    // Walk every descendant: controls carry their starting values through bindings.
    struct Walker : pugi::xml_tree_walker {
        Instrument* inst;
        bool for_each(pugi::xml_node& node) override {
            if (source_kind(node.name()) == SourceKind::UiControl) parse_source(node, inst);
            return true;
        }
    } w;
    w.inst = inst;
    pugi::xml_node u = ui;
    u.traverse(w);
}

} // namespace

float Binding::translate(float in) const {
    in = std::clamp(in, 0.0f, 1.0f);
    switch (translation) {
        case Translation::FixedValue: return fixed_value;
        case Translation::Table: {
            if (table.empty()) return in;
            if (in <= table.front().first) return table.front().second;
            for (size_t i = 1; i < table.size(); i++) {
                if (in <= table[i].first) {
                    const float x0 = table[i - 1].first, y0 = table[i - 1].second, x1 = table[i].first, y1 = table[i].second;
                    const float t = x1 > x0 ? (in - x0) / (x1 - x0) : 0.0f;
                    return y0 + (y1 - y0) * t;
                }
            }
            return table.back().second;
        }
        default: return (out_min + (out_max - out_min) * in) * factor;
    }
}

const char* note_copy(uint32_t bit) {
    switch (bit) {
        case NOTE_CHORUS:          return "Chorus: this preset uses it; it will play without it.";
        case NOTE_CONVOLUTION:     return "Convolution reverb: this preset uses it; it will play without it.";
        case NOTE_MODULATORS:      return "Modulators (LFOs, envelopes, sequences): this preset uses them; it will play without them.";
        case NOTE_UI:              return "Custom interface: not shown here; its controls' starting values apply.";
        case NOTE_STREAMING:       return "Disk streaming: this preset asks for it; everything is loaded to memory instead.";
        case NOTE_SEQUENCES:       return "Note sequences: this preset uses them; it will play without them.";
        case NOTE_OTHER_FILTERS:   return "EQ and other filters (peak, notch, high-pass, band-pass): this preset uses them; it will play without them.";
        case NOTE_UNKNOWN_EFFECT:  return "An effect this version does not know: it will play without it.";
        case NOTE_MISSING_SAMPLES: return "Samples that could not be read: those notes stay silent.";
        case NOTE_UNKNOWN_BINDING: return "Bindings this version does not know: they are ignored.";
        case NOTE_MEMORY:          return "Memory: this library is larger than advised for this device; it is loaded anyway.";
        default:                   return "";
    }
}

bool parse(const char* text, size_t len, Instrument* out, std::string* why) {
    auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
    if (!text || len == 0) return fail("the file is empty");
    pugi::xml_document doc;
    const pugi::xml_parse_result r = doc.load_buffer(text, len, pugi::parse_default);
    if (!r) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "not well-formed XML (%s at byte %ld)", r.description(), (long)r.offset);
        return fail(buf);
    }
    const pugi::xml_node root = doc.child("DecentSampler");
    if (!root) return fail("not a Decent Sampler preset (no <DecentSampler> root)");
    out->min_version = attr(root, "minVersion") ? attr(root, "minVersion") : "";

    // <groups> with instrument-wide defaults, <group>s, <sample>s.
    const pugi::xml_node groups = root.child("groups");
    for (pugi::xml_node g = groups.child("group"); g; g = g.next_sibling("group")) {
        if (out->groups.size() >= 4096) break;
        Group grp;
        grp.name = attr(g, "name") ? attr(g, "name") : "";
        grp.tags = attr(g, "tags") ? attr(g, "tags") : "";
        grp.attack  = parse_float(attr_cascade(g, "attack", 1), 0.0f);
        grp.decay   = parse_float(attr_cascade(g, "decay", 1), 0.0f);
        grp.sustain = parse_float(attr_cascade(g, "sustain", 1), 1.0f);
        grp.release = parse_float(attr_cascade(g, "release", 1), 0.0f);
        grp.amp_vel_track = parse_float(attr_cascade(g, "ampVelTrack", 1), 1.0f);
        grp.volume_db = parse_db(attr_cascade(g, "volume", 1), 0.0f);
        grp.pan = parse_float(attr_cascade(g, "pan", 1), 0.0f);
        grp.tuning = parse_float(attr_cascade(g, "tuning", 1), 0.0f);
        grp.seq_mode = parse_seq(attr_cascade(g, "seqMode", 1), SeqMode::Always);
        grp.seq_length = parse_int(attr_cascade(g, "seqLength", 1), 0, 0, 1024);
        grp.trigger = parse_trigger(attr_cascade(g, "trigger", 1), Trigger::Attack);
        for (pugi::xml_node e = g.child("effects").child("effect"); e; e = e.next_sibling("effect")) {
            if (grp.effects.size() >= 64) break;
            grp.effects.push_back(parse_effect(e, out));
        }
        for (pugi::xml_node s = g.child("sample"); s; s = s.next_sibling("sample")) {
            if (grp.zones.size() >= 65536) break;
            Zone z;
            z.path = attr(s, "path") ? attr(s, "path") : "";
            z.lo_note = parse_int(attr(s, "loNote"), 0, 0, 127);
            z.hi_note = parse_int(attr(s, "hiNote"), 127, 0, 127);
            z.root_note = parse_int(attr(s, "rootNote"), 60, 0, 127);
            z.lo_vel = parse_int(attr(s, "loVel"), 0, 0, 127);
            z.hi_vel = parse_int(attr(s, "hiVel"), 127, 0, 127);
            if (z.hi_note < z.lo_note) std::swap(z.hi_note, z.lo_note);
            if (z.hi_vel < z.lo_vel) std::swap(z.hi_vel, z.lo_vel);
            z.tuning = parse_float(attr(s, "tuning"), 0.0f) + grp.tuning;
            z.volume_db = parse_db(attr(s, "volume"), 0.0f);
            z.pan = parse_float(attr(s, "pan"), grp.pan);
            z.start = parse_i64(attr(s, "start"), 0);
            z.end = parse_i64(attr(s, "end"), -1);
            z.loop_enabled = parse_bool(attr_cascade(s, "loopEnabled", 2), false);
            z.loop_start = parse_i64(attr_cascade(s, "loopStart", 2), 0);
            z.loop_end = parse_i64(attr_cascade(s, "loopEnd", 2), -1);
            z.loop_crossfade = parse_i64(attr_cascade(s, "loopCrossfade", 2), 0);
            z.seq_position = parse_int(attr(s, "seqPosition"), 1, 1, 1024);
            z.release = parse_float(attr(s, "release"), -1.0f);
            z.trigger = parse_trigger(attr(s, "trigger"), grp.trigger);
            if (const char* pm = attr_cascade(s, "playbackMode", 2)) {
                if (!std::strcmp(pm, "disk_streaming") || !std::strcmp(pm, "auto")) out->notes |= NOTE_STREAMING;
            }
            grp.zones.push_back(std::move(z));
            out->zone_count++;
        }
        out->groups.push_back(std::move(grp));
    }
    if (const char* pm = attr(groups, "playbackMode")) {
        if (!std::strcmp(pm, "disk_streaming") || !std::strcmp(pm, "auto")) out->notes |= NOTE_STREAMING;
    }
    // Instrument-level effects (the bus).
    for (pugi::xml_node e = root.child("effects").child("effect"); e; e = e.next_sibling("effect")) {
        if (out->effects.size() >= 64) break;
        out->effects.push_back(parse_effect(e, out));
    }
    // <midi>: cc / note / velocity / aftertouch / pitchBend sources.
    for (pugi::xml_node m = root.child("midi").first_child(); m; m = m.next_sibling()) {
        if (m.type() != pugi::node_element) continue;
        parse_source(m, out);
    }
    // <modulators>: the DS MPE bindings are supported; LFOs, envelopes and sequences are noted.
    for (pugi::xml_node m = root.child("modulators").first_child(); m; m = m.next_sibling()) {
        if (m.type() != pugi::node_element) continue;
        parse_source(m, out);
        const SourceKind k = source_kind(m.name());
        if (k == SourceKind::Lfo || k == SourceKind::Envelope || k == SourceKind::Sequence || k == SourceKind::Unknown) out->notes |= NOTE_MODULATORS;
    }
    if (root.child("noteSequences").child("sequence")) out->notes |= NOTE_SEQUENCES;
    if (root.child("ui")) parse_ui(root.child("ui"), out);
    return true;
}

// ---- readers ---------------------------------------------------------------
namespace {

std::string dir_of(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? std::string() : path.substr(0, p + 1);
}
std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
bool ends_with(const std::string& s, const char* suffix) {
    const size_t n = std::strlen(suffix);
    return s.size() >= n && lower(s.substr(s.size() - n)) == suffix;
}

bool read_file(const std::string& path, std::vector<uint8_t>* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (1L << 30)) { std::fclose(f); return false; }
    out->resize((size_t)n);
    const size_t got = n ? std::fread(out->data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == (size_t)n;
}

struct FolderReader : Reader {
    std::string base;
    bool read(const std::string& relative, std::vector<uint8_t>* out) override {
        std::string rel = relative;
        for (char& c : rel) if (c == '\\') c = '/';
        return read_file(base + rel, out);
    }
    bool read_head(const std::string& relative, size_t max_bytes, std::vector<uint8_t>* out, uint64_t* file_size) override {
        std::string rel = relative;
        for (char& c : rel) if (c == '\\') c = '/';
        FILE* f = std::fopen((base + rel).c_str(), "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        *file_size = n > 0 ? (uint64_t)n : 0;
        out->resize(max_bytes);
        const size_t got = std::fread(out->data(), 1, max_bytes, f);
        std::fclose(f);
        out->resize(got);
        return true;
    }
};

struct ZipReader : Reader {
    mz_zip_archive zip{};
    std::string base;          // the preset's folder inside the archive
    bool open = false;
    ~ZipReader() override { if (open) mz_zip_reader_end(&zip); }
    bool read(const std::string& relative, std::vector<uint8_t>* out) override {
        if (!open) return false;
        std::string rel = relative;
        for (char& c : rel) if (c == '\\') c = '/';
        const std::string name = base + rel;
        int idx = mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, 0);
        if (idx < 0) idx = mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
        if (idx < 0) return false;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, (mz_uint)idx, &st)) return false;
        if (st.m_uncomp_size > (1ull << 30)) return false;
        out->resize((size_t)st.m_uncomp_size);
        return mz_zip_reader_extract_to_mem(&zip, (mz_uint)idx, out->data(), out->size(), 0) != 0;
    }
    bool read_head(const std::string& relative, size_t max_bytes, std::vector<uint8_t>* out, uint64_t* file_size) override {
        if (!open) return false;
        std::string rel = relative;
        for (char& c : rel) if (c == '\\') c = '/';
        const std::string name = base + rel;
        int idx = mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, 0);
        if (idx < 0) return false;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, (mz_uint)idx, &st)) return false;
        *file_size = st.m_uncomp_size;
        // Streamed: only the head is inflated.
        mz_zip_reader_extract_iter_state* it = mz_zip_reader_extract_iter_new(&zip, (mz_uint)idx, 0);
        if (!it) return false;
        out->resize(max_bytes);
        const size_t got = mz_zip_reader_extract_iter_read(it, out->data(), max_bytes);
        mz_zip_reader_extract_iter_free(it);
        out->resize(got);
        return true;
    }
};

// The decoded size of one sample from its header: frames x channels x 4 bytes.
uint64_t decoded_size_from_head(const std::vector<uint8_t>& h, uint64_t file_size) {
    auto rd32le = [&](size_t at) { return (uint32_t)h[at] | ((uint32_t)h[at + 1] << 8) | ((uint32_t)h[at + 2] << 16) | ((uint32_t)h[at + 3] << 24); };
    auto rd16le = [&](size_t at) { return (uint32_t)h[at] | ((uint32_t)h[at + 1] << 8); };
    auto rd32be = [&](size_t at) { return ((uint32_t)h[at] << 24) | ((uint32_t)h[at + 1] << 16) | ((uint32_t)h[at + 2] << 8) | (uint32_t)h[at + 3]; };
    auto rd16be = [&](size_t at) { return ((uint32_t)h[at] << 8) | (uint32_t)h[at + 1]; };
    if (h.size() >= 12 && std::memcmp(h.data(), "RIFF", 4) == 0 && std::memcmp(h.data() + 8, "WAVE", 4) == 0) {
        uint32_t channels = 0, bits = 0, data = 0;
        size_t pos = 12;
        while (pos + 8 <= h.size()) {
            const uint32_t csize = rd32le(pos + 4);
            if (std::memcmp(h.data() + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= h.size()) { channels = rd16le(pos + 10); bits = rd16le(pos + 22); }
            else if (std::memcmp(h.data() + pos, "data", 4) == 0) { data = csize; break; }
            pos += 8 + (size_t)csize + (csize & 1u);
        }
        if (channels && bits && data) return (uint64_t)(data / (bits / 8)) * 4;
    } else if (h.size() >= 42 && std::memcmp(h.data(), "fLaC", 4) == 0) {
        // STREAMINFO at byte 8: channels-1 in 3 bits, bits-1 in 5 bits, total samples in 36 bits (bytes 20..25 of the block).
        const uint8_t* b = h.data() + 8;
        const uint32_t channels = ((b[12] >> 1) & 0x7) + 1;
        const uint64_t total = ((uint64_t)(b[13] & 0x0F) << 32) | ((uint64_t)b[14] << 24) | ((uint64_t)b[15] << 16) | ((uint64_t)b[16] << 8) | b[17];
        if (total) return total * channels * 4;
    } else if (h.size() >= 12 && std::memcmp(h.data(), "FORM", 4) == 0) {
        size_t pos = 12;
        while (pos + 8 <= h.size()) {
            const uint32_t csize = rd32be(pos + 4);
            if (std::memcmp(h.data() + pos, "COMM", 4) == 0 && pos + 8 + 6 <= h.size()) {
                const uint32_t channels = rd16be(pos + 8), frames = rd32be(pos + 10);
                if (channels && frames) return (uint64_t)frames * channels * 4;
                break;
            }
            pos += 8 + (size_t)csize + (csize & 1u);
        }
    }
    return file_size * 2;   // a header we could not read: assume 16-bit PCM
}

} // namespace

uint64_t estimate_decoded_bytes(const Instrument& inst, Reader& reader) {
    std::vector<std::string> seen;
    uint64_t total = 0;
    for (const Group& g : inst.groups) {
        for (const Zone& z : g.zones) {
            if (z.path.empty() || std::find(seen.begin(), seen.end(), z.path) != seen.end()) continue;
            seen.push_back(z.path);
            std::vector<uint8_t> head;
            uint64_t size = 0;
            if (!reader.read_head(z.path, 4096, &head, &size)) continue;
            total += decoded_size_from_head(head, size);
        }
    }
    return total;
}

void load_samples(Instrument* inst, Reader& reader) {
    // Decode each distinct path once; zones point at the decoded sample.
    inst->samples.clear();
    inst->memory_bytes = 0;
    inst->missing = 0;
    for (Group& g : inst->groups) {
        for (Zone& z : g.zones) {
            int found = -1;
            for (size_t i = 0; i < inst->samples.size(); i++) if (inst->samples[i].path == z.path) { found = (int)i; break; }
            if (found < 0) {
                SampleData sd;
                sd.path = z.path;
                std::vector<uint8_t> bytes;
                if (z.path.empty()) sd.why = "no path";
                else if (!reader.read(z.path, &bytes)) sd.why = "file not found";
                else sd.ok = decode_audio(z.path, bytes.data(), bytes.size(), &sd.frames, &sd.channels, &sd.rate, &sd.why);
                if (sd.ok) inst->memory_bytes += sd.frames.size() * sizeof(float);
                inst->samples.push_back(std::move(sd));
                found = (int)inst->samples.size() - 1;
            }
            z.sample = inst->samples[(size_t)found].ok ? found : -1;
            if (z.sample < 0) inst->missing++;
        }
    }
    if (inst->missing) inst->notes |= NOTE_MISSING_SAMPLES;
}

void build_report(Instrument* inst) {
    char line[512];
    std::string r;
    const double mb = (double)inst->memory_bytes / (1024.0 * 1024.0);
    std::snprintf(line, sizeof(line), "%s: %u zones in %u groups, %u samples, %.1f MB in memory.\n",
                  inst->name.empty() ? "Preset" : inst->name.c_str(), inst->zone_count, (unsigned)inst->groups.size(),
                  (unsigned)inst->samples.size(), mb);
    r += line;
    const uint32_t order[] = {NOTE_MEMORY, NOTE_MISSING_SAMPLES, NOTE_STREAMING, NOTE_CHORUS, NOTE_CONVOLUTION, NOTE_OTHER_FILTERS,
                              NOTE_UNKNOWN_EFFECT, NOTE_MODULATORS, NOTE_SEQUENCES, NOTE_UNKNOWN_BINDING, NOTE_UI};
    for (uint32_t bit : order) {
        if (!(inst->notes & bit)) continue;
        r += note_copy(bit);
        if (bit == NOTE_MEMORY) {
            std::snprintf(line, sizeof(line), " (about %.0f MB against %.0f MB advised)",
                          (double)inst->memory_estimate / (1024.0 * 1024.0), (double)inst->memory_budget / (1024.0 * 1024.0));
            r += line;
        } else if (bit == NOTE_MISSING_SAMPLES) {
            std::string first;
            for (const SampleData& s : inst->samples) if (!s.ok) { first = s.path + " (" + s.why + ")"; break; }
            std::snprintf(line, sizeof(line), " (%u of %u zones; first: %s)", inst->missing, inst->zone_count, first.c_str());
            r += line;
        } else if (bit == NOTE_UNKNOWN_EFFECT) {
            r += " (";
            for (size_t i = 0; i < inst->unknown_effects.size(); i++) r += (i ? ", " : "") + inst->unknown_effects[i];
            r += ")";
        } else if (bit == NOTE_UNKNOWN_BINDING) {
            r += " (";
            for (size_t i = 0; i < inst->unknown_bindings.size(); i++) r += (i ? ", " : "") + inst->unknown_bindings[i];
            r += ")";
        }
        r += "\n";
    }
    inst->report = r;
}

bool load(const std::string& path, Instrument* out, std::string* why, uint64_t budget) {
    auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
    out->source_path = path;
    out->memory_budget = budget;
    auto gate = [&](Reader& reader) {   // the advisory gate: an estimate before decoding, a note when over
        out->memory_estimate = estimate_decoded_bytes(*out, reader);
        if (budget && out->memory_estimate > budget) out->notes |= NOTE_MEMORY;
    };
    std::vector<uint8_t> xml;
    if (ends_with(path, ".dslibrary")) {
        ZipReader zr;
        std::memset(&zr.zip, 0, sizeof(zr.zip));
        if (!mz_zip_reader_init_file(&zr.zip, path.c_str(), 0)) return fail("cannot open the library archive");
        zr.open = true;
        // The first .dspreset in the archive is the preset; its folder is the base.
        std::string preset_name;
        const mz_uint n = mz_zip_reader_get_num_files(&zr.zip);
        for (mz_uint i = 0; i < n; i++) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&zr.zip, i, &st)) continue;
            std::string nm = st.m_filename;
            if (ends_with(nm, ".dspreset") && !mz_zip_reader_is_file_a_directory(&zr.zip, i)) { preset_name = nm; break; }
        }
        if (preset_name.empty()) return fail("the library archive holds no .dspreset");
        zr.base = dir_of(preset_name);
        const std::string leaf = preset_name.substr(zr.base.size());
        if (!zr.read(leaf, &xml)) return fail("cannot read the preset inside the archive");
        out->name = leaf.substr(0, leaf.size() - 9);
        if (!parse((const char*)xml.data(), xml.size(), out, why)) return false;
        gate(zr);
        load_samples(out, zr);
    } else {
        if (!read_file(path, &xml)) return fail("cannot open the file");
        const std::string leaf = path.substr(dir_of(path).size());
        out->name = ends_with(leaf, ".dspreset") ? leaf.substr(0, leaf.size() - 9) : leaf;
        if (!parse((const char*)xml.data(), xml.size(), out, why)) return false;
        FolderReader fr;
        fr.base = dir_of(path);
        gate(fr);
        load_samples(out, fr);
    }
    build_report(out);
    return true;
}

} // namespace voxo_ds
