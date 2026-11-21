package com.xern.xrnkey

import com.xern.xrnkey.engine.TypingState
import com.xern.xrnkey.ime.Typist
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class TypistTest {
    private fun typist(field: FakeEditor, state: TypingState = TypingState()) =
        Typist(state, field).also { it.reset() }

    @Test
    fun theWordInProgressIsComposingTextAndSpaceCommitsIt() {
        val f = FakeEditor()
        val t = typist(f)
        t.text("chaof")
        assertEquals("", f.committed.toString())
        assertEquals("chào", f.composing)
        t.text(" ")
        assertEquals("chào ", f.text)
        assertEquals("", f.composing)
    }

    @Test
    fun backspaceEditsTheWordThenTheField() {
        val f = FakeEditor()
        val t = typist(f)
        t.text("xin toans")
        t.backspace()
        assertEquals("xin tóa", f.text)  // the tone stays, placed for "toa"
        t.backspace(); t.backspace(); t.backspace()
        assertEquals("xin ", f.text)
        assertEquals("", f.composing)
        t.backspace()
        assertEquals("xin", f.text)  // no word in progress: the field's own Backspace
    }

    @Test
    fun enterCommitsTheWordAndReachesTheFieldInOnePress() {
        val f = FakeEditor()
        val t = typist(f)
        t.text("ddi")
        t.enter()
        assertEquals("đi\n", f.text)
        assertEquals(listOf("compose:d", "compose:đ", "compose:đi", "commit:đi", "enter"), f.log)
    }

    @Test
    fun digitsAndPunctuationAreTextAndEndTheWord() {
        val f = FakeEditor()
        val t = typist(f)
        t.text("vieetj, 2 laanf.")
        assertEquals("việt, 2 lần.", f.text)
    }

    @Test
    fun aPasswordFieldGetsEveryKeyAsTyped() {
        val f = FakeEditor()
        val t = typist(f)
        t.raw = true
        t.text("aa ddj")
        t.backspace()
        assertEquals("aa dd", f.text)
        assertTrue(f.log.none { it.startsWith("compose") })
    }

    @Test
    fun movingTheCursorEndsTheWordWhereItIs() {
        val f = FakeEditor()
        val t = typist(f)
        t.text("terms")
        assertEquals("tém", f.composing)  // shown as Telex until the word ends
        t.finish()
        assertEquals("terms", f.text)  // the core restores it as it ends
        t.text("vieetj")
        t.finish()
        assertEquals("termsviệt", f.text)
    }

    @Test
    fun theContextIsTheLastWordsOfTheSentence() {
        val f = FakeEditor("Hôm qua đi chơi. Tôi ")
        val t = typist(f)
        assertEquals(listOf("<s>", "Tôi"), t.context())
        t.text("khoong bieets ")
        assertEquals(listOf("Tôi", "không", "biết"), t.context().takeLast(3))
        t.enter()
        assertEquals(listOf("<s>"), t.context())
    }

    @Test
    fun aHeldLettersFormJoinsTheWordInProgress() {
        val f = FakeEditor()
        val t = typist(f)
        t.text("vi"); t.text("ê"); t.text("tj")
        t.text(" ")
        assertEquals("việt ", f.text)
        t.text("đi ")
        assertEquals("việt đi ", f.text)
    }

    @Test
    fun theKeysForEachForm() {
        assertEquals("eej", com.xern.xrnkey.ime.VietKeys.of('ệ', com.xern.xrnkey.engine.TypingState.TELEX))
        assertEquals("uw", com.xern.xrnkey.ime.VietKeys.of('ư', com.xern.xrnkey.engine.TypingState.TELEX))
        assertEquals("OWX", com.xern.xrnkey.ime.VietKeys.of('Ỡ', com.xern.xrnkey.engine.TypingState.TELEX)?.uppercase())
        assertEquals("a8", com.xern.xrnkey.ime.VietKeys.of('ă', com.xern.xrnkey.engine.TypingState.VNI))
        assertEquals("e65", com.xern.xrnkey.ime.VietKeys.of('ệ', com.xern.xrnkey.engine.TypingState.VNI))
        assertEquals("dd", com.xern.xrnkey.ime.VietKeys.of('đ', com.xern.xrnkey.engine.TypingState.TELEX))
        assertEquals(null, com.xern.xrnkey.ime.VietKeys.of('e', com.xern.xrnkey.engine.TypingState.TELEX))
        assertEquals(null, com.xern.xrnkey.ime.VietKeys.of('ç', com.xern.xrnkey.engine.TypingState.TELEX))
    }
}
