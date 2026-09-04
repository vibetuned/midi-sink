package com.vibetuned.midisink

import android.media.midi.MidiDeviceService
import android.media.midi.MidiDeviceStatus
import android.media.midi.MidiReceiver
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import java.io.IOException

private const val TAG = "sumi-shell"

/**
 * PHASE4 §5.4(b): the virtual MIDI device — the guaranteed path for on-device
 * DAWs. Declared in the manifest with res/xml/midi_device_info.xml (one
 * OUTPUT port: the play surface as a 15-voice MPE controller). MidiManager
 * binds this service in OUR process when a client opens the device; bytes
 * reach the client through the output port's receiver. A client opening the
 * port is a sink appearing mid-session: the handshake is re-sent
 * (DECISIONS_3 #28) so the DAW learns the ±48 bend range.
 */
class SumiMidiDeviceService : MidiDeviceService() {

    companion object {
        @Volatile private var receiver: MidiReceiver? = null
        /** Clients currently holding our output port open. A Compose state so
         *  the settings sheet's client count refreshes when it changes, not
         *  when something unrelated happens to recompose. */
        val openClientsState = mutableStateOf(0)
        @Volatile var openClients = 0
            private set

        /** Native MIDI thread: raw bytes to every connected client. */
        @JvmStatic
        fun send(bytes: ByteArray, len: Int): Boolean {
            val r = receiver ?: return false
            if (openClients <= 0) return false
            return try {
                r.send(bytes, 0, len)
                true
            } catch (e: IOException) {
                false
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        receiver = outputPortReceivers.firstOrNull()
        Log.i(TAG, "[virtual] MidiDeviceService created, receiver=${receiver != null}")
    }

    /** We publish only; nothing flows from a DAW into the surface here. */
    override fun onGetInputPortReceivers(): Array<MidiReceiver> = emptyArray()

    override fun onDeviceStatusChanged(status: MidiDeviceStatus) {
        val n = status.getOutputPortOpenCount(0)
        val was = openClients
        openClients = n
        openClientsState.value = n
        Log.i(TAG, "[virtual] output port open count $was -> $n")
        if (n > was && MidiOutputs.started) {
            NativeBridge.nativeSinkAppeared(MidiOutputs.SINK_VIRTUAL)
        }
    }

    override fun onDestroy() {
        receiver = null
        openClients = 0
        openClientsState.value = 0
        super.onDestroy()
    }
}
