package com.vibetuned.midisink

import android.app.AlertDialog
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.SystemClock
import android.text.InputType
import android.view.MotionEvent
import android.view.View
import android.widget.EditText
import android.widget.Toast
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/**
 * Performance control strip (PHASE4_SPEC §8, DECISIONS_3 #30/#31): a compact
 * FLOATING palette over the full-canvas lattice — never a docked band
 * (displacing the lattice broke drop-under-finger on device). Wheels and a
 * button built from the joystick primitive: a touch anchors its origin and
 * the §3.2 soft knee shapes Δy (via hostmpe, one implementation). All VALUE
 * state lives in hostmpe_strip_t on the MIDI thread; this view keeps display
 * mirrors only. Every message the strip causes goes out on the master
 * channel — the widget engines enforce that, unit-tested headlessly.
 *
 * Touch hygiene learned on iOS (#31): every widget touch is tracked in the
 * grab table (sustain included) so a release resolves by its grab record,
 * and the long-press CC editor exists ONLY for the two assignable wheels —
 * nothing may ever cancel a held sustain pad.
 */
class ControlStripView(context: Context) : View(context) {

    enum class Widget { PITCH, MOD, ASSIGN_A, ASSIGN_B, SUSTAIN }

    private val density = resources.displayMetrics.density
    // Travel bound for the wheel joysticks, in dp (≙ iOS points): the §3.2
    // knee needs Δ and r_max in ONE metric; the strip is a fixed-size
    // palette, not the lattice.
    private val wheelTravelDp = 60f
    // Full-travel latch sweep in CC units per grab (slow drags accumulate).
    private val latchSweep = 64f

    private class Grab(val widget: Widget, val originY: Float, val originX: Float) {
        var lastShaped = 0f
        var moved = false
        var longPress: Runnable? = null
    }
    private val grabs = HashMap<Int, Grab>()

    // Display mirrors (the engine on the MIDI thread is the truth).
    private var mirrorPitch = 0f
    private val mirrorLatch = floatArrayOf(0f, 0f, 0f)
    private var mirrorSustain = false
    private var mirrorToggleMode = false
    private val mirrorCCs = intArrayOf(1, 23, 24)
    private var ramping = false
    private val effOut = FloatArray(2)
    private val stateOut = FloatArray(8)

