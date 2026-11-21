package com.xern.xrnkey

import android.content.Context
import com.xern.xrnkey.engine.PersonalWords
import com.xern.xrnkey.engine.Shortcuts
import com.xern.xrnkey.engine.TypingState
import com.xern.xrnkey.ime.OnDevice
import com.xern.xrnkey.ime.StripMaker
import com.xern.xrnkey.ime.Typist
import com.xern.xrnkey.keyboard.Strip
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.File

/** Personal words and shortcuts: learned and kept on the phone, made-up samples only. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class PersonalTest {
    @get:Rule val tmp = TemporaryFolder()

    private fun prefs() = RuntimeEnvironment.getApplication().getSharedPreferences("t", Context.MODE_PRIVATE)

    @Test
    fun onlySettledWordsAreLearnedEachWithTheWordBefore() {
        TestModels.test().use { m ->
            TypingState().use { s ->
                s.useModels(m.models)
                val f = FakeEditor()
                val t = Typist(s, f, m.models)
                PersonalWords().use { p ->
                    t.personal = p
                    t.reset()
                    t.text("Mai ddi hocj nhes, ")
                    // Four words, the last three kept back: look-back may still change them.
                    assertEquals(1.0, p.count("<s>", "mai"), 0.0)
                    assertEquals(0.0, p.count("mai", "đi"), 0.0)
                    t.finish()
                    assertEquals(1.0, p.count("mai", "đi"), 0.0)
                    assertEquals(1.0, p.count("đi", "học"), 0.0)
                    assertEquals(1.0, p.count("học", "nhé"), 0.0)
                    t.text("ok. Vaang")
                    t.finish()
                    assertEquals(1.0, p.count("<p>", "ok"), 0.0)   // after the comma
                    assertEquals(1.0, p.count("<s>", "vâng"), 0.0)  // after the full stop
                }
            }
        }
    }

    @Test
    fun pastedTextPasswordsAndIncognitoFieldsTeachNothing() {
        TestModels.test().use { m ->
            TypingState().use { s ->
                s.useModels(m.models)
                PersonalWords().use { p ->
                    val t = Typist(s, FakeEditor(), m.models)
                    t.personal = p
                    t.reset()
                    t.insert("chuyển khoản số tiền lớn")
                    t.finish()
                    t.raw = true
                    t.text("matkhau bimat ")
                    t.finish()
                    t.raw = false
                    t.learnHere = false
                    t.text("ddi ddaau ")
                    t.finish()
                    for (w in listOf("chuyển", "lớn", "matkhau", "bimat", "đi", "đâu")) assertEquals(w, 0.0, p.count(null, w), 0.0)
                }
            }
        }
    }

    @Test
    fun theWordsSurviveARestartAndClearingForgetsThem() {
        val dir = tmp.root
        OnDevice(dir, prefs()).use { od ->
            od.refresh(now = 1_000)
            od.words!!.learn("<s>", "chào")
            od.words!!.learn("chào", "bạn")
        }
        OnDevice(dir, prefs()).use { od ->
            od.refresh(now = 2_000)
            assertEquals(1.0, od.words!!.count("chào", "bạn"), 0.0)
            OnDevice.clearWords(dir, prefs(), now = 3_000)
            od.refresh(now = 4_000)
            assertEquals(0.0, od.words!!.count("chào", "bạn"), 0.0)
        }
    }

    @Test
    fun oldHabitsFadeOnceAMonth() {
        val dir = tmp.root
        OnDevice(dir, prefs()).use { od ->
            od.refresh(now = 1_000)
            repeat(10) { od.words!!.learn("<s>", "alo") }
        }
        val month = 31L * 24 * 3600 * 1000
        OnDevice(dir, prefs()).use { od ->
            od.refresh(now = 1_000 + month)
            assertEquals(9.0, od.words!!.count("<s>", "alo"), 0.001)
        }
        OnDevice(dir, prefs()).use { od ->
            od.refresh(now = 2_000 + month)  // not a month since
            assertEquals(9.0, od.words!!.count("<s>", "alo"), 0.001)
        }
    }

    /** A made-up table in the three formats an import takes. */
    private val sample = "# made-up sample\nko\tkhông\nvs,với\ndc=được\nko\tkhông\nthis line has no separator\n"

    @Test
    fun anImportSaysWhatItWouldDoThenDoesIt() {
        Shortcuts().use { sc ->
            sc.set("ko", "không")
            val preview = sc.import(sample, apply = false)!!
            assertEquals(2, preview.added)
            assertTrue(preview.same >= 1)
            assertTrue(preview.skipped >= 1)
            assertEquals(listOf("ko" to "không"), sc.all())  // nothing done yet
            sc.import(sample, apply = true)
            assertEquals(listOf("dc" to "được", "ko" to "không", "vs" to "với"), sc.all())
            val f = File(tmp.root, "shortcuts.tsv")
            assertTrue(sc.save(f))
            Shortcuts().use { again ->
                again.load(f)
                assertEquals(sc.all(), again.all())
            }
        }
    }

    @Test
    fun aShortcutExpandsWhenItsWordEndsAndBackspacePutsItBack() {
        TestModels.test().use { m ->
            TypingState().use { s ->
                s.useModels(m.models)
                Shortcuts().use { sc ->
                    sc.import(sample, apply = true)
                    val f = FakeEditor()
                    val t = Typist(s, f, m.models)
                    t.shortcuts = sc
                    t.reset()
                    t.text("Ko")
                    val strip = StripMaker(m.models).make(t) as Strip.Candidates
                    assertEquals("Không", strip.centre)  // the strip shows what Space will do
                    t.text(" ")
                    assertEquals("Không ", f.text)
                    t.text("ddi vs")
                    t.text(",")
                    assertEquals("Không đi với,", f.text)
                    t.backspace()
                    assertEquals("Không đi vs,", f.text)
                    // Not in address fields.
                    t.autoCorrect = false
                    t.text(" ko")
                    assertNull(t.expansion())
                }
            }
        }
    }

    @Test
    fun aNewTableIsPickedUpWhenAFieldStarts() {
        OnDevice(tmp.root, prefs()).use { od ->
            od.refresh()
            assertNull(od.shortcuts!!.expand("ko"))
            Shortcuts().use { sc ->
                sc.import(sample, apply = true)
                sc.save(od.shortcutsFile)
            }
            od.shortcutsFile.setLastModified(od.shortcutsFile.lastModified() + 2_000)
            od.refresh()
            assertEquals("không", od.shortcuts!!.expand("ko"))
        }
    }
}
