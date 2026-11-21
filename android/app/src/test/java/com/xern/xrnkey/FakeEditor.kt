package com.xern.xrnkey

import com.xern.xrnkey.ime.Editor

/**
 * A text field as InputConnection edits it: committed text, then the composing word at the
 * cursor. A commit replaces the composing word; finishing keeps it as text.
 */
class FakeEditor(initial: String = "") : Editor {
    val committed = StringBuilder(initial)
    var composing = ""
        private set
    val log = mutableListOf<String>()

    /** What the field shows. */
    val text: String get() = committed.toString() + composing

    override fun setComposing(text: String) {
        composing = text
        log += "compose:$text"
    }

    override fun commit(text: String) {
        composing = ""
        committed.append(text)
        log += "commit:$text"
    }

    override fun finishComposing() {
        committed.append(composing)
        composing = ""
        log += "finish"
    }

    override fun backspace() {
        if (committed.isNotEmpty()) {
            val n = Character.charCount(committed.codePointBefore(committed.length))
            committed.setLength(committed.length - n)
        }
        log += "backspace"
    }

    override fun enter() {
        committed.append('\n')
        log += "enter"
    }

    override fun textBefore(n: Int): String = committed.takeLast(n).toString()
}
