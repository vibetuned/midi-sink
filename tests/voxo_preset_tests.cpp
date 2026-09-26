// voxo_preset_tests.cpp — the Decent Sampler front end's headless suite
// (Phase 7 step 50, SOUND §3; DECISIONS_6 #13–#15): every hand-written
// fixture loads with the expected model and the expected compat notes; the
// formats (WAV 16/24, AIFF, FLAC, a missing file, a non-audio file); a
// .dslibrary zip; the malformed inputs refuse with a reason and never crash;
// the report's copy is the docs' copy (voxo/COMPAT_REPORT.md quotes the
// library's sentences verbatim). Through the C ABI only.
#include "voxo.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR must be defined (tests/CMakeLists.txt)"
#endif
#ifndef VOXO_DIR
#error "VOXO_DIR must be defined"
#endif

static int g_fail = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf("ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } \
                              else { g_fail++; std::printf("FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string fx(const char* rel) { return std::string(FIXTURES_DIR) + "/" + rel; }
static bool has(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }

static std::string read_text(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "";
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
    return s;
}

int main() {
    std::printf("[voxo presets] fixtures %s\n", FIXTURES_DIR);
    voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 4;
    voxo_t* v = voxo_create(&c);
    voxo_report_t r;

    // 1. minimal: one zone, loads, no notes; the bridge publishes it as the sample.
    {
        const bool ok = voxo_load_preset(v, fx("minimal/minimal.dspreset").c_str(), &r);
        CHECK(ok && r.ok == 1, "minimal loads (%s)", r.text);
        CHECK(r.groups == 1 && r.zones == 1 && r.samples == 1 && r.samples_missing == 0, "minimal: 1 group, 1 zone, 1 sample");
        CHECK(r.notes == 0, "minimal: no compat notes (0x%x)", r.notes);
        CHECK(has(r.text, "minimal: 1 zones in 1 groups, 1 samples"), "the summary line names the preset and the counts");
        float out[2 * 128];
        voxo_render(v, out, 128);   // the swap lands
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.preset_loaded == 1 && st.preset_zones == 1 && st.sample_frames == 400 && st.sample_rate_hz == 8000 && st.sample_root_note == 60.0f,
              "the zone under middle C is the player's sample (%u frames at %u Hz, root %.0f)", st.sample_frames, st.sample_rate_hz, (double)st.sample_root_note);
    }
    // 2. features: every feature of the subset and every note of the report.
    {
        const bool ok = voxo_load_preset(v, fx("features/features.dspreset").c_str(), &r);
        CHECK(ok, "features loads");
        CHECK(r.groups == 3 && r.zones == 5 && r.samples == 2, "features: 3 groups, 5 zones, 2 distinct samples (got %u/%u/%u)", r.groups, r.zones, r.samples);
        const uint32_t expect = VOXO_NOTE_CHORUS | VOXO_NOTE_CONVOLUTION | VOXO_NOTE_MODULATORS | VOXO_NOTE_UI | VOXO_NOTE_STREAMING |
                                VOXO_NOTE_SEQUENCES | VOXO_NOTE_OTHER_FILTERS | VOXO_NOTE_UNKNOWN_EFFECT | VOXO_NOTE_UNKNOWN_BINDING;
        CHECK(r.notes == expect, "features: exactly the nine notes it provokes (0x%x, expected 0x%x)", r.notes, expect);
        CHECK(!(r.notes & VOXO_NOTE_MISSING_SAMPLES), "features: no missing samples");
        CHECK(has(r.text, "(phaser)"), "the unknown effect is named: %s", has(r.text, "(phaser)") ? "phaser" : r.text);
        CHECK(has(r.text, "(sparkle)"), "the unknown binding type is named");
        CHECK(has(r.text, voxo_note_copy(VOXO_NOTE_CHORUS)) && has(r.text, voxo_note_copy(VOXO_NOTE_UI)), "the report carries the canonical sentences");
        // The order of the lines is the documented one: missing/streaming first, UI last.
        const size_t p_stream = std::string(r.text).find(voxo_note_copy(VOXO_NOTE_STREAMING));
        const size_t p_ui = std::string(r.text).find(voxo_note_copy(VOXO_NOTE_UI));
        CHECK(p_stream != std::string::npos && p_ui != std::string::npos && p_stream < p_ui, "the notes come in the documented order");
        float out[2 * 128]; voxo_render(v, out, 128);
        voxo_stats_t st; voxo_stats(v, &st);
        // The bridge: middle C at velocity 100 is the "loud" group's first zone (root 60, no tuning).
        CHECK(st.sample_root_note == 60.0f && st.preset_zones == 5, "the bridge picked the zone under middle C at velocity 100 (root %.2f)", (double)st.sample_root_note);
    }
    // 3. formats: AIFF, FLAC, 24-bit WAV read; a missing file and a text file are notes, not refusals.
    {
        const bool ok = voxo_load_preset(v, fx("formats/formats.dspreset").c_str(), &r);
        CHECK(ok, "formats loads");
        CHECK(r.zones == 5 && r.samples == 5 && r.samples_missing == 2, "formats: 5 zones, 2 unreadable (got missing %u)", r.samples_missing);
        CHECK((r.notes & VOXO_NOTE_MISSING_SAMPLES) && has(r.text, "2 of 5 zones") && has(r.text, "missing.wav (file not found)"),
              "the missing-samples line counts and names the first: %s", r.text);
        CHECK(r.memory_bytes == (400 * 2 + 400 + 400) * 4, "AIFF stereo + FLAC mono + 24-bit WAV decoded: %u bytes", r.memory_bytes);
        float out[2 * 128]; voxo_render(v, out, 128);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.sample_root_note == 60.0f && st.sample_channels == 1, "middle C's zone is the FLAC one (root %.0f, %u ch)", (double)st.sample_root_note, st.sample_channels);
    }
    // 4. the .dslibrary: the same minimal preset zipped.
    {
        const bool ok = voxo_load_preset(v, fx("library.dslibrary").c_str(), &r);
        CHECK(ok && r.zones == 1 && r.samples_missing == 0 && std::strcmp(r.name, "minimal") == 0, "the .dslibrary loads from the zip (name %s: %s)", r.name, r.text);
    }
    // 5. malformed: refused with a reason, or loaded with notes; never a crash.
    {
        const char* refused[] = {"malformed/truncated.dspreset", "malformed/notds.dspreset", "malformed/empty.dspreset", "malformed/garbage.dspreset", "does/not/exist.dspreset"};
        const char* reasons[] = {"not well-formed XML", "no <DecentSampler> root", "the file is empty", "not well-formed XML", "cannot open the file"};
        for (int i = 0; i < 5; i++) {
            const bool ok = voxo_load_preset(v, fx(refused[i]).c_str(), &r);
            CHECK(!ok && r.ok == 0 && has(r.text, reasons[i]), "%s refused: %s", refused[i], r.text);
        }
        const bool ok = voxo_load_preset(v, fx("malformed/badnumbers.dspreset").c_str(), &r);
        CHECK(ok && r.zones == 2 && r.samples_missing == 1, "badnumbers loads: every number clamped or defaulted, the empty path a missing sample (%s)", r.text);
        voxo_stats_t st; float out[2 * 128]; voxo_render(v, out, 128); voxo_stats(v, &st);
        CHECK(st.sample_frames == 400, "the clamped zone still plays (root clamped into range)");
        const bool ok2 = voxo_load_preset(v, fx("minimal/Samples/tone.wav").c_str(), &r);
        CHECK(!ok2 && has(r.text, "not well-formed XML"), "a WAV handed in as a preset is refused as XML: %s", r.text);
        voxo_unload_preset(v);
        voxo_render(v, out, 128); voxo_stats(v, &st);
        CHECK(st.preset_loaded == 0 && st.sample_frames == 0, "unload returns to the sine");
    }
    // 6. The report's copy is the docs' copy: every sentence in voxo/COMPAT_REPORT.md verbatim.
    {
        const std::string md = read_text(std::string(VOXO_DIR) + "/COMPAT_REPORT.md");
        CHECK(!md.empty(), "voxo/COMPAT_REPORT.md read");
        int missing = 0;
        for (uint32_t bit = 1; bit <= VOXO_NOTE_UNKNOWN_BINDING; bit <<= 1) {
            const char* s = voxo_note_copy(bit);
            if (!*s || !has(md, s)) { missing++; std::printf("     not in the docs: %s\n", s); }
        }
        CHECK(missing == 0, "all ten canonical sentences appear in COMPAT_REPORT.md verbatim");
        CHECK(std::strlen(voxo_note_copy(0)) == 0 && std::strlen(voxo_note_copy(1u << 20)) == 0, "an unknown note bit has no sentence");
    }
    voxo_destroy(v);
    std::printf("[voxo presets] %s (%d failures)\n", g_fail ? "FAIL" : "all ok", g_fail);
    return g_fail;
}
