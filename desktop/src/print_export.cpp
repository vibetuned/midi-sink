// print_export.cpp — save the last paper-dip print (sumi_read_print, §5.3)
// as a PNG on a background thread. A product feature (the settings window's
// "Save last print" button) and a lab-bench tool (--print-out, --dip-burst).
#include "print_export.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Save the last paper-dip print via sumi_read_print (SPEC 5.3) as PNG.
bool save_print_png(sumi_instance_t* inst, const char* path) {
    uint32_t w = 0, h = 0;
    if (!sumi_read_print(inst, nullptr, 0, &w, &h)) {
        std::fprintf(stderr, "[print] no print ready (dip first: key 9 or CC64)\n");
        return false;
    }
    const size_t bytes = (size_t)w * h * 4;
    uint8_t* pixels = (uint8_t*)std::malloc(bytes);
    if (!pixels || !sumi_read_print(inst, pixels, bytes, &w, &h)) {
        std::free(pixels);
        return false;
    }
    // Encode on a background thread: a 2560x1440 PNG takes ~1 s and must not
    // stall the render loop (step-7 DONE: no hitch at dip/export time).
    std::string path_copy(path);
    std::thread([pixels, w, h, path_copy]() {
        const int ok = stbi_write_png(path_copy.c_str(), (int)w, (int)h, 4, pixels, (int)w * 4);
        std::printf("[print] %s %ux%u -> %s\n", ok ? "saved" : "FAILED to save",
                    w, h, path_copy.c_str());
        std::fflush(stdout);
        std::free(pixels);
    }).detach();
    return true;
}

std::string default_print_path(const std::string& dir) {
    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&t));
    std::string base = dir.empty() ? std::string(".") : dir;
    return base + "/midi-sink-print-" + stamp + ".png";
}
