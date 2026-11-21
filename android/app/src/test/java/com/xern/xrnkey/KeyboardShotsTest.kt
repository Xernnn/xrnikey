package com.xern.xrnkey

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View.MeasureSpec
import com.xern.xrnkey.keyboard.KeyboardView
import com.xern.xrnkey.keyboard.Page
import com.xern.xrnkey.keyboard.Palette
import com.xern.xrnkey.keyboard.Strip
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/**
 * The keyboard drawn by the real view with Robolectric's native graphics, saved as PNGs to
 * -Pxrnkey.shots=<dir> (else build/shots), at the owner's phone's size (1080 px wide, xxhdpi).
 * Each shot must not be blank.
 */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w411dp-h914dp-xxhdpi")
class KeyboardShotsTest {
    private val out = File(System.getProperty("xrnkey.shots") ?: "build/shots")

    private fun view(palette: Palette, setup: KeyboardView.() -> Unit = {}): KeyboardView {
        val v = KeyboardView(RuntimeEnvironment.getApplication())
        v.palette = palette
        v.setup()
        v.measure(
            MeasureSpec.makeMeasureSpec(1080, MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
        )
        v.layout(0, 0, v.measuredWidth, v.measuredHeight)
        return v
    }

    private fun shot(name: String, v: KeyboardView) {
        val bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
        v.draw(Canvas(bmp))
        val colours = HashSet<Int>()
        for (y in 0 until bmp.height step 7) for (x in 0 until bmp.width step 7) colours += bmp.getPixel(x, y)
        assertTrue("$name looks blank", colours.size > 8)
        out.mkdirs()
        File(out, "$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private fun both(name: String, setup: KeyboardView.() -> Unit = {}) {
        shot("$name-dark", view(Palette.DARK, setup))
        shot("$name-light", view(Palette.LIGHT, setup))
    }

    @Test
    fun letters() = both("letters")

    @Test
    fun symbols() {
        both("symbols") { page = Page.SYMBOLS }
        both("symbols-more") { page = Page.MORE_SYMBOLS }
    }

    /** The strip after typing "duoc" following "không": the engine's own candidates. */
    @Test
    fun stripAfterDuoc() {
        val shown = TestModels.best().use { m ->
            val c = m.models.restoreWord(listOf("<s>", "không"), "duoc", 6).map { it.text }
            Strip.Candidates(c.getOrNull(1), c.getOrNull(0), c.getOrNull(2), more = c.size > 3)
        }
        both("strip-duoc") { strip = shown }
    }

    /** A finger on "d": the key-press popup above it. */
    @Test
    fun keyPressPopup() {
        for ((name, palette) in listOf("dark" to Palette.DARK, "light" to Palette.LIGHT)) {
            val v = view(palette)
            val dp = v.resources.displayMetrics.density
            val x = v.width * 0.3f
            val y = 48 * dp + 2 * 54 * dp + 27 * dp  // the a..l row, below the number and q..p rows
            val t = SystemClock.uptimeMillis()
            v.onTouchEvent(MotionEvent.obtain(t, t, MotionEvent.ACTION_DOWN, x, y, 0))
            shot("pressed-d-$name", v)
        }
    }
}
