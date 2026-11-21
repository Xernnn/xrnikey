package com.xern.xrnkey

import com.xern.xrnkey.engine.EmojiIndex
import com.xern.xrnkey.engine.TypingState
import java.io.File
import com.xern.xrnkey.ime.StripMaker
import com.xern.xrnkey.ime.Typist
import com.xern.xrnkey.keyboard.Strip
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class StripMakerTest {
    @Test
    fun aWordTypedWithoutMarksShowsItsReadingsTheBestInTheCentre() {
        TestModels.test().use { m ->
            TypingState().use { s ->
                s.useModels(m.models)
                val f = FakeEditor("Tôi ")
                val t = Typist(s, f, m.models).also { it.reset() }
                val strip = StripMaker(m.models)
                assertEquals(Strip.Tools, strip.make(t))
                t.text("khong ")
                t.text("duoc")
                val shown = strip.make(t) as Strip.Candidates
                assertEquals("được", shown.centre)
                // The sides are the word's next readings (which ones depends on the model:
                // with the real one "dược" is next).
                val readings = m.models.restoreWord(t.context(), "duoc", 6).map { it.text }
                for (side in listOf(shown.left, shown.right)) {
                    assertTrue(side in readings)
                    assertTrue(side != shown.centre)
                }
                assertEquals(StripMaker.Pick("được"), strip.pick(1))
                t.pick(strip.pick(1)!!.text)
                assertEquals("Tôi không được ", f.text)  // Space restored "khong"
                assertEquals(Strip.Tools, strip.make(t))
            }
        }
    }

    @Test
    fun aWordTypedWithMarksShowsAsItIs() {
        TestModels.test().use { m ->
            TypingState().use { s ->
                val f = FakeEditor()
                val t = Typist(s, f).also { it.reset() }
                t.text("dduowcj")
                assertEquals(Strip.Candidates(null, "được", null), StripMaker(m.models).make(t))
            }
        }
    }

    @Test
    fun anEmojiThatGoesWithTheWordsTakesTheRightSlot() {
        val index = EmojiIndex.load(File(System.getProperty("xrnkey.emojiIndex")!!).readBytes())!!
        TestModels.test().use { m ->
            TypingState().use { s ->
                s.useModels(m.models)
                val f = FakeEditor("Chúc mừng ")
                val t = Typist(s, f, m.models).also { it.reset() }
                val strip = StripMaker(m.models, index)
                t.text("sinh nhaatj")
                val shown = strip.make(t) as Strip.Candidates
                assertEquals("nhật", shown.centre)
                assertTrue(shown.right in setOf("🎂", "🥳", "🎉", "🎁"))
                val pick = strip.pick(2)!!
                assertTrue(pick.emoji)
                strip.emojiSuggestions = false
                assertTrue((strip.make(t) as Strip.Candidates).right != shown.right)
            }
        }
        index.close()
    }
}
