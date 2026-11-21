package com.xern.xrnkey.ime

import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.keyboard.Strip

/**
 * What the strip shows for the word in progress. For a word typed without marks ("duoc"), the
 * centre is exactly what Space would commit (with look-back over the words before it), and the
 * two sides are its next readings. A word typed with Telex marks shows as it is. With no word,
 * the tool row.
 */
class StripMaker(private val models: Models?) {
    private var offered: List<String?> = emptyList()

    fun make(t: Typist): Strip {
        val word = t.shown
        if (word.isEmpty()) {
            offered = emptyList()
            return Strip.Tools
        }
        val centre = t.preview()
        if (models == null || centre == null) {
            offered = listOf(null, word, null)
            return Strip.Candidates(null, word, null)
        }
        val all = models.restoreWord(t.context(), t.keys(), 6).map { it.text }
        val others = all.filter { it != centre }
        offered = listOf(others.getOrNull(0), centre, others.getOrNull(1))
        return Strip.Candidates(offered[0], offered[1], offered[2], more = others.size > 2)
    }

    /** The candidate in slot 0 (left), 1 (centre) or 2 (right). */
    fun pick(slot: Int): String? = offered.getOrNull(slot)
}
