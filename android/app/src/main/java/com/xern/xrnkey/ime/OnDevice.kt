package com.xern.xrnkey.ime

import android.content.SharedPreferences
import com.xern.xrnkey.engine.PersonalWords
import com.xern.xrnkey.engine.Shortcuts
import java.io.File

/**
 * What the keyboard keeps about its owner, on the phone only: the personal words it learned and
 * the shortcuts table. Both are files in credential-encrypted storage ([dir]), so they exist only
 * after the first unlock and survive restarts and app updates. The settings screen changes them
 * through the same files; [refresh] picks the changes up when a field starts.
 */
class OnDevice(private val dir: File, private val prefs: SharedPreferences) : AutoCloseable {
    val wordsFile get() = File(dir, WORDS)
    val shortcutsFile get() = File(dir, SHORTCUTS)

    var words: PersonalWords? = null
        private set
    var shortcuts: Shortcuts? = null
        private set

    private var wordsCleared = -1L
    private var shortcutsStamp = -1L

    /** Loads what isn't loaded yet, or changed since. Old habits fade once a month. */
    fun refresh(now: Long = System.currentTimeMillis()) {
        val cleared = prefs.getLong(KEY_CLEARED, 0L)
        if (words == null || cleared != wordsCleared) {
            words?.close()
            words = PersonalWords().also { w ->
                if (wordsFile.exists()) w.load(wordsFile)
                val last = prefs.getLong(KEY_DECAYED, 0L)
                if (last == 0L) {
                    prefs.edit().putLong(KEY_DECAYED, now).apply()
                } else if (now - last > MONTH) {
                    w.decay(DECAY)
                    w.save(wordsFile)
                    prefs.edit().putLong(KEY_DECAYED, now).apply()
                }
            }
            wordsCleared = cleared
        }
        val stamp = if (shortcutsFile.exists()) shortcutsFile.lastModified() else 0L
        if (shortcuts == null || stamp != shortcutsStamp) {
            shortcuts?.close()
            shortcuts = Shortcuts().also { if (shortcutsFile.exists()) it.load(shortcutsFile) }
            shortcutsStamp = stamp
        }
    }

    /** Writes the personal words, if any were learned since the last save. */
    fun save() {
        val w = words ?: return
        if (w.unsaved > 0) w.save(wordsFile)
    }

    override fun close() {
        save()
        words?.close()
        shortcuts?.close()
        words = null
        shortcuts = null
    }

    companion object {
        const val WORDS = "personal-words.tsv"
        const val SHORTCUTS = "shortcuts.tsv"
        private const val KEY_CLEARED = "words-cleared"
        private const val KEY_DECAYED = "words-decayed"
        private const val MONTH = 30L * 24 * 3600 * 1000
        private const val DECAY = 0.9

        /** Forgets every personal word (from the settings screen). */
        fun clearWords(dir: File, prefs: SharedPreferences, now: Long = System.currentTimeMillis()) {
            File(dir, WORDS).delete()
            prefs.edit().putLong(KEY_CLEARED, now).commit()
        }
    }
}
