// VoxoSpike.swift — Phase 7 step 48b: the iOS latency spike (SOUND §1, §4).
// Voxo on the iPad through miniaudio's CoreAudio path, the AVAudioSession
// owned here (playback, 48 kHz, a 128-frame preferred IO buffer). The
// numbers the roadmap asks for, on Voxo's clock (voxo_now_seconds):
//   push -> callback: 40 note-ons through the canvas's serial MIDI queue (the
//     ONE producer), each paired with the start of the block that consumed it;
//   touch -> callback: every play-surface touch-down during the window (the
//     mark taken in playTouchBegin, as the Phase-4 latency marks) — the
//     author's finger supplies them; the storm (10 synthetic voices) is the
//     load for the second half of the window;
//   the session: the rate and IO buffer duration it granted, its reported
//     output latency; Voxo's period, XRun proxy and render time.
// Launched with `--voxo-spike <seconds>`; writes Documents/voxo_spike.csv and
// logs VOXO_SPIKE_DONE. Until step 53 wires the setting, this is the only
// path that starts the device.
import AVFoundation
import Foundation
import Voxo

final class VoxoSpike {
    static let shared = VoxoSpike()
    private let lock = NSLock()
    private var lastTouchDown: Double = 0
    private(set) var running = false

    func markTouchDown(_ t: Double) { lock.lock(); lastTouchDown = t; lock.unlock() }
    private func touchMark() -> Double { lock.lock(); defer { lock.unlock() }; return lastTouchDown }

    static func armFromLaunchArguments() {
        let args = CommandLine.arguments
        guard let i = args.firstIndex(of: "--voxo-spike"), i + 1 < args.count,
              let seconds = Double(args[i + 1]), seconds > 0 else { return }
        DispatchQueue.global(qos: .userInitiated).asyncAfter(deadline: .now() + 2.5) {
            VoxoSpike.shared.run(seconds: seconds)
        }
    }

    func run(seconds: Double) {
        guard let canvas = SumiCanvasView.shared, let v = canvas.voxo, !running else { return }
        running = true
        defer { running = false }
        let path = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("voxo_spike.csv")
        var lines: [String] = []
        func both(_ s: String) { NSLog("[voxo spike] %@", s); lines.append(s) }

        // The session, the shell's (SOUND §4): playback, 48 kHz, 128 frames asked.
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playback, mode: .default, options: [])
            try session.setPreferredSampleRate(48000)
            try session.setPreferredIOBufferDuration(128.0 / 48000.0)
            try session.setActive(true)
        } catch {
            both("# session error: \(error)")
        }
        voxo_set_input_mode(v, 1)
        voxo_set_gain(v, 0.8)
        guard voxo_start(v) else {
            both("# FAIL: no output device")
            try? lines.joined(separator: "\n").write(to: path, atomically: true, encoding: .utf8)
            return
        }
        var st = voxo_stats_t()
        voxo_stats(v, &st)
        both(String(format: "# voxo %u.%u.%u spike; device \"%@\"; %u Hz; burst %u frames; buffer %u frames; low_latency %u; session rate %.0f, io buffer %.3f ms (%.1f frames), output latency %.2f ms",
                    voxo_version() >> 16, (voxo_version() >> 8) & 0xFF, voxo_version() & 0xFF,
                    deviceName(st), st.sample_rate, st.frames_per_burst, st.buffer_frames, st.low_latency,
                    session.sampleRate, session.ioBufferDuration * 1000.0, session.ioBufferDuration * session.sampleRate,
                    session.outputLatency * 1000.0))
        both("kind,index,t_ref_s,t_callback_s,delta_ms")
        Thread.sleep(forTimeInterval: 0.5)
        var seen = st.note_ons
        // Phase A: push -> callback.
        for i in 0..<40 {
            let note = UInt8(48 + (i * 5) % 24)
            let ch = UInt8(1 + i % 15)
            let tRef = voxo_now_seconds()
            canvas.spikePush(0x90 | ch, note, 100)
            Thread.sleep(forTimeInterval: 0.06)
            voxo_stats(v, &st)
            if st.note_ons > seen {
                both(String(format: "push,%d,%.6f,%.6f,%.3f", i, tRef, st.last_note_on_seconds, (st.last_note_on_seconds - tRef) * 1000.0))
                seen = st.note_ons
            } else {
                both(String(format: "push,%d,%.6f,0,nan", i, tRef))
            }
            canvas.spikePush(0x80 | ch, note, 0)
            Thread.sleep(forTimeInterval: 0.09)
        }
        // Phase B: the touch window; the storm as load for its second half.
        let tEnd = voxo_now_seconds() + seconds
        var touches = 0
        var lastPaired = 0.0
        var stormStarted = false
        while voxo_now_seconds() < tEnd {
            Thread.sleep(forTimeInterval: 0.005)
            if !stormStarted && voxo_now_seconds() > tEnd - seconds * 0.5 {
                stormStarted = true
                DispatchQueue.main.async { canvas.startStormTest(seconds: seconds * 0.4) }
                both("# storm started as load")
            }
            voxo_stats(v, &st)
            if st.note_ons > seen {
                seen = st.note_ons
                let mark = touchMark()
                if mark > 0, mark != lastPaired, st.last_note_on_seconds >= mark {
                    lastPaired = mark
                    both(String(format: "touch,%d,%.6f,%.6f,%.3f", touches, mark, st.last_note_on_seconds, (st.last_note_on_seconds - mark) * 1000.0))
                    touches += 1
                }
            }
        }
        voxo_stats(v, &st)
        both(String(format: "# end: %u callbacks, %u voxo_xruns (proxy), render max %.3f ms, burst %u, buffer %u, low_latency %u, session io buffer %.3f ms, output latency %.2f ms, dropped %u, note_ons %u, touches %d",
                    st.callbacks, st.xruns, st.render_max_ms, st.frames_per_burst, st.buffer_frames, st.low_latency,
                    session.ioBufferDuration * 1000.0, session.outputLatency * 1000.0, st.dropped_midi, st.note_ons, touches))
        voxo_stop(v)
        try? lines.joined(separator: "\n").write(to: path, atomically: true, encoding: .utf8)
        NSLog("VOXO_SPIKE_DONE %@", path.path)
    }

    private func deviceName(_ st: voxo_stats_t) -> String {
        let d = st.device
        let cap = MemoryLayout.size(ofValue: d)
        return withUnsafePointer(to: d) { p in
            p.withMemoryRebound(to: CChar.self, capacity: cap) { String(cString: $0) }
        }
    }
}
