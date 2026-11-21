package com.xern.xrnkey

import com.xern.xrnkey.engine.Candidate
import com.xern.xrnkey.engine.Model
import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.engine.TypingState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.RandomAccessFile

/** The shared core through the JNI bridge, built for this machine (android/host). */
class EngineTest {
    private fun type(s: TypingState, keys: String): String {
        val out = StringBuilder()
        var preedit = ""
        for (c in keys) {
            val r = s.key(c.code)
            out.append(r.commit)
            preedit = r.preedit
            if (!r.consumed) out.append(c)
        }
        return out.toString() + preedit
    }

    @Test
    fun telexComposesThroughTheCore() {
        TypingState().use { s ->
            assertEquals("xin chào ", type(s, "xin chaof "))
            assertEquals("tiếng Việt ", type(s, "tieengs Vieetj "))
            assertEquals("được", type(s, "dduowcj"))
            assertEquals("dduowcj", s.keys())
        }
    }

    @Test
    fun vietnameseTextSurvivesTheBridgeWholeIncludingEmoji() {
        TestModels.test().use { m ->
            val r = m.models.restoreRun(listOf("<s>", "tôi"), listOf("khong"))
            assertEquals(listOf("không"), r)
        }
        TypingState().use { s ->
            s.setContext(listOf("😀", "đây"))  // must not throw or mangle
            assertEquals("đi ", type(s, "ddi "))
        }
    }

    @Test
    fun theWordsBeforeDecideEnglishOrVietnamese() {
        TestModels.test().use { m ->
            TypingState().use { s ->
                s.useModels(m.models)
                assertEquals("my list ", type(s, "my list "))
                s.reset()
                assertEquals("hai lít ", type(s, "hai list "))
            }
        }
    }

    @Test
    fun restorationOffersTheReadingsOfAWord() {
        TestModels.test().use { m ->
            val c = m.models.restoreWord(listOf("<s>", "tôi", "không"), "duoc", 6)
            assertEquals("được", c.first().text)
            assertEquals(Candidate.RESTORED, c.first().type)
            assertTrue(c.any { it.text == "dược" })
            assertEquals(listOf("cố", "gắng"), m.models.restoreRun(listOf("<s>"), listOf("co", "gang")))
            assertNull(m.models.restoreRun(listOf("<s>"), listOf("8h")))
        }
    }

    @Test
    fun aModelOpensFromInsideALargerFileAsFromTheAPK() {
        val model = File(TestModels.testDir, "vi.xklm").readBytes()
        val packed = File.createTempFile("packed", ".bin")
        packed.writeBytes(ByteArray(5000) + model + ByteArray(300))
        RandomAccessFile(packed, "r").use { f ->
            val fd = fdOf(f)
            val vi = Model.open(fd, 5000, model.size.toLong())
            assertNotNull(vi)
            Models(vi, null).use { m ->
                assertEquals("không", m.restoreWord(listOf("<s>", "tôi"), "khong").first().text)
            }
            vi!!.close()
        }
        packed.delete()
        assertNull(Model.open("/nonexistent/vi.xklm"))
    }

    /** The descriptor number behind a RandomAccessFile (what AssetFileDescriptor gives on a phone). */
    private fun fdOf(f: RandomAccessFile): Int {
        val field = java.io.FileDescriptor::class.java.getDeclaredField("fd")
        field.isAccessible = true
        return field.getInt(f.fd)
    }
}
