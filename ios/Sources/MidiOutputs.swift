// Outbound transports (PHASE4 §5.3/§5.4, Step 17): one generator, N sinks,
// each behind its OWN hostmpe limiter — virtual source and Network Session at
// ≤100 Hz per voice-dimension, BLE at a ~300 msg/s global budget with
// round-robin fairness. The loopback never passes through here.
//
// Threading: every method is called on the canvas's serial MIDI queue (the
// §5.2 producer context); the limiters share that serialization.
import CoreMIDI
import Foundation
import HostMPE

final class MidiOutputs {
    private var client = MIDIClientRef()
    private var virtualSource = MIDIEndpointRef()   // DAW path (≤100 Hz/dim)
    private var bleSource = MIDIEndpointRef()       // BLE path (~300 msg/s budget)
    private var outPort = MIDIPortRef()             // for the network destination
    private var limVirtual: OpaquePointer!          // hostmpe_limiter_t*
    private var limNetwork: OpaquePointer!
    private var limBLE: OpaquePointer!

    var virtualEnabled = true
    var networkEnabled = false
    var bleEnabled = false

    /// Messages actually written to each sink (diagnostic, read on any thread).
    private(set) var sentVirtual = 0
    private(set) var sentNetwork = 0
    private(set) var sentBLE = 0

    /// Unique IDs of our own endpoints — the INPUT side must skip these or the
    /// outbound stream feeds back into observe_external and the occupancy
    /// mask starves our own allocator.
    private(set) var ownUniqueIDs = Set<MIDIUniqueID>()

    /// Monotonic outbound timestamp: a strictly increasing stamp (1 µs apart)
    /// pins delivery order for any transport that batches or sorts by
    /// timestamp, while staying essentially "now". Ordering matters most for
    /// the MCM/RPN0 handshake (data entry MUST follow the RPN select) and for
    /// center-bend-before-Note-On. Kept as hygiene — measured order is
    /// correct on USB/IDAM, rtpMIDI and BLE (DECISIONS_3 #22).
    private var lastStamp: MIDITimeStamp = 0
    private var machPerMicro: UInt64 = 1
    private func nextStamp() -> MIDITimeStamp {
        let now = mach_absolute_time()
        lastStamp = max(now, lastStamp &+ machPerMicro)
        return lastStamp
    }

    // Stable unique IDs so the endpoints are the SAME across app launches:
    // without these, every relaunch mints new sources and DAWs stay bound to
    // a dead endpoint from a previous launch (the "GarageBand can't hear us"
    // bug). Arbitrary fixed constants, one per source.
    private static let kVirtualUID: MIDIUniqueID = 0x5355_4D31   // "SUM1"
    private static let kBleUID:     MIDIUniqueID = 0x5355_4D32   // "SUM2"

    /// Fired when a sink comes up mid-session (§5.4: enabling USB/IDAM in
    /// Audio MIDI Setup happens AFTER our session opened, so the MCM/RPN0
    /// handshake must be re-sent or the DAW never learns the ±48 range).
    var onSinkAppeared: (() -> Void)?
    private var lastSinkSignature = (0, 0, 0)

    init() {
        // Every CoreMIDI call's status is logged: a silent failure here is
        // indistinguishable from "the DAW isn't listening" without it.
        // The notification block is what detects IDAM being enabled later.
        let s1 = MIDIClientCreateWithBlock("midi-sink-out" as CFString, &client) {
            [weak self] note in
            guard note.pointee.messageID == .msgSetupChanged else { return }
            DispatchQueue.main.async { self?.checkForNewSink() }
        }
        let s2 = MIDISourceCreate(client, "midi-sink Play Surface" as CFString, &virtualSource)
        let s3 = MIDISourceCreate(client, "midi-sink (BLE)" as CFString, &bleSource)
        let s4 = MIDIOutputPortCreate(client, "out" as CFString, &outPort)
        let s5 = MIDIObjectSetIntegerProperty(virtualSource, kMIDIPropertyUniqueID, Self.kVirtualUID)
        let s6 = MIDIObjectSetIntegerProperty(bleSource, kMIDIPropertyUniqueID, Self.kBleUID)
        NSLog("[out] client=%d(%d) vsrc=%d(%d) ble=%d(%d) port=%d(%d) uid=(%d,%d)",
              Int(client), Int(s1), Int(virtualSource), Int(s2),
              Int(bleSource), Int(s3), Int(outPort), Int(s4), Int(s5), Int(s6))
        // Advertise as a normal MIDI device so DAWs list/connect it cleanly.
        for ep in [virtualSource, bleSource] {
            MIDIObjectSetStringProperty(ep, kMIDIPropertyManufacturer, "Vibetuned" as CFString)
            MIDIObjectSetStringProperty(ep, kMIDIPropertyModel, "midi-sink" as CFString)
        }
        for ep in [virtualSource, bleSource] where ep != 0 {
            var uid: MIDIUniqueID = 0
            MIDIObjectGetIntegerProperty(ep, kMIDIPropertyUniqueID, &uid)
            ownUniqueIDs.insert(uid)
        }
        limVirtual = hostmpe_limiter_create_rate(100.0)
        limNetwork = hostmpe_limiter_create_rate(100.0)
        limBLE = hostmpe_limiter_create_budget(300.0)
        var tb = mach_timebase_info()
        mach_timebase_info(&tb)
        machPerMicro = max(UInt64(1000) * UInt64(tb.denom) / UInt64(tb.numer), 1)
    }

