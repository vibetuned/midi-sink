/* sumi_rtpmidi.h — RTP-MIDI (RFC 6295) with Apple's session protocol (the
 * "network session" of macOS, iOS and Windows's rtpMIDI), as a host library
 * for the shells whose OS has none — the Tab first (Phase 10 step 67's app
 * fixes, DECISIONS_9 #14). Pure C surface, POSIX UDP sockets underneath, no
 * threads of its own: the owner polls it from the thread that owns its MIDI
 * (the Tab's MIDI thread), and every received MIDI message reaches the owner
 * through the poll's callback on that thread.
 *
 * The session is both a LISTENER — advertised by the shell through mDNS as
 * `_apple-midi._udp` on the control port, invited by Audio MIDI Setup or
 * rtpMIDI, answering the invitation on the control port and again on the
 * data port, answering the host's clock syncs — and an INITIATOR
 * (sumi_rtpmidi_connect), which the headless test uses to run two endpoints
 * over the loopback. Up to eight participants; a ninth invitation is
 * refused ('NO'). MIDI goes out as one RTP-MIDI packet per batch, no
 * recovery journal (Apple's receiver takes journal-less packets; a lost
 * UDP packet is a lost batch, as it is for every rtpMIDI sender without a
 * journal), delta times zero within a batch; received packets' command
 * sections are parsed with their delta times and running status, a journal
 * after the commands skipped, SysEx dropped (the engine ignores it).
 * Timestamps are the session's own 100 µs clock from the owner's `now_s`.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sumi_rtpmidi_t sumi_rtpmidi_t;

#define SUMI_RTPMIDI_MAX_PARTICIPANTS 8
#define SUMI_RTPMIDI_NAME_MAX 64

typedef void (*sumi_rtpmidi_midi_fn)(void* user, uint8_t status, uint8_t d1, uint8_t d2);

typedef struct sumi_rtpmidi_stats_t {
    uint32_t participants;   /* sessions established now (invited on both ports) */
    uint32_t joined, left;   /* since create */
    uint32_t syncs;          /* clock-sync exchanges completed (as initiator: CK1 answered; as responder: CK2 received) */
    uint64_t packets_rx, packets_tx;
    uint64_t midi_rx, midi_tx;
    uint32_t rejected;       /* invitations refused, packets from unknown senders, malformed packets */
} sumi_rtpmidi_stats_t;

/* Binds the control port and the data port (control + 1): `control_port` 0 tries 5004, then the even ports up
   to 5100, then an OS-chosen pair. `name` is the session's name on the wire (the invitation's reply), at most
   SUMI_RTPMIDI_NAME_MAX − 1 bytes. NULL when no pair could be bound. */
sumi_rtpmidi_t* sumi_rtpmidi_create(const char* name, uint16_t control_port, double now_s);
void            sumi_rtpmidi_destroy(sumi_rtpmidi_t* s);
uint16_t        sumi_rtpmidi_port(const sumi_rtpmidi_t* s);   /* the control port bound (the mDNS advertisement's) */

/* Invite a listener at an IPv4 address (dotted) and its control port — the initiator role. The invitation is
   repeated by poll() every second, ten times at most; a refusal or silence leaves no participant. */
bool            sumi_rtpmidi_connect(sumi_rtpmidi_t* s, const char* ipv4, uint16_t control_port, double now_s);

/* Non-blocking: every datagram waiting on either socket — invitations answered, clock syncs answered or
   completed, MIDI handed to `on_midi` in wire order — then the housekeeping (the initiator's repeats and its
   periodic syncs, silent participants dropped after 60 s). Call it every millisecond or so. */
void            sumi_rtpmidi_poll(sumi_rtpmidi_t* s, double now_s, sumi_rtpmidi_midi_fn on_midi, void* user);

/* One RTP-MIDI packet carrying `bytes` — MIDI 1.0 channel messages back to back, each with its status — to
   every established participant. false when there is none (nothing was sent) or the batch is empty. */
bool            sumi_rtpmidi_send(sumi_rtpmidi_t* s, double now_s, const uint8_t* bytes, size_t n);

uint32_t        sumi_rtpmidi_participants(const sumi_rtpmidi_t* s);
const char*     sumi_rtpmidi_participant_name(const sumi_rtpmidi_t* s, uint32_t index);   /* NULL past the end */
void            sumi_rtpmidi_close(sumi_rtpmidi_t* s, double now_s);   /* 'BY' to every participant; the table cleared */
void            sumi_rtpmidi_stats(const sumi_rtpmidi_t* s, sumi_rtpmidi_stats_t* out);

/* The codec alone, for the tests and any host: an RTP-MIDI packet (the 12-byte RTP header, payload type 97, then
   the MIDI command section) from a flat stream of MIDI 1.0 messages, and back. encode returns the packet's length
   (0 when `out` is too small or a message is malformed); decode returns the MIDI bytes written, statuses restored
   where the sender used running status, SysEx dropped, the journal (if any) skipped — 0 on a malformed packet. */
size_t          sumi_rtpmidi_encode(const uint8_t* midi, size_t n, uint16_t seq, uint32_t timestamp, uint32_t ssrc,
                                    uint8_t* out, size_t cap);
size_t          sumi_rtpmidi_decode(const uint8_t* packet, size_t n, uint8_t* midi_out, size_t cap,
                                    uint16_t* seq, uint32_t* timestamp, uint32_t* ssrc);

#ifdef __cplusplus
}
#endif
