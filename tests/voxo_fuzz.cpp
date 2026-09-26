// voxo_fuzz.cpp — mutation fuzzing of the Decent Sampler front end (Phase 7
// step 50: "an hour of fuzzing the parser never crashes"). Takes the fixture
// presets and their sample files as seeds; for --seconds, picks a seed,
// applies random mutations (byte flips, truncation, insertion of chunks from
// another seed, digit scrambling, attribute duplication), writes it beside a
// real Samples folder and loads it through voxo_load_preset — the XML, the
// zip and the decoders all under fire. Any crash is the failure; every load
// must either refuse with a reason or load with notes. ctest runs it for a
// few seconds; the evidence run is the hour.
#include "voxo.h"

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

int main(int argc, char** argv) {
    double seconds = 5.0;
    for (int i = 1; i + 1 < argc; i++) if (!std::strcmp(argv[i], "--seconds")) seconds = std::atof(argv[i + 1]);
    const std::string dir = std::string(FIXTURES_DIR);
    const char* preset_seeds[] = {"minimal/minimal.dspreset", "features/features.dspreset", "formats/formats.dspreset", "malformed/badnumbers.dspreset"};
    const char* sample_seeds[] = {"minimal/Samples/tone.wav", "formats/Samples/tone.aif", "formats/Samples/tone.flac", "formats/Samples/tone24.wav"};
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
        loads++;
        if (r.ok == 0 && !r.text[0]) { std::printf("FAIL: a refusal without a reason\n"); return 1; }
    }
    voxo_destroy(v);
    std::printf("[voxo fuzz] %lu loads in %.0f s: %lu loaded, %lu refused (%lu mutated samples, %lu mutated zips); no crash\n",
                loads, seconds, loaded, refused, sample_mut, zip_mut);
    return 0;
}
