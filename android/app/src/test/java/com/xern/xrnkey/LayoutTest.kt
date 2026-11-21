package com.xern.xrnkey

import com.xern.xrnkey.keyboard.KeyKind
import com.xern.xrnkey.keyboard.Layouts
import com.xern.xrnkey.keyboard.Page
import com.xern.xrnkey.keyboard.Shift
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LayoutTest {
    @Test
    fun everyRowIsTenKeysWide() {
        for (page in Page.entries) for (row in Layouts.rows(page)) assertEquals(10f, row.width, 0.001f)
    }

    @Test
    fun lettersHaveTheNumberRowOnTopWhenItIsOn() {
        val rows = Layouts.rows(Page.LETTERS)
        assertEquals(5, rows.size)
        assertEquals("1234567890", rows[0].keys.joinToString("") { it.label })
        assertEquals("qwertyuiop", rows[1].keys.joinToString("") { it.label })
        assertEquals(4, Layouts.rows(Page.LETTERS, numberRow = false).size)
    }

    @Test
    fun holdingATopRowLetterGivesItsDigit() {
        val top = Layouts.rows(Page.LETTERS)[1].keys
        assertEquals("1", top.first().hold)
        assertEquals("0", top.last().hold)
        assertTrue(Layouts.rows(Page.LETTERS)[3].keys.all { it.hold == null })
    }

    @Test
    fun theBottomRowIsSymbolsCommaSpacePeriodEnter() {
        val bottom = Layouts.rows(Page.LETTERS).last().keys
        assertEquals(
            listOf(KeyKind.SYMBOLS, KeyKind.COMMA, KeyKind.SPACE, KeyKind.PERIOD, KeyKind.ENTER),
            bottom.map { it.kind },
        )
        assertEquals("?123", bottom[0].label)
        assertEquals(5f, bottom[2].width)
    }

    @Test
    fun shiftCapitalisesTheLettersOnly() {
        val rows = Layouts.rows(Page.LETTERS, Shift.ONCE)
        assertEquals("QWERTYUIOP", rows[1].keys.joinToString("") { it.label })
        assertEquals("1234567890", rows[0].keys.joinToString("") { it.label })
    }

    @Test
    fun theSymbolPagesHaveTheDongSignAndAWayBack() {
        val symbols = Layouts.rows(Page.SYMBOLS).flatMap { it.keys }
        assertTrue(symbols.any { it.label == "₫" })
        assertTrue(symbols.any { it.kind == KeyKind.LETTERS })
        assertTrue(symbols.any { it.kind == KeyKind.MORE_SYMBOLS })
        assertFalse(symbols.any { it.kind == KeyKind.SHIFT })
        assertTrue(Layouts.rows(Page.MORE_SYMBOLS).flatMap { it.keys }.any { it.kind == KeyKind.SYMBOLS })
    }

    @Test
    fun holdingALetterOffersItsVietnameseForms() {
        val top = Layouts.rows(Page.LETTERS, numberRow = false)[0].keys.associateBy { it.label }
        assertEquals(listOf("3", "ê", "é", "è", "ẻ", "ẽ", "ẹ"), top["e"]!!.variants)
        assertEquals(listOf("9", "ô", "ơ", "ó", "ò", "ỏ", "õ", "ọ"), top["o"]!!.variants)
        assertEquals(listOf("1"), top["q"]!!.variants)
        val home = Layouts.rows(Page.LETTERS)[2].keys.associateBy { it.label }
        assertEquals(listOf("â", "ă", "á", "à", "ả", "ã", "ạ"), home["a"]!!.variants)
        assertEquals(listOf("đ"), home["d"]!!.variants)
        val shifted = Layouts.rows(Page.LETTERS, Shift.ONCE)[2].keys.associateBy { it.label }
        assertEquals("Đ", shifted["D"]!!.hold)
        // Without the digits, the forms come first.
        val plain = Layouts.rows(Page.LETTERS, numberRow = false, holdDigits = false)[0].keys.associateBy { it.label }
        assertEquals("ê", plain["e"]!!.hold)
        assertEquals(null, plain["q"]!!.hold)
    }
}