    deinit {
        hostmpe_limiter_destroy(limVirtual)
        hostmpe_limiter_destroy(limNetwork)
        hostmpe_limiter_destroy(limBLE)
        if client != 0 { MIDIClientDispose(client) }
    }

    /// CoreMIDI destinations owned by the Bluetooth driver. When the iPad
    /// advertises as a BLE MIDI peripheral and a central (Mac/DAW) connects,
    /// iOS creates such a destination for that link — sending to it is what
    /// actually puts bytes on the air. Our own "midi-sink (BLE)" virtual
    /// source only publishes locally, so the BLE pipe targets these.
    /// Matched by kMIDIPropertyDriverOwner, not by name (names vary by peer).
    private func bluetoothDestinations() -> [MIDIEndpointRef] {
        destinations(matching: ["bluetooth"])
    }

    /// Wired host links (USB/IDAM). Measured reality (DECISIONS_3 #27): a
    /// virtual source is visible to on-device apps but is NOT bridged to a
    /// tethered Mac — the wired path is an explicit send to the
    /// `AppleIDAMDriver` destination ("IDAM MIDI Host"). Same sink and same
    /// ≤100 Hz policy as the virtual source, per §5.4's framing.
    private func hostLinkDestinations() -> [MIDIEndpointRef] {
        destinations(matching: ["idam", "usb"])
    }

