// print_export.h — PNG export of the last paper-dip print.
#pragma once

#include "sumi_core.h"
#include <string>

#include <cstdint>
#include <vector>

// Encodes on a background thread (a 2560x1440 PNG takes ~1 s and must not
// stall the render loop). Returns false if no print is ready. The write's
// outcome arrives through print_write_poll.
bool        save_print_png(sumi_instance_t* inst, const char* path);
// DECISIONS_5 #86: THE ONE BACKGROUND PNG WRITER, and its outcome for the UI.
// Takes the pixels (RGBA8, w x h; freed when done), encodes on its own
// thread, and queues the result: ok, and one line for the status row —
// "Saved 4096x2304 -> <path>", or "Could not write <path>: <why>" (the
// folder does not exist; the C runtime's reason). `what` names the write in
// the stdout log ("export", "last print"). print_write_poll hands the next
// finished write to the caller (render thread; consumes; false = none).
void        print_write_async(std::vector<uint8_t>* px, uint32_t w, uint32_t h, const std::string& path, const char* what);
bool        print_write_poll(bool* out_ok, std::string* out_message);
// <dir>/midi-sink-print-<timestamp>.png
std::string default_print_path(const std::string& dir);
