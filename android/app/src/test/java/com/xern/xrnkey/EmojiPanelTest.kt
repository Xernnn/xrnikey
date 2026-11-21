package com.xern.xrnkey

import android.content.Context
import android.view.View.MeasureSpec
import com.xern.xrnkey.engine.EmojiIndex
import com.xern.xrnkey.ime.EmojiMemory
import com.xern.xrnkey.keyboard.EmojiPanelView
import com.xern.xrnkey.keyboard.EmojiTab
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h914dp-xxhdpi")
class EmojiPanelTest {
    private val typed = mutableListOf<String>()
    private var closed = 0
    private var searched = 0
    private var deleted = 0

    private fun panel(): EmojiPanelView {
        val p = EmojiPanelView(RuntimeEnvironment.getApplication())
        EmojiIndex.load(File(System.getProperty("xrnkey.emojiIndex")!!).readBytes())!!.use { p.emoji = it.all }
        p.recents = listOf("🎂", "☕")
        p.suggestions = listOf("🥳", "🎉")
        p.listener = object : EmojiPanelView.Listener {
            override fun onEmoji(text: String) { typed += text }
            override fun onClosePanel() { closed++ }
            override fun onEmojiSearch() { searched++ }
            override fun onBackspace() { deleted++ }
        }
        p.measure(MeasureSpec.makeMeasureSpec(1080, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(954, MeasureSpec.EXACTLY))
        p.layout(0, 0, 1080, 954)
        return p
    }

    @Test
    fun tapsTypeSwitchTabsAndClose() {
        val p = panel()
        val dp = 3f
        p.tap(450f, 220f)                      // the first suggestion (two, centred)
        p.tap(10f, 340f)                       // the first recent
        p.tap(1060f, 940f)                     // Backspace
        p.tap(30f, 20f)                        // ABC
        p.tap(940f, 680f)                      // the search button
        assertEquals(listOf("🥳", "🎂"), typed)
        assertEquals(1, deleted)
        assertEquals(1, closed)
        assertEquals(1, searched)
        p.tap(64 * dp * 2 + 10, 20f)
        assertEquals(EmojiTab.EMOTICONS, p.tab)
        p.tap(30f, 200f)
        assertEquals(":)", typed.last())
    }

    @Test
    fun recentsComeFirstWithoutRepeatsAndTonesAreKept() {
        val prefs = RuntimeEnvironment.getApplication().getSharedPreferences("t", Context.MODE_PRIVATE)
        val m = EmojiMemory(prefs)
        m.addRecent("😀"); m.addRecent("🎂"); m.addRecent("😀")
        assertEquals(listOf("😀", "🎂"), m.recents())
        repeat(60) { m.addRecent("x$it") }
        assertEquals(EmojiMemory.RECENTS, m.recents().size)
        m.setTone("👍", "👍🏽")
        m.setTone("👋", "👋🏿")
        assertEquals(mapOf("👍" to "👍🏽", "👋" to "👋🏿"), EmojiMemory(prefs).tones())
        assertTrue(EmojiMemory(prefs).recents().first() == "x59")
    }
}
