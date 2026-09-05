// print_export.h — PNG export of the last paper-dip print.
#pragma once

#include "sumi_core.h"
#include <string>

// Encodes on a background thread (a 2560x1440 PNG takes ~1 s and must not
// stall the render loop). Returns false if no print is ready.
bool        save_print_png(sumi_instance_t* inst, const char* path);
// <dir>/midi-sink-print-<timestamp>.png
std::string default_print_path(const std::string& dir);
