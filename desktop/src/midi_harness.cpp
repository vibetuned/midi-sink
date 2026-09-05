// midi_harness.cpp — libremidi observer: hotplug, open all inputs, forward
// bytes (PROJECT_SPEC.md §1, §6). §5.2 requires exactly ONE producer thread
// for sumi_push_midi; multiple devices/backends may call back from different
// threads, so the harness serializes pushes with a mutex (host-side only —
// the core stays lock-free).
#include "midi_harness.h"

#include <libremidi/libremidi.hpp>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace {

// One clock for the rescan cadence and its age readout.
double harness_now() {
#if defined(__APPLE__)
    return CFAbsoluteTimeGetCurrent();
#else
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

struct MidiHarness {
    sumi_instance_t* inst = nullptr;
    bool log_raw = false;                       // SUMI_MIDI_LOG=1: dump every message
    std::mutex push_mutex;                      // §5.2 producer serialization
    std::unique_ptr<libremidi::observer> observer;
    struct OpenInput {
        libremidi::input_port port;
        std::unique_ptr<libremidi::midi_in> in;
    };
    std::vector<OpenInput> inputs;
    std::mutex inputs_mutex;                    // hotplug callbacks vs teardown
    double last_rescan = 0.0;
    bool rescan_requested = false;              // settings window: "Rescan now"

    void forward(const libremidi::message& msg) {
        const size_t n = msg.size();
        if (n == 0 || n > 3) return;            // SysEx / oversized: out of scope v1
        const uint8_t status = msg[0];
        if (status >= 0xF0) return;             // system messages: skip at the source
        const uint8_t d1 = n > 1 ? msg[1] : 0;
        const uint8_t d2 = n > 2 ? msg[2] : 0;
        if (log_raw) {
            static const char* kinds[] = {"noteoff", "noteon", "polyAT", "cc",
                                          "prog", "chanAT", "bend", "sys"};
            std::fprintf(stderr, "[midi raw] ch%-2u %-7s %3u %3u  (0x%02X)\n",
                         (status & 0x0F) + 1, kinds[(status >> 4) & 0x07], d1, d2, status);
        }
        std::lock_guard<std::mutex> lock(push_mutex);
        sumi_push_midi(inst, status, d1, d2);
    }

    // Rescan-based hotplug: libremidi's CoreMIDI notification client proved
    // unreliable here (its input_added never fires even though raw CoreMIDI
    // notifications reach this process — see DECISIONS.md). Enumerate
    // periodically from the main loop instead: open what's new, prune what's
    // gone. Also inherently correct on the phase-2 backends.
    void rescan() {
        const auto ports = observer->get_input_ports();
        for (const auto& p : ports) open_input(p);
        std::lock_guard<std::mutex> lock(inputs_mutex);
        for (auto it = inputs.begin(); it != inputs.end();) {
            bool still_here = false;
            for (const auto& p : ports) {
                if (p.port == it->port.port) { still_here = true; break; }
            }
            if (!still_here) {
                std::fprintf(stderr, "[midi] input removed: %s\n",
                             it->port.display_name.c_str());
                it = inputs.erase(it);
            } else {
                ++it;
            }
        }
    }

    void open_input(const libremidi::input_port& port) {
        std::lock_guard<std::mutex> lock(inputs_mutex);
        for (const auto& o : inputs) {
            if (o.port.port == port.port) return;   // already open (port_handle id)
        }
        auto in = std::make_unique<libremidi::midi_in>(libremidi::input_configuration{
            .on_message = [this](const libremidi::message& msg) { forward(msg); }});
        if (auto err = in->open_port(port); err != stdx::error{}) {
            std::fprintf(stderr, "[midi] failed to open input: %s\n",
                         port.display_name.c_str());
            return;
        }
        std::fprintf(stderr, "[midi] input opened: %s\n", port.display_name.c_str());
        inputs.push_back({port, std::move(in)});
    }

    void close_input(const libremidi::input_port& port) {
        std::lock_guard<std::mutex> lock(inputs_mutex);
        for (auto it = inputs.begin(); it != inputs.end(); ++it) {
            if (it->port.port == port.port) {
                std::fprintf(stderr, "[midi] input removed: %s\n",
                             port.display_name.c_str());
                inputs.erase(it);
                return;
            }
        }
    }
};

} // namespace

void* sumi_midi_harness_start(sumi_instance_t* inst) {
    if (!inst) return nullptr;
    auto* h = new MidiHarness();
    h->inst = inst;
    const char* log_env = getenv("SUMI_MIDI_LOG");
    h->log_raw = log_env && log_env[0] == '1';

    libremidi::observer_configuration conf{
        .input_added = [h](const libremidi::input_port& p) { h->open_input(p); },
        .input_removed = [h](const libremidi::input_port& p) { h->close_input(p); },
    };
    // libremidi tracks only hardware endpoints by default; virtual sources
    // (DAWs, IAC, test synths) must be opted into explicitly.
    conf.track_hardware = true;
    conf.track_virtual = true;
    h->observer = std::make_unique<libremidi::observer>(conf);

    for (const auto& port : h->observer->get_input_ports()) {
        h->open_input(port);
    }
    return h;
}

void sumi_midi_harness_inject(void* harness, uint8_t status, uint8_t d1, uint8_t d2) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h || !h->inst || status >= 0xF0 || status < 0x80) return;
    std::lock_guard<std::mutex> lock(h->push_mutex);   // §5.2: the ONE producer
    sumi_push_midi(h->inst, status, d1, d2);
}

void sumi_midi_harness_poll(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h) return;
#if defined(__APPLE__)
    // Service the thread's run loop (some CoreMIDI plumbing depends on it).
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
#endif
    // WinMM/ALSA need no run-loop servicing; the 1 Hz rescan (DECISIONS.md
    // #25 — hotplug callbacks are unreliable, polling is the portable fix)
    // just needs a monotonic clock.
    const double now = harness_now();
    if (h->rescan_requested || now - h->last_rescan >= 1.0) {
        h->rescan_requested = false;
        h->last_rescan = now;
        h->rescan();
    }
}

int sumi_midi_harness_input_count(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h) return 0;
    std::lock_guard<std::mutex> lock(h->inputs_mutex);
    return (int)h->inputs.size();
}

bool sumi_midi_harness_input_name(void* harness, int index, char* buf, size_t cap) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h || !buf || cap == 0) return false;
    std::lock_guard<std::mutex> lock(h->inputs_mutex);
    if (index < 0 || index >= (int)h->inputs.size()) return false;
    std::snprintf(buf, cap, "%s", h->inputs[(size_t)index].port.display_name.c_str());
    return true;
}

double sumi_midi_harness_seconds_since_rescan(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h || h->last_rescan <= 0.0) return 0.0;
    return harness_now() - h->last_rescan;
}

void sumi_midi_harness_rescan_now(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (h) h->rescan_requested = true;   // honoured by the next poll (main thread)
}

void sumi_midi_harness_set_raw_log(void* harness, bool on) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (h) h->log_raw = on;
}

bool sumi_midi_harness_raw_log(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    return h && h->log_raw;
}

void sumi_midi_harness_stop(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h) return;
    h->observer.reset();   // stop hotplug callbacks first
    {
        std::lock_guard<std::mutex> lock(h->inputs_mutex);
        h->inputs.clear();
    }
    delete h;
}
