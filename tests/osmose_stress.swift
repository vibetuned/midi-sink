// osmose_stress.swift — scripted Osmose-density MPE stress source (roadmap
// step 5 DONE check): 10 voices (member channels 2..11) x 200 channel-pressure
// messages per second each (= 2000 press/s), plus per-voice glide bends and
// CC74, after an MCM configuring the lower zone.
//
// Run while midi-sink is open:   swift tests/osmose_stress.swift [seconds]
// Expects: 60+ fps, `dropped MIDI messages: 0` at app exit, deformation
// budget engaging gracefully (coalescing keeps deform passes <= budget).
import CoreMIDI
import Foundation

var client = MIDIClientRef()
MIDIClientCreate("osmose-stress" as CFString, nil, nil, &client)
var src = MIDIEndpointRef()
guard MIDISourceCreate(client, "osmose-stress-source" as CFString, &src) == noErr else {
    fatalError("MIDISourceCreate failed")
}

var sent = 0
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
    sent += 1
}

let duration = CommandLine.arguments.count > 1 ? Double(CommandLine.arguments[1]) ?? 30.0 : 30.0
Thread.sleep(forTimeInterval: 2.5)   // let the app's port rescan open us

// MCM: lower zone, 15 member channels (RPN 6 on ch 1).
send([0xB0, 101, 0]); send([0xB0, 100, 6]); send([0xB0, 6, 15])

// 10 held voices on member channels 2..11 (indices 1..10), spread pitches.
let notes: [UInt8] = [48, 55, 60, 64, 67, 72, 76, 79, 84, 91]
for (i, n) in notes.enumerated() {
    send([UInt8(0x90 | (1 + i)), n, UInt8(60 + i * 6)])
}

// 200 Hz per voice: pressure sine (dense, Osmose-like), plus 20 Hz bend and
// 10 Hz CC74 per voice.
var tick = 0
let start = Date()
while Date().timeIntervalSince(start) < duration {
    let t = Date().timeIntervalSince(start)
    for v in 0..<10 {
        let ch = UInt8(1 + v)
        let press = UInt8(max(0.0, min(127.0, 64.0 + 60.0 * sin(t * (1.1 + 0.13 * Double(v)) + Double(v)))))
        send([0xD0 | ch, press, 0])
        if tick % 10 == v {   // staggered 20 Hz bends
            let bend = Int(8192.0 + 2000.0 * sin(t * 0.7 + Double(v) * 0.9))
            send([0xE0 | ch, UInt8(bend & 0x7F), UInt8((bend >> 7) & 0x7F)])
        }
        if tick % 20 == 2 * v {   // staggered 10 Hz CC74
            let slide = UInt8(max(0.0, min(127.0, 64.0 + 60.0 * sin(t * 0.5 + Double(v)))))
            send([0xB0 | ch, 74, slide])
        }
    }
    tick += 1
    Thread.sleep(forTimeInterval: 0.005)   // 200 Hz loop
}
for (i, n) in notes.enumerated() {
    send([UInt8(0x80 | (1 + i)), n, 64])
}
Thread.sleep(forTimeInterval: 0.3)
print("osmose_stress: \(sent) messages in \(String(format: "%.1f", Date().timeIntervalSince(start)))s")
