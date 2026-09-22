// print_ledger.h — Phase 6 step 43 (QOL §4): THE PRINT LEDGER. Every paper
// dip of the session lands here with the FIELD it printed (sumi_read_field,
// RGBA16F), the look it was printed with (params and palette) and, once the
// dip's print arrives, a thumbnail — so any dip re-exports at any size while
// the session lives (sumi_export_begin over the kept field). Memory-capped:
// the oldest entries go first. The newest entry also keeps its full print,
// the "last print" the Save button writes.
#pragma once

#include "sumi_core.h"
#include <cstdint>
#include <string>
#include <vector>

struct AppSettings;

struct PrintEntry {
    std::vector<uint8_t> field;   // RGBA16F, fw x fh x 8
    uint32_t fw = 0, fh = 0;
    sumi_params_t  params{};      // the look at the dip
    sumi_palette_t palette{};
    std::string when;             // HH:MM:SS
    std::vector<uint8_t> print;   // RGBA8, the newest entry only
    uint32_t pw = 0, ph = 0;
    std::vector<uint8_t> thumb;   // RGBA8
    uint32_t tw = 0, th = 0;
    unsigned gl_tex = 0;          // the settings window's texture (it creates and frees it)
    bool print_seen = false;
};

class PrintLedger {
public:
    static constexpr size_t kMaxEntries = 8;
    static constexpr size_t kMaxBytes = 384u << 20;   // fields + prints + thumbnails

    // The product's dip: keep the field as it stands and the look, then dip.
    bool dip(sumi_instance_t* inst, const AppSettings& s);
    // Per frame: the arriving print becomes the newest entry's print and thumbnail; an export in flight is polled.
    void tick(sumi_instance_t* inst);
    // Re-export entry `index` at w x h (the entry's look restored round the render, the current look after) to a PNG.
    bool export_png(sumi_instance_t* inst, size_t index, uint32_t w, uint32_t h, bool anod_alpha,
                    const std::string& path, const AppSettings& current);
    bool save_last_print(const std::string& path) const;
    bool busy() const { return export_pending_; }
    const std::vector<PrintEntry>& entries() const { return entries_; }
    std::vector<unsigned>& dead_textures() { return dead_textures_; }
    void set_texture(size_t index, unsigned tex) { if (index < entries_.size()) entries_[index].gl_tex = tex; }
    const std::string& status() const { return status_; }

private:
    size_t bytes() const;
    void evict();
    std::vector<PrintEntry> entries_;       // newest last
    std::vector<unsigned> dead_textures_;   // evicted entries' textures, for the UI to free
    bool export_pending_ = false;
    uint32_t export_w_ = 0, export_h_ = 0;
    std::string export_path_;
    std::string status_;
};
