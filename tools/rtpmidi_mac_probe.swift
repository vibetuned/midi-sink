// rtpmidi_mac_probe.swift — step 67's app fixes (DECISIONS_9 #14): the Mac's own MIDI network session (Apple's
// rtpMIDI implementation, the strict one) connecting to the Tab's RTP-MIDI session by address — the evidence that
// the library's listener answers Audio MIDI Setup's invitation, its clock syncs, and carries MIDI both ways.
//
//   swift tools/rtpmidi_mac_probe.swift <tab-ip> <control-port> [seconds]
//
// Prints the session's connection state, every MIDI message received from the tablet (the play surface, the
// fingering demo), and sends a note on / off to it two seconds in (the tablet paints it as a device's note).
import Foundation
import CoreMIDI

let args = CommandLine.arguments
guard args.count >= 3, let port = Int(args[2]) else { print("usage: rtpmidi_mac_probe <ip> <port> [seconds]"); exit(2) }
let ip = args[1]
let seconds = args.count > 3 ? (Double(args[3]) ?? 10) : 10

// The MIDI client first: the network driver lives in the MIDI server, and the session is its — a process
// without a client has no session to enable. Then the run loop turns a few times for the driver to answer.
var client = MIDIClientRef()
MIDIClientCreateWithBlock("rtpmidi_mac_probe" as CFString, &client) { _ in }
let session = MIDINetworkSession.default()
session.isEnabled = true
session.connectionPolicy = .anyone
for _ in 0..<10 { RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.05)) }
print("local session: enabled \(session.isEnabled), name \"\(session.localName)\", port \(session.networkPort), policy anyone")
let host = MIDINetworkHost(name: "midi-sink on the Tab", address: ip, port: port)
let conn = MIDINetworkConnection(host: host)
print("connecting to \(ip):\(port) … \(session.addConnection(conn) ? "added" : "NOT added")")
var inPort = MIDIPortRef()
var received = 0
var byKind: [String: Int] = [:]
let t0 = Date()
MIDIInputPortCreateWithProtocol(client, "in" as CFString, ._1_0, &inPort) { evlist, _ in
    for packet in evlist.unsafeSequence() {
        for word in packet.words() {
            let type = (word >> 28) & 0xF
            if type != 2 { continue }   // MIDI 1.0 channel voice
            let status = UInt8((word >> 16) & 0xFF), d1 = UInt8((word >> 8) & 0xFF), d2 = UInt8(word & 0xFF)
            received += 1
            let kind: String
            switch status & 0xF0 {
            case 0x90: kind = d2 > 0 ? "note on" : "note off"
            case 0x80: kind = "note off"
            case 0xB0: kind = "CC \(d1)"
            case 0xD0: kind = "pressure"
            case 0xE0: kind = "bend"
            case 0xA0: kind = "poly pressure"
            default: kind = String(format: "%02X", status)
            }
            byKind[kind, default: 0] += 1
            if received <= 24 { print(String(format: "  rx %6.3f s  %02X %3d %3d  (%@, ch %d)", Date().timeIntervalSince(t0), status, d1, d2, kind, Int(status & 0x0F) + 1)) }
        }
    }
}
MIDIPortConnectSource(inPort, session.sourceEndpoint(), nil)
var outPort = MIDIPortRef()
MIDIOutputPortCreate(client, "out" as CFString, &outPort)

func send(_ bytes: [UInt8]) {
    var list = MIDIPacketList()
    var packet = MIDIPacketListInit(&list)
    packet = MIDIPacketListAdd(&list, 1024, packet, 0, bytes.count, bytes)
    MIDISend(outPort, session.destinationEndpoint(), &list)
}

var sent = false, stopped = false
var lastState = -1
let deadline = Date().addingTimeInterval(seconds)
while Date() < deadline {
    RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.1))
    let elapsed = Date().timeIntervalSince(t0)
    let n = session.connections().count
    if n != lastState { print(String(format: "  %6.3f s  session connections: %d", elapsed, n)); lastState = n }
    if !sent && elapsed >= 2 { send([0x91, 60, 100]); sent = true; print(String(format: "  %6.3f s  tx note on C4 (ch 2)", elapsed)) }
    if sent && !stopped && elapsed >= 3 { send([0x81, 60, 64]); stopped = true; print(String(format: "  %6.3f s  tx note off", elapsed)) }
}
print("received \(received) messages from the tablet: \(byKind.sorted { $0.key < $1.key }.map { "\($0.key) ×\($0.value)" }.joined(separator: ", "))")
session.removeConnection(conn)
exit(received > 0 ? 0 : 1)
