# Xrnikey Troubleshooting

## 1. IME not appearing in input method list

**Symptom:** Xrnikey doesn't show up after installation.

**Fix (Fcitx5):** both files must exist, and Fcitx5 must have been restarted since:
```bash
ls /usr/lib/fcitx5/libxrnikey5.so /usr/share/fcitx5/addon/xrnikey5.conf \
   /usr/share/fcitx5/inputmethod/xrnikey.conf
fcitx5 -r -d
```
If they are under `/usr/local`, reinstall with `-DCMAKE_INSTALL_PREFIX=/usr`.

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
QT_IM_MODULE=fcitx        # needs fcitx5-qt
XMODIFIERS=@im=fcitx      # X11 apps under XWayland
SDL_IM_MODULE=fcitx
# X11 sessions only:
GTK_IM_MODULE=fcitx
```

`xk-setup` checks these and offers to write the missing ones to
`~/.config/environment.d/90-xrnikey.conf`; it asks first and never changes ones already set. It
offers `QT_IM_MODULE=fcitx` only when fcitx5-qt is installed: without it Qt apps would get no
input method at all.

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

The IBus engine is old and unmaintained: it is written against the long-gone `python-ibus` API,
and the PKGBUILD does not build it. Use Fcitx5.

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

## 20. Clicking elsewhere while a word is underlined (Wayland)

With the preedit (the default), the word being typed is underlined in the app until it ends. In
apps that use Fcitx5's GTK or Qt module (`QT_IM_MODULE=fcitx`, X11 `GTK_IM_MODULE=fcitx`) a click
elsewhere ends it: the app keeps the word where it was and tells Fcitx5. A native Wayland app
(text-input-v3) tells Fcitx5 nothing about the click, only, at some point, its new text and
cursor, and what happens to the word depends on the app (as their source code reads; not yet
checked in each):

- **Chromium, Electron (with `--enable-wayland-ime`), Firefox** keep the word where it was. Xrnikey
  sees it in the app's text and starts the next word at the new cursor. A click right at the end
  of the word cannot be told apart from typing on, so there the word goes on (and can end up
  twice).
- **GTK** apps draw the underlined word at the new cursor and report the move only with the next
  key; the word goes on there.
- **Qt** apps without `QT_IM_MODULE=fcitx` drop the underlined word; it comes back at the new
  cursor with the next key.

To avoid the last two, end the word (Space, punctuation, an arrow key) before clicking, or use
`InstantCommit=True` (§8), where the word is in the field all along.

## Getting Help

Run the diagnostic tool for an automated report:
```bash
xk-diagnose                          # prints it
xk-diagnose --output report.txt      # also saves it
```

It checks the environment variables, the running input method framework, the library, fonts,
the session type and the data files, and suggests fixes. It leaves out the host and user name
unless given `--full`.
