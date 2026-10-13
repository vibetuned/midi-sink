package com.vibetuned.midisink

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.media.midi.MidiDevice
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiInputPort
import android.media.midi.MidiManager
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import java.io.IOException

private const val TAG = "sumi-shell"

/**
 * Outbound transports (PROJECT_SPEC.md §8.5, Android rows): one generator, N
 * sinks. The per-sink rate limiters live NATIVELY on the MIDI thread (the
 * iOS MidiOutputs' limiter fan-out, moved into sumi_play.cpp so hostmpe stays
 * the single owner); this object owns only the WIRE endpoints and the sink
 * lifecycle:
 *
 *  0  USB gadget MIDI — the PRIMARY sink. When the user flips the system USB
 *     mode to MIDI, Android exposes the class-compliant peripheral as a
 *     MidiDeviceInfo ("Android USB Peripheral Port", TYPE_USB with no host
 *     UsbDevice attached); we open its input port and any host OS sees the
 *     tablet as a USB-MIDI device (amidi -l on Linux). Status is surfaced
 *     from the sticky USB_STATE broadcast: active / charge-only / unsupported.
 *  1  MidiDeviceService virtual device — on-device DAWs (SumiMidiDeviceService).
 *  2  BLE-MIDI peripheral (BleMidiPeripheral) — advertise; budget-limited.
 *
 * Every sink coming up mid-session reports nativeSinkAppeared, which re-sends
 * the MCM/RPN0 handshake + strip announce (debounced natively, DECISIONS_3 #28).
 *
 * Per-sink message counters live NATIVELY beside the limiters (the status
 * line's `out u…/v…/b…/n…`) — one number, not two that can disagree.
 *
 * write() is called from the native MIDI thread; everything else runs on the
 * main thread. Endpoint fields are @Volatile for that reason.
 */
object MidiOutputs {
    const val SINK_USB = 0
    const val SINK_VIRTUAL = 1
    const val SINK_BLE = 2

    private const val ACTION_USB_STATE = "android.hardware.usb.action.USB_STATE"

    @Volatile private var usbPort: MidiInputPort? = null
    private var usbDevice: MidiDevice? = null
    private var usbDeviceId = -1
    @Volatile var ble: BleMidiPeripheral? = null
        private set

    val usbStatus = mutableStateOf("USB: …")
    val usbActive = mutableStateOf(false)
    @Volatile var started = false
        private set

    private var midiManager: MidiManager? = null
    private var appContext: Context? = null
    private val handler = Handler(Looper.getMainLooper())
    private var usbConnected = false
    private var usbMidiFunction = false
    private var usbReceiver: BroadcastReceiver? = null
    private var callback: MidiManager.DeviceCallback? = null

