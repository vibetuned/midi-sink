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

#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>

namespace {

struct MidiHarness {
    sumi_instance_t* inst = nullptr;
    std::mutex push_mutex;                      // §5.2 producer serialization
    std::unique_ptr<libremidi::observer> observer;
    struct OpenInput {
        libremidi::input_port port;
        std::unique_ptr<libremidi::midi_in> in;
    };
    std::vector<OpenInput> inputs;
    std::mutex inputs_mutex;                    // hotplug callbacks vs teardown
    double last_rescan = 0.0;

    void forward(const libremidi::message& msg) {
        const size_t n = msg.size();
        if (n == 0 || n > 3) return;            // SysEx / oversized: out of scope v1
        const uint8_t status = msg[0];
        if (status >= 0xF0) return;             // system messages: skip at the source
        const uint8_t d1 = n > 1 ? msg[1] : 0;
        const uint8_t d2 = n > 2 ? msg[2] : 0;
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

void sumi_midi_harness_poll(void* harness) {
    auto* h = static_cast<MidiHarness*>(harness);
    if (!h) return;
#if defined(__APPLE__)
    // Service the thread's run loop (some CoreMIDI plumbing depends on it).
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    const double now = CFAbsoluteTimeGetCurrent();
#else
    const double now = 0.0;
#endif
    if (now - h->last_rescan >= 1.0) {
        h->last_rescan = now;
        h->rescan();
    }
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
