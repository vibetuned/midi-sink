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
    /// Fired when a previously-connected source disappears (Phase 4: the
    /// shell clears hostmpe's external-occupancy mask on device disconnect).
    var onSourcesRemoved: (() -> Void)?
    /// Our own outbound virtual sources (Step 17): NEVER connect to these —
    /// the outbound stream would feed back into observe_external and the
    /// occupancy mask would starve our own allocator.
    var excludedUniqueIDs = Set<MIDIUniqueID>()

    // Step 33 (#55): what the settings' "MIDI inputs" list shows — the same
    // three facts the desktop harness exposes (port names, rescan age, a live
    // message count), so "the USB keyboard does nothing" splits into "CoreMIDI
    // never listed it" / "listed, no bytes" / "bytes arrive, the canvas ignores
    // them" in one glance. Names and counters are written on the CoreMIDI
    // thread and read on main: one lock.
    struct Snapshot {
        var inputs: [String] = []
        var words: UInt64 = 0        // every UMP word received, any type
        var forwarded: UInt64 = 0    // MIDI 1.0 channel/system messages pushed
        var last: String = ""        // last forwarded message, hex
        var sourcesSeen: Int = 0     // MIDIGetNumberOfSources at the last scan
    }
    private let lock = NSLock()
    private var snap = Snapshot()
    private var names: [MIDIUniqueID: String] = [:]
    private var rescan: Timer?

    func snapshot() -> Snapshot { lock.lock(); defer { lock.unlock() }; return snap }
    func rescanNow() { connectAllSources() }

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
        // Belt and braces (#55): CoreMIDI delivers setup notifications on the
        // run loop that was current at the process's FIRST MIDIClientCreate;
        // if that ever happens off the main run loop they never arrive and a
        // USB device plugged in after launch stays unconnected. The desktop
        // harness polls at 1 Hz for the same reason (DECISIONS #25) — so do we.
        // MIDIGetNumberOfSources + a uid lookup per source is microseconds.
        rescan = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.connectAllSources()
        }
    }

    func stop() {
        rescan?.invalidate(); rescan = nil
        if client != 0 { MIDIClientDispose(client) }   // disposes the port too
        client = 0
        inPort = 0
    }

    private func connectAllSources() {
        guard inPort != 0 else { return }
        var present = Set<MIDIUniqueID>()
        for i in 0..<MIDIGetNumberOfSources() {
            let src = MIDIGetSource(i)
            var uid: MIDIUniqueID = 0
            MIDIObjectGetIntegerProperty(src, kMIDIPropertyUniqueID, &uid)
            if excludedUniqueIDs.contains(uid) { continue }
            present.insert(uid)
            if !connected.contains(uid) {
                var name: Unmanaged<CFString>?
                MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &name)
                var owner: Unmanaged<CFString>?
                MIDIObjectGetStringProperty(src, kMIDIPropertyDriverOwner, &owner)
                let n = (name?.takeRetainedValue() as String?) ?? "(unnamed)"
                let o = (owner?.takeRetainedValue() as String?) ?? "?"
                let st = MIDIPortConnectSource(inPort, src, nil)
                // Every status is logged (the MidiOutputs rule): a silent
                // connect failure looks exactly like a dead keyboard.
                NSLog("[midi] input %@: %@ (driver %@, uid %d)",
                      st == noErr ? "opened" : "connect FAILED \(st)", n, o, uid)
                if st == noErr {
                    connected.insert(uid)
                    names[uid] = n
                }
            }
        }
        let removed = connected.subtracting(present)
        if !removed.isEmpty {
            connected.subtract(removed)
            for uid in removed { names.removeValue(forKey: uid) }
            NSLog("[midi] %d input(s) removed", removed.count)
            onSourcesRemoved?()
        }
        lock.lock()
        snap.inputs = connected.sorted().compactMap { names[$0] }
        snap.sourcesSeen = MIDIGetNumberOfSources()
        lock.unlock()
    }

    private func handle(packet: UnsafePointer<MIDIEventPacket>) {
        let count = min(Int(packet.pointee.wordCount), 64)
        var forwarded: UInt64 = 0
        var last: UInt32 = 0
        withUnsafePointer(to: packet.pointee.words) { tuple in
            tuple.withMemoryRebound(to: UInt32.self, capacity: 64) { words in
                for i in 0..<count {
                    let w = words[i]
                    // UMP MIDI1UP word: [mt:4][group:4][status:8][d1:8][d2:8]
                    if (w >> 28) & 0xF == 0x2 {
                        push(UInt8((w >> 16) & 0xFF),
                             UInt8((w >> 8) & 0x7F),
                             UInt8(w & 0x7F))
                        forwarded += 1
                        last = w
                    }
                }
            }
        }
        lock.lock()
        snap.words += UInt64(count)
        snap.forwarded += forwarded
        if forwarded > 0 {
            snap.last = String(format: "%02X %02X %02X", (last >> 16) & 0xFF, (last >> 8) & 0x7F, last & 0x7F)
        }
        lock.unlock()
    }
}
