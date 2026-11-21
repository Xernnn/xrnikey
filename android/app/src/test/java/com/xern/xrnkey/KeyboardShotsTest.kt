package com.xern.xrnkey

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View.MeasureSpec
import com.xern.xrnkey.engine.EmojiIndex
import com.xern.xrnkey.clip.Clip
import com.xern.xrnkey.keyboard.ClipboardPanelView
import com.xern.xrnkey.keyboard.EmojiPanelView
import com.xern.xrnkey.keyboard.EmojiTab
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

    private fun panel(palette: Palette, tab: EmojiTab): EmojiPanelView {
        val keys = view(palette)
        val index = EmojiIndex.load(File(System.getProperty("xrnkey.emojiIndex")!!).readBytes())!!
        val p = EmojiPanelView(RuntimeEnvironment.getApplication())
        p.palette = palette
        p.emoji = index.all
        p.recents = listOf("😂", "❤️", "👍", "🙏", "🥰", "😭", "🎂", "☕", "🔥", "😅")
        p.suggestions = index.suggest(listOf("chúc", "mừng", "sinh", "nhật"), 8)
        p.tab = tab
        index.close()
        p.measure(
            MeasureSpec.makeMeasureSpec(1080, MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(keys.height, MeasureSpec.EXACTLY),
        )
        p.layout(0, 0, p.measuredWidth, p.measuredHeight)
        return p
    }

    private fun shotPanel(name: String, v: android.view.View) {
        val bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
        v.draw(Canvas(bmp))
        out.mkdirs()
        File(out, "$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    @Test
    fun emojiPanel() {
        for ((name, palette) in listOf("dark" to Palette.DARK, "light" to Palette.LIGHT)) {
            shotPanel("emoji-$name", panel(palette, EmojiTab.EMOJI))
            shotPanel("emoticons-$name", panel(palette, EmojiTab.EMOTICONS))
            shotPanel("symbols-tab-$name", panel(palette, EmojiTab.SYMBOLS))
        }
    }

    @Test
    fun emojiSearchAndTheStripsEmoji() {
        val index = EmojiIndex.load(File(System.getProperty("xrnkey.emojiIndex")!!).readBytes())!!
        val found = index.search("cuoi")
        index.close()
        both("emoji-search") { strip = Strip.Search("cuoi", found) }
        both("strip-emoji") { strip = Strip.Candidates("sinh", "nhật", "🎂", more = true) }
    }

    @Test
    fun clipboardPanel() {
        for ((name, palette) in listOf("dark" to Palette.DARK, "light" to Palette.LIGHT)) {
            val keys = view(palette)
            val p = ClipboardPanelView(RuntimeEnvironment.getApplication())
            p.palette = palette
            p.clips = listOf(
                Clip("Hẹn 7 giờ tối thứ Sáu ở quán cà phê cũ nhé, nhớ mang theo sách", 6, pinned = true),
                Clip("https://example.com/menu", 5),
                Clip("0901 234 567", 4),
                Clip("Địa chỉ: 12 Đường Hoa Sữa,\nPhường 1, Quận 3", 3),
                Clip("Thanks, see you tomorrow!", 2),
                Clip("mã đơn hàng ABC-12345", 1),
            )
            p.measure(
                MeasureSpec.makeMeasureSpec(1080, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(keys.height, MeasureSpec.EXACTLY),
            )
            p.layout(0, 0, p.measuredWidth, p.measuredHeight)
            shotPanel("clipboard-$name", p)
            p.hold(1)
            shotPanel("clipboard-hold-$name", p)
        }
    }
}
