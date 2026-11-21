package com.xern.xrnkey

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.PersistableBundle
import android.text.InputType
import android.view.View.MeasureSpec
import android.view.inputmethod.EditorInfo
import com.xern.xrnkey.clip.Clip
import com.xern.xrnkey.clip.ClipHistory
import com.xern.xrnkey.ime.XrnkeyService
import com.xern.xrnkey.keyboard.ClipboardPanelView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h914dp-xxhdpi")
class ClipboardTest {
    @get:Rule val tmp = TemporaryFolder()

    @Test
    fun historyIsNewestFirstKeepsPinsAndSurvivesARestart() {
        val f = File(tmp.root, "clips.tsv")
        val h = ClipHistory(f)
        h.add("Hẹn 7 giờ\nở quán cũ", 1)
        h.add("tab\there \\ back", 2)
        h.add("Hẹn 7 giờ\nở quán cũ", 3)  // copied again: to the top, once
        h.add("   ", 4)
        assertEquals(listOf("Hẹn 7 giờ\nở quán cũ", "tab\there \\ back"), h.clips.map { it.text })
        h.pin("tab\there \\ back", true)
        h.clear()
        assertEquals(listOf(Clip("tab\there \\ back", 2, pinned = true)), h.clips)
        h.add("mới", 5)
        assertEquals(h.clips, ClipHistory(f).clips)
        repeat(ClipHistory.MAX + 5) { h.add("clip $it", 10L + it) }
        assertEquals(ClipHistory.MAX + 1, h.clips.size)  // the pinned one stays on top of the limit
        assertTrue(h.clips.any { it.pinned })
        h.delete("tab\there \\ back")
        assertFalse(ClipHistory(f).clips.any { it.pinned })
    }

    private fun service(): XrnkeyService = Robolectric.setupService(XrnkeyService::class.java)

    private fun history(s: XrnkeyService) = ClipHistory(File(s.filesDir, "clips.tsv")).clips.map { it.text }

    private fun copy(text: String, sensitive: Boolean = false) {
        val cm = RuntimeEnvironment.getApplication().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText("", text)
        if (sensitive) clip.description.extras = PersistableBundle().apply { putBoolean(XrnkeyService.EXTRA_IS_SENSITIVE, true) }
        cm.setPrimaryClip(clip)
    }

    @Test
    fun sensitiveClipsAndPasswordFieldsStayOutOfTheHistory() {
        val s = service()
        copy("số tài khoản 0000 1111")
        copy("hunter2-made-up", sensitive = true)
        s.onStartInputView(EditorInfo().apply { inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD }, false)
        copy("copied-in-a-password-field")
        s.onStartInputView(EditorInfo().apply { inputType = InputType.TYPE_CLASS_TEXT }, false)
        copy("bình thường")
        assertEquals(listOf("bình thường", "số tài khoản 0000 1111"), history(s))
        s.onDestroy()
    }

    @Test
    fun theFilterOnItsOwn() {
        assertTrue(ClipHistory.keep("abc", sensitive = false, fromPasswordField = false))
        assertFalse(ClipHistory.keep("abc", sensitive = true, fromPasswordField = false))
        assertFalse(ClipHistory.keep("abc", sensitive = false, fromPasswordField = true))
        assertFalse(ClipHistory.keep(null, sensitive = false, fromPasswordField = false))
    }

    @Test
    fun thePanelPastesPinsDeletesAndClears() {
        val events = mutableListOf<String>()
        val p = ClipboardPanelView(RuntimeEnvironment.getApplication())
        p.clips = listOf(Clip("một", 3), Clip("hai", 2, pinned = true), Clip("ba", 1))
        p.listener = object : ClipboardPanelView.Listener {
            override fun onPaste(text: String) { events += "paste $text" }
            override fun onPin(text: String, pinned: Boolean) { events += "pin $text $pinned" }
            override fun onDelete(text: String) { events += "delete $text" }
            override fun onClearClips() { events += "clear" }
            override fun onClosePanel() { events += "close" }
        }
        p.measure(MeasureSpec.makeMeasureSpec(1080, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(954, MeasureSpec.EXACTLY))
        p.layout(0, 0, 1080, 954)
        // dp = 3: header 144 px, gap 24, cards 516 x 252, two to a row.
        p.tap(200f, 250f)               // the first card
        p.tap(800f, 250f)               // the second
        p.hold(0)
        p.tap(270f - 90f, 168f + 126f)  // pin, left of the card's centre
        p.hold(2)
        p.tap(270f + 90f, 444f + 126f)  // delete, right of the third card's centre
        p.tap(1040f, 70f)               // clear
        p.tap(30f, 70f)                 // ABC
        assertEquals(listOf("paste một", "paste hai", "pin một true", "delete ba", "clear", "close"), events)
    }
}
