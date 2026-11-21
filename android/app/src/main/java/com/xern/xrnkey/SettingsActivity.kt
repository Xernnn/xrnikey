package com.xern.xrnkey

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.provider.Settings
import android.view.inputmethod.InputMethodManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * Setting the keyboard up: the two steps Android needs, Enable and Choose, each ticked when
 * done. The xrnkey tile is the title; there is no explanatory text.
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

    @Composable
    private fun Screen(@Suppress("UNUSED_PARAMETER") tick: Int) {
        val misans = FontFamily(Font(R.font.misans_vf, FontWeight.Normal), Font(R.font.misans_vf, FontWeight.SemiBold))
        Column(
            Modifier.fillMaxSize().background(Color(0xFF263238)).padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Spacer(Modifier.size(32.dp))
            Image(painterResource(R.drawable.tile), contentDescription = stringResource(R.string.app_name), Modifier.size(56.dp))
            Spacer(Modifier.size(16.dp))
            Step(stringResource(R.string.enable), enabled(), misans) {
                startActivity(Intent(Settings.ACTION_INPUT_METHOD_SETTINGS))
            }
            Step(stringResource(R.string.choose), chosen(), misans) { imm().showInputMethodPicker() }
        }
    }

    @Composable
    private fun Step(label: String, done: Boolean, font: FontFamily, onClick: () -> Unit) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Button(
                onClick = onClick,
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF5F3DC4)),
            ) { Text(label, fontFamily = font, fontWeight = FontWeight.SemiBold, fontSize = 17.sp) }
            Spacer(Modifier.width(16.dp))
            if (done) Text("✓", color = Color(0xFFECEFF1), fontFamily = font, fontSize = 22.sp)
        }
    }
}