    fun start(context: Context) {
        if (started) return
        appContext = context.applicationContext
        val mm = context.getSystemService(Context.MIDI_SERVICE) as MidiManager
        midiManager = mm
        started = true
        val cb = object : MidiManager.DeviceCallback() {
            override fun onDeviceAdded(info: MidiDeviceInfo) {
                if (isGadgetPort(info)) openGadget(info)
            }
            override fun onDeviceRemoved(info: MidiDeviceInfo) {
                if (info.id == usbDeviceId) closeGadget("peripheral port removed")
            }
        }
        callback = cb
        @Suppress("DEPRECATION")
        mm.registerDeviceCallback(cb, handler)
        @Suppress("DEPRECATION")
        mm.devices.firstOrNull { isGadgetPort(it) }?.let { openGadget(it) }

        // USB mode status: the sticky USB_STATE broadcast carries "connected"
        // and one boolean per ACTIVE gadget function ("midi" when the user
        // picked MIDI in the USB preferences).
        val rcv = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) = onUsbState(i)
        }
        usbReceiver = rcv
        val filter = IntentFilter(ACTION_USB_STATE)
        val sticky = if (android.os.Build.VERSION.SDK_INT >= 33)
            context.registerReceiver(rcv, filter, Context.RECEIVER_EXPORTED)
        else
            context.registerReceiver(rcv, filter)
        if (sticky != null) onUsbState(sticky) else updateUsbStatus()
        ble = BleMidiPeripheral(context.applicationContext) {
            NativeBridge.nativeSinkAppeared(SINK_BLE)
        }
    }

    fun stop(context: Context) {
        if (!started) return
        started = false
        callback?.let { midiManager?.unregisterDeviceCallback(it) }
        callback = null
        usbReceiver?.let { runCatching { context.unregisterReceiver(it) } }
        usbReceiver = null
        closeGadget("shutdown")
        ble?.stop()
        ble = null
        stopNetwork()
    }

    // -- step 67 (DECISIONS_9 #14): the network session — RTP-MIDI with Apple's session protocol, the endpoint
    // in the native library on the MIDI thread; this object only ADVERTISES it over mDNS as _apple-midi._udp,
    // so Audio MIDI Setup (macOS) and rtpMIDI (Windows) list the tablet and invite it.
    private var nsd: NsdManager? = null
    private var nsdListener: NsdManager.RegistrationListener? = null
    val netStatus = mutableStateOf("")
    val netRunning: Boolean get() = nsdListener != null
    private var netName = ""

    fun startNetwork(context: Context, name: String): Int {
        if (nsdListener != null) return 1
        val port = NativeBridge.nativeNetworkStart(name)
        if (port <= 0) { netStatus.value = "Network session: no free port pair"; return 0 }
        val info = NsdServiceInfo().apply { serviceName = name; serviceType = "_apple-midi._udp"; setPort(port) }
        val mgr = context.getSystemService(Context.NSD_SERVICE) as NsdManager
        val l = object : NsdManager.RegistrationListener {
            override fun onServiceRegistered(i: NsdServiceInfo) { Log.i(TAG, "[net] advertised '${i.serviceName}' on port $port") }
            override fun onRegistrationFailed(i: NsdServiceInfo, err: Int) { Log.w(TAG, "[net] advertisement failed: $err"); netStatus.value = "Network session: on port $port, not advertised (mDNS error $err) — connect by address" }
            override fun onServiceUnregistered(i: NsdServiceInfo) {}
            override fun onUnregistrationFailed(i: NsdServiceInfo, err: Int) {}
        }
        mgr.registerService(info, NsdManager.PROTOCOL_DNS_SD, l)
        nsd = mgr; nsdListener = l; netName = name
        netStatus.value = "Network session: \"$name\" on port $port — no host connected yet"
        return port
    }
    fun stopNetwork() {
        nsdListener?.let { l -> runCatching { nsd?.unregisterService(l) } }
        nsdListener = null; nsd = null
        NativeBridge.nativeNetworkStop()
        netStatus.value = ""
    }
    /** Once a second from the activity: the MIDI thread's line ("port P|N|names|tx|rx") as the settings' status. */
    fun tickNetwork() {
        if (nsdListener == null) return
        val s = NativeBridge.nativeNetworkStatus()
        if (s.isEmpty()) return
        val f = s.split("|")
        val port = f.getOrNull(0) ?: ""; val n = f.getOrNull(1)?.toIntOrNull() ?: 0; val names = f.getOrNull(2) ?: ""
        val tx = f.getOrNull(3) ?: "0"; val rx = f.getOrNull(4) ?: "0"
        netStatus.value = if (n == 0) "Network session: \"$netName\" on $port — no host connected yet"
                          else "Network session: \"$netName\" on $port — connected: $names · sent $tx, received $rx"
    }

    // -- USB gadget (peripheral) port -------------------------------------------

    /**
     * The peripheral port is a TYPE_USB device WITHOUT a host-side UsbDevice
     * (host-mode MIDI devices carry PROPERTY_USB_DEVICE); the framework names
     * it "Android USB Peripheral Port". Either signal is enough.
     */
    private fun isGadgetPort(info: MidiDeviceInfo): Boolean {
        if (info.inputPortCount == 0) return false
        val name = info.properties.getString(MidiDeviceInfo.PROPERTY_NAME) ?: ""
        val product = info.properties.getString(MidiDeviceInfo.PROPERTY_PRODUCT) ?: ""
        val peripheralNamed = name.contains("Peripheral", ignoreCase = true) ||
            product.contains("Peripheral", ignoreCase = true)
        val usbNoHost = info.type == MidiDeviceInfo.TYPE_USB &&
            !info.properties.containsKey(MidiDeviceInfo.PROPERTY_USB_DEVICE)
        return peripheralNamed || usbNoHost
    }

    private fun openGadget(info: MidiDeviceInfo) {
        if (usbDeviceId == info.id || usbPort != null) return
        usbDeviceId = info.id
        midiManager?.openDevice(info, { dev ->
            if (dev == null) {
                Log.w(TAG, "[usb] openDevice failed for the peripheral port")
                usbDeviceId = -1
                updateUsbStatus()
                return@openDevice
            }
            val port = dev.openInputPort(0)
            if (port == null) {
                Log.w(TAG, "[usb] openInputPort(0) failed on the peripheral port")
                runCatching { dev.close() }
                usbDeviceId = -1
                updateUsbStatus()
                return@openDevice
            }
            usbDevice = dev
            usbPort = port
            Log.i(TAG, "[usb] gadget MIDI port open: ${info.properties.getString(MidiDeviceInfo.PROPERTY_NAME)}")
            updateUsbStatus()
            NativeBridge.nativeSinkAppeared(SINK_USB)
        }, handler)
    }

    private fun closeGadget(why: String) {
        val p = usbPort
        usbPort = null
        p?.let { runCatching { it.close() } }
        usbDevice?.let { runCatching { it.close() } }
        usbDevice = null
        if (usbDeviceId != -1) Log.i(TAG, "[usb] gadget MIDI port closed ($why)")
        usbDeviceId = -1
        updateUsbStatus()
    }

    private fun onUsbState(i: Intent) {
        usbConnected = i.getBooleanExtra("connected", false)
        usbMidiFunction = i.getBooleanExtra("midi", false)
        Log.i(TAG, "[usb] state: connected=$usbConnected midi=$usbMidiFunction port=${usbPort != null}")
        updateUsbStatus()
        if (usbMidiFunction && usbPort == null) {
            // The peripheral device normally appears within a second of the
            // mode flip; an OEM without the ALSA gadget never publishes it.
            handler.postDelayed({ updateUsbStatus() }, 3000)
        }
    }

    private fun updateUsbStatus() {
        val text = when {
            usbPort != null -> "USB-MIDI: active — peripheral port open (host sees a USB-MIDI device)"
            !usbConnected -> "USB: not connected"
            usbMidiFunction -> "USB-MIDI: mode on, no peripheral port yet (unsupported on this device if it never appears)"
            else -> "USB: connected in charge/file-transfer mode — set \"Use USB for\" to MIDI in the system USB preferences"
        }
        usbStatus.value = text
        usbActive.value = usbPort != null
    }

    // -- BLE peripheral ------------------------------------------------------------

    fun hasAdvertisePermissions(context: Context): Boolean {
        if (android.os.Build.VERSION.SDK_INT < 31) return true
        return context.checkSelfPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE) ==
            PackageManager.PERMISSION_GRANTED &&
            context.checkSelfPermission(android.Manifest.permission.BLUETOOTH_CONNECT) ==
            PackageManager.PERMISSION_GRANTED
    }

    // -- wire (native MIDI thread) --------------------------------------------------

    /** Raw MIDI 1.0 bytes for one sink; true when a live endpoint took them. */
    @JvmStatic
    fun write(sink: Int, bytes: ByteArray, len: Int): Boolean {
        return when (sink) {
            SINK_USB -> {
                val p = usbPort ?: return false
                try {
                    p.send(bytes, 0, len)
                    true
                } catch (e: IOException) {
                    Log.w(TAG, "[usb] send failed: $e")
                    false
                } catch (e: IllegalStateException) {
                    false
                }
            }
            SINK_VIRTUAL -> SumiMidiDeviceService.send(bytes, len)
            SINK_BLE -> ble?.send(bytes, len) ?: false
            else -> false
        }
    }
}
