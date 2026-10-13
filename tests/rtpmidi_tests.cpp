// rtpmidi_tests.cpp — step 67's app fixes (DECISIONS_9 #14): the RTP-MIDI session library's headless suite.
//   1. the codec: a batch of channel messages encoded into one packet and decoded back, byte for byte; the
//      two-byte length past fifteen bytes; a journal after the commands skipped; running status restored;
//      SysEx dropped; a short packet refused;
//   2. two endpoints over the loopback: B invites A (the role Audio MIDI Setup plays toward the Tab), both
//      ports answered, the clock sync completed, MIDI both ways in wire order, a packet from an unknown
//      sender refused, a 'BY' leaving no participant.
#include "sumi_rtpmidi.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf("ok   "); } else { std::printf("FAIL "); g_fail++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

struct Rx { std::vector<uint8_t> bytes; };
static void on_midi(void* user, uint8_t s, uint8_t d1, uint8_t d2) {
    Rx* r = (Rx*)user; r->bytes.push_back(s);
    const uint8_t hi = s & 0xF0;
    if (hi != 0xC0 && hi != 0xD0) { r->bytes.push_back(d1); r->bytes.push_back(d2); } else r->bytes.push_back(d1);
}

// --serve <seconds>: the library as a listener on this machine, for the Mac's own network session to invite
// (tools/rtpmidi_mac_probe.swift connects to 127.0.0.1 and the port printed) — Apple's rtpMIDI against the
// library without a tablet: prints every message received and sends a note once a second while a host is in.
static int serve(double seconds, uint16_t port) {
    sumi_rtpmidi_t* s = sumi_rtpmidi_create("midi-sink rtpmidi_tests", port, 0.0);
    if (!s) { std::printf("serve: no port pair\n"); return 1; }
    std::printf("serve: listening on control port %u (data %u) for %.0f s\n", sumi_rtpmidi_port(s), sumi_rtpmidi_port(s) + 1, seconds);
    std::fflush(stdout);
    Rx rx; double t = 0.0, last_note = 0.0; uint32_t last_n = 0; size_t printed = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (t < seconds) {
        t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        sumi_rtpmidi_poll(s, t, on_midi, &rx);
        const uint32_t n = sumi_rtpmidi_participants(s);
        if (n != last_n) { std::printf("  %6.3f s  participants: %u%s%s\n", t, n, n ? " — " : "", n ? sumi_rtpmidi_participant_name(s, 0) : ""); std::fflush(stdout); last_n = n; }
        while (printed < rx.bytes.size()) {
            const uint8_t st = rx.bytes[printed]; const size_t l = (st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0 ? 2 : 3;
            if (printed + l > rx.bytes.size()) break;
            std::printf("  %6.3f s  rx %02X %3d %3d\n", t, st, rx.bytes[printed + 1], l > 2 ? rx.bytes[printed + 2] : 0); std::fflush(stdout);
            printed += l;
        }
        if (n && t - last_note >= 1.0) { last_note = t; const uint8_t on[] = { 0x90, 64, 90 }; sumi_rtpmidi_send(s, t, on, 3); const uint8_t off[] = { 0x80, 64, 64 }; sumi_rtpmidi_send(s, t, off, 3); }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    sumi_rtpmidi_stats_t st; sumi_rtpmidi_stats(s, &st);
    std::printf("serve: done — joined %u, left %u, syncs %u, rx %llu packets / %llu messages, tx %llu / %llu, rejected %u\n",
                st.joined, st.left, st.syncs, (unsigned long long)st.packets_rx, (unsigned long long)st.midi_rx,
                (unsigned long long)st.packets_tx, (unsigned long long)st.midi_tx, st.rejected);
    sumi_rtpmidi_close(s, t); sumi_rtpmidi_destroy(s);
    return st.joined > 0 && st.midi_rx > 0 ? 0 : 1;
}

// --connect <ip> <port> <seconds>: the library as an initiator toward another implementation's listener (an
// independent peer such as pymidi, or a Mac's enabled network session): prints the handshake, sends a note
// once a second, prints every message received.
static int connect_to(const char* ip, uint16_t port, double seconds) {
    sumi_rtpmidi_t* s = sumi_rtpmidi_create("midi-sink rtpmidi_tests", 0, 0.0);
    if (!s) { std::printf("connect: no port pair\n"); return 1; }
    std::printf("connect: from control port %u to %s:%u for %.0f s\n", sumi_rtpmidi_port(s), ip, port, seconds); std::fflush(stdout);
    if (!sumi_rtpmidi_connect(s, ip, port, 0.0)) { std::printf("connect: the invitation could not be sent\n"); return 1; }
    Rx rx; double t = 0.0, last_note = 0.0; uint32_t last_n = 0; size_t printed = 0; uint8_t note = 60;
    const auto t0 = std::chrono::steady_clock::now();
    while (t < seconds) {
        t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        sumi_rtpmidi_poll(s, t, on_midi, &rx);
        const uint32_t n = sumi_rtpmidi_participants(s);
        if (n != last_n) { std::printf("  %6.3f s  participants: %u%s%s\n", t, n, n ? " — " : "", n ? sumi_rtpmidi_participant_name(s, 0) : ""); std::fflush(stdout); last_n = n; }
        while (printed < rx.bytes.size()) {
            const uint8_t st = rx.bytes[printed]; const size_t l = (st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0 ? 2 : 3;
            if (printed + l > rx.bytes.size()) break;
            std::printf("  %6.3f s  rx %02X %3d %3d\n", t, st, rx.bytes[printed + 1], l > 2 ? rx.bytes[printed + 2] : 0); std::fflush(stdout);
            printed += l;
        }
        if (n && t - last_note >= 1.0) {
            last_note = t; const uint8_t on[] = { 0x90, note, 90 }; sumi_rtpmidi_send(s, t, on, 3); const uint8_t off[] = { 0x80, note, 64 }; sumi_rtpmidi_send(s, t, off, 3);
            std::printf("  %6.3f s  tx note %u on / off\n", t, note); std::fflush(stdout); note = (uint8_t)(note + 2);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    sumi_rtpmidi_stats_t st; sumi_rtpmidi_stats(s, &st);
    std::printf("connect: done — joined %u, left %u, syncs %u, rx %llu packets / %llu messages, tx %llu / %llu, rejected %u\n",
                st.joined, st.left, st.syncs, (unsigned long long)st.packets_rx, (unsigned long long)st.midi_rx,
                (unsigned long long)st.packets_tx, (unsigned long long)st.midi_tx, st.rejected);
    sumi_rtpmidi_close(s, t); sumi_rtpmidi_destroy(s);
    return st.joined > 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc >= 5 && std::strcmp(argv[1], "--connect") == 0) return connect_to(argv[2], (uint16_t)std::atoi(argv[3]), std::atof(argv[4]));
    if (argc >= 3 && std::strcmp(argv[1], "--serve") == 0) return serve(std::atof(argv[2]), argc >= 4 ? (uint16_t)std::atoi(argv[3]) : 5010);   // 5010: clear of the Mac's own session on 5004
    std::printf("[rtpmidi] the codec\n");
    {
        const uint8_t midi[] = { 0x90, 0x3C, 0x64, 0xB0, 0x01, 0x40, 0xD0, 0x50, 0xE0, 0x00, 0x40 };
        uint8_t pkt[256], back[64]; uint16_t seq; uint32_t ts, ssrc;
        const size_t n = sumi_rtpmidi_encode(midi, sizeof midi, 0x1234, 0xABCDEF01u, 0xDEADBEEFu, pkt, sizeof pkt);
        CHECK(n == 12 + 1 + sizeof midi + 3, "four messages encode into one packet: %zu bytes (12 header, 1 flags, 11 MIDI, 3 deltas)", n);
        CHECK(pkt[0] == 0x80 && pkt[1] == 0x61 && pkt[12] == 14, "RTP v2, payload type 97, a one-byte command section length of 14");
        const size_t m = sumi_rtpmidi_decode(pkt, n, back, sizeof back, &seq, &ts, &ssrc);
        CHECK(m == sizeof midi && std::memcmp(back, midi, m) == 0 && seq == 0x1234 && ts == 0xABCDEF01u && ssrc == 0xDEADBEEFu,
              "decoded back byte for byte (%zu bytes), the sequence, the timestamp and the SSRC with them", m);
        // sixteen note-ons: the command section passes fifteen bytes — the B flag and a two-byte length
        std::vector<uint8_t> many; for (int i = 0; i < 16; i++) { many.push_back(0x91); many.push_back((uint8_t)(40 + i)); many.push_back(100); }
        const size_t n2 = sumi_rtpmidi_encode(many.data(), many.size(), 1, 2, 3, pkt, sizeof pkt);
        uint8_t back2[128]; const size_t m2 = sumi_rtpmidi_decode(pkt, n2, back2, sizeof back2, nullptr, nullptr, nullptr);
        CHECK(n2 == 12 + 2 + many.size() + 15 && (pkt[12] & 0x80) && m2 == many.size() && std::memcmp(back2, many.data(), m2) == 0,
              "sixteen messages: the two-byte length (B set), %zu bytes, decoded back whole", n2);
        // a packet with a journal after the commands (J set) and running status inside: the commands alone come back
        const uint8_t hand[] = { 0x80, 0x61, 0x00, 0x01, 0, 0, 0, 2, 0, 0, 0, 3,
                                 0x40 | 7,                    // J = 1, length 7
                                 0x90, 0x40, 0x7F, 0x00, 0x41, 0x7F,   // note on, delta 0, running status note on
                                 0x00,                        // (a seventh byte: a delta the list ends on)
                                 0xAA, 0xBB, 0xCC, 0xDD };    // the "journal"
        uint8_t back3[32]; const size_t m3 = sumi_rtpmidi_decode(hand, sizeof hand, back3, sizeof back3, nullptr, nullptr, nullptr);
        const uint8_t want3[] = { 0x90, 0x40, 0x7F, 0x90, 0x41, 0x7F };
        CHECK(m3 == sizeof want3 && std::memcmp(back3, want3, m3) == 0, "running status restored and the journal skipped: %zu bytes", m3);
        const uint8_t sysex[] = { 0x80, 0x61, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0xF0, 0x7E, 0x09, 0x01, 0xF7, 0x00, 0xB0, 0x07, 0x64, 0x00 };
        uint8_t back4[16]; const size_t m4 = sumi_rtpmidi_decode(sysex, sizeof sysex, back4, sizeof back4, nullptr, nullptr, nullptr);
        CHECK(m4 == 3 && back4[0] == 0xB0 && back4[1] == 0x07, "SysEx dropped, the CC after it kept (%zu bytes)", m4);
        CHECK(sumi_rtpmidi_decode(hand, 10, back3, sizeof back3, nullptr, nullptr, nullptr) == 0, "a packet shorter than its header is refused");
        const uint8_t bad[] = { 0x90, 0x3C };   // a truncated message does not encode
        CHECK(sumi_rtpmidi_encode(bad, sizeof bad, 0, 0, 0, pkt, sizeof pkt) == 0, "a truncated message does not encode");
    }

    std::printf("[rtpmidi] two endpoints over the loopback\n");
    {
        double now = 100.0;
        sumi_rtpmidi_t* A = sumi_rtpmidi_create("A the listener", 0, now);
        sumi_rtpmidi_t* B = sumi_rtpmidi_create("B the initiator", 0, now);
        CHECK(A && B && sumi_rtpmidi_port(A) && sumi_rtpmidi_port(B) && sumi_rtpmidi_port(A) != sumi_rtpmidi_port(B),
              "two sessions bound their control and data ports: %u and %u", A ? sumi_rtpmidi_port(A) : 0, B ? sumi_rtpmidi_port(B) : 0);
        if (!A || !B) return 1;
        Rx rxA, rxB;
        CHECK(sumi_rtpmidi_connect(B, "127.0.0.1", sumi_rtpmidi_port(A), now), "B invites A on its control port");
        // the pump: a millisecond of real time between polls — a loopback datagram is not synchronous with its sendto
        auto pump = [&](int iterations) {
            for (int i = 0; i < iterations; i++) {
                now += 0.005; sumi_rtpmidi_poll(A, now, on_midi, &rxA); sumi_rtpmidi_poll(B, now, on_midi, &rxB);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        pump(60);
        sumi_rtpmidi_stats_t sa, sb; sumi_rtpmidi_stats(A, &sa); sumi_rtpmidi_stats(B, &sb);
        CHECK(sumi_rtpmidi_participants(A) == 1 && sumi_rtpmidi_participants(B) == 1, "both ports invited and answered: one participant each (A %u, B %u)",
              sumi_rtpmidi_participants(A), sumi_rtpmidi_participants(B));
        const char* an = sumi_rtpmidi_participant_name(A, 0); const char* bn = sumi_rtpmidi_participant_name(B, 0);
        CHECK(an && bn && std::strcmp(an, "B the initiator") == 0 && std::strcmp(bn, "A the listener") == 0, "each knows the other's name: A sees '%s', B sees '%s'", an ? an : "?", bn ? bn : "?");
        CHECK(sb.syncs >= 1 && sa.syncs >= 1, "the clock sync completed on both sides (B %u, A %u)", sb.syncs, sa.syncs);
        const uint8_t play[] = { 0x90, 0x3C, 0x64, 0xD0, 0x40, 0xB0, 0x4A, 0x20, 0x80, 0x3C, 0x40 };
        CHECK(sumi_rtpmidi_send(B, now, play, sizeof play), "B sends a batch of four messages");
        pump(20);
        CHECK(rxA.bytes.size() == sizeof play && std::memcmp(rxA.bytes.data(), play, sizeof play) == 0, "A received them in wire order (%zu bytes)", rxA.bytes.size());
        const uint8_t host[] = { 0x91, 0x40, 0x50, 0xE1, 0x00, 0x60 };
        CHECK(sumi_rtpmidi_send(A, now, host, sizeof host), "A sends two messages back");
        pump(20);
        CHECK(rxB.bytes.size() == sizeof host && std::memcmp(rxB.bytes.data(), host, sizeof host) == 0, "B received them (%zu bytes)", rxB.bytes.size());
        sumi_rtpmidi_stats(A, &sa); sumi_rtpmidi_stats(B, &sb);
        CHECK(sa.packets_rx == 1 && sa.midi_rx == 4 && sb.packets_tx == 1 && sb.midi_tx == 4 && sb.packets_rx == 1 && sb.midi_rx == 2,
              "the counters: A rx 1 packet / 4 messages, B tx 1 / 4, B rx 1 / 2");
        const size_t before = rxA.bytes.size();
        sumi_rtpmidi_close(B, now);
        pump(20);
        sumi_rtpmidi_stats(A, &sa);
        CHECK(sumi_rtpmidi_participants(A) == 0 && sa.left == 1 && rxA.bytes.size() == before, "B's 'BY' leaves A with no participant (left %u)", sa.left);
        CHECK(!sumi_rtpmidi_send(A, now, host, sizeof host), "a send with no participant sends nothing");
        sumi_rtpmidi_destroy(A); sumi_rtpmidi_destroy(B);
    }

    std::printf("[rtpmidi] %s (%d failures)\n", g_fail ? "FAILED" : "all green", g_fail);
    return g_fail ? 1 : 0;
}
