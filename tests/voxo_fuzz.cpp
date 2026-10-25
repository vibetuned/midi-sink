// voxo_fuzz.cpp — mutation fuzzing of the Decent Sampler front end (Phase 7
// step 50: "an hour of fuzzing the parser never crashes"). Takes the fixture
// presets and their sample files as seeds; for --seconds, picks a seed,
// applies random mutations (byte flips, truncation, insertion of chunks from
// another seed, digit scrambling, attribute duplication), writes it beside a
// real Samples folder and loads it through voxo_load_preset — the XML, the
// zip and the decoders all under fire. Any crash is the failure; every load
// must either refuse with a reason or load with notes. ctest runs it for a
// few seconds; the evidence run is the hour.
// Step 75: --strobe-seconds <s> — the Poincaré strobe under fire: two Suzu instances fed the same random performance
// (notes, bends, the wheel, the press, patch changes across the kinds), one strobed and polled every block, one not;
// the audio must match bit for bit block by block and every return must be finite. ctest runs a few seconds of it too.
#include "voxo.h"
#include <cmath>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR must be defined"
#endif

static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return (uint32_t)(g_rng >> 16); }

static bool read_file(const std::string& p, std::vector<uint8_t>* out) {
    FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    out->resize(n > 0 ? (size_t)n : 0);
    const size_t got = n > 0 ? std::fread(out->data(), 1, (size_t)n, f) : 0; std::fclose(f);
    return got == out->size();
}
static bool write_file(const std::string& p, const std::vector<uint8_t>& b) {
    FILE* f = std::fopen(p.c_str(), "wb"); if (!f) return false;
    const size_t w = b.empty() ? 0 : std::fwrite(b.data(), 1, b.size(), f); std::fclose(f);
    return w == b.size();
}

static void mutate(std::vector<uint8_t>& b, const std::vector<std::vector<uint8_t>>& seeds) {
    const int rounds = 1 + (int)(rnd() % 8);
    for (int r = 0; r < rounds; r++) {
        if (b.empty()) { b.push_back((uint8_t)rnd()); continue; }
        switch (rnd() % 7) {
            case 0: b[rnd() % b.size()] ^= (uint8_t)(1u << (rnd() % 8)); break;                 // bit flip
            case 1: b.resize(rnd() % b.size()); break;                                          // truncate
            case 2: { const size_t at = rnd() % b.size(); b.insert(b.begin() + (long)at, (uint8_t)"<>\"'=/ \n0123456789-.eE"[rnd() % 22]); break; }
            case 3: { const std::vector<uint8_t>& o = seeds[rnd() % seeds.size()]; if (o.empty()) break;   // a chunk from another seed
                      const size_t from = rnd() % o.size(), len = rnd() % 64, at = rnd() % b.size();
                      b.insert(b.begin() + (long)at, o.begin() + (long)from, o.begin() + (long)(from + (len < o.size() - from ? len : o.size() - from))); break; }
            case 4: { for (size_t i = 0; i < b.size(); i++) if (b[i] >= '0' && b[i] <= '9' && rnd() % 4 == 0) b[i] = (uint8_t)('0' + rnd() % 10); break; }   // digits
            case 5: { const size_t at = rnd() % b.size(), len = rnd() % 200; if (at + len <= b.size()) { std::vector<uint8_t> dup(b.begin() + (long)at, b.begin() + (long)(at + len)); b.insert(b.begin() + (long)at, dup.begin(), dup.end()); } break; }   // duplicate
            case 6: { const size_t at = rnd() % b.size(); b[at] = (uint8_t)rnd(); break; }
        }
        if (b.size() > (1u << 20)) b.resize(1u << 20);
    }
}

