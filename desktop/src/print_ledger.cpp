// print_ledger.cpp — see print_ledger.h.
#include "print_ledger.h"
#include "app_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>

#include "stb_image_write.h"   // the implementation lives in print_export.cpp

static void write_png_async(std::vector<uint8_t>* px, uint32_t w, uint32_t h, const std::string& path, const char* what) {
    std::string p(path); std::string tag(what);
    std::thread([px, w, h, p, tag]() {
        const int ok = stbi_write_png(p.c_str(), (int)w, (int)h, 4, px->data(), (int)w * 4);
        std::printf("[print] %s %s %ux%u -> %s\n", ok ? "saved" : "FAILED to save", tag.c_str(), w, h, p.c_str());
        std::fflush(stdout);
        delete px;
    }).detach();
}

size_t PrintLedger::bytes() const {
    size_t n = 0;
    for (const auto& e : entries_) n += e.field.size() + e.print.size() + e.thumb.size();
    return n;
}

void PrintLedger::evict() {
    while (!entries_.empty() && (entries_.size() > kMaxEntries || bytes() > kMaxBytes)) {
        if (entries_.front().gl_tex) dead_textures_.push_back(entries_.front().gl_tex);
        entries_.erase(entries_.begin());
    }
}

bool PrintLedger::dip(sumi_instance_t* inst, const AppSettings& s) {
    (void)s;
    if (!inst) return false;
    dip_requested_ = true;   // runs in tick(), in the core's context (#77)
    status_ = "Dipping...";
    return true;
}

bool PrintLedger::do_dip(sumi_instance_t* inst) {
    if (!inst) return false;
    PrintEntry e;
    uint32_t w = 0, h = 0;
    if (!sumi_read_field(inst, nullptr, 0, &w, &h) || w == 0 || h == 0) { sumi_trigger_paper_dip(inst); return false; }
    e.field.resize((size_t)w * h * 8u);
    if (!sumi_read_field(inst, e.field.data(), e.field.size(), &w, &h)) { sumi_trigger_paper_dip(inst); return false; }
    e.fw = w; e.fh = h;
    sumi_get_params(inst, &e.params);
    sumi_get_palette(inst, &e.palette);
    char stamp[16];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%H:%M:%S", std::localtime(&t));
    e.when = stamp;
    for (auto& old : entries_) { old.print.clear(); old.print.shrink_to_fit(); }   // only the newest keeps its full print
    entries_.push_back(std::move(e));
    evict();
    sumi_trigger_paper_dip(inst);
    status_ = "Dipped: the sheet is kept in the ledger";
    return true;
}

void PrintLedger::tick(sumi_instance_t* inst) {
    if (!inst) return;
    if (dip_requested_) { dip_requested_ = false; do_dip(inst); }
    if (export_requested_ && !export_pending_) { export_requested_ = false; do_export(inst); }
    // the newest entry's print, when the dip's readback lands
    if (!entries_.empty() && !entries_.back().print_seen) {
        PrintEntry& e = entries_.back();
        uint32_t pw = 0, ph = 0;
        if (sumi_read_print(inst, nullptr, 0, &pw, &ph) && pw && ph) {
            e.print.resize((size_t)pw * ph * 4u);
            if (sumi_read_print(inst, e.print.data(), e.print.size(), &pw, &ph)) {
                e.pw = pw; e.ph = ph; e.print_seen = true;
                // a thumbnail, box-averaged, at most 192 wide / 108 tall
                uint32_t sx = (pw + 191) / 192, sy = (ph + 107) / 108, sc = sx > sy ? sx : sy; if (sc < 1) sc = 1;
                e.tw = pw / sc; e.th = ph / sc;
                e.thumb.assign((size_t)e.tw * e.th * 4u, 0);
                for (uint32_t y = 0; y < e.th; y++) for (uint32_t x = 0; x < e.tw; x++) {
                    uint32_t acc[4] = {0, 0, 0, 0};
                    for (uint32_t yy = 0; yy < sc; yy++) for (uint32_t xx = 0; xx < sc; xx++) {
                        const uint8_t* p = &e.print[(((size_t)(y * sc + yy)) * pw + (x * sc + xx)) * 4u];
                        for (int c = 0; c < 4; c++) acc[c] += p[c];
                    }
                    uint8_t* q = &e.thumb[((size_t)y * e.tw + x) * 4u];
                    for (int c = 0; c < 4; c++) q[c] = (uint8_t)(acc[c] / (sc * sc));
                }
                evict();
            } else e.print.clear();
        }
    }
    // an export in flight
    if (export_pending_) {
        uint32_t w = 0, h = 0;
        const int st0 = sumi_export_poll(inst, nullptr, 0, &w, &h);
        if (st0 == 0) { export_pending_ = false; status_ = "Export failed"; return; }
        std::vector<uint8_t>* px = new std::vector<uint8_t>((size_t)export_w_ * export_h_ * 4u);
        const int st = sumi_export_poll(inst, px->data(), px->size(), &w, &h);
        if (st == 2) {
            export_pending_ = false;
            char b[160]; std::snprintf(b, sizeof b, "Exported %ux%u, writing the PNG in the background", w, h);
            status_ = b;
            write_png_async(px, w, h, export_path_, "export");
        } else {
            delete px;
            if (st == 0) { export_pending_ = false; status_ = "Export failed"; }
        }
    }
}

bool PrintLedger::export_png(sumi_instance_t* inst, size_t index, uint32_t w, uint32_t h, bool anod_alpha,
                             const std::string& path, const AppSettings& current) {
    (void)current;
    if (!inst || export_pending_ || export_requested_ || index >= entries_.size() || w == 0 || h == 0) return false;
    export_requested_ = true; req_index_ = index; req_w_ = w; req_h_ = h; req_alpha_ = anod_alpha; req_path_ = path;
    char b[160]; std::snprintf(b, sizeof b, "Exporting %ux%u...", w, h);
    status_ = b;
    return true;   // runs in tick(), in the core's context (#77)
}

bool PrintLedger::do_export(sumi_instance_t* inst) {
    if (!inst || req_index_ >= entries_.size()) { status_ = "Export failed"; return false; }
    const PrintEntry& e = entries_[req_index_];
    // the entry's look round the render, the look as it stands after (the render happens inside begin)
    sumi_params_t cur_p; sumi_palette_t cur_pal;
    sumi_get_params(inst, &cur_p);
    sumi_get_palette(inst, &cur_pal);
    sumi_set_params(inst, &e.params);
    sumi_set_palette(inst, &e.palette);
    const uint32_t flags = (req_alpha_ && e.params.medium == SUMI_MEDIUM_ANOD) ? SUMI_EXPORT_ANOD_ALPHA : 0u;
    const bool ok = sumi_export_begin(inst, e.field.data(), e.fw, e.fh, req_w_, req_h_, flags);
    sumi_set_params(inst, &cur_p);
    sumi_set_palette(inst, &cur_pal);
    if (!ok) { status_ = "Export could not start (a readback is in flight, or the size is out of range)"; return false; }
    export_pending_ = true; export_w_ = req_w_; export_h_ = req_h_; export_path_ = req_path_;
    return true;
}

bool PrintLedger::save_last_print(const std::string& path) const {
    for (size_t k = entries_.size(); k-- > 0;) {
        const PrintEntry& e = entries_[k];
        if (!e.print_seen || e.print.empty()) continue;
        write_png_async(new std::vector<uint8_t>(e.print), e.pw, e.ph, path, "last print");
        return true;
    }
    return false;
}
