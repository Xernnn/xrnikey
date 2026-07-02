# VietKey Troubleshooting

## 1. IME not appearing in input method list

**Symptom:** VietKey doesn't show up after installation.

**Fix (Fcitx5):**
```bash
sudo cp /usr/share/fcitx5/addon/vietkey5.conf /usr/share/fcitx5/inputmethod/
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

Only one IMF should run. Use `vk-diagnose` to detect conflicts.

## 3. Wrong GTK_IM_MODULE for desktop environment

**Symptom:** Input method works in some apps but not others.

**Fix:**
```bash
# KDE, Sway, Hyprland, XFCE:
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx

# GNOME:
export GTK_IM_MODULE=ibus
export QT_IM_MODULE=ibus
export XMODIFIERS=@im=ibus
```

Run `vk-setup` to configure automatically.

## 4. Electron/Chromium apps not accepting input

**Symptom:** VS Code, Discord, Slack, Obsidian ignore the IME.

**Fix:**
```bash
vk-setup   # auto-patches Electron app flags
# Manual fix for VS Code:
echo "--gtk-version=4" >> ~/.config/Code/flags.conf
```

For Chromium: launch with `--gtk-version=4` or set `GTK_IM_MODULE=fcitx`.

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

## 7. vkd daemon not running

**Symptom:** Prediction and emoji search don't work.

**Fix:**
```bash
systemctl --user enable vkd
systemctl --user start vkd
systemctl --user status vkd
```

## 8. libvietkey.so not found

**Symptom:** `vk-diagnose` reports missing library.

**Fix:**
```bash
sudo ldconfig
ldconfig -p | grep vietkey
# If missing:
sudo cmake --install build
sudo ldconfig
```

## 9. Terminal apps processing Vietnamese input

**Symptom:** Terminal receives raw Telex keys instead of composed characters.

**Fix:** Add terminal WM_CLASS to app overrides:
```json
// ~/.config/vietkey/app-overrides.json
["code", "alacritty", "kitty", "foot", "wezterm", "terminal", "konsole"]
```

Or disable VietKey in terminals via Fcitx5 config: `AppOverrideList`.

## 10. Flatpak/Snap app isolation

**Symptom:** IME doesn't work in Flatpak or Snap applications.

**Fix (Flatpak):**
```bash
flatpak override --user --env=GTK_IM_MODULE=fcitx <app-id>
```

**Fix (Snap):**
Snap apps may not inherit host IME. Use native packages when possible.

## 11. Tone marks on wrong vowel

**Symptom:** `tuong` + hỏi produces incorrect tone placement.

**Fix:** This follows Vietnamese orthographic rules. Report specific syllables as bugs. The tone placement algorithm implements Rules 1–4 from Vietnamese linguistics standards.

## 12. User dictionary not updating

**Symptom:** Added words don't appear in completions.

**Fix:**
```bash
# Ensure vkd is running (handles hot-reload):
systemctl --user restart vkd

# Or manually reload:
vk-dict add "từ mới" 9000
```

## 13. Space commits instead of selecting candidate

**Symptom:** Pressing Space commits preedit instead of selecting candidate #1.

**Fix:** Configure in `~/.config/fcitx5/conf/vietkey5.conf`:
```
CommitKey=space
```
Use number keys 1–9 to select candidates explicitly.

## 14. Ctrl+Shift toggle not working

**Symptom:** Cannot toggle IME on/off.

**Fix:** Check for keybinding conflicts with desktop environment or window manager. Try rebinding in Fcitx5 global options.

## 15. High latency / slow candidate window

**Symptom:** Noticeable delay between keypress and candidate display.

**Fix:**
```bash
# Check vkd socket latency:
echo '{"type":"stats"}' | nc -U /run/user/$(id -u)/vietkey.sock

# Ensure dictionary is mmap'd (not loading from slow storage):
ls -la /usr/share/vietkey/dict/vi.dict
```

## 16. IBus engine crashes on startup

**Symptom:** `vietkey-ibus` exits immediately.

**Fix:**
```bash
# Check libvietkey is loadable:
python3 -c "import ctypes; ctypes.CDLL('/usr/lib/libvietkey.so')"
# Check IBus is running:
ibus-daemon -drx
```

## 17. Environment variables not persisting

**Symptom:** IME works in current session but not after reboot.

**Fix:**
```bash
vk-setup   # writes to ~/.bashrc, ~/.pam_environment, etc.
# Or manually add to ~/.pam_environment:
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
```

## 18. CachyOS-specific: fcitx5-unikey conflict

**Symptom:** Both unikey and VietKey active simultaneously.

**Fix:**
```bash
# Disable fcitx5-unikey:
sudo pacman -R fcitx5-unikey
fcitx5-configtool   # remove Unikey, add VietKey
```

## 19. Emoji search returns no results

**Symptom:** `:smile` shows no emoji candidates.

**Fix:**
```bash
# Verify emoji data:
ls /usr/share/vietkey/emoji/emoji.json
# Regenerate:
python3 /usr/share/vietkey/scripts/generate.py
systemctl --user restart vkd
```

## 20. Build fails on missing Fcitx5 headers

**Symptom:** CMake can't find Fcitx5Core.

**Fix:**
```bash
sudo pacman -S fcitx5 fcitx5-qt fcitx5-gtk extra-cmake-modules
# Or build without Fcitx5:
cmake -B build -DVIETKEY_BUILD_FCITX5=OFF
```

## Getting Help

Run the diagnostic tool for an automated report:
```bash
vk-diagnose
```

This checks all 12 common misconfiguration patterns and suggests exact fix commands.
