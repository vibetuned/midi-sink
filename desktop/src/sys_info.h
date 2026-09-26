// sys_info.h — the desktop shell's two system questions for Voxo (Phase 7
// step 52): how much memory is free (the advisory gate's advice) and where
// the bundled resources live (the demo instrument's slot).
#pragma once

#include <cstdint>
#include <string>

// Free physical memory right now (macOS: free + inactive pages; Linux:
// MemAvailable; Windows: available physical); 0 when unknown.
uint64_t sys_free_memory_bytes();
// The directory of bundled resources: <bundle>/Contents/Resources on macOS,
// the executable's directory elsewhere.
std::string app_resource_dir();
