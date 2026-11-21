package com.xern.xrnkey

import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.engine.TypingState
import com.xern.xrnkey.ime.StripMaker
import com.xern.xrnkey.ime.Typist
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * What a keystroke costs on the JVM, through the JNI bridge, without any instrumentation: all
 * the keyboard does for one key (the core's Telex step, the strip's centre and readings for the
 * word in progress, and, when Space or punctuation ends a word, its restoration with look-back
 * and correction), and the restoration run alone. With the real models when this machine has
 * them. Prints p50/p95/p99 and writes build/reports/engine-bench.txt.
 */
class EngineBenchmarkTest {
    private val text = listOf(
        "toi khong biet ban co di duoc khong. ",
        "hom nay troi mua to qua, minh o nha thoi. ",
        "chieu nay hop luc 3 gio nhe moi nguoi. ",
        "gui file cho anh qua email truoc 5h nhe. ",
        "tieengs Vieetj cos daauf, khoong cos daauf cungx dduowcj. ",
        "we can chat after the meeting, my list is long. ",
    ).joinToString("")

    private fun percentiles(name: String, ns: List<Long>): String {
        val s = ns.sorted()
        fun at(q: Double) = s[minOf(s.size - 1, (q * s.size).toInt())] / 1e6
        return "%-46s p50 %6.3f  p95 %6.3f  p99 %6.3f  max %7.3f ms  (%d)".format(
            name, at(0.50), at(0.95), at(0.99), s.last() / 1e6, s.size,
        )
    }

    private fun keystrokes(models: Models, rounds: Int): List<Long> {
        val times = ArrayList<Long>()
        TypingState().use { state ->
            state.useModels(models)
            val typist = Typist(state, FakeEditor(), models).also { it.reset() }
            // As on the phone: emoji in the strip, next words after a Space.
            val strip = StripMaker(models, emoji)
            repeat(rounds) {
                for (c in text) {
                    val t0 = System.nanoTime()
                    typist.text(c.toString())
                    strip.make(typist)
                    times += System.nanoTime() - t0
                }
            }
        }
        return times
    }

    private val emoji = com.xern.xrnkey.engine.EmojiIndex.load(File(System.getProperty("xrnkey.emojiIndex")!!).readBytes())

    private fun nextWords(models: Models, rounds: Int): List<Long> {
        val words = text.split(Regex("[^a-zA-Z]+")).filter { it.isNotEmpty() }.map { it.lowercase() }
        val times = ArrayList<Long>()
        repeat(rounds) {
            for (i in words.indices) {
                val ctx = listOf("<s>") + words.subList(maxOf(0, i - 2), i + 1)
                val t0 = System.nanoTime()
                models.predict(ctx, 3)
                times += System.nanoTime() - t0
            }
        }
        return times
    }

    private fun wordEnds(models: Models, rounds: Int): List<Long> {
        val words = text.split(Regex("[^a-zA-Z]+")).filter { it.isNotEmpty() }
        val times = ArrayList<Long>()
        repeat(rounds) {
            for (i in words.indices) {
                val from = maxOf(0, i - 2)
                val t0 = System.nanoTime()
                models.restoreRun(listOf("<s>"), words.subList(from, i + 1))
                times += System.nanoTime() - t0
            }
        }
        return times
    }

    @Test
    fun whatAKeystrokeCosts() {
        val lines = mutableListOf<String>()
        TestModels.best().use { m ->
            m.models.correction = Models.CORRECT_FLEXIBLE
            lines += "models: " + if (TestModels.realAvailable) "real (vi-phone.xklm, en.xklm)" else "small test models"
            keystrokes(m.models, 3)  // warm the JIT
            val keys = keystrokes(m.models, 20)
            lines += percentiles("key: all the keyboard does, through JNI", keys)
            m.models.correction = Models.CORRECT_OFF
            wordEnds(m.models, 3)
            lines += percentiles("word end: run of up to 3, correction off", wordEnds(m.models, 20))
            m.models.correction = Models.CORRECT_FLEXIBLE
            lines += percentiles("word end: run of up to 3, correction on", wordEnds(m.models, 20))
            nextWords(m.models, 3)
            lines += percentiles("next words after Space (3)", nextWords(m.models, 20))
            assertTrue(keys.isNotEmpty())
        }
        val report = lines.joinToString("\n")
        println(report)
        File("build/reports").mkdirs()
        File("build/reports/engine-bench.txt").writeText(report + "\n")
    }
}
