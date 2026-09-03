import CoreMIDI
import Foundation
func str(_ o: MIDIObjectRef, _ p: CFString) -> String {
    var r: Unmanaged<CFString>?
    MIDIObjectGetStringProperty(o, p, &r)
    return (r?.takeRetainedValue() as String?) ?? "-"
}
print("=== SOURCES (\(MIDIGetNumberOfSources())) ===")
for i in 0..<MIDIGetNumberOfSources() {
    let e = MIDIGetSource(i)
    var ent = MIDIEntityRef(); MIDIEndpointGetEntity(e, &ent)
    var dev = MIDIDeviceRef(); if ent != 0 { MIDIEntityGetDevice(ent, &dev) }
    print("  [\(i)] name='\(str(e, kMIDIPropertyDisplayName))' port='\(str(e, kMIDIPropertyName))'")
    print("        device='\(dev != 0 ? str(dev, kMIDIPropertyName) : "-")' entity='\(ent != 0 ? str(ent, kMIDIPropertyName) : "-")' driver='\(str(e, kMIDIPropertyDriverOwner))' mfr='\(str(e, kMIDIPropertyManufacturer))' model='\(str(e, kMIDIPropertyModel))'")
}
print("=== DESTINATIONS (\(MIDIGetNumberOfDestinations())) ===")
for i in 0..<MIDIGetNumberOfDestinations() {
    let e = MIDIGetDestination(i)
    print("  [\(i)] name='\(str(e, kMIDIPropertyDisplayName))' driver='\(str(e, kMIDIPropertyDriverOwner))'")
}
