// voxo_preset_tests.cpp — the Decent Sampler front end's headless suite
// (Phase 7 step 50, SOUND §3; DECISIONS_6 #13–#15): every hand-written
// fixture loads with the expected model and the expected compat notes; the
// formats (WAV 16/24, AIFF, FLAC, a missing file, a non-audio file); a
// .dslibrary zip; the malformed inputs refuse with a reason and never crash;
// the report's copy is the docs' copy (voxo/COMPAT_REPORT.md quotes the
// library's sentences verbatim). Through the C ABI only.
#include "voxo.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

/* The counting allocator (as voxo_tests.cpp's): the step-51 contract check. */
static std::atomic<long> g_allocs{0};
void* operator new(std::size_t n) { g_allocs++; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void* operator new[](std::size_t n) { g_allocs++; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { g_allocs++; return std::malloc(n ? n : 1); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { g_allocs++; return std::malloc(n ? n : 1); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

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
    voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 16;   // the fifteen members and the master
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
        CHECK(st.preset_loaded == 1 && st.preset_zones == 1 && st.sample_frames == 400 && st.sample_rate_hz == 8000 && std::fabs(st.sample_root_note - 60.0f) < 0.01f,
              "the instrument is published: its first zone is %u frames at %u Hz, root %.0f", st.sample_frames, st.sample_rate_hz, (double)st.sample_root_note);
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
        // The dispatch (step 51): middle C at velocity 100 is the "loud" group's zone alone (one layer);
        // at velocity 50 the "soft" group's two round-robin zones answer one at a time.
        voxo_set_input_mode(v, 2); voxo_render(v, out, 128);
        voxo_push_midi(v, 0x90, 60, 100); voxo_render(v, out, 128);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 1 && st.active_layers == 1 && st.preset_zones == 5, "middle C at velocity 100: one voice, one layer (the loud group) — %u/%u", st.active_voices, st.active_layers);
        voxo_push_midi(v, 0x80, 60, 0); for (int i = 0; i < 40; i++) voxo_render(v, out, 128);
        voxo_push_midi(v, 0x90, 60, 50); voxo_render(v, out, 128); voxo_stats(v, &st);
        CHECK(st.active_layers == 1, "middle C at velocity 50: the soft group's round robin gives one layer (%u)", st.active_layers);
        voxo_push_midi(v, 0x80, 60, 0); for (int i = 0; i < 40; i++) voxo_render(v, out, 128);
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
        voxo_stats_t st;
        voxo_push_midi(v, 0x90, 60, 100); voxo_render(v, out, 128); voxo_stats(v, &st);
        const bool flac_plays = st.active_layers == 1;
        voxo_push_midi(v, 0x80, 60, 0); for (int i = 0; i < 40; i++) voxo_render(v, out, 128);
        voxo_push_midi(v, 0x90, 90, 100); voxo_render(v, out, 128); voxo_stats(v, &st);
        const bool missing_silent = st.active_layers == 0;
        voxo_push_midi(v, 0x80, 90, 0); for (int i = 0; i < 40; i++) voxo_render(v, out, 128);
        CHECK(flac_plays && missing_silent, "middle C plays its FLAC zone; a note on a missing zone stays silent (%s/%s)", flac_plays ? "ok" : "no layer", missing_silent ? "silent" : "a layer!");
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
        CHECK(st.sample_frames == 400, "the clamped zone is in the instrument (root clamped into range)");
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
        for (uint32_t bit = 1; bit <= VOXO_NOTE_MEMORY; bit <<= 1) {
            const char* s = voxo_note_copy(bit);
            if (!*s || !has(md, s)) { missing++; std::printf("     not in the docs: %s\n", s); }
        }
        CHECK(missing == 0, "all eleven canonical sentences appear in COMPAT_REPORT.md verbatim");
        CHECK(std::strlen(voxo_note_copy(0)) == 0 && std::strlen(voxo_note_copy(1u << 20)) == 0, "an unknown note bit has no sentence");
    }
    // ---- Step 51: the voice's interior (DECISIONS_6 #16–#18) ----
    voxo_set_input_mode(v, 2);   // classic: velocity and the bindings only, no pressure floor
    // Renders `seconds` and returns the RMS of the LEFT channel over the last `tail` seconds.
    auto rms_tail = [&](double seconds, double tail) {
        const uint32_t frames = (uint32_t)(seconds * 48000), tail_frames = (uint32_t)(tail * 48000);
        std::vector<float> out(2u * frames);
        for (uint32_t f = 0; f < frames; f += 128) voxo_render(v, out.data() + 2u * f, (frames - f) < 128 ? (frames - f) : 128);
        double acc = 0; uint32_t n = 0;
        for (uint32_t f = frames - tail_frames; f < frames; f++) { acc += (double)out[2u * f] * out[2u * f]; n++; }
        return std::sqrt(acc / n);
    };
    auto silence = [&](double seconds) { rms_tail(seconds, 0.001); };
    // A centred layer of a constant K at velocity-independent gain: K x 0.40 (a layer) x cos(45 deg) (the pan).
    const double UNIT = 0.40 * 0.70710678;
    // 7. Velocity layers with a crossfade: 0.5 below the overlap, 0.25 above, both across it.
    {
        CHECK(voxo_load_preset(v, fx("levels/layers.dspreset").c_str(), &r), "layers loads");
        voxo_push_midi(v, 0x90, 60, 40); const double lo = rms_tail(0.1, 0.05); voxo_push_midi(v, 0x80, 60, 0); silence(0.2);
        voxo_push_midi(v, 0x90, 60, 120); const double hi = rms_tail(0.1, 0.05); voxo_push_midi(v, 0x80, 60, 0); silence(0.2);
        voxo_push_midi(v, 0x90, 60, 70); const double mid = rms_tail(0.1, 0.05); voxo_push_midi(v, 0x80, 60, 0); silence(0.2);
        CHECK(std::fabs(lo - 0.5 * UNIT) < 0.01, "velocity 40: the 0.5 layer alone (%.4f, expected %.4f)", lo, 0.5 * UNIT);
        CHECK(std::fabs(hi - 0.25 * UNIT) < 0.01, "velocity 120: the 0.25 layer alone (%.4f, expected %.4f)", hi, 0.25 * UNIT);
        CHECK(mid > 0.25 * UNIT && mid < 0.75 * UNIT && std::fabs(mid - (0.5 + 0.25) * UNIT * 0.7071) < 0.03,
              "velocity 70, mid-overlap: both layers at equal power (%.4f, expected ~%.4f)", mid, 0.75 * UNIT * 0.7071);
    }
    // 8. Round robins: three positions at 0.5 / 0.25 / 0.125, cycling.
    {
        CHECK(voxo_load_preset(v, fx("levels/roundrobin.dspreset").c_str(), &r), "roundrobin loads");
        double got[6];
        for (int i = 0; i < 6; i++) { voxo_push_midi(v, 0x90, 60, 100); got[i] = rms_tail(0.1, 0.05); voxo_push_midi(v, 0x80, 60, 0); silence(0.2); }
        const double want[3] = {0.5 * UNIT, 0.25 * UNIT, 0.125 * UNIT};
        bool ok = true;
        for (int i = 0; i < 6; i++) if (std::fabs(got[i] - want[i % 3]) > 0.01) ok = false;
        CHECK(ok, "six strikes cycle the three positions: %.3f %.3f %.3f %.3f %.3f %.3f", got[0], got[1], got[2], got[3], got[4], got[5]);
    }
    // 9. Release samples: the 0.25 zone fires on note-off while the 0.5 zone fades.
    {
        CHECK(voxo_load_preset(v, fx("levels/release.dspreset").c_str(), &r), "release loads");
        voxo_push_midi(v, 0x90, 60, 100); const double held = rms_tail(0.1, 0.05);
        voxo_push_midi(v, 0x80, 60, 0); const double after = rms_tail(0.2, 0.05);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(std::fabs(held - 0.5 * UNIT) < 0.01 && std::fabs(after - 0.25 * UNIT) < 0.01 && st.active_layers == 1,
              "held %.4f, after the note-off the release zone alone %.4f (layers %u)", held, after, st.active_layers);
        silence(0.5);
    }
    // 10. The loop: a 3 s pad holds thirty seconds; without the loop it ends.
    {
        CHECK(voxo_load_preset(v, fx("loop/loop.dspreset").c_str(), &r), "loop loads");
        voxo_push_midi(v, 0x90, 57, 100);
        const double t1 = rms_tail(1.0, 0.2);
        double lowest = 1.0;
        for (int s2 = 0; s2 < 29; s2++) { const double x = rms_tail(1.0, 0.5); if (x < lowest) lowest = x; }
        const double t30 = rms_tail(0.5, 0.2);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_layers == 1 && t30 > 0.8 * t1 && lowest > 0.8 * t1, "the pad holds 30 s: %.4f at 1 s, %.4f at 30 s, never under %.4f", t1, t30, lowest);
        voxo_push_midi(v, 0x80, 57, 0); silence(0.5);
        CHECK(voxo_load_preset(v, fx("loop/noloop.dspreset").c_str(), &r), "noloop loads");
        voxo_push_midi(v, 0x90, 57, 100); rms_tail(3.2, 0.1); voxo_stats(v, &st);
        CHECK(st.active_layers == 0, "without the loop the 3 s pad has ended by 3.2 s (layers %u)", st.active_layers);
        voxo_push_midi(v, 0x80, 57, 0); silence(0.5);
    }
    // 11. The filter and CC 74: the default map moves the group's cutoff; brightness by the first difference's energy.
    auto brightness = [&](double seconds) {
        const uint32_t frames = (uint32_t)(seconds * 48000);
        std::vector<float> out(2u * frames);
        for (uint32_t f = 0; f < frames; f += 128) voxo_render(v, out.data() + 2u * f, (frames - f) < 128 ? (frames - f) : 128);
        double d = 0, e = 0;
        for (uint32_t f = frames / 2; f < frames; f++) { const double x = out[2u * f], px = out[2u * (f - 1)]; d += (x - px) * (x - px); e += x * x; }
        return e > 0 ? d / e : 0.0;
    };
    {
        CHECK(voxo_load_preset(v, fx("filter/filter_default.dspreset").c_str(), &r), "filter_default loads");
        voxo_push_midi(v, 0xB0, 74, 127); voxo_push_midi(v, 0x90, 45, 100); const double open = brightness(0.3);
        voxo_push_midi(v, 0xB0, 74, 0); const double closed = brightness(0.3);
        voxo_push_midi(v, 0xB0, 74, 64); const double centre = brightness(0.3);
        voxo_push_midi(v, 0x80, 45, 0); silence(0.3);
        CHECK(open > centre * 2.0 && centre > closed * 2.0, "CC 74 sweeps the cutoff: brightness open %.4f > centre %.4f > closed %.4f", open, centre, closed);
    }
    // 12. The preset's bindings override the defaults: pressure to AMP_VOLUME 0.5..1, timbre by its table.
    {
        CHECK(voxo_load_preset(v, fx("filter/filter_bound.dspreset").c_str(), &r), "filter_bound loads");
        voxo_set_input_mode(v, 1);   // MPE: the pressure is live
        voxo_push_midi(v, 0xB1, 74, 127); voxo_push_midi(v, 0xD1, 0, 0); voxo_push_midi(v, 0x91, 45, 100);
        const double quiet = rms_tail(0.3, 0.1);
        voxo_push_midi(v, 0xD1, 127, 0); const double loud = rms_tail(0.3, 0.1);
        CHECK(std::fabs(loud / quiet - 2.0) < 0.15, "pressure 0 -> 127 doubles the level through AMP_VOLUME 0.5..1 (%.3f -> %.3f)", quiet, loud);
        const double bright = brightness(0.3);
        voxo_push_midi(v, 0xB1, 74, 0); const double dark = brightness(0.3);
        CHECK(bright > dark * 3.0, "the timbre table 300 Hz..22 kHz: brightness %.4f at 127 vs %.4f at 0", bright, dark);
        voxo_push_midi(v, 0x81, 45, 0); silence(0.3);
        voxo_set_input_mode(v, 2);
    }
    // 13. The contract under fifteen voices of stacked samples: no allocation, and the block time.
    {
        CHECK(voxo_load_preset(v, fx("levels/stack.dspreset").c_str(), &r), "stack loads");
        std::vector<float> out(2u * 128);
        voxo_render(v, out.data(), 128);
        for (int ch = 1; ch <= 15; ch++) voxo_push_midi(v, (uint8_t)(0x90 | ch), (uint8_t)(48 + ch), 100);
        voxo_render(v, out.data(), 128);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 15 && st.active_layers == 90, "fifteen voices of six stacked, looping, filtered layers: %u voices, %u layers", st.active_voices, st.active_layers);
        const long armed = g_allocs.load();
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < 750; b++) {   // 2 s of blocks with bends, pressure and CC 74 every block
            for (int ch = 1; ch <= 15; ch++) {
                const int bend = 8192 + (int)(2000.0 * std::sin(b * 0.02 + ch));
                voxo_push_midi(v, (uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7));
                voxo_push_midi(v, (uint8_t)(0xD0 | ch), (uint8_t)((b * 3 + ch) % 128), 0);
                voxo_push_midi(v, (uint8_t)(0xB0 | ch), 74, (uint8_t)((b + ch * 5) % 128));
            }
            voxo_render(v, out.data(), 128);
        }
        const double ms_per_block = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 750.0;
        CHECK(g_allocs.load() == armed, "the storm over ninety stacked layers allocated nothing in voxo_render (%ld)", g_allocs.load() - armed);
        CHECK(ms_per_block < 2.667, "a 128-frame block of ninety layers renders in %.3f ms (the period is 2.667 ms)", ms_per_block);
        for (int ch = 1; ch <= 15; ch++) voxo_push_midi(v, (uint8_t)(0x80 | ch), (uint8_t)(48 + ch), 0);
        silence(0.5);
    }
    // ---- Step 52: the bus and the gate (DECISIONS_6 #19–#20) ----
    // 14. The reverb's tail and the delay's echoes: a 20 ms constant, then silence; the dry preset for comparison.
    {
        auto rms_at = [&](std::vector<float>& out, double t0, double t1) {
            double acc = 0; uint32_t n = 0;
            for (uint32_t f = (uint32_t)(t0 * 48000); f < (uint32_t)(t1 * 48000) && 2u * f + 1 < out.size(); f++) { acc += (double)out[2u * f] * out[2u * f]; n++; }
            return n ? std::sqrt(acc / n) : 0.0;
        };
        auto strike = [&](const char* preset, std::vector<float>& out) {
            CHECK(voxo_load_preset(v, fx(preset).c_str(), &r), "%s loads", preset);
            const uint32_t frames = 48000;   // 1 s
            out.assign(2u * frames, 0.0f);
            voxo_render(v, out.data(), 128);   // the swap
            voxo_push_midi(v, 0x90, 60, 100);
            for (uint32_t f = 0; f < frames; f += 128) {
                if (f == 896) voxo_push_midi(v, 0x80, 60, 0);   // 19 ms of the constant (seven blocks)
                voxo_render(v, out.data() + 2u * f, (frames - f) < 128 ? (frames - f) : 128);
            }
        };
        std::vector<float> wet, dry;
        strike("bus/dry.dspreset", dry);
        strike("bus/bus.dspreset", wet);
        const double dry_tail = rms_at(dry, 0.10, 0.20), wet_tail = rms_at(wet, 0.10, 0.20);
        CHECK(dry_tail < 1e-4 && wet_tail > 0.005, "after the note the dry preset is silent (%.5f) and the reverb rings (%.4f)", dry_tail, wet_tail);
        const double echo1 = rms_at(wet, 0.25, 0.27), between = rms_at(wet, 0.18, 0.24), echo2 = rms_at(wet, 0.50, 0.52);
        CHECK(echo1 > between * 1.5 && echo2 > between * 1.1 && echo1 > echo2, "the delay's echoes at 0.25 s (%.4f) and 0.5 s (%.4f) stand above the reverb between them (%.4f)", echo1, echo2, between);
        double peak = 0; for (float x : wet) peak = std::fmax(peak, std::fabs(x));
        CHECK(peak <= 1.0, "the bus never wraps (peak %.3f)", peak);
        // Both effects, fifteen looping voices, the storm: no allocation, and the block time.
        CHECK(voxo_load_preset(v, fx("bus/pad_bus.dspreset").c_str(), &r), "pad_bus loads");
        std::vector<float> out(2u * 128);
        voxo_render(v, out.data(), 128);
        for (int ch = 1; ch <= 15; ch++) voxo_push_midi(v, (uint8_t)(0x90 | ch), (uint8_t)(40 + ch * 2), 100);
        voxo_render(v, out.data(), 128);
        const long armed = g_allocs.load();
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < 750; b++) {
            for (int ch = 1; ch <= 15; ch++) {
                const int bend = 8192 + (int)(1500.0 * std::sin(b * 0.02 + ch));
                voxo_push_midi(v, (uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7));
                voxo_push_midi(v, (uint8_t)(0xB0 | ch), 74, (uint8_t)((b + ch * 5) % 128));
            }
            voxo_render(v, out.data(), 128);
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 750.0;
        CHECK(g_allocs.load() == armed, "fifteen looping voices through the reverb and the delay allocate nothing (%ld)", g_allocs.load() - armed);
        CHECK(ms < 2.667, "a 128-frame block with both effects renders in %.3f ms (the period is 2.667 ms)", ms);
        for (int ch = 1; ch <= 15; ch++) voxo_push_midi(v, (uint8_t)(0x80 | ch), (uint8_t)(40 + ch * 2), 0);
        silence(0.5);
    }
    // 15. The advisory gate: a warning, not a wall.
    {
        voxo_set_memory_budget(v, 1000);   // 1 KB: everything is oversized
        CHECK(voxo_load_preset(v, fx("minimal/minimal.dspreset").c_str(), &r), "over the budget, minimal still loads");
        CHECK((r.notes & VOXO_NOTE_MEMORY) && r.memory_estimate == 400 * 4 && r.memory_budget == 1000 && has(r.text, "about 0 MB against 0 MB advised"),
              "the memory note: estimate %u bytes (the WAV header's 400 frames x 4) against %u; text: %s", r.memory_estimate, r.memory_budget, r.text);
        std::vector<float> out(2u * 128); voxo_render(v, out.data(), 128);
        voxo_push_midi(v, 0x90, 60, 100); voxo_render(v, out.data(), 128);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_layers == 1, "and it plays (a layer sounds)");
        voxo_push_midi(v, 0x80, 60, 0); silence(0.2);
        voxo_set_memory_budget(v, 0);
        // 6400 decoded (AIFF stereo, FLAC, 24-bit WAV) plus the 18-byte text file counted at twice its size, as any unreadable header.
        CHECK(voxo_load_preset(v, fx("formats/formats.dspreset").c_str(), &r) && !(r.notes & VOXO_NOTE_MEMORY) && r.memory_estimate == 6400 + 36,
              "no budget, no note; the estimate reads AIFF, FLAC and 24-bit WAV headers: %u bytes (decoded %u; the text file at twice its 18 bytes)", r.memory_estimate, r.memory_bytes);
        CHECK(voxo_load_preset(v, fx("library.dslibrary").c_str(), &r) && r.memory_estimate == 1600, "the estimate reads a zip entry's head too: %u", r.memory_estimate);
    }
    // ---- Step 53: the instrument's reach and the shell's CC map into the bus (DECISIONS_6 #27) ----
    {
        uint8_t mask[16];
        CHECK(voxo_load_preset(v, fx("features/features.dspreset").c_str(), &r), "features loads");
        CHECK(voxo_covered_notes(v, mask), "a loaded preset reports its reach");
        auto has = [&](int n) { return (mask[n / 8] >> (n % 8)) & 1; };
        // features: the soft/loud groups cover 48..72 (attack), a release zone 73..127 (not counted), the streamed group 0..47.
        CHECK(has(0) && has(47) && has(48) && has(72) && !has(73) && !has(127), "the mask is the attack zones' union: 0..72 covered, 73..127 not (release zones do not count)");
        voxo_unload_preset(v);
        CHECK(!voxo_covered_notes(v, mask), "no preset: no mask (every cell shows)");
        // The shell's routes: CC 91 -> the reverb's amount turns the dry preset's reverb on.
        CHECK(voxo_load_preset(v, fx("bus/dry.dspreset").c_str(), &r), "dry loads");
        voxo_clear_cc_map(v);
        voxo_map_cc(v, 0xFF, 91, VOXO_CTL_REVERB_WET);
        voxo_map_cc(v, 0xFF, 94, VOXO_CTL_DELAY_WET);
        auto strike_tail = [&]() {
            const uint32_t frames = 24000;
            std::vector<float> out(2u * frames);
            voxo_render(v, out.data(), 128);
            voxo_push_midi(v, 0x90, 60, 100);
            for (uint32_t f = 0; f < frames; f += 128) {
                if (f == 896) voxo_push_midi(v, 0x80, 60, 0);
                voxo_render(v, out.data() + 2u * f, (frames - f) < 128 ? (frames - f) : 128);
            }
            double acc = 0; uint32_t n = 0;
            for (uint32_t f = 4800; f < 9600; f++) { acc += (double)out[2u * f] * out[2u * f]; n++; }
            return std::sqrt(acc / n);
        };
        const double silent = strike_tail();
        voxo_push_midi(v, 0xB0, 91, 100); voxo_render(v, nullptr, 0);
        const double ringing = strike_tail();
        CHECK(silent < 1e-4 && ringing > 0.005, "CC 91 through the shell's map turns the reverb on and up: tail %.5f -> %.4f", silent, ringing);
        // A route removed leaves its last value in the bus (the preset's live copy); turn the
        // reverb down through the route first, then clear, then a route on channel 6 alone.
        voxo_push_midi(v, 0xB0, 91, 0); strike_tail();
        voxo_clear_cc_map(v);
        voxo_map_cc(v, 5, 91, VOXO_CTL_REVERB_WET);   // channel 6 only
        voxo_push_midi(v, 0xB0, 91, 127);              // channel 1: ignored
        const double still = strike_tail();
        CHECK(still < 1e-3, "a route on channel 6 ignores channel 1's CC (tail %.5f)", still);
        voxo_push_midi(v, 0xB5, 91, 127);              // channel 6: taken
        const double again = strike_tail();
        CHECK(again > 0.005, "and takes channel 6's (tail %.4f)", again);
        voxo_clear_cc_map(v);
    }
    voxo_destroy(v);
    std::printf("[voxo presets] %s (%d failures)\n", g_fail ? "FAIL" : "all ok", g_fail);
    return g_fail;
}