static int strobe_fuzz(double seconds) {
    voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 16;
    voxo_t* a = voxo_create(&c); voxo_t* b = voxo_create(&c);
    for (voxo_t* v : { a, b }) { voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU); voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); }
    voxo_set_trace(a, 0x1FFu); voxo_set_strobe(a, 1u);
    struct Ev { unsigned long block; uint8_t st, d1, d2; }; static Ev ring[96]; unsigned ring_n = 0;   // the last events, dumped at the first non-finite block
    unsigned long blocks = 0;
    auto both = [&](uint8_t st, uint8_t d1, uint8_t d2) { voxo_push_midi(a, st, d1, d2); voxo_push_midi(b, st, d1, d2); ring[ring_n++ % 96] = { blocks, st, d1, d2 }; };
    const uint32_t patches = voxo_suzu_patch_count();
    float out_a[256], out_b[256]; static voxo_strobe_t st[16];
    unsigned long returns = 0, mismatches = 0, nonfinite = 0, patch_changes = 0, strobe_changes = 0, nan_blocks = 0;
    bool held[16] = {}; uint8_t held_note[16] = {};
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds) {
        // the performance: a few events a block over channels 1..8
        const int ev = (int)(rnd() % 4);
        for (int e = 0; e < ev; e++) {
            const int ch = 1 + (int)(rnd() % 8);
            switch (rnd() % 6) {
                case 0: if (!held[ch]) { held[ch] = true; held_note[ch] = (uint8_t)(36 + rnd() % 48); both((uint8_t)(0x90 | ch), held_note[ch], (uint8_t)(40 + rnd() % 88)); } break;
                case 1: if (held[ch]) { held[ch] = false; both((uint8_t)(0x80 | ch), held_note[ch], 0); } break;
                case 2: { const uint32_t bend = rnd() % 16384; both((uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7)); } break;
                case 3: both((uint8_t)(0xD0 | ch), (uint8_t)(rnd() % 128), 0); break;
                case 4: both(0xB0, 1, (uint8_t)(rnd() % 128)); break;
                case 5: both((uint8_t)(0xB0 | ch), 2, (uint8_t)(rnd() % 128)); break;
            }
        }
        if (rnd() % 400 == 0 && patches) { voxo_suzu_params_t sp; const uint32_t pi = rnd() % patches; voxo_suzu_patch(pi, &sp); voxo_set_suzu_params(a, &sp); voxo_set_suzu_params(b, &sp); patch_changes++; ring[ring_n++ % 96] = { blocks, 0xFF, (uint8_t)pi, (uint8_t)sp.voice_kind }; }
        if (rnd() % 200 == 0) { voxo_set_strobe(a, 1u + rnd() % 2); strobe_changes++; }
        voxo_render(a, out_a, 128); voxo_render(b, out_b, 128); blocks++;
        if (std::memcmp(out_a, out_b, sizeof out_a) != 0) mismatches++;
        { bool fin = true; for (float x : out_a) if (!std::isfinite(x)) fin = false;
          if (!fin) { if (nan_blocks == 0) { std::printf("  non-finite audio at block %lu; the last events (block status d1 d2; 0xFF = patch index, kind):\n", blocks);
                                            for (unsigned k = ring_n > 96 ? ring_n - 96 : 0; k < ring_n; k++) { const Ev& e = ring[k % 96]; std::printf("    %lu  %02X %3u %3u\n", e.block, e.st, e.d1, e.d2); }
                                            static voxo_inspect_t ins[16]; const uint32_t ni = voxo_suzu_inspect(b, ins, 16);
                                            for (uint32_t i = 0; i < ni; i++) { const voxo_inspect_t& t = ins[i]; bool fa = true, fb = true, fc = true, fk = true;
                                                for (uint32_t j = 0; j < t.n && j < VOXO_INSPECT_MAX; j++) { if (!std::isfinite(t.a[j])) fa = false; if (!std::isfinite(t.b[j])) fb = false; }
                                                for (int j = 0; j < 64; j++) if (!std::isfinite(t.c[j])) fc = false;
                                                for (int j = 0; j < 16; j++) if (!std::isfinite(t.k[j])) fk = false;
                                                std::printf("    voice: kind %u ch %u note %u held %u n %u  a %s b %s c %s k %s  k[0..5] %g %g %g %g %g %g\n", t.voice_kind, t.channel, t.note, t.held, t.n, fa ? "ok" : "NaN", fb ? "ok" : "NaN", fc ? "ok" : "NaN", fk ? "ok" : "NaN", t.k[0], t.k[1], t.k[2], t.k[3], t.k[4], t.k[5]); } }
                      nan_blocks++; } }
        const uint32_t n = voxo_strobe_poll(a, st, 16, VOXO_STROBE_POINTS_MAX);
        for (uint32_t i = 0; i < n; i++) for (uint32_t k = 0; k < st[i].count; k++) {
            returns++;
            if (!std::isfinite(st[i].x[k]) || !std::isfinite(st[i].y[k]) || !std::isfinite(st[i].u[k]) || !std::isfinite(st[i].w[k])) {
                if (nonfinite < 8) std::printf("  non-finite return: kind %u note %u ch %u held %u hz %g: x %g y %g u %g w %g (block %lu)\n", st[i].voice_kind, st[i].note, st[i].channel, st[i].held, st[i].hz, st[i].x[k], st[i].y[k], st[i].u[k], st[i].w[k], blocks);
                nonfinite++;
            }
        }
    }
    voxo_destroy(a); voxo_destroy(b);
    std::printf("[voxo fuzz] the strobe: %lu blocks in %.0f s, %lu returns polled, %lu patch changes, %lu clock changes; %lu blocks differ from the unstrobed twin, %lu non-finite returns, %lu non-finite audio blocks\n",
                blocks, seconds, returns, patch_changes, strobe_changes, mismatches, nonfinite, nan_blocks);
    if (mismatches || nonfinite || nan_blocks) { std::printf("FAIL: the strobe changed the rendering, returned a non-finite point, or the performance rendered a non-finite block\n"); return 1; }
    return 0;
}

