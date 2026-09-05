// BLE MIDI peripheral advertising (PROJECT_SPEC.md §8.5): Apple's stock local
// peripheral controller — the iPad advertises as a BLE MIDI device; a central
// (Mac/DAW) connects and subscribes to our sources. Pushed inside the
// settings NavigationStack like the central picker.
import SwiftUI
import CoreAudioKit

struct BluetoothMidiAdvertiseView: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> CABTMIDILocalPeripheralViewController {
        CABTMIDILocalPeripheralViewController()
    }
    func updateUIViewController(_ vc: CABTMIDILocalPeripheralViewController, context: Context) {}
}
