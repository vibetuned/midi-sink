// print_export.cpp — save the last paper-dip print (sumi_read_print, §5.3)
// as a PNG on a background thread. A product feature (the settings window's
// "Save last print" button) and a lab-bench tool (--print-out, --dip-burst).
#include "print_export.h"

#include <cerrno>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <utility>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Save the last paper-dip print via sumi_read_print (SPEC 5.3) as PNG.
// The writes' outcomes, worker -> render thread (#86).
static std::mutex g_write_mu;
static std::deque<std::pair<bool, std::string>> g_write_results;

void print_write_async(std::vector<uint8_t>* px, uint32_t w, uint32_t h, const std::string& path, const char* what) {
    if (!px) return;
    std::string p(path); std::string tag(what ? what : "print");
    std::thread([px, w, h, p, tag]() {
        std::string why;
        bool ok = false;
        // The folder first: the failure a user can act on, named as such (#84 / #85 hid behind a
        // bare "FAILED" on stdout). std::filesystem takes the same UTF-8 the shell holds.
        std::error_code ec;
        const std::filesystem::path fp(p);
        const std::filesystem::path dir = fp.has_parent_path() ? fp.parent_path() : std::filesystem::path(".");
        if (!std::filesystem::is_directory(dir, ec)) {
            why = "the folder does not exist";
        } else {
            errno = 0;
            ok = stbi_write_png(p.c_str(), (int)w, (int)h, 4, px->data(), (int)w * 4) != 0;
            if (!ok) why = errno ? std::strerror(errno) : "the file could not be written";
        }
        std::printf("[print] %s %s %ux%u -> %s%s%s\n", ok ? "saved" : "FAILED to save", tag.c_str(), w, h, p.c_str(),
                    ok ? "" : ": ", ok ? "" : why.c_str());
        std::fflush(stdout);
        delete px;
        char line[1200];
        if (ok) std::snprintf(line, sizeof line, "Saved %ux%u -> %s", w, h, p.c_str());
        else    std::snprintf(line, sizeof line, "Could not write %s: %s", p.c_str(), why.c_str());
        std::lock_guard<std::mutex> lk(g_write_mu);
        g_write_results.emplace_back(ok, std::string(line));
    }).detach();
}

bool print_write_poll(bool* out_ok, std::string* out_message) {
    std::lock_guard<std::mutex> lk(g_write_mu);
    if (g_write_results.empty()) return false;
    if (out_ok) *out_ok = g_write_results.front().first;
    if (out_message) *out_message = g_write_results.front().second;
    g_write_results.pop_front();
    return true;
}

bool save_print_png(sumi_instance_t* inst, const char* path) {
    uint32_t w = 0, h = 0;
    if (!sumi_read_print(inst, nullptr, 0, &w, &h)) {
        std::fprintf(stderr, "[print] no print ready (dip first: key 9 or CC64)\n");
        return false;
    }
    std::vector<uint8_t>* px = new std::vector<uint8_t>((size_t)w * h * 4);
    if (!sumi_read_print(inst, px->data(), px->size(), &w, &h)) {
        delete px;
        return false;
    }
    // Encode on a background thread: a 2560x1440 PNG takes ~1 s and must not
    // stall the render loop (step-7 DONE: no hitch at dip/export time).
    print_write_async(px, w, h, path, "last print");
    return true;
}

std::string default_print_path(const std::string& dir) {
    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&t));
    std::string base = dir.empty() ? std::string(".") : dir;
    return base + "/midi-sink-print-" + stamp + ".png";
}
