<p align="center">
  <img src="data/icons/xrnikey.svg" width="72" height="72" alt="">
</p>

<h1 align="center">Xrnikey</h1>

Production-grade bilingual Vietnamese–English Input Method Engine for Linux.

Xrnikey provides Telex and VNI input methods with intelligent bilingual auto-switching, dictionary completion, n-gram prediction, autocorrect, and emoji input. It ships as a Fcitx5 addon (primary) and IBus engine (GNOME fallback).

## Features

- **Telex & VNI** — full Vietnamese input encoding with tone marks, vowel modifications, and backspace undo
- **Unikey-style instant commit** — corrects tones in place as you type (no visible composition buffer), like Unikey on Windows, instead of holding the word in preedit until Space; opt-in via `InstantCommit=True`, and only where the app's surrounding text makes it safe (classic preedit in terminals and apps without it; see TROUBLESHOOTING.md §8)
- **Bilingual auto-switch** — type English without toggling the IME off
- **Dictionary completion** — 800+ word seed dictionary with user dictionary hot-reload
- **N-gram prediction** — context-aware word suggestions
- **Autocorrect** — Damerau-Levenshtein with Vietnamese phonetic similarity
- **Fcitx5 + IBus** — works on KDE, Sway, Hyprland, XFCE (Fcitx5) and GNOME (IBus)
- **Wayland & X11** — full compositor compatibility

## Download

```bash
git clone https://github.com/Xernnn/xernikey.git xrnikey
cd xrnikey
```

## Quick Start (Arch Linux / CachyOS)

```bash
# Install dependencies
sudo pacman -S cmake extra-cmake-modules fcitx5 fcitx5-gtk fcitx5-qt \
    boost python python-gobject noto-fonts noto-fonts-cjk

# Configure, build, install
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DXRNIKEY_ENABLE_TESTS=ON
cmake --build build -- -j$(nproc)
sudo cmake --install build
sudo ldconfig

# Run setup
xk-setup
```

`-DCMAKE_INSTALL_PREFIX=/usr` is required, not optional — it's what makes Fcitx5's own CMake config route the addon into the real system dirs (`/usr/lib/fcitx5`, `/usr/share/fcitx5/addon`) that the running `fcitx5` daemon actually scans. Any other prefix (including the CMake default, `/usr/local`) installs the addon somewhere Fcitx5 never looks, and it silently never loads.

Log out and back in (so the env vars `xk-setup` writes take effect), then switch to Xrnikey with **Ctrl+Space** (Fcitx5) or **Super+Space** (IBus).

To verify it actually loaded: `fcitx5-diagnose | grep -i xrnikey`, or run `xk-diagnose`.

## Switching from Bamboo (and back)

Xrnikey and fcitx5-bamboo can both be in Fcitx5's input method list; only the current one types.
With Fcitx5 running, add Xrnikey next to Bamboo (or use `fcitx5-configtool`):

```bash
# 1. See the current list of the "Default" group:
busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 InputMethodGroupInfo s Default
#    e.g.  "us" 2 "keyboard-us" "" "bamboo" ""
# 2. Give the same list back with xrnikey added, and save it:
busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 \
    SetInputMethodGroupInfo 'ssa(ss)' Default us 3 keyboard-us '' bamboo '' xrnikey ''
busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 Save
# 3. Switch:
fcitx5-remote -s xrnikey      # Xrnikey
fcitx5-remote -s bamboo       # back to Bamboo
```

To take Xrnikey out again, repeat step 2 with the original list (`... Default us 2 keyboard-us ''
bamboo ''`). Ctrl+Space toggles between the keyboard layout and the input method used last.

## Keybindings

| Action | Default Key |
|--------|-------------|
| Switch Vietnamese on/off | Fcitx5's trigger key (Ctrl+Space by default) |
| Commit word | Space |
| Pick suggestion 1–9 (when `PredictionEnabled=True`) | Ctrl+1–9 (`CandidateSelectionKeys`) |
| Page candidates | Tab / Shift+Tab |
| Undo composition | Backspace |

## Input Methods

| Method | Example | Output |
|--------|---------|--------|
| Telex | `aas` | `âs` → `ấ` (with tone) |
| Telex | `dd` | `đ` |
| Telex | `ow` | `ơ` |
| VNI | `a1` | `á` |
| VNI | `o6` | `ô` |
| VNI | `d9` | `đ` |

Type `xin chao` in Telex mode → `xin chào`.

## CLI Tools

```bash
xk-setup          # Interactive setup and configuration
xk-setup --rollback  # Restore backed-up config files
xk-diagnose       # Comprehensive diagnostic report
xk-dict lookup "hạnh phúc"  # Dictionary lookup
xk-dict add "từ mới" 9000   # Add to user dictionary
xk-macro add ko không       # Abbreviation: "ko" + Space -> "không" (needs Macro=True)
xk-macro list               # Show them; xk-macro remove ko to drop one
```

## Configuration

Fcitx5: `~/.config/fcitx5/conf/xrnikey5.conf`

```
[General]
InputMode=Telex
InstantCommit=False
PredictionEnabled=False
CandidateLayout=Vertical
```

State file: `~/.local/share/xrnikey/state.json`
User dictionary: `~/.local/share/xrnikey/user-dict.txt`
Abbreviations: `~/.local/share/xrnikey/macros.txt` (`abbreviation<TAB>expansion` per line)

## Project Structure

```
xrnikey/
├── core/           libxrnikey C++20 engine
├── fcitx5-addon/   Fcitx5 input method plugin
├── ibus-engine/    IBus fallback (Python + ctypes)
├── cli-tools/      xk-setup, xk-diagnose, xk-dict
├── data/           Dictionary, n-gram, emoji data
├── tests/          Catch2 unit and integration tests
└── packaging/      PKGBUILD and INSTALL.sh
```

## License

GPL-3.0