    private func destinations(matching needles: [String]) -> [MIDIEndpointRef] {
        var found: [MIDIEndpointRef] = []
        var seenEntities = Set<MIDIEntityRef>()
        for i in 0..<MIDIGetNumberOfDestinations() {
            let dest = MIDIGetDestination(i)
            var owner: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyDriverOwner, &owner)
            let name = (owner?.takeRetainedValue() as String?) ?? ""
            guard needles.contains(where: { name.localizedCaseInsensitiveContains($0) })
            else { continue }
            // One send per ENTITY: some drivers expose several endpoints for
            // one physical link, and sending to each delivers duplicates to
            // the peer — wasted budget on the scarcest transport.
            var entity = MIDIEntityRef()
            MIDIEndpointGetEntity(dest, &entity)
            if entity != 0 {
                if seenEntities.contains(entity) { continue }
                seenEntities.insert(entity)
            }
            found.append(dest)
        }
        return found
    }

    /// Log every destination with its driver owner — the only reliable way to
    /// see what a BLE connection actually creates on the device.
    func logDestinations() {
        NSLog("[out] destinations: %d", MIDIGetNumberOfDestinations())
        for i in 0..<MIDIGetNumberOfDestinations() {
            let dest = MIDIGetDestination(i)
            var n: Unmanaged<CFString>?
            var o: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyDisplayName, &n)
            MIDIObjectGetStringProperty(dest, kMIDIPropertyDriverOwner, &o)
            NSLog("[out]   dest[%d] '%@' driver='%@'", i,
                  (n?.takeRetainedValue() as String?) ?? "?",
                  (o?.takeRetainedValue() as String?) ?? "?")
        }
    }

    /// A sink "appeared" when the MIDI world grew — a USB/IDAM enable, a BLE
    /// central connecting, a network peer joining. Growth only: teardown
    /// needs no handshake.
    private func checkForNewSink() {
        let sig = (MIDIGetNumberOfDestinations(), MIDIGetNumberOfSources(),
                   MIDIGetNumberOfDevices())
        let grew = sig.0 > lastSinkSignature.0 || sig.1 > lastSinkSignature.1
                || sig.2 > lastSinkSignature.2
        let first = lastSinkSignature == (0, 0, 0)
        lastSinkSignature = sig
        if grew && !first {
            NSLog("[out] sink appeared (dst=%d src=%d dev=%d) -> re-sending MCM",
                  sig.0, sig.1, sig.2)
            onSinkAppeared?()
        }
    }

    /// Call once after construction so the baseline is the world as it was
    /// at session open (otherwise the first notification looks like growth).
    func primeSinkSignature() {
        lastSinkSignature = (MIDIGetNumberOfDestinations(),
                             MIDIGetNumberOfSources(), MIDIGetNumberOfDevices())
    }

    func setNetworkEnabled(_ on: Bool) {
        networkEnabled = on
        let session = MIDINetworkSession.default()
        session.isEnabled = on
        session.connectionPolicy = .anyone
    }

    /// Fan one generated message out to every enabled transport, each through
    /// its own limiter. `exempt` per §5.3: Note On/Off, initial center bend,
    /// pressure-0-before-Note-Off, session config.
    private var sendCalls = 0

    func send(_ msg: hostmpe_msg_t, exempt: Bool, now: Double) {
        var out = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 64)
        sendCalls += 1
        if virtualEnabled {
            let n = hostmpe_limiter_push(limVirtual, now, msg, exempt, &out, 64)
            if sendCalls <= 6 {
                NSLog("[out] send#%d st=0x%02X exempt=%d -> limiter n=%d vsrc=%d",
                      sendCalls, Int(msg.status), exempt ? 1 : 0, Int(n), Int(virtualSource))
            }
            emit(out, n, to: .virtualSrc)
        } else if sendCalls <= 6 {
            NSLog("[out] send#%d DROPPED: virtualEnabled=false", sendCalls)
        }
        if networkEnabled {
            let n = hostmpe_limiter_push(limNetwork, now, msg, exempt, &out, 64)
            emit(out, n, to: .network)
        }
        if bleEnabled {
            let n = hostmpe_limiter_push(limBLE, now, msg, exempt, &out, 64)
            emit(out, n, to: .ble)
        }
    }

    /// Send an exempt message to ONE sink regardless of its enabled flag —
    /// used to silence a transport as it is switched off (a sink that stops
    /// receiving must not be left holding notes).
    func sendSilence(_ msgs: [hostmpe_msg_t], count: UInt32, toVirtual: Bool,
                     toNetwork: Bool, toBLE: Bool) {
        if toVirtual { emit(msgs, count, to: .virtualSrc) }
        if toNetwork { emit(msgs, count, to: .network) }
        if toBLE { emit(msgs, count, to: .ble) }
    }

    /// Surface messages the limiters held back — call every frame.
    func drain(now: Double) {
        var out = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 64)
        if virtualEnabled { emit(out, hostmpe_limiter_drain(limVirtual, now, &out, 64), to: .virtualSrc) }
        if networkEnabled { emit(out, hostmpe_limiter_drain(limNetwork, now, &out, 64), to: .network) }
        if bleEnabled { emit(out, hostmpe_limiter_drain(limBLE, now, &out, 64), to: .ble) }
    }

    // -- wire ------------------------------------------------------------------

    private enum Sink { case virtualSrc, network, ble }

    /// #66: every byte that actually LEAVES the device is recorded, so a
    /// looping transport's echo can be recognised on the way back in. Set by
    /// the canvas to hostmpe_echo_record; called on the MIDI queue.
    var onDelivered: ((hostmpe_msg_t) -> Void)?

    private func emit(_ msgs: [hostmpe_msg_t], _ count: UInt32, to sink: Sink) {
        guard count > 0 else { return }
        if let hook = onDelivered {
            for i in 0..<Int(count) { hook(msgs[i]) }
        }
        // A bare `MIDIPacketList()` has room for ONE packet — passing a
        // larger listSize to MIDIPacketListAdd would let a drained batch
        // write past the struct. Back the list with a real buffer instead.
        let listSize = 4096
        var storage = [UInt8](repeating: 0, count: listSize)
        storage.withUnsafeMutableBytes { raw in
            let pl = raw.baseAddress!.assumingMemoryBound(to: MIDIPacketList.self)
            var packet = MIDIPacketListInit(pl)
            for i in 0..<Int(count) {
                let bytes = [msgs[i].status, msgs[i].data1, msgs[i].data2]
                let stamp = nextStamp()   // strictly increasing: preserves order
                bytes.withUnsafeBufferPointer { bp in
                    packet = MIDIPacketListAdd(pl, listSize, packet, stamp,
                                               bp.count, bp.baseAddress!)
                }
            }
            switch sink {
            case .virtualSrc:
                // Two delivery mechanisms, one sink: the virtual source for
                // on-device apps, plus an explicit send to any wired host
                // link (USB/IDAM) since iOS does not bridge virtual sources
                // to a tethered Mac (DECISIONS_3 #27).
                if virtualSource != 0 { MIDIReceived(virtualSource, pl) }
                for d in hostLinkDestinations() { MIDISend(outPort, d, pl) }
                if virtualSource != 0 { sentVirtual += Int(count) }
            case .ble:
                // Send ONLY to the real BLE link(s). No local mirror: iOS
                // bridges the device's virtual sources over the same BLE
                // peripheral link, so mirroring to our own "midi-sink (BLE)"
                // source made every message arrive TWICE at the central
                // (measured: 23% identical consecutive repeats in wire order, pushing the
                // observed rate past the budget — DECISIONS_3 #25).
                let dests = bluetoothDestinations()
                for d in dests { MIDISend(outPort, d, pl) }
                if !dests.isEmpty { sentBLE += Int(count) }
            case .network:
                let dest = MIDINetworkSession.default().destinationEndpoint()
                if dest != 0 {
                    MIDISend(outPort, dest, pl)
                    sentNetwork += Int(count)
                }
            }
        }
    }
}
