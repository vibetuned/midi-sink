package com.vibetuned.midisink

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.media.midi.MidiDevice
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog

private const val TAG = "sumi-shell"

/** The BLE-MIDI service UUID (MMA BLE-MIDI 1.0). */
private val MIDI_SERVICE_UUID =
    ParcelUuid.fromString("03B80E5A-EDE8-4B33-A751-6CE34EC4C700")

/**
 * MIDI plumbing: every MidiManager device with output ports (wired USB,
 * virtual, and BLE devices once opened) is handed to the JNI AMidi path;
 * hotplug is callback-driven. Opened MidiDevice objects are held here so the
 * GC cannot close them behind AMidi's back.
 *
 * Phase 4: our OWN virtual device (SumiMidiDeviceService) is never opened for
 * ingest — the outbound stream would feed back into hostmpe's external-
 * occupancy mask and starve the allocator (the iOS excludedUniqueIDs rule).
 * A device leaving clears the mask for its channels (§5.1).
 */
class MidiInputs(private val activity: Activity) {

    private val midiManager =
        activity.getSystemService(Context.MIDI_SERVICE) as MidiManager
    private val handler = Handler(Looper.getMainLooper())
    private val opened = mutableMapOf<Int, MidiDevice>()
    private val openedIds = mutableSetOf<Int>()
    /** Held so stop() can unregister it: an un-unregistered callback keeps the
     *  destroyed Activity alive AND, after a relaunch in the same process
     *  (launchMode singleTask), opens every appearing device twice — every
     *  incoming message would then be parsed and pushed to the loopback
     *  twice. */
    private var deviceCallback: MidiManager.DeviceCallback? = null

    val bleResults = mutableStateListOf<Pair<String, BluetoothDevice>>()
    private var scanning: ScanCallback? = null

    fun start() {
        if (deviceCallback != null) return
        val cb = object : MidiManager.DeviceCallback() {
            override fun onDeviceAdded(info: MidiDeviceInfo) = open(info)
            override fun onDeviceRemoved(info: MidiDeviceInfo) {
                Log.i(TAG, "MIDI device removed: ${name(info)}")
                if (openedIds.remove(info.id)) {
                    // The native poller must forget the ports too, or it keeps
                    // reading a dead port every millisecond and a replug
                    // doubles them.
                    NativeBridge.nativeRemoveMidiDevice(info.id)
                    opened.remove(info.id)?.let { runCatching { it.close() } }
                    NativeBridge.nativeExternalClear()
                }
            }
        }
        deviceCallback = cb
        @Suppress("DEPRECATION")
        midiManager.registerDeviceCallback(cb, handler)
        @Suppress("DEPRECATION")
        midiManager.devices.forEach { open(it) }
    }

    fun stop() {
        stopBleScan()
        deviceCallback?.let { runCatching { midiManager.unregisterDeviceCallback(it) } }
        deviceCallback = null
        opened.keys.toList().forEach { NativeBridge.nativeRemoveMidiDevice(it) }
        opened.values.forEach { runCatching { it.close() } }
        opened.clear()
        openedIds.clear()
    }

    private fun name(info: MidiDeviceInfo): String =
        info.properties.getString(MidiDeviceInfo.PROPERTY_NAME) ?: "midi-${info.id}"

