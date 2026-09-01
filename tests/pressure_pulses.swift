// pressure_pulses.swift — §4.4 nested-ring check (roadmap step 8): ONE voice,
// held note, 5 channel-pressure pulses at a fixed center. Each pulse after
// the first must stamp a NEW nested ring (feed episodes advance the ink
// band); a solid overwritten interior is the failure mode.
//
// Run while midi-sink is open:   swift tests/pressure_pulses.swift
import CoreMIDI
import Foundation

var client = MIDIClientRef()
MIDIClientCreate("pressure-pulses" as CFString, nil, nil, &client)
var src = MIDIEndpointRef()
guard MIDISourceCreate(client, "pressure-pulses-source" as CFString, &src) == noErr else {
    fatalError("MIDISourceCreate failed")
}
func send(_ bytes: [UInt8]) {
    var buffer = [UInt8](repeating: 0, count: 256)
    buffer.withUnsafeMutableBytes { raw in
        let pl = raw.baseAddress!.assumingMemoryBound(to: MIDIPacketList.self)
        var pkt = MIDIPacketListInit(pl)
        bytes.withUnsafeBufferPointer { bp in
            pkt = MIDIPacketListAdd(pl, 256, pkt, 0, bytes.count, bp.baseAddress!)
        }
        MIDIReceived(src, pl)
    }
}
Thread.sleep(forTimeInterval: 2.5)   // port rescan
send([0xB0, 101, 0]); send([0xB0, 100, 6]); send([0xB0, 6, 15])   // MCM -> MPE
send([0x91, 60, 90])   // ch 2: C4, held for the whole run
Thread.sleep(forTimeInterval: 0.3)
for pulse in 0..<5 {
    for v in stride(from: 8, through: 120, by: 16) {     // ramp up
        send([0xD1, UInt8(v), 0]); Thread.sleep(forTimeInterval: 0.03)
    }
    Thread.sleep(forTimeInterval: 0.5)                    // hold: band grows
    for v in stride(from: 120, through: 0, by: -16) {     // ramp down
        send([0xD1, UInt8(max(v, 0)), 0]); Thread.sleep(forTimeInterval: 0.03)
    }
    send([0xD1, 0, 0])
    Thread.sleep(forTimeInterval: 0.6)                    // full release gap
    print("pulse \(pulse + 1) done")
}
send([0x81, 60, 40])
Thread.sleep(forTimeInterval: 0.3)
