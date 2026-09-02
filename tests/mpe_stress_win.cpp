// mpe_stress_win.cpp — Windows-native port of tests/osmose_stress.swift
// (CoreMIDI is macOS-only; step-11 handoff). Scripted Osmose-density MPE
// stress source: 10 voices (member channels 2..11) x 200 channel-pressure
// messages per second each (= 2000 press/s), plus per-voice glide bends and
// CC74, after an MCM configuring the lower zone.
//
// Windows has no built-in virtual MIDI loopback: create a port with loopMIDI
// (https://www.tobias-erichsen.de/software/loopmidi.html) first. This tool
// opens the WinMM OUTPUT side of it; midi-sink's libremidi/WinMM backend sees
// the same port as an input (the 1 Hz rescan opens it automatically).
//
// Build: part of the CMake tests on Windows.
// Run while midi-sink is open:  mpe_stress_win [seconds] [port-substring]
//   defaults: 30 seconds, first port whose name contains "loopMIDI".
// Expects: 60+ fps, `dropped MIDI messages: 0` at app exit.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

static HMIDIOUT g_out = nullptr;
static long g_sent = 0;

static void send(uint8_t status, uint8_t d1, uint8_t d2) {
    const DWORD msg = (DWORD)status | ((DWORD)d1 << 8) | ((DWORD)d2 << 16);
    midiOutShortMsg(g_out, msg);
    g_sent++;
}

static uint8_t clamp7(double v) {
    if (v < 0.0) return 0;
    if (v > 127.0) return 127;
    return (uint8_t)v;
}

int main(int argc, char** argv) {
    const double duration = argc > 1 ? atof(argv[1]) : 30.0;
    const char* port_match = argc > 2 ? argv[2] : "loopMIDI";

    const UINT n = midiOutGetNumDevs();
    int port = -1;
    for (UINT i = 0; i < n; i++) {
        MIDIOUTCAPSA caps = {};
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            printf("midi out %u: %s\n", i, caps.szPname);
            if (port < 0 && strstr(caps.szPname, port_match)) port = (int)i;
        }
    }
    if (port < 0) {
        fprintf(stderr,
                "no MIDI output matching \"%s\" found (%u ports).\n"
                "Create a virtual port with loopMIDI first.\n",
                port_match, n);
        return 1;
    }
    if (midiOutOpen(&g_out, (UINT)port, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        fprintf(stderr, "midiOutOpen(%d) failed\n", port);
        return 1;
    }
    printf("feeding port %d for %.0fs\n", port, duration);

    // 1 ms scheduler granularity for the 200 Hz loop (default is ~15.6 ms).
    timeBeginPeriod(1);

    using clock = std::chrono::steady_clock;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));   // let the 1 Hz rescan open us

    // MCM: lower zone, 15 member channels (RPN 6 on ch 1).
    send(0xB0, 101, 0);
    send(0xB0, 100, 6);
    send(0xB0, 6, 15);

    // 10 held voices on member channels 2..11 (indices 1..10), spread pitches.
    static const uint8_t notes[10] = {48, 55, 60, 64, 67, 72, 76, 79, 84, 91};
    for (int i = 0; i < 10; i++) {
        send((uint8_t)(0x90 | (1 + i)), notes[i], (uint8_t)(60 + i * 6));
    }

    // 200 Hz per voice: pressure sine (dense, Osmose-like), plus 20 Hz bend
    // and 10 Hz CC74 per voice — the exact schedule of osmose_stress.swift.
    long tick = 0;
    const auto start = clock::now();
    auto next = start;
    for (;;) {
        const double t = std::chrono::duration<double>(clock::now() - start).count();
        if (t >= duration) break;
        for (int v = 0; v < 10; v++) {
            const uint8_t ch = (uint8_t)(1 + v);
            const uint8_t press = clamp7(64.0 + 60.0 * sin(t * (1.1 + 0.13 * v) + v));
            send((uint8_t)(0xD0 | ch), press, 0);
            if (tick % 10 == v) {   // staggered 20 Hz bends
                const int bend = (int)(8192.0 + 2000.0 * sin(t * 0.7 + v * 0.9));
                send((uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)((bend >> 7) & 0x7F));
            }
            if (tick % 20 == 2 * v) {   // staggered 10 Hz CC74
                const uint8_t slide = clamp7(64.0 + 60.0 * sin(t * 0.5 + v));
                send((uint8_t)(0xB0 | ch), 74, slide);
            }
        }
        tick++;
        next += std::chrono::milliseconds(5);   // absolute pacing: no drift
        std::this_thread::sleep_until(next);
    }
    for (int i = 0; i < 10; i++) {
        send((uint8_t)(0x80 | (1 + i)), notes[i], 64);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    timeEndPeriod(1);
    midiOutClose(g_out);
    const double total = std::chrono::duration<double>(clock::now() - start).count();
    printf("mpe_stress_win: %ld messages in %.1fs\n", g_sent, total);
    return 0;
}