    /** Our own MidiDeviceService — never ingest what we transmit. The
     *  ServiceInfo property key is hidden API ("service_info"), so the
     *  manufacturer/product pair from midi_device_info.xml is the fallback. */
    private fun isOwnVirtualDevice(info: MidiDeviceInfo): Boolean {
        if (info.type != MidiDeviceInfo.TYPE_VIRTUAL) return false
        @Suppress("DEPRECATION")
        val si = info.properties.getParcelable<ServiceInfo>("service_info")
        if (si != null) return si.packageName == activity.packageName
        return info.properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) == "Vibetuned" &&
            info.properties.getString(MidiDeviceInfo.PROPERTY_PRODUCT) == "midi-sink Play Surface"
    }

    private fun open(info: MidiDeviceInfo) {
        if (info.outputPortCount == 0 || isOwnVirtualDevice(info) || !openedIds.add(info.id)) return
        midiManager.openDevice(info, { device ->
            if (device == null) {
                Log.w(TAG, "openDevice failed: ${name(info)}")
                openedIds.remove(info.id)
                return@openDevice
            }
            opened[info.id] = device
            NativeBridge.nativeAddMidiDevice(device, info.id)
            Log.i(TAG, "MIDI input opened: ${name(info)}")
        }, handler)
    }

    // -- BLE-MIDI (the ROLI): scan for the MIDI service, then
    //    MidiManager.openBluetoothDevice routes it into the same path. -------

    fun hasBlePermissions(): Boolean {
        if (Build.VERSION.SDK_INT >= 31) {
            return activity.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) ==
                PackageManager.PERMISSION_GRANTED &&
                activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) ==
                PackageManager.PERMISSION_GRANTED
        }
        return activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) ==
            PackageManager.PERMISSION_GRANTED
    }

    fun requestBlePermissions() {
        val perms = if (Build.VERSION.SDK_INT >= 31)
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        activity.requestPermissions(perms, 71)
    }

    fun startBleScan() {
        if (scanning != null) return
        val bt = activity.getSystemService(BluetoothManager::class.java) ?: return
        val scanner = bt.adapter?.bluetoothLeScanner ?: return
        bleResults.clear()
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val dev = result.device
                val label = try {
                    dev.name ?: dev.address
                } catch (_: SecurityException) {
                    dev.address
                }
                if (bleResults.none { it.second.address == dev.address }) {
                    bleResults.add(label to dev)
                }
            }
        }
        try {
            scanner.startScan(
                listOf(ScanFilter.Builder().setServiceUuid(MIDI_SERVICE_UUID).build()),
                ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
                cb)
            scanning = cb
            Log.i(TAG, "BLE-MIDI scan started")
        } catch (e: SecurityException) {
            Log.w(TAG, "BLE scan needs permissions: $e")
        }
    }

    fun stopBleScan() {
        val cb = scanning ?: return
        scanning = null
        val bt = activity.getSystemService(BluetoothManager::class.java) ?: return
        try {
            bt.adapter?.bluetoothLeScanner?.stopScan(cb)
        } catch (_: SecurityException) {
        }
    }

    fun connectBle(device: BluetoothDevice) {
        midiManager.openBluetoothDevice(device, { midiDevice ->
            if (midiDevice == null) {
                Log.w(TAG, "openBluetoothDevice failed: ${device.address}")
                return@openBluetoothDevice
            }
            // The freshly opened BLE device ALSO surfaces through
            // MidiManager's onDeviceAdded — without this id dedup both paths
            // hand the device to AMidi and every message ingests twice
            // (both callbacks run on the same handler, so no lock needed).
            if (!openedIds.add(midiDevice.info.id)) {
                Log.i(TAG, "BLE-MIDI ${device.address} already opened via device callback")
                runCatching { midiDevice.close() }
                return@openBluetoothDevice
            }
            opened[midiDevice.info.id] = midiDevice
            NativeBridge.nativeAddMidiDevice(midiDevice, midiDevice.info.id)
            Log.i(TAG, "BLE-MIDI opened: ${device.address}")
        }, handler)
    }
}

@Composable
fun BluetoothMidiPairingDialog(midi: MidiInputs, onDismiss: () -> Unit) {
    DisposableEffect(Unit) {
        if (midi.hasBlePermissions()) midi.startBleScan() else midi.requestBlePermissions()
        onDispose { midi.stopBleScan() }
    }
    Dialog(onDismissRequest = onDismiss) {
        Column(
            Modifier
                .background(Color(0xEE18143A))
                .padding(20.dp)
                .fillMaxWidth()
        ) {
            BasicText(
                "Bluetooth MIDI",
                style = TextStyle(color = Color.White, fontSize = 18.sp))
            BasicText(
                if (midi.hasBlePermissions()) "scanning…  (tap a device to connect)"
                else "grant Bluetooth permissions, then reopen",
                style = TextStyle(color = Color(0xAAFFFFFF), fontSize = 13.sp),
                modifier = Modifier.padding(top = 6.dp, bottom = 10.dp))
            midi.bleResults.forEach { (label, dev) ->
                BasicText(
                    label,
                    style = TextStyle(color = Color.White, fontSize = 15.sp),
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable {
                            midi.connectBle(dev)
                            onDismiss()
                        }
                        .padding(vertical = 10.dp))
            }
        }
    }
}
