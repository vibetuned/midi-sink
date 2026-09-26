// sys_info.cpp — see sys_info.h.
#include "sys_info.h"

#include <cstdio>
#include <cstring>
#include <string>

#if defined(__APPLE__)
  #include <mach/mach.h>
  #include <mach-o/dyld.h>
  #include <unistd.h>
#elif defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <unistd.h>
#endif

uint64_t sys_free_memory_bytes() {
#if defined(__APPLE__)
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS) return 0;
    vm_size_t page = 0;
    host_page_size(mach_host_self(), &page);
    return ((uint64_t)vm.free_count + (uint64_t)vm.inactive_count) * (uint64_t)page;
#elif defined(_WIN32)
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    return GlobalMemoryStatusEx(&ms) ? (uint64_t)ms.ullAvailPhys : 0;
#else
    FILE* f = std::fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256]; uint64_t kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
        unsigned long long v = 0;
        if (std::sscanf(line, "MemAvailable: %llu kB", &v) == 1) { kb = v; break; }
    }
    std::fclose(f);
    return kb * 1024ull;
#endif
}

std::string app_resource_dir() {
    std::string exe;
#if defined(__APPLE__)
    char buf[4096]; uint32_t n = sizeof(buf);
    if (_NSGetExecutablePath(buf, &n) == 0) exe = buf;
#elif defined(_WIN32)
    wchar_t wbuf[4096];
    const DWORD n = GetModuleFileNameW(nullptr, wbuf, 4096);
    if (n > 0 && n < 4096) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)n, nullptr, 0, nullptr, nullptr);
        exe.resize((size_t)len);
        WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)n, exe.data(), len, nullptr, nullptr);
    }
#else
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) exe.assign(buf, (size_t)n);
#endif
    const size_t slash = exe.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? std::string(".") : exe.substr(0, slash);
#if defined(__APPLE__)
    // <bundle>/Contents/MacOS/midi-sink -> <bundle>/Contents/Resources; a bare
    // binary (the build tree's harness before bundling) keeps its own folder.
    const size_t macos = dir.rfind("/Contents/MacOS");
    if (macos != std::string::npos && macos + 15 == dir.size()) return dir.substr(0, macos) + "/Contents/Resources";
#endif
#if !defined(_WIN32)
    // An installed binary (<prefix>/bin/midi-sink) keeps its resources under
    // <prefix>/share/midi-sink; the build tree keeps them beside the binary.
    FILE* beside = std::fopen((dir + "/demo/demo.dspreset").c_str(), "rb");
    if (beside) { std::fclose(beside); return dir; }
    const size_t bin = dir.rfind("/bin");
    if (bin != std::string::npos && bin + 4 == dir.size()) return dir.substr(0, bin) + "/share/midi-sink";
#endif
    return dir;
}
