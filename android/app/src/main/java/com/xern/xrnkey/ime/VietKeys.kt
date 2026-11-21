package com.xern.xrnkey.ime

import com.xern.xrnkey.engine.TypingState
import java.text.Normalizer

/** The Telex or VNI keys that type a Vietnamese letter ("ệ" is "eej" in Telex, "e65" in VNI). */
object VietKeys {
    private val telex = mapOf(
        '\u0306' to "w", '\u031B' to "w",  // breve, horn
        '\u0301' to "s", '\u0300' to "f", '\u0309' to "r", '\u0303' to "x", '\u0323' to "j",
    )
    private val vni = mapOf(
        '\u0302' to "6", '\u0306' to "8", '\u031B' to "7",
        '\u0301' to "1", '\u0300' to "2", '\u0309' to "3", '\u0303' to "4", '\u0323' to "5",
    )

    /** The keys for [c], or null when it is a plain letter (or not a Vietnamese one). */
    fun of(c: Char, mode: Int): String? {
        val vniMode = mode == TypingState.VNI
        when (c) {
            'đ' -> return if (vniMode) "d9" else "dd"
            'Đ' -> return if (vniMode) "D9" else "DD"
        }
        val d = Normalizer.normalize(c.toString(), Normalizer.Form.NFD)
        if (d.length < 2 || d[0] !in "aeiouyAEIOUY") return null
        val base = d[0]
        val out = StringBuilder().append(base)
        // The shape (circumflex, breve, horn) first, then the tone.
        val marks = d.substring(1).toList().sortedBy { if (it == '\u0302' || it == '\u0306' || it == '\u031B') 0 else 1 }
        for (m in marks) {
            val k = if (vniMode) vni[m] else if (m == '\u0302') base.toString() else telex[m]
            out.append(k ?: return null)
        }
        return out.toString()
    }
}