int main(int argc, char** argv) {
    double seconds = 5.0, strobe_seconds = -1.0;
    for (int i = 1; i + 1 < argc; i++) { if (!std::strcmp(argv[i], "--seconds")) seconds = std::atof(argv[i + 1]); if (!std::strcmp(argv[i], "--strobe-seconds")) strobe_seconds = std::atof(argv[i + 1]); }
    if (strobe_seconds >= 0.0) return strobe_fuzz(strobe_seconds);   // step 75: the strobe alone
    const std::string dir = std::string(FIXTURES_DIR);
    const char* preset_seeds[] = {"minimal/minimal.dspreset", "features/features.dspreset", "formats/formats.dspreset", "malformed/badnumbers.dspreset",
                                  "pedal/pedal.dspreset", "pedal/choke.dspreset"};   // step 87: the pedal attributes under mutation
    const char* sample_seeds[] = {"minimal/Samples/tone.wav", "formats/Samples/tone.aif", "formats/Samples/tone.flac", "formats/Samples/tone24.wav",
                                  "pedal/Samples/k_half_2s.wav", "pedal/Samples/k_quarter.wav", "pedal/Samples/k_eighth.wav"};
    std::vector<std::vector<uint8_t>> presets, samples, all;
    for (const char* s : preset_seeds) { std::vector<uint8_t> b; if (!read_file(dir + "/" + s, &b)) { std::printf("FAIL: seed %s\n", s); return 1; } presets.push_back(b); all.push_back(b); }
    for (const char* s : sample_seeds) { std::vector<uint8_t> b; if (!read_file(dir + "/" + s, &b)) { std::printf("FAIL: seed %s\n", s); return 1; } samples.push_back(b); all.push_back(b); }
    std::vector<uint8_t> zip;
    read_file(dir + "/library.dslibrary", &zip);
    // The work folder: a Samples/ dir beside the mutated preset so paths resolve.
    const std::string work = dir + "/fuzz_work";
    (void)std::system(("mkdir -p \"" + work + "/Samples\"").c_str());
    write_file(work + "/Samples/tone.wav", samples[0]);
    write_file(work + "/Samples/tone_b.wav", samples[0]);
    write_file(work + "/Samples/tone.aif", samples[1]);
    write_file(work + "/Samples/tone.flac", samples[2]);
    write_file(work + "/Samples/tone24.wav", samples[3]);
    write_file(work + "/Samples/k_half_2s.wav", samples[4]);   // step 87: the pedal seeds' samples
    write_file(work + "/Samples/k_quarter.wav", samples[5]);
    write_file(work + "/Samples/k_eighth.wav", samples[6]);

    voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 4;
    voxo_t* v = voxo_create(&c);
    const auto t0 = std::chrono::steady_clock::now();
    unsigned long loads = 0, refused = 0, loaded = 0, sample_mut = 0, zip_mut = 0;
    g_rng ^= (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    float out[2 * 128];
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds) {
        const uint32_t pick = rnd() % 10;
        voxo_report_t r;
        if (pick < 6) {                                   // a mutated preset over intact samples
            std::vector<uint8_t> b = presets[rnd() % presets.size()];
            mutate(b, all);
            write_file(work + "/fuzz.dspreset", b);
            if (voxo_load_preset(v, (work + "/fuzz.dspreset").c_str(), &r)) loaded++; else refused++;
        } else if (pick < 9) {                            // an intact preset over a mutated sample
            std::vector<uint8_t> b = samples[rnd() % samples.size()];
            mutate(b, all);
            write_file(work + "/Samples/tone.wav", b);     // minimal.dspreset reads it
            write_file(work + "/fuzz.dspreset", presets[0]);
            if (voxo_load_preset(v, (work + "/fuzz.dspreset").c_str(), &r)) loaded++; else refused++;
            write_file(work + "/Samples/tone.wav", samples[0]);
            sample_mut++;
        } else if (!zip.empty()) {                        // a mutated zip
            std::vector<uint8_t> b = zip;
            mutate(b, all);
            write_file(work + "/fuzz.dslibrary", b);
            if (voxo_load_preset(v, (work + "/fuzz.dslibrary").c_str(), &r)) loaded++; else refused++;
            zip_mut++;
        }
        voxo_render(v, out, 128);                         // the swap, if any, lands; the voice pool sees the new sample
        if (r.ok) {                                       // step 87: a short performance through the pedal on whatever loaded —
            const uint8_t notes[5] = {48, 60, 46, 47, 42};   //   the noises, the filter, the choke, the half-pedal, the catch
            for (uint8_t n : notes) voxo_push_midi(v, 0x90, n, (uint8_t)(1 + rnd() % 127));
            voxo_push_midi(v, 0xB0, 64, (uint8_t)(rnd() % 128)); for (int b = 0; b < 3; b++) voxo_render(v, out, 128);
            for (uint8_t n : notes) voxo_push_midi(v, 0x80, n, 0);
            voxo_push_midi(v, 0xB0, 64, (uint8_t)(rnd() % 128)); for (int b = 0; b < 3; b++) voxo_render(v, out, 128);
            voxo_push_midi(v, 0xB0, 64, 0); voxo_push_midi(v, 0xB0, 64, 127); for (int b = 0; b < 3; b++) voxo_render(v, out, 128);
            voxo_push_midi(v, 0xB0, 123, 0); voxo_push_midi(v, 0xB0, 64, 0); voxo_render(v, out, 128);
            for (int i = 0; i < 256; i++) if (!std::isfinite(out[i])) { std::printf("FAIL: a non-finite sample after the pedal performance\n"); return 1; }
        }
        loads++;
        if (r.ok == 0 && !r.text[0]) { std::printf("FAIL: a refusal without a reason\n"); return 1; }
    }
    voxo_destroy(v);
    std::printf("[voxo fuzz] %lu loads in %.0f s: %lu loaded, %lu refused (%lu mutated samples, %lu mutated zips); no crash\n",
                loads, seconds, loaded, refused, sample_mut, zip_mut);
    return 0;
}
