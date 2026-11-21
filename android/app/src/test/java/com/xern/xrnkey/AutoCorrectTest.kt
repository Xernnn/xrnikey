package com.xern.xrnkey

import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.engine.TypingState
import com.xern.xrnkey.ime.StripMaker
import com.xern.xrnkey.ime.Typist
import com.xern.xrnkey.keyboard.Strip
import org.junit.Assert.assertEquals
import org.junit.Test

/**
 * Space and punctuation restoring words typed without marks, with look-back and Revert
 * (android/docs/feature-spec.md, "Auto-correct"), against the small test models.
 */
class AutoCorrectTest {
    private fun <T> typing(field: String = "", correction: Int = Models.CORRECT_FLEXIBLE,
                           body: (Typist, FakeEditor, Models) -> T): T =
        TestModels.test().use { m ->
            m.models.correction = correction
            TypingState().use { s ->
                s.useModels(m.models)
                val f = FakeEditor(field)
                val t = Typist(s, f, m.models).also { it.reset() }
                body(t, f, m.models)
            }
        }

    @Test
    fun spaceRestoresAWordTypedWithoutMarks() = typing { t, f, _ ->
        t.text("khong ")
        assertEquals("không ", f.text)
        t.text("Toi khong biet ")
        assertEquals("không Tôi không biết ", f.text)
    }

    @Test
    fun backspaceRightAfterPutsBackWhatWasTyped() = typing { t, f, _ ->
        t.text("khong ")
        t.backspace()
        assertEquals("khong ", f.text)
        t.backspace()
        assertEquals("khong", f.text)  // then Backspace deletes as usual
    }

    @Test
    fun aLaterWordFixesAnEarlierOneAndBackspaceUndoesThat() = typing { t, f, _ ->
        t.text("co ")
        assertEquals("có ", f.text)
        t.text("gang ")
        assertEquals("cố gắng ", f.text)
        t.backspace()
        assertEquals("có gang ", f.text)  // as it was before that Space
    }

    @Test
    fun lookBackLeavesAloneTextThisKeyboardDidNotWrite() = typing { t, f, _ ->
        t.text("co ")
        f.committed.insert(0, "x")  // the app changed the field meanwhile ...
        f.committed.setLength(f.committed.length - 1)
        f.committed.append("! ")  // ... "có " is now "xcó! "
        t.text("gang ")
        assertEquals("xcó! gắng ", f.text)
    }

    @Test
    fun theSentenceEndPicksTheReadingThatEndsIt() {
        typing("Bạn ") { t, f, _ ->
            t.text("kha nhe ")
            assertEquals("Bạn khá nhẹ ", f.text)
        }
        typing("Bạn ") { t, f, _ ->
            t.text("kha nhe.")
            assertEquals("Bạn khá nhé.", f.text)
        }
        typing("Bạn ") { t, f, _ ->
            t.text("kha nhe")
            t.enter()
            assertEquals("Bạn khá nhé\n", f.text)
        }
    }

    @Test
    fun wordsTypedWithMarksAndEnglishStayAsTheCoreGivesThem() = typing { t, f, _ ->
        t.text("dduowcj ")
        assertEquals("được ", f.text)
        t.text("we can chat ")
        assertEquals("được we can chat ", f.text)
    }

    @Test
    fun aSlipToANeighbouringKeyIsFixed() = typing("Tôi ") { t, f, _ ->
        t.text("khonh ")
        assertEquals("Tôi không ", f.text)
    }

    @Test
    fun twoSpacesAfterAWordGiveAFullStop() = typing { t, f, _ ->
        t.text("khong  ")
        assertEquals("không. ", f.text)
        t.text("  ")
        assertEquals("không.   ", f.text)  // only right after a word
    }

    @Test
    fun anAddressFieldGetsNoAutoCorrect() = typing { t, f, _ ->
        t.autoCorrect = false
        t.text("khong  ")
        assertEquals("khong  ", f.text)
    }

    @Test
    fun theStripCentreIsWhatSpaceCommits() = typing { t, f, m ->
        val strip = StripMaker(m)
        t.text("co ")
        t.text("gang")
        val shown = strip.make(t) as Strip.Candidates
        assertEquals("gắng", shown.centre)
        t.text(" ")
        assertEquals("cố gắng ", f.text)
    }
}
