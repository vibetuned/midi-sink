// Bluetooth MIDI pairing (ROLI Piano et al.): Apple's stock central picker.
// Once paired, the instrument appears as a normal CoreMIDI source and
// MidiSource's setup-changed notification connects it automatically.
// Bare view controller (no UINavigationController): it is PUSHED inside the
// settings sheet's NavigationStack — presenting it as a second sheet fought
// the settings sheet for the presentation slot.
import SwiftUI
import CoreAudioKit

struct BluetoothMidiPairingView: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> CABTMIDICentralViewController {
        CABTMIDICentralViewController()
    }
    func updateUIViewController(_ vc: CABTMIDICentralViewController, context: Context) {}
}
