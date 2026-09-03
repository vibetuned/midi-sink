// Mac-side Step 17 validation listener: enables the local MIDI network
// session, auto-connects to an _apple-midi._udp peer whose name matches
// argv[1], subscribes to EVERY CoreMIDI source, and logs each message with
// a host timestamp to argv[3] (CSV) for argv[2] seconds.
import CoreMIDI
import Foundation

let filter = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "iPad"
let seconds = CommandLine.arguments.count > 2 ? Double(CommandLine.arguments[2])! : 70.0
let outPath = CommandLine.arguments.count > 3 ? CommandLine.arguments[3] : "midilisten.csv"

let session = MIDINetworkSession.default()
session.isEnabled = true
session.connectionPolicy = .anyone

final class Browser: NSObject, NetServiceBrowserDelegate, NetServiceDelegate {
    let browser = NetServiceBrowser()
    var services: [NetService] = []
    func start() {
        browser.delegate = self
        browser.searchForServices(ofType: "_apple-midi._udp", inDomain: "local.")
    }
    func netServiceBrowser(_ b: NetServiceBrowser, didFind svc: NetService, moreComing: Bool) {
        print("[bonjour] found: \(svc.name)")
        services.append(svc)
        if svc.name.localizedCaseInsensitiveContains(filter) {
            svc.delegate = self
            svc.resolve(withTimeout: 8)
        }
    }
    func netServiceDidResolveAddress(_ svc: NetService) {
        print("[bonjour] resolved: \(svc.name) host \(svc.hostName ?? "?") port \(svc.port)")
        var ip = ""
        for data in svc.addresses ?? [] {
            data.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
                let sa = raw.baseAddress!.assumingMemoryBound(to: sockaddr.self)
                if sa.pointee.sa_family == UInt8(AF_INET) {
                    let sin = raw.baseAddress!.assumingMemoryBound(to: sockaddr_in.self)
                    var addr = sin.pointee.sin_addr
                    var buf = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
                    inet_ntop(AF_INET, &addr, &buf, socklen_t(INET_ADDRSTRLEN))
                    if ip.isEmpty { ip = String(cString: buf) }
                }
            }
        }
        print("[net] session enabled=\(session.isEnabled) local=\(session.localName) ip=\(ip)")
        let host = ip.isEmpty ? MIDINetworkHost(name: svc.name, netService: svc)
                              : MIDINetworkHost(name: svc.name, address: ip, port: svc.port)
        let conn = MIDINetworkConnection(host: host)
        for attempt in 1...5 {
            session.isEnabled = true
            session.connectionPolicy = .anyone
            let ok = session.addConnection(conn)
            print("[net] addConnection attempt \(attempt) -> \(ok) (connections: \(session.connections().count))")
            if ok { return }
            RunLoop.current.run(until: Date(timeIntervalSinceNow: 1.0))
        }
    }
    func netService(_ svc: NetService, didNotResolve errorDict: [String: NSNumber]) {
        print("[bonjour] resolve FAILED: \(errorDict)")
    }
}

var log = "t,src,status,d1,d2\n"
var msgCount = 0
var client = MIDIClientRef()
var inPort = MIDIPortRef()
var names: [MIDIUniqueID: String] = [:]

MIDIClientCreateWithBlock("midilisten" as CFString, &client) { note in
    if note.pointee.messageID == .msgSetupChanged { DispatchQueue.main.async { connectAll() } }
}
MIDIInputPortCreateWithProtocol(client, "in" as CFString, ._1_0, &inPort) { evtlist, srcCtx in
    let t = Date().timeIntervalSince1970
    let srcName = srcCtx.map { String(cString: $0.assumingMemoryBound(to: CChar.self)) } ?? "?"
    for packet in evtlist.unsafeSequence() {
        var p = packet.pointee
        let count = min(Int(p.wordCount), 64)
        withUnsafePointer(to: &p.words) { tuple in
            tuple.withMemoryRebound(to: UInt32.self, capacity: 64) { words in
                for i in 0..<count {
                    let w = words[i]
                    if (w >> 28) & 0xF == 0x2 {
                        log += String(format: "%.6f,%@,%d,%d,%d\n", t, srcName,
                                      (w >> 16) & 0xFF, (w >> 8) & 0x7F, w & 0x7F)
                        msgCount += 1
                    }
                }
            }
        }
    }
}

var connected = Set<MIDIUniqueID>()
func connectAll() {
    for i in 0..<MIDIGetNumberOfSources() {
        let src = MIDIGetSource(i)
        var uid: MIDIUniqueID = 0
        MIDIObjectGetIntegerProperty(src, kMIDIPropertyUniqueID, &uid)
        if connected.contains(uid) { continue }
        var nameRef: Unmanaged<CFString>?
        MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &nameRef)
        let name = (nameRef?.takeRetainedValue() as String?) ?? "?"
        let ctx = strdup(name)
        if MIDIPortConnectSource(inPort, src, ctx) == noErr {
            connected.insert(uid)
            print("[midi] listening: \(name)")
        }
    }
}

let browserObj = Browser()
browserObj.start()
connectAll()
print("[listen] \(seconds)s, filter '\(filter)' -> \(outPath)")
RunLoop.main.run(until: Date(timeIntervalSinceNow: seconds))
try? log.write(toFile: outPath, atomically: true, encoding: .utf8)
print("[listen] done: \(msgCount) messages -> \(outPath)")
