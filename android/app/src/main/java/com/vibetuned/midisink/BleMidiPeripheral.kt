package com.vibetuned.midisink

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattServerCallback
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.os.Build
import android.os.ParcelUuid
import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import java.util.ArrayDeque
import java.util.UUID

private const val TAG = "sumi-shell"

/**
 * PHASE4 §5.4(c): BLE-MIDI PERIPHERAL — the tablet advertises the MMA
 * BLE-MIDI service so a desktop DAW (BlueZ/PipeWire on Linux, CoreMIDI on a
 * Mac) connects to the surface. Android's MidiManager only implements the
 * CENTRAL role (that is how the ROLI reaches us), so the peripheral is a
 * plain GATT server here: service 03B80E5A…, one characteristic 7772E5DB…
 * (read / write-without-response / notify) with the CCCD, packets framed
 * per BLE-MIDI 1.0 (header timestamp-high byte, then per message a
 * timestamp-low byte followed by the full MIDI message), batched up to the
 * negotiated MTU. Notifications are flow-controlled one in flight per link
 * (Android drops back-to-back notifies otherwise). The budget itself is the
 * native limiter's job (~300 msg/s, DECISIONS_3 #3); this class only frames
 * and ships.
 *
 * send() runs on the native MIDI thread; GATT callbacks on binder threads —
 * everything below is guarded by `lock`.
 */
class BleMidiPeripheral(private val context: Context, private val onSubscribed: () -> Unit) {

    companion object {
        /** Packets held for a stalled link before the safety valve trims. */
        private const val MAX_QUEUE = 256
        val MIDI_SERVICE: UUID = UUID.fromString("03B80E5A-EDE8-4B33-A751-6CE34EC4C700")
        val MIDI_CHAR: UUID = UUID.fromString("7772E5DB-3868-4112-A1A9-F2669D106BF3")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val bt = context.getSystemService(BluetoothManager::class.java)
    private var server: BluetoothGattServer? = null
    private var advertiser: BluetoothLeAdvertiser? = null
    private var characteristic: BluetoothGattCharacteristic? = null
    private val lock = Any()
    private val subscribed = LinkedHashMap<String, BluetoothDevice>()
    private val mtu = HashMap<String, Int>()
    private val queue = ArrayDeque<Packet>()
    /** Notifications sent and not yet acknowledged. It is a COUNT over the
     *  subscribed set, so every membership change must re-clamp it: a central
     *  that drops (or unsubscribes) mid-notification never acks, and without
     *  the clamp the counter never returns to 0 and the pipe stays dead for
     *  the remaining centrals for the rest of the session. */
    private var acksPending = 0
    private var lastPumpMs = 0L
    private var cccdValue = BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE

    val state = mutableStateOf("BLE: off")
    /** True from the moment start() is accepted — `advertising` only turns
     *  true in the async onStartSuccess, so a second start() before that
     *  callback would fail with ALREADY_STARTED (3). */
    @Volatile var requested = false
        private set
    @Volatile var advertising = false
        private set

    fun start(): Boolean {
        if (requested || advertising) return true
        val adapter = bt?.adapter
        if (adapter == null || !adapter.isEnabled) {
            state.value = "BLE: Bluetooth is off"
            return false
        }
        val adv = adapter.bluetoothLeAdvertiser
        if (adv == null) {
            state.value = "BLE: advertising unsupported on this device"
            return false
        }
        try {
            val srv = bt.openGattServer(context, gattCallback) ?: run {
                state.value = "BLE: GATT server unavailable"
                return false
            }
            server = srv
            val ch = BluetoothGattCharacteristic(
                MIDI_CHAR,
                BluetoothGattCharacteristic.PROPERTY_READ or
                    BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE or
                    BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ or
                    BluetoothGattCharacteristic.PERMISSION_WRITE)
            val cccd = BluetoothGattDescriptor(
                CCCD,
                BluetoothGattDescriptor.PERMISSION_READ or BluetoothGattDescriptor.PERMISSION_WRITE)
            ch.addDescriptor(cccd)
            characteristic = ch
            val service = BluetoothGattService(MIDI_SERVICE, BluetoothGattService.SERVICE_TYPE_PRIMARY)
            service.addCharacteristic(ch)
            srv.addService(service)

            val settings = AdvertiseSettings.Builder()
                .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                .setConnectable(true)
                .setTimeout(0)
                .build()
            val data = AdvertiseData.Builder()
                .addServiceUuid(ParcelUuid(MIDI_SERVICE))
                .setIncludeDeviceName(false)
                .build()
            val scanResponse = AdvertiseData.Builder()
                .setIncludeDeviceName(true)
                .build()
            adv.startAdvertising(settings, data, scanResponse, advertiseCallback)
            advertiser = adv
            requested = true
            state.value = "BLE: starting advertising…"
            return true
        } catch (e: SecurityException) {
            state.value = "BLE: missing Bluetooth permissions"
            Log.w(TAG, "[ble] $e")
            stop()
            return false
        }
    }

    fun stop() {
        try {
            advertiser?.stopAdvertising(advertiseCallback)
        } catch (_: SecurityException) {
        }
        advertiser = null
        requested = false
        advertising = false
        // Null what pumpLocked reads UNDER the lock, then close outside it
        // (close() can call back into the GATT callbacks).
        val srv: BluetoothGattServer?
        synchronized(lock) {
            srv = server
            server = null
            characteristic = null
            subscribed.clear()
            mtu.clear()
            queue.clear()
            acksPending = 0
        }
        try {
            srv?.close()
        } catch (_: SecurityException) {
        }
        state.value = "BLE: off"
    }

    val connectedCount: Int
        get() = synchronized(lock) { subscribed.size }

    private val advertiseCallback = object : AdvertiseCallback() {
        override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
            advertising = true
            state.value = "BLE: advertising as a BLE-MIDI device (connect from the DAW's Bluetooth settings)"
            Log.i(TAG, "[ble] advertising started")
        }
        override fun onStartFailure(errorCode: Int) {
            requested = false
            advertising = false
            state.value = "BLE: advertising failed (code $errorCode)"
            Log.w(TAG, "[ble] advertising failed: $errorCode")
        }
    }

