# Xrnikey Troubleshooting

## 1. IME not appearing in input method list

**Symptom:** Xrnikey doesn't show up after installation.

**Fix (Fcitx5):**
```bash
sudo cp /usr/share/fcitx5/addon/xrnikey5.conf /usr/share/fcitx5/inputmethod/
fcitx5 -r
```

**Fix (IBus):**
```bash
ibus write-cache
ibus restart
```

## 2. Both Fcitx5 and IBus running

**Symptom:** Keys produce garbled output or no input at all.

**Fix:**
```bash
killall ibus-daemon   # if using Fcitx5
# OR
killall fcitx5        # if using IBus
```

Only one IMF should run. Use `xk-diagnose` to detect conflicts.

## 3. Wrong IM environment variables

**Symptom:** Input method works in some apps but not others.

**Fix:** On a Wayland compositor with an input-method frontend (Hyprland, Sway, KDE), GTK apps
reach Fcitx5 through text-input-v3, so **don't** set `GTK_IM_MODULE` (Fcitx5 wiki, "Using Fcitx 5
on Wayland"). Set the others where your session gets its environment (Hyprland: `env = ...`
lines in `hyprland.conf`):

```bash
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx      # X11 apps under XWayland
SDL_IM_MODULE=fcitx
# X11 sessions only:
GTK_IM_MODULE=fcitx
```

`xk-setup` checks these and offers to write the missing ones to
`~/.config/environment.d/90-xrnikey.conf`; it asks first and never changes ones already set.

## 4. Electron/Chromium apps not accepting input

**Symptom:** VS Code, Discord, Slack, Obsidian ignore the IME.

**Fix:** Under XWayland they use XIM (`XMODIFIERS=@im=fcitx`). As native Wayland apps they need
`--enable-wayland-ime` (with `--ozone-platform=wayland`), e.g. in `~/.config/code-flags.conf` for
VS Code. Xrnikey uses its preedit in them unless they report surrounding text (§8).

## 5. Wayland compositor missing text-input protocol

**Symptom:** IME works on X11 but not Wayland.

**Fix:** Ensure your compositor supports `text-input-v3` (wlroots-based compositors) or `input-method-v2`. Fcitx5 handles protocol negotiation automatically.

```bash
echo $XDG_SESSION_TYPE   # should be "wayland"
echo $WAYLAND_DISPLAY     # should be set
```

## 6. Vietnamese characters display as boxes

**Symptom:** Committed text shows tofu/boxes instead of diacritics.

**Fix:**
```bash
sudo pacman -S noto-fonts noto-fonts-cjk
fc-cache -fv
fc-match :lang=vi
```

## 7. libxrnikey.so not found

**Symptom:** `xk-diagnose` reports missing library.

**Fix:**
```bash
sudo ldconfig
ldconfig -p | grep xrnikey
# If missing:
sudo cmake --install build
sudo ldconfig
```

## 8. Instant commit: which apps get it

With `InstantCommit=True`, a word is typed straight into the field and corrected in place, with
no underlined preedit. Xrnikey uses it only where that is safe, and falls back to the preedit
otherwise:

- the app must send surrounding text (foot, Alacritty and kitty never do; X11 apps through XIM
  cannot), with nothing selected;
- the field must not be a terminal (Fcitx5 is told so by the app), and the app must not match
  `AppOverrideList` (terminals, VS Code and its forks, LibreOffice by default);
- while a word is typed, the text the app reports must still be the text Xrnikey typed. After a
  click elsewhere, or when the app rewrites or selects text, the word is left as it is and the
  next key starts a new word in preedit.

`AppOverrideList` takes shell-style patterns, matched without case against the app id Fcitx5
reports and against its last dot-separated part. To see the names, list Fcitx5's input contexts
(`program:` on each line; the focused one has `focus:1`):

```bash
busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 DebugInfo
```

Add one in `fcitx5-configtool` (Xrnikey's settings, "Force Preedit In These Apps"); it applies
from the next word.

## 9. Flatpak/Snap app isolation

**Symptom:** IME doesn't work in Flatpak or Snap applications.

**Fix (Flatpak):**
```bash
flatpak override --user --env=GTK_IM_MODULE=fcitx <app-id>
```

**Fix (Snap):**
Snap apps may not inherit host IME. Use native packages when possible.

## 10. Tone marks on wrong vowel

**Symptom:** `tuong` + hỏi produces incorrect tone placement.

**Fix:** This follows Vietnamese orthographic rules. Report specific syllables as bugs. The tone placement algorithm implements Rules 1–4 from Vietnamese linguistics standards.

## 11. User dictionary not updating

**Symptom:** Added words don't appear in completions.

**Fix:**
```bash
# Add the word (it goes to ~/.local/share/xrnikey/user-dict.txt), then have Fcitx5 reload
# so the addon reads your words again: "Reload Config" in its tray menu, or
xk-dict add "từ mới" 9000
fcitx5-remote -r
```

## 12. Picking a word suggestion

**Symptom:** Typing a digit does not pick the suggestion under it.

That is deliberate: digits are text in Telex (`mp3`, `a1`) and tone marks in VNI, so they are
never taken as picks. Suggestions are off by default; with `PredictionEnabled=True`, pick one
with **Ctrl+1–9**. Space always ends the word as typed. To use other keys, set
`CandidateSelectionKeys` in `~/.config/fcitx5/conf/xrnikey5.conf` (or in Fcitx5's settings).

## 13. Turning Vietnamese on and off

Xrnikey has no toggle key of its own: switch with Fcitx5's trigger key (Ctrl+Space by default,
set in Fcitx5's global options), or `fcitx5-remote -s keyboard-us` / `fcitx5-remote -s xrnikey`.

## 14. High latency / slow candidate window

**Symptom:** Noticeable delay between keypress and candidate display.

**Fix:**
```bash
# Ensure dictionary is mmap'd (not loading from slow storage):
ls -la /usr/share/xrnikey/dict/vi.dict
```

## 15. IBus engine crashes on startup

**Symptom:** `xrnikey-ibus` exits immediately.

**Fix:**
```bash
# Check libxrnikey is loadable:
python3 -c "import ctypes; ctypes.CDLL('/usr/lib/libxrnikey.so')"
# Check IBus is running:
ibus-daemon -drx
```

## 16. Environment variables not persisting

**Symptom:** IME works in current session but not after reboot.

**Fix:** Variables exported in `~/.bashrc` only reach programs started from a shell. Put them where
the session reads them: Hyprland's `env = NAME,value` lines, or `~/.config/environment.d/*.conf`
(what `xk-setup` offers to write). See §3 for which ones.

## 17. CachyOS-specific: fcitx5-unikey conflict

**Symptom:** Both unikey and Xrnikey active simultaneously.

**Fix:**
```bash
# Disable fcitx5-unikey:
sudo pacman -R fcitx5-unikey
fcitx5-configtool   # remove Unikey, add Xrnikey
```

## 18. Typing emoji

Xrnikey has no emoji input of its own. Use Fcitx5's Unicode picker (Ctrl+Alt+Shift+U by default),
which searches characters, emoji included, by name.

## 19. Build fails on missing Fcitx5 headers

**Symptom:** CMake can't find Fcitx5Core.

**Fix:**
```bash
sudo pacman -S fcitx5 fcitx5-qt fcitx5-gtk extra-cmake-modules
# Or build without Fcitx5:
cmake -B build -DXRNIKEY_BUILD_FCITX5=OFF
```

## Getting Help

Run the diagnostic tool for an automated report:
```bash
xk-diagnose
```

This checks all 12 common misconfiguration patterns and suggests exact fix commands.
