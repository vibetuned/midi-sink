// wind_breath.swift — scripted mono wind-instrument stream (roadmap step 6
// DONE check): a legato melody on ONE channel with a dense CC2 breath stream
// (~150 msgs/s), preceded by a CC2 burst so §2.5 auto-detection settles on
// wind mode before the first note.
//
// Run while midi-sink is open:   swift tests/wind_breath.swift [seconds]
// Expects: one continuous wandering ink line whose thickness follows breath.
import CoreMIDI
import Foundation

var client = MIDIClientRef()
MIDIClientCreate("wind-breath" as CFString, nil, nil, &client)
var src = MIDIEndpointRef()
guard MIDISourceCreate(client, "wind-breath-source" as CFString, &src) == noErr else {
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

let duration = CommandLine.arguments.count > 1 ? Double(CommandLine.arguments[1]) ?? 20.0 : 20.0
Thread.sleep(forTimeInterval: 2.5)   // let the app's port rescan open us

for _ in 0..<20 { send([0xB0, 2, 0]) }   // breath prelude: settles wind detection

// A wandering legato line (overlapping note-ons before offs, single channel).
let melody: [UInt8] = [55, 57, 60, 62, 64, 67, 64, 69, 67, 72, 70, 65, 62, 58, 60]
var idx = 0
var current: UInt8 = melody[0]
send([0x90, current, 80])

var tick = 0
let start = Date()
while Date().timeIntervalSince(start) < duration {
    let t = Date().timeIntervalSince(start)
    // ~150 Hz breath: slow swells with vibrato-ish ripple.
    let breath = UInt8(max(2.0, min(127.0, 70.0 + 50.0 * sin(t * 0.9) + 8.0 * sin(t * 7.0))))
    send([0xB0, 2, breath])
    // Legato note change every ~1.2 s: new note ON first, old note OFF after.
    if tick % 180 == 179 {
        idx += 1
        let next = melody[idx % melody.count]
        send([0x90, next, 80])
        Thread.sleep(forTimeInterval: 0.01)
        send([0x80, current, 40])
        current = next
    }
    tick += 1
    Thread.sleep(forTimeInterval: 0.0066)
}
send([0x80, current, 40])
Thread.sleep(forTimeInterval: 0.3)
print("wind_breath: \(sent) messages in \(String(format: "%.1f", Date().timeIntervalSince(start)))s")
