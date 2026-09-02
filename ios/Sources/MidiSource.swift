// CoreMIDI → sumi_push_midi (§5.2: bytes cross the ABI; exactly one producer
// thread — CoreMIDI's receive thread). All sources (wired, network, Bluetooth
// once paired) are connected automatically; unlike libremidi on macOS
// (DECISIONS #25), raw CoreMIDI setup notifications DO fire, so hotplug is
// notification-driven here rather than polled.
import CoreMIDI

final class MidiSource {
    private var client = MIDIClientRef()
    private var inPort = MIDIPortRef()
    private var connected = Set<MIDIUniqueID>()
    private let push: (UInt8, UInt8, UInt8) -> Void

    init(push: @escaping (UInt8, UInt8, UInt8) -> Void) {
        self.push = push
        MIDIClientCreateWithBlock("midi-sink" as CFString, &client) { [weak self] note in
            if note.pointee.messageID == .msgSetupChanged {
                DispatchQueue.main.async { self?.connectAllSources() }
            }
        }
        // MIDI 1.0 protocol: UMP words arrive as MIDI1UP (message type 2) —
        // one complete status/data1/data2 message per word, no running status.
        MIDIInputPortCreateWithProtocol(client, "in" as CFString, ._1_0, &inPort) {
            [weak self] eventList, _ in
            guard let self else { return }
            for packet in eventList.unsafeSequence() {
                self.handle(packet: packet)
            }
        }
        connectAllSources()
    }

    func stop() {
        if client != 0 { MIDIClientDispose(client) }   // disposes the port too
        client = 0
        inPort = 0
    }

    private func connectAllSources() {
        guard inPort != 0 else { return }
        for i in 0..<MIDIGetNumberOfSources() {
            let src = MIDIGetSource(i)
            var uid: MIDIUniqueID = 0
            MIDIObjectGetIntegerProperty(src, kMIDIPropertyUniqueID, &uid)
            if !connected.contains(uid),
               MIDIPortConnectSource(inPort, src, nil) == noErr {
                connected.insert(uid)
                var name: Unmanaged<CFString>?
                MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &name)
                NSLog("[midi] input opened: %@",
                      (name?.takeRetainedValue() as String?) ?? "(unnamed)")
            }
        }
    }

    private func handle(packet: UnsafePointer<MIDIEventPacket>) {
        let count = min(Int(packet.pointee.wordCount), 64)
        withUnsafePointer(to: packet.pointee.words) { tuple in
            tuple.withMemoryRebound(to: UInt32.self, capacity: 64) { words in
                for i in 0..<count {
                    let w = words[i]
                    // UMP MIDI1UP word: [mt:4][group:4][status:8][d1:8][d2:8]
                    if (w >> 28) & 0xF == 0x2 {
                        push(UInt8((w >> 16) & 0xFF),
                             UInt8((w >> 8) & 0x7F),
                             UInt8(w & 0x7F))
                    }
                }
            }
        }
    }
}
