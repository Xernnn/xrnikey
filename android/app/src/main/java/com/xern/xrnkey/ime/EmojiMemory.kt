package com.xern.xrnkey.ime

import android.content.SharedPreferences

/** The emoji used lately (newest first) and the skin tone chosen for each emoji, kept on the phone. */
class EmojiMemory(private val prefs: SharedPreferences) {
    fun recents(): List<String> = prefs.getString("recents", "")!!.split(SEP).filter { it.isNotEmpty() }

    fun addRecent(e: String) {
        val list = (listOf(e) + recents().filter { it != e }).take(RECENTS)
        prefs.edit().putString("recents", list.joinToString(SEP)).apply()
    }

    fun tones(): Map<String, String> =
        prefs.getString("tones", "")!!.split(SEP).mapNotNull {
            val p = it.split(PAIR)
            if (p.size == 2) p[0] to p[1] else null
        }.toMap()

    fun setTone(base: String, toned: String) {
        val t = tones() + (base to toned)
        prefs.edit().putString("tones", t.entries.joinToString(SEP) { "${it.key}$PAIR${it.value}" }).apply()
    }

    companion object {
        private const val SEP = "\u001F"
        private const val PAIR = "\u001E"
        const val RECENTS = 48
    }
}