    private val gattCallback = object : BluetoothGattServerCallback() {
        override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) {
            Log.i(TAG, "[ble] ${device.address} connection state $newState (status $status)")
            if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                synchronized(lock) {
                    subscribed.remove(device.address)
                    mtu.remove(device.address)
                    clampAcksLocked()
                    if (acksPending == 0) pumpLocked()   // the pipe must not stall
                }
                updateState()
            }
        }

        override fun onMtuChanged(device: BluetoothDevice, newMtu: Int) {
            synchronized(lock) { mtu[device.address] = newMtu }
            Log.i(TAG, "[ble] ${device.address} MTU $newMtu")
        }

        override fun onCharacteristicReadRequest(device: BluetoothDevice, requestId: Int, offset: Int,
                                                 ch: BluetoothGattCharacteristic) {
            // BLE-MIDI 1.0: a read of the MIDI I/O characteristic returns an
            // empty payload (the central uses it to confirm the profile).
            try {
                server?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, ByteArray(0))
            } catch (_: SecurityException) {
            }
        }

        /**
         * A central writing TO us (a DAW playing the visualizer over BLE) is
         * acknowledged and dropped: inbound MIDI has its own path — the
         * CENTRAL role through MidiManager (that is how the ROLI reaches
         * hostmpe's occupancy mask), and mixing a second ingest route in
         * would break the §5.2 single-producer accounting. Out of scope
         * deliberately, not forgotten.
         */
        override fun onCharacteristicWriteRequest(device: BluetoothDevice, requestId: Int,
                                                  ch: BluetoothGattCharacteristic, preparedWrite: Boolean,
                                                  responseNeeded: Boolean, offset: Int, value: ByteArray?) {
            if (responseNeeded) {
                try {
                    server?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null)
                } catch (_: SecurityException) {
                }
            }
        }

        override fun onDescriptorReadRequest(device: BluetoothDevice, requestId: Int, offset: Int,
                                             descriptor: BluetoothGattDescriptor) {
            try {
                server?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, cccdValue)
            } catch (_: SecurityException) {
            }
        }

        override fun onDescriptorWriteRequest(device: BluetoothDevice, requestId: Int,
                                              descriptor: BluetoothGattDescriptor, preparedWrite: Boolean,
                                              responseNeeded: Boolean, offset: Int, value: ByteArray?) {
            var newlySubscribed = false
            if (descriptor.uuid == CCCD && value != null) {
                val enable = value.contentEquals(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) ||
                    value.contentEquals(BluetoothGattDescriptor.ENABLE_INDICATION_VALUE)
                synchronized(lock) {
                    cccdValue = value
                    if (enable) {
                        newlySubscribed = subscribed.put(device.address, device) == null
                    } else {
                        subscribed.remove(device.address)
                        clampAcksLocked()
                        if (acksPending == 0) pumpLocked()
                    }
                }
                Log.i(TAG, "[ble] ${device.address} notifications ${if (enable) "enabled" else "disabled"}")
            }
            if (responseNeeded) {
                try {
                    server?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null)
                } catch (_: SecurityException) {
                }
            }
            updateState()
            // A central subscribing IS a sink appearing: re-send the handshake.
            if (newlySubscribed) onSubscribed()
        }

        override fun onNotificationSent(device: BluetoothDevice, status: Int) {
            synchronized(lock) {
                if (acksPending > 0) acksPending--
                if (acksPending == 0) pumpLocked()
            }
        }
    }

    private fun updateState() {
        val n = connectedCount
        state.value = when {
            !advertising -> "BLE: off"
            n == 0 -> "BLE: advertising as a BLE-MIDI device (connect from the DAW's Bluetooth settings)"
            else -> "BLE: $n central(s) subscribed — streaming"
        }
    }

    // -- framing + flow control (native MIDI thread + binder threads) ------------

    /**
     * One BLE-MIDI packet under construction or waiting for the link: a
     * header byte (timestamp-high) followed by (timestamp-low, MIDI message)
     * pairs. Messages accumulate into the TAIL packet while a notification is
     * in flight — that is what keeps a burst (the 80-message MCM/RPN0
     * handshake, dispatched one message at a time) to a couple of packets
     * instead of eighty (DECISIONS_3 #44).
     */
    private class Packet(val capacity: Int, tsHigh: Int) {
        val buf = ByteArray(capacity)
        var len = 0
        val tsHigh = tsHigh
        var lastTsLow = -1
        /** §5.3's never-dropped class: a packet carrying a Note On/Off is the
         *  last thing the safety valve may discard. */
        var hasNotes = false
        init {
            buf[0] = (0x80 or (tsHigh and 0x3F)).toByte()
            len = 1
        }
        fun room(n: Int) = len + 1 + n <= capacity
        /** BLE-MIDI 1.0: timestamps inside one packet must not go backwards. */
        fun accepts(tsLow: Int, n: Int) = room(n) && tsLow >= lastTsLow
        fun append(tsLow: Int, src: ByteArray, off: Int, n: Int) {
            buf[len++] = (0x80 or (tsLow and 0x7F)).toByte()
            System.arraycopy(src, off, buf, len, n)
            len += n
            lastTsLow = tsLow
            val kind = src[off].toInt() and 0xF0
            if (kind == 0x90 || kind == 0x80) hasNotes = true
        }
        fun bytes(): ByteArray = buf.copyOf(len)
    }

    /** Raw MIDI 1.0 bytes (complete messages) -> BLE-MIDI packets -> notify. */
    fun send(bytes: ByteArray, len: Int): Boolean {
        synchronized(lock) {
            if (subscribed.isEmpty() || characteristic == null) return false
            // Payload budget: the smallest negotiated MTU minus the 3-byte ATT
            // header; default MTU 23 -> 20 bytes (4 three-byte messages).
            // Only the MTUs of devices that are actually subscribed count, and
            // a subscriber missing from the map never negotiated one — it gets
            // the 23-byte default rather than a packet sized for a peer.
            val minMtu = subscribed.keys.minOfOrNull { mtu[it] ?: 23 } ?: 23
            val maxPayload = (minMtu - 3).coerceAtLeast(20)
            val ms = SystemClock.uptimeMillis() and 0x1FFF
            val tsHigh = ((ms shr 7) and 0x3F).toInt()
            val tsLow = (ms and 0x7F).toInt()
            var i = 0
            while (i < len) {
                val st = bytes[i].toInt() and 0xFF
                val kind = st and 0xF0
                val msgLen = when {
                    st < 0x80 -> 1                       // stray data byte: pass through
                    kind == 0xC0 || kind == 0xD0 -> 2
                    st >= 0xF0 -> 1                      // realtime/system: never generated
                    else -> 3
                }
                val n = minOf(msgLen, len - i)
                var tail = queue.peekLast()
                if (tail == null || tail.tsHigh != tsHigh || !tail.accepts(tsLow, n)) {
                    tail = Packet(maxPayload, tsHigh)
                    queue.addLast(tail)
                }
                tail.append(tsLow, bytes, i, n)
                i += n
            }
            trimLocked()
            // Watchdog: an ack that never arrives (a link that went away
            // between the notify and its callback) must not park the pipe for
            // the rest of the session.
            val now = SystemClock.uptimeMillis()
            if (acksPending == 0 || now - lastPumpMs > 250) {
                if (acksPending != 0) {
                    acksPending = 0
                    Log.w(TAG, "[ble] notification ack overdue — resuming the pipe")
                }
                pumpLocked()
            }
            return true
        }
    }

    /**
     * lock held. Safety valve: the native limiter already budgets the rate, so
     * a backlog this deep means the link stalled. Continuous packets go
     * newest-first (a superseded bend or pressure value costs nothing);
     * packets carrying notes are touched only when nothing else is left,
     * because §5.3 puts Note On/Off outside the droppable set — and dropping
     * the OLDEST silently ate the MCM the first time this shipped (#44).
     */
    private fun trimLocked() {
        if (queue.size <= MAX_QUEUE) return
        val it = queue.descendingIterator()
        while (queue.size > MAX_QUEUE && it.hasNext()) {
            if (!it.next().hasNotes) it.remove()
        }
        var dropped = 0
        while (queue.size > MAX_QUEUE) {
            queue.pollLast()
            dropped++
        }
        if (dropped > 0) Log.w(TAG, "[ble] link stalled: dropped $dropped packet(s) with notes")
    }

    /** lock held. Clamp the in-flight count to what can still acknowledge it. */
    private fun clampAcksLocked() {
        if (acksPending > subscribed.size) acksPending = subscribed.size
        if (subscribed.isEmpty()) {
            acksPending = 0
            queue.clear()
        }
    }

    /** lock held. One packet in flight per link; the next goes on the ack. */
    private fun pumpLocked() {
        val srv = server ?: return
        val ch = characteristic ?: return
        // Never ship the packet still accumulating if a newer one exists to
        // send first — the queue is strictly FIFO, so just take the head.
        val pkt = queue.pollFirst() ?: return
        lastPumpMs = SystemClock.uptimeMillis()
        val payload = pkt.bytes()
        var sent = 0
        for (dev in subscribed.values) {
            try {
                val ok = if (Build.VERSION.SDK_INT >= 33) {
                    srv.notifyCharacteristicChanged(dev, ch, false, payload) == BluetoothGatt.GATT_SUCCESS
                } else {
                    @Suppress("DEPRECATION")
                    ch.value = payload
                    @Suppress("DEPRECATION")
                    srv.notifyCharacteristicChanged(dev, ch, false)
                }
                if (ok) sent++
            } catch (_: SecurityException) {
            }
        }
        acksPending = sent
        if (sent == 0) {
            // No live link took it: stop draining, or the loop spins the
            // whole backlog into the void on a half-open connection.
            queue.clear()
        }
    }
}
