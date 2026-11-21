package com.xern.xrnkey

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Looper
import android.view.MotionEvent
import com.xern.xrnkey.keyboard.KeyboardView
import com.xern.xrnkey.keyboard.Palette
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import java.time.Duration

/**
 * Holding and sliding on the keys, through real touch events on a keyboard in a window.
 * At xxhdpi (dp = 3) and 1080 px wide: the strip is 144 px, then five rows of 162 px (the
 * number row, qwerty, asdf, zxcv, the bottom row); a letter key is 108 px wide.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w360dp-h914dp-xxhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class KeyTouchTest {
    private val events = mutableListOf<String>()
    private var t = 0L

    private fun keyboard(palette: Palette = Palette.DARK, setup: KeyboardView.() -> Unit = {}): KeyboardView {
        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        val v = KeyboardView(activity)
        v.palette = palette
        v.setup()
        v.listener = object : KeyboardView.Listener {
            override fun onText(text: String) { events += text }
            override fun onBackspace() { events += "⌫" }
            override fun onEnter() { events += "⏎" }
            override fun onPickKeyboard() { events += "pick" }
            override fun onCursorStart() { events += "cursor" }
            override fun onCursor(dx: Int, dy: Int) { events += "move $dx $dy" }
            override fun onCursorEnd() { events += "end" }
        }
        activity.setContentView(v)
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(1080, v.width)
        return v
    }

    private fun touch(v: KeyboardView, action: Int, x: Float, y: Float) {
        val e = MotionEvent.obtain(0, t, action, x, y, 0)
        v.onTouchEvent(e)
        e.recycle()
    }

    private fun hold(ms: Long) {
        t += ms
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(ms))
    }

    private fun shot(name: String, v: KeyboardView) {
        val dir = File(System.getProperty("xrnkey.shots") ?: return)
        val bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
        v.draw(Canvas(bmp))
        dir.mkdirs()
        File(dir, "$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private val spaceY = 873f
    private val topRowY = 387f

    @Test
    fun slidingAlongTheSpaceBarMovesTheCursorAndTypesNoSpace() {
        val v = keyboard()
        val slop = android.view.ViewConfiguration.get(v.context).scaledTouchSlop
        touch(v, MotionEvent.ACTION_DOWN, 540f, spaceY)
        touch(v, MotionEvent.ACTION_MOVE, 545f, spaceY)        // within the slop: nothing yet
        // Past the slop, every 10 dp (30 px) is a character.
        touch(v, MotionEvent.ACTION_MOVE, 540f + slop + 150f, spaceY)
        touch(v, MotionEvent.ACTION_MOVE, 540f + slop + 90f, spaceY)
        touch(v, MotionEvent.ACTION_UP, 540f + slop + 90f, spaceY)
        assertEquals(listOf("cursor", "move 5 0", "move -2 0", "end"), events)
        events.clear()
        touch(v, MotionEvent.ACTION_DOWN, 540f, spaceY)
        touch(v, MotionEvent.ACTION_UP, 545f, spaceY)
        assertEquals(listOf(" "), events)
    }

    @Test
    fun holdingTheSpaceBarMakesACursorPadThatMovesByLinesToo() {
        val v = keyboard()
        touch(v, MotionEvent.ACTION_DOWN, 540f, spaceY)
        hold(450)
        touch(v, MotionEvent.ACTION_MOVE, 540f + 60f, spaceY - 170f)
        shot("cursor-pad-dark", v)
        touch(v, MotionEvent.ACTION_UP, 540f + 60f, spaceY - 170f)
        assertEquals(listOf("cursor", "move 2 -2", "end"), events)
    }

    @Test
    fun theSettingsTurnTheGesturesOff() {
        val v = keyboard { spaceSwipe = false; spaceHold = false; commaHold = false }
        touch(v, MotionEvent.ACTION_DOWN, 540f, spaceY)
        hold(450)
        touch(v, MotionEvent.ACTION_MOVE, 740f, spaceY)
        touch(v, MotionEvent.ACTION_UP, 740f, spaceY)
        touch(v, MotionEvent.ACTION_DOWN, 216f, spaceY)       // the comma
        hold(450)
        touch(v, MotionEvent.ACTION_UP, 216f, spaceY)
        assertEquals(listOf(" ", ","), events)
    }

    @Test
    fun holdingTheCommaPicksAKeyboard() {
        val v = keyboard()
        touch(v, MotionEvent.ACTION_DOWN, 216f, spaceY)
        hold(450)
        touch(v, MotionEvent.ACTION_UP, 216f, spaceY)
        assertEquals(listOf("pick"), events)
    }

    @Test
    fun holdingALetterOffersItsFormsAndTheFingerChooses() {
        for (palette in listOf(Palette.DARK, Palette.LIGHT)) {
            events.clear()
            val v = keyboard(palette)
            touch(v, MotionEvent.ACTION_DOWN, 270f, topRowY)  // e
            hold(450)
            touch(v, MotionEvent.ACTION_MOVE, 330f, topRowY)  // one to the right: ê
            shot("variants-e-${if (palette == Palette.DARK) "dark" else "light"}", v)
            touch(v, MotionEvent.ACTION_UP, 330f, topRowY)
            // Held and let go where it was: the digit.
            touch(v, MotionEvent.ACTION_DOWN, 270f, topRowY)
            hold(450)
            touch(v, MotionEvent.ACTION_UP, 270f, topRowY)
            // A letter with one form gives it at once: d -> đ.
            touch(v, MotionEvent.ACTION_DOWN, 324f, 549f)
            hold(450)
            touch(v, MotionEvent.ACTION_UP, 324f, 549f)
            // Slid far right: the last form.
            touch(v, MotionEvent.ACTION_DOWN, 108f, 549f)     // a
            hold(450)
            touch(v, MotionEvent.ACTION_MOVE, 1079f, 549f)
            touch(v, MotionEvent.ACTION_UP, 1079f, 549f)
            assertEquals(listOf("ê", "3", "đ", "ạ"), events)
        }
    }
}