    private val paintBg = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.42f * 255).toInt(), 255, 255, 255)
    }
    private val paintBorder = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.15f * 255).toInt(), 0, 0, 0)
        strokeWidth = 0.5f * density
    }
    private val paintSlot = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.25f * 255).toInt(), 0, 0, 0)
        strokeWidth = 1f * density
    }
    private val paintTrack = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.12f * 255).toInt(), 0, 0, 0)
    }
    private val paintNotch = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.30f * 255).toInt(), 0, 0, 0)
    }
    private val paintThumb = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.55f * 255).toInt(), 0, 0, 0)
    }
    private val paintPadOn = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.45f * 255).toInt(), 0, 0, 0)
    }
    private val paintPadOff = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.40f * 255).toInt(), 0, 0, 0)
        strokeWidth = 1f * density
    }
    private val paintText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb((0.60f * 255).toInt(), 0, 0, 0)
        textSize = 11f * density
        textAlign = Paint.Align.CENTER
    }

    init {
        setWillNotDraw(false)
    }

    /** Pull the engine's latched state (mode re-entry, sustain-mode changes). */
    fun syncMirrors(toggleMode: Boolean) {
        NativeBridge.nativeStripState(stateOut)
        mirrorPitch = stateOut[0]
        for (w in 0 until 3) {
            mirrorLatch[w] = stateOut[1 + w]
            mirrorCCs[w] = stateOut[5 + w].toInt()
        }
        mirrorSustain = stateOut[4] > 0.5f
        mirrorToggleMode = toggleMode
        invalidate()
    }

    // -- geometry ------------------------------------------------------------------

    private fun slotRect(w: Widget): RectF {
        val n = Widget.values().size
        val sw = width.toFloat() / n
        val inset = 6f * density
        return RectF(w.ordinal * sw + inset, inset, (w.ordinal + 1) * sw - inset, height - inset)
    }

    private fun widgetAt(x: Float, y: Float): Widget? {
        val slack = 6f * density
        for (w in Widget.values()) {
            val r = slotRect(w)
            if (x >= r.left - slack && x <= r.right + slack && y >= r.top - slack && y <= r.bottom + slack) return w
        }
        return null
    }

    // -- touches: the joystick primitive per widget --------------------------------

    /** Clamped, knee-shaped vertical deflection for a wheel grab (up = +1). */
    private fun shaped(dyPx: Float): Float {
        NativeBridge.nativeJoystickEff(0f, dyPx / density, wheelTravelDp, effOut)
        return -effOut[1]   // screen y grows down; up is positive
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = e.actionIndex
                val x = e.getX(i)
                val y = e.getY(i)
                val w = widgetAt(x, y) ?: return true
                val id = e.getPointerId(i)
                val g = Grab(w, y, x)
                grabs[id] = g
                when (w) {
                    Widget.SUSTAIN -> {
                        mirrorSustain = if (mirrorToggleMode) !mirrorSustain else true
                        NativeBridge.nativeStripSustainDown()
                    }
                    Widget.PITCH -> ramping = false
                    Widget.ASSIGN_A, Widget.ASSIGN_B -> {
                        // The editor gesture is a hand-rolled long-press that
                        // exists ONLY for the assignable wheels (#31).
                        val lp = Runnable {
                            val cur = grabs[id]
                            if (cur === g && !g.moved) {
                                grabs.remove(id)
                                openEditor(w)
                            }
                        }
                        g.longPress = lp
                        postDelayed(lp, 500)
                    }
                    Widget.MOD -> {}
                }
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until e.pointerCount) {
                    val g = grabs[e.getPointerId(i)] ?: continue
                    val y = e.getY(i)
                    if (!g.moved && (abs(y - g.originY) > 8f * density || abs(e.getX(i) - g.originX) > 8f * density)) {
                        g.moved = true
                        g.longPress?.let { removeCallbacks(it) }
                        g.longPress = null
                    }
                    val s = shaped(y - g.originY)
                    when (g.widget) {
                        Widget.PITCH -> {
                            mirrorPitch = s
                            NativeBridge.nativeStripPitchMove(s)
                        }
                        Widget.MOD, Widget.ASSIGN_A, Widget.ASSIGN_B -> {
                            // Relative accumulation (§8): only the CHANGE in
                            // shaped position moves the value — a regrasp
                            // contributes zero.
                            val wheel = g.widget.ordinal - Widget.MOD.ordinal
                            val delta = (s - g.lastShaped) * latchSweep
                            mirrorLatch[wheel] = (mirrorLatch[wheel] + delta).coerceIn(0f, 127f)
                            NativeBridge.nativeStripLatchMove(wheel, delta)
                        }
                        Widget.SUSTAIN -> {}
                    }
                    g.lastShaped = s
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> release(e.getPointerId(e.actionIndex))
            MotionEvent.ACTION_CANCEL -> {
                for (id in grabs.keys.toList()) release(id)
            }
        }
        invalidate()
        return true
    }

    private fun release(id: Int) {
        val g = grabs.remove(id) ?: return
        g.longPress?.let { removeCallbacks(it) }
        when (g.widget) {
            Widget.PITCH -> {
                NativeBridge.nativeStripPitchRelease()
                animateSpringReturn()
            }
            Widget.SUSTAIN -> {
                if (!mirrorToggleMode) mirrorSustain = false
                NativeBridge.nativeStripSustainUp()   // toggle mode: engine-side no-op
            }
            Widget.MOD, Widget.ASSIGN_A, Widget.ASSIGN_B -> {}
        }
    }

    // Visual mirror of the engine's 50 ms return ramp (the MIDI ramp itself is
    // emitted by hostmpe_strip_tick on the MIDI thread's drain).
    private fun animateSpringReturn() {
        ramping = true
        val v0 = mirrorPitch
        val t0 = SystemClock.uptimeMillis()
        val step = object : Runnable {
            override fun run() {
                if (!ramping) return
                val f = (SystemClock.uptimeMillis() - t0) / 50f
                if (f >= 1f) {
                    mirrorPitch = 0f
                    ramping = false
                } else {
                    mirrorPitch = v0 * (1f - f)
                    postOnAnimation(this)
                }
                invalidate()
            }
        }
        step.run()
    }

    // -- the assignable-wheel CC editor (§8) -----------------------------------------

    private fun openEditor(w: Widget) {
        val wheel = w.ordinal - Widget.MOD.ordinal
        val current = mirrorCCs[wheel]
        val input = EditText(context).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            setText(current.toString())
            // Typing REPLACES the current number: the field opens pre-filled,
            // and without this every keystroke inserts at the cursor (a tap
            // then "25" over "23" gives 2523, silently out of range).
            setSelectAllOnFocus(true)
            requestFocus()
        }
        AlertDialog.Builder(context)
            .setTitle("Assign wheel ${if (w == Widget.ASSIGN_A) "A" else "B"}")
            .setMessage("7-bit CC number (0–127). Currently CC $current. " +
                "Protocol CCs (1, 6, 38, 64, 98–101, 120–127) are refused.")
            .setView(input)
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Assign") { _, _ ->
                val cc = input.text.toString().trim().toIntOrNull()
                if (cc == null || cc < 0 || cc > 127) {
                    // Say so — a silently ignored entry is indistinguishable
                    // from a refused one.
                    Toast.makeText(context, "Enter a CC number from 0 to 127",
                                   Toast.LENGTH_SHORT).show()
                    return@setPositiveButton
                }
                val assigned = NativeBridge.nativeStripAssign(wheel, cc)
                if (assigned >= 0) {
                    mirrorCCs[wheel] = assigned
                } else {
                    // The engine refuses the protocol CCs (§8 / DECISIONS_3
                    // #30): a strip-assigned CC 6 on the master would corrupt
                    // the DAW's RPN handshake mid-performance.
                    Toast.makeText(context, "CC $cc is a protocol CC — refused",
                                   Toast.LENGTH_SHORT).show()
                }
                invalidate()
            }
            .show()
    }

    // -- drawing -----------------------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val corner = 12f * density
        canvas.drawRoundRect(0f, 0f, width.toFloat(), height.toFloat(), corner, corner, paintBg)
        canvas.drawRoundRect(0f, 0f, width.toFloat(), height.toFloat(), corner, corner, paintBorder)
        for (w in Widget.values()) {
            val r = slotRect(w)
            canvas.drawRoundRect(r, 8f * density, 8f * density, paintSlot)
            val label = when (w) {
                Widget.PITCH -> "Pitch"
                Widget.MOD -> "Mod"
                Widget.ASSIGN_A -> "CC ${mirrorCCs[1]}"
                Widget.ASSIGN_B -> "CC ${mirrorCCs[2]}"
                Widget.SUSTAIN -> if (mirrorToggleMode) "Sus ⇥" else "Sus"
            }
            canvas.drawText(label, r.centerX(), r.bottom - 5f * density, paintText)

            if (w == Widget.SUSTAIN) {
                val pad = RectF(r.left + r.width() * 0.22f, r.top + r.height() * 0.24f - 6f * density,
                                r.right - r.width() * 0.22f, r.bottom - r.height() * 0.24f - 6f * density)
                canvas.drawRoundRect(pad, 6f * density, 6f * density,
                                     if (mirrorSustain) paintPadOn else paintPadOff)
                continue
            }
            // Wheel track + thumb.
            val track = RectF(r.centerX() - 1.5f * density, r.top + 8f * density,
                              r.centerX() + 1.5f * density, r.bottom - 24f * density)
            canvas.drawRect(track, paintTrack)
            val frac = if (w == Widget.PITCH) {
                canvas.drawRect(r.centerX() - 8f * density, track.centerY() - 0.5f * density,
                                r.centerX() + 8f * density, track.centerY() + 0.5f * density, paintNotch)
                0.5f + 0.5f * mirrorPitch
            } else {
                mirrorLatch[w.ordinal - Widget.MOD.ordinal] / 127f
            }
            val ty = track.bottom - frac * track.height()
            canvas.drawCircle(r.centerX(), ty, 7f * density, paintThumb)
        }
    }

    @Suppress("unused")
    private fun clamp01(v: Float) = max(0f, min(1f, v))
}
