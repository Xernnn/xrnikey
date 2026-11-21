package com.xern.xrnkey

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.view.inputmethod.InputMethodManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.xern.xrnkey.engine.ImportCounts
import com.xern.xrnkey.engine.Shortcuts
import com.xern.xrnkey.ime.OnDevice
import java.io.File

private val Background = Color(0xFF263238)
private val Card = Color(0xFF2E3B42)
private val Glyph = Color(0xFFECEFF1)
private val Dim = Color(0xFFB0BEC5)
private val Accent = Color(0xFF5F3DC4)
private val Pill = Color(0xFF37474F)

/**
 * The settings: the two steps Android needs (Enable, Choose) until they are done, then every
 * setting with the owner's choices as defaults (android/docs/feature-spec.md, "Settings"). The
 * xrnkey tile is the title; labels are short, with no explanatory text.
 */
class SettingsActivity : ComponentActivity() {
    private var refresh by mutableIntStateOf(0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { Screen(refresh) }
    }

    override fun onResume() {
        super.onResume()
        refresh++
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) refresh++  // the keyboard picker closed
    }

    private fun imm() = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager

    private fun enabled() = imm().enabledInputMethodList.any { it.packageName == packageName }

    private fun chosen() =
        Settings.Secure.getString(contentResolver, Settings.Secure.DEFAULT_INPUT_METHOD)
            ?.startsWith("$packageName/") == true

    private val prefs by lazy { Options.prefs(this) }
    private val shortcutsFile get() = File(filesDir, OnDevice.SHORTCUTS)

    private fun <T> withShortcuts(change: (Shortcuts) -> T): T = Shortcuts().use { sc ->
        if (shortcutsFile.exists()) sc.load(shortcutsFile)
        val r = change(sc)
        sc.save(shortcutsFile)
        r
    }

    private fun readShortcuts(): List<Pair<String, String>> = Shortcuts().use { sc ->
        if (shortcutsFile.exists()) sc.load(shortcutsFile)
        sc.all()
    }

    private fun readText(uri: Uri): String? = try {
        contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) }
    } catch (e: Exception) {
        null
    }

    @Composable
    private fun Screen(@Suppress("UNUSED_PARAMETER") tick: Int) {
        val font = FontFamily(Font(R.font.misans_vf, FontWeight.Normal), Font(R.font.misans_vf, FontWeight.SemiBold))
        var o by remember { mutableStateOf(Options.load(prefs)) }
        fun set(n: Options) { o = n; n.save(prefs) }
        var shortcuts by remember { mutableStateOf(readShortcuts()) }
        var pending by remember { mutableStateOf<Pair<String, ImportCounts>?>(null) }
        var confirmClear by remember { mutableStateOf(false) }
        val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            val text = uri?.let { readText(it) } ?: return@rememberLauncherForActivityResult
            val counts = Shortcuts().use { sc ->
                if (shortcutsFile.exists()) sc.load(shortcutsFile)
                sc.import(text, apply = false)
            } ?: return@rememberLauncherForActivityResult
            pending = text to counts
        }
        val exporter = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("text/plain")) { uri ->
            uri ?: return@rememberLauncherForActivityResult
            contentResolver.openOutputStream(uri)?.use { out ->
                out.write(readShortcuts().joinToString("") { "${it.first}\t${it.second}\n" }.toByteArray())
            }
        }
        val label = TextStyle(color = Glyph, fontFamily = font, fontSize = 16.sp)

        Column(
            Modifier.fillMaxSize().background(Background).verticalScroll(rememberScrollState()).padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Spacer(Modifier.size(24.dp))
            Image(painterResource(R.drawable.tile), contentDescription = stringResource(R.string.app_name), Modifier.size(56.dp))
            Spacer(Modifier.size(8.dp))
            val isEnabled = enabled()
            val isChosen = chosen()
            if (!isEnabled || !isChosen) {
                Step(stringResource(R.string.enable), isEnabled, font) {
                    startActivity(Intent(Settings.ACTION_INPUT_METHOD_SETTINGS))
                }
                Step(stringResource(R.string.choose), isChosen, font) { imm().showInputMethodPicker() }
            }

            Section("Typing", font)
            Choice("Method", listOf("Telex", "VNI"), if (o.vni) 1 else 0, font) { set(o.copy(vni = it == 1)) }
            Choice("Tones", listOf("hòa", "hoà"), if (o.modernTones) 1 else 0, font) { set(o.copy(modernTones = it == 1)) }
            Toggle("Underline", o.underline, font) { set(o.copy(underline = it)) }

            Section("Suggestions", font)
            Toggle("Suggestions", o.suggestions, font) { set(o.copy(suggestions = it)) }
            Toggle("Emoji", o.emojiSuggestions, font) { set(o.copy(emojiSuggestions = it)) }
            Toggle("Personal words", o.learn, font) { set(o.copy(learn = it)) }
            Action("Clear personal words", font) { confirmClear = true }

            Section("Auto-correct", font)
            Choice("Level", listOf("Off", "Flexible", "Strong"), o.correction, font) { set(o.copy(correction = it)) }
            Toggle("English as typed", o.keepForeign, font) { set(o.copy(keepForeign = it)) }
            Toggle("Double space  .", o.doubleSpace, font) { set(o.copy(doubleSpace = it)) }

            Section("Keys", font)
            val rows = listOf("auto", "on", "off")
            Choice("Number row", listOf("Auto", "On", "Off"), rows.indexOf(o.numberRow), font) { set(o.copy(numberRow = rows[it])) }
            Toggle("Hold top row  1–0", o.holdDigits, font) { set(o.copy(holdDigits = it)) }
            Toggle("Key popup", o.keyPopup, font) { set(o.copy(keyPopup = it)) }
            Choice("Hold", Options.HOLD_CHOICES.map { "$it" }, Options.HOLD_CHOICES.indexOf(o.holdMillis), font) {
                set(o.copy(holdMillis = Options.HOLD_CHOICES[it]))
            }
            Toggle("Swipe space  ↔", o.spaceSwipe, font) { set(o.copy(spaceSwipe = it)) }
            Toggle("Hold space  ✥", o.spaceHold, font) { set(o.copy(spaceHold = it)) }
            Toggle("Hold comma  ⌨", o.commaHold, font) { set(o.copy(commaHold = it)) }
            Choice("Height", Options.HEIGHT_CHOICES.map { "$it%" }, Options.HEIGHT_CHOICES.indexOf(o.height), font) {
                set(o.copy(height = Options.HEIGHT_CHOICES[it]))
            }

            Section("Clipboard", font)
            Toggle("History", o.clipboard, font) { set(o.copy(clipboard = it)) }

            Section("Shortcuts", font)
            Toggle("Shortcuts", o.shortcuts, font) { set(o.copy(shortcuts = it)) }
            ShortcutAdder(font) { a, e ->
                withShortcuts { it.set(a, e) }
                shortcuts = readShortcuts()
            }
            for ((abbr, expansion) in shortcuts) {
                Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(abbr, style = label.copy(fontWeight = FontWeight.SemiBold), modifier = Modifier.width(96.dp))
                    Text(expansion, style = label.copy(color = Dim), modifier = Modifier.weight(1f))
                    Text(
                        "×",
                        style = label.copy(fontSize = 22.sp, color = Dim),
                        modifier = Modifier.clickable {
                            withShortcuts { it.remove(abbr) }
                            shortcuts = readShortcuts()
                        }.padding(horizontal = 12.dp),
                    )
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Pill("Import", false, font) { importer.launch(arrayOf("text/*")) }
                Pill("Export", false, font) { exporter.launch("shortcuts.txt") }
            }

            Section("Theme", font)
            val themes = listOf("dark", "light", "system")
            Choice("Theme", listOf("Dark", "Light", "System"), themes.indexOf(o.theme), font) { set(o.copy(theme = themes[it])) }

            Section("About", font)
            Text("xrnkey ${packageManager.getPackageInfo(packageName, 0).versionName}", style = label)
            Text("Code: MIT", style = label.copy(color = Dim))
            Text("Language data: CC BY 4.0 — Leipzig Corpora Collection, Tatoeba (CC BY 2.0 FR), Common Voice (CC0)", style = label.copy(color = Dim, fontSize = 14.sp))
            Text("Emoji names: Unicode CLDR, Unicode License v3", style = label.copy(color = Dim, fontSize = 14.sp))
            Text("MiSans: Xiaomi MiSans font licence", style = label.copy(color = Dim, fontSize = 14.sp))
            Spacer(Modifier.height(32.dp))
        }

        pending?.let { (text, c) ->
            AlertDialog(
                onDismissRequest = { pending = null },
                containerColor = Card,
                title = { Text("Import", style = label.copy(fontWeight = FontWeight.SemiBold)) },
                text = {
                    Column {
                        for ((name, n) in listOf("New" to c.added, "Same" to c.same, "Changed" to c.changed, "Skipped" to c.skipped)) {
                            Row(Modifier.fillMaxWidth()) {
                                Text(name, style = label, modifier = Modifier.weight(1f))
                                Text("$n", style = label)
                            }
                        }
                    }
                },
                confirmButton = {
                    TextButton(onClick = {
                        withShortcuts { it.import(text, apply = true) }
                        shortcuts = readShortcuts()
                        pending = null
                    }) { Text("Import", style = label.copy(color = Accent, fontWeight = FontWeight.SemiBold)) }
                },
                dismissButton = { TextButton(onClick = { pending = null }) { Text("Cancel", style = label.copy(color = Dim)) } },
            )
        }
        if (confirmClear) {
            AlertDialog(
                onDismissRequest = { confirmClear = false },
                containerColor = Card,
                title = { Text("Clear personal words", style = label.copy(fontWeight = FontWeight.SemiBold)) },
                confirmButton = {
                    TextButton(onClick = {
                        OnDevice.clearWords(filesDir, prefs)
                        confirmClear = false
                    }) { Text("Clear", style = label.copy(color = Accent, fontWeight = FontWeight.SemiBold)) }
                },
                dismissButton = { TextButton(onClick = { confirmClear = false }) { Text("Cancel", style = label.copy(color = Dim)) } },
            )
        }
    }

    @Composable
    private fun Section(title: String, font: FontFamily) {
        Spacer(Modifier.height(16.dp))
        Text(title, color = Dim, fontFamily = font, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
    }

    @Composable
    private fun Toggle(title: String, on: Boolean, font: FontFamily, change: (Boolean) -> Unit) {
        Row(Modifier.fillMaxWidth().clickable { change(!on) }.padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(title, color = Glyph, fontFamily = font, fontSize = 16.sp, modifier = Modifier.weight(1f))
            Switch(
                checked = on,
                onCheckedChange = change,
                colors = SwitchDefaults.colors(
                    checkedTrackColor = Accent, checkedThumbColor = Glyph,
                    uncheckedTrackColor = Pill, uncheckedThumbColor = Dim, uncheckedBorderColor = Pill,
                ),
            )
        }
    }

    @Composable
    private fun Choice(title: String, options: List<String>, selected: Int, font: FontFamily, change: (Int) -> Unit) {
        Column(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(title, color = Glyph, fontFamily = font, fontSize = 16.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                options.forEachIndexed { i, name -> Pill(name, i == selected, font) { change(i) } }
            }
        }
    }

    @Composable
    private fun Pill(text: String, on: Boolean, font: FontFamily, onClick: () -> Unit) {
        Text(
            text,
            color = Glyph,
            fontFamily = font,
            fontWeight = if (on) FontWeight.SemiBold else FontWeight.Normal,
            fontSize = 15.sp,
            modifier = Modifier.clip(RoundedCornerShape(18.dp)).background(if (on) Accent else Pill)
                .clickable(onClick = onClick).padding(horizontal = 14.dp, vertical = 7.dp),
        )
    }

    @Composable
    private fun Action(title: String, font: FontFamily, onClick: () -> Unit) {
        Text(
            title,
            color = Glyph,
            fontFamily = font,
            fontSize = 16.sp,
            modifier = Modifier.fillMaxWidth().clickable(onClick = onClick).padding(vertical = 10.dp),
        )
    }

    @Composable
    private fun ShortcutAdder(font: FontFamily, add: (String, String) -> Unit) {
        var abbr by remember { mutableStateOf("") }
        var expansion by remember { mutableStateOf("") }
        val style = TextStyle(color = Glyph, fontFamily = font, fontSize = 16.sp)
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            BasicTextField(
                abbr, { abbr = it.trim() }, singleLine = true, textStyle = style, cursorBrush = SolidColor(Accent),
                modifier = Modifier.width(88.dp).clip(RoundedCornerShape(8.dp)).background(Pill).padding(10.dp),
            )
            Text("→", color = Dim, fontFamily = font, fontSize = 16.sp)
            BasicTextField(
                expansion, { expansion = it }, singleLine = true, textStyle = style, cursorBrush = SolidColor(Accent),
                modifier = Modifier.weight(1f).clip(RoundedCornerShape(8.dp)).background(Pill).padding(10.dp),
            )
            Button(
                onClick = {
                    if (abbr.isNotEmpty() && expansion.isNotBlank()) {
                        add(abbr, expansion)
                        abbr = ""
                        expansion = ""
                    }
                },
                colors = ButtonDefaults.buttonColors(containerColor = Accent),
            ) { Text("+", fontFamily = font, fontSize = 18.sp) }
        }
    }

    @Composable
    private fun Step(label: String, done: Boolean, font: FontFamily, onClick: () -> Unit) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Button(
                onClick = onClick,
                colors = ButtonDefaults.buttonColors(containerColor = Accent),
            ) { Text(label, fontFamily = font, fontWeight = FontWeight.SemiBold, fontSize = 17.sp) }
            Spacer(Modifier.width(16.dp))
            if (done) Text("✓", color = Glyph, fontFamily = font, fontSize = 22.sp)
        }
    }
}
