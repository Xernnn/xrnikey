# VietKey

Production-grade bilingual Vietnamese–English Input Method Engine for Linux.

VietKey provides Telex and VNI input methods with intelligent bilingual auto-switching, dictionary completion, n-gram prediction, autocorrect, and emoji input. It ships as a Fcitx5 addon (primary) and IBus engine (GNOME fallback).

## Features

- **Telex & VNI** — full Vietnamese input encoding with tone marks, vowel modifications, and backspace undo
- **Unikey-style instant commit** — corrects tones in place as you type (no visible composition buffer), like Unikey on Windows, instead of holding the word in preedit until Space; opt-in via `InstantCommit=True`, falls back to classic preedit in apps without surrounding-text support
- **Bilingual auto-switch** — type English without toggling the IME off
- **Dictionary completion** — 800+ word seed dictionary with user dictionary hot-reload
- **N-gram prediction** — context-aware word suggestions
- **Autocorrect** — Damerau-Levenshtein with Vietnamese phonetic similarity
- **Emoji input** — fuzzy search with Vietnamese tags (`:cười` → 😊)
- **Fcitx5 + IBus** — works on KDE, Sway, Hyprland, XFCE (Fcitx5) and GNOME (IBus)
- **Wayland & X11** — full compositor compatibility

## Download

```bash
git clone https://github.com/Xernnn/xernikey.git vietkey
cd vietkey
```

## Quick Start (Arch Linux / CachyOS)

```bash
# Install dependencies
sudo pacman -S cmake extra-cmake-modules fcitx5 fcitx5-gtk fcitx5-qt \
    boost python python-gobject noto-fonts noto-fonts-cjk

# Configure, build, install
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DVIETKEY_ENABLE_TESTS=ON
cmake --build build -- -j$(nproc)
sudo cmake --install build
sudo ldconfig

# Run setup
vk-setup
```

`-DCMAKE_INSTALL_PREFIX=/usr` is required, not optional — it's what makes Fcitx5's own CMake config route the addon into the real system dirs (`/usr/lib/fcitx5`, `/usr/share/fcitx5/addon`) that the running `fcitx5` daemon actually scans. Any other prefix (including the CMake default, `/usr/local`) installs the addon somewhere Fcitx5 never looks, and it silently never loads.

Log out and back in (so the env vars `vk-setup` writes take effect), then switch to VietKey with **Ctrl+Space** (Fcitx5) or **Super+Space** (IBus).

To verify it actually loaded: `fcitx5-diagnose | grep -i vietkey`, or run `vk-diagnose`.

## Keybindings

| Action | Default Key |
|--------|-------------|
| Toggle IME on/off | Ctrl+Shift |
| Commit word | Space |
| Select candidate 1–9 | 1–9 |
| Page candidates | Tab / Shift+Tab |
| Emoji trigger | `:` |
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
vk-setup          # Interactive setup and configuration
vk-setup --rollback  # Restore backed-up config files
vk-diagnose       # Comprehensive diagnostic report
vk-dict lookup "hạnh phúc"  # Dictionary lookup
vk-dict add "từ mới" 9000   # Add to user dictionary
```

## Configuration

Fcitx5: `~/.config/fcitx5/conf/vietkey5.conf`

```
[General]
InputMode=Telex
AutoSwitch=True
InstantCommit=False
PredictionEnabled=True
EmojiEnabled=True
CandidateLayout=Vertical
```

State file: `~/.local/share/vietkey/state.json`
User dictionary: `~/.local/share/vietkey/user-dict.txt`

## Project Structure

```
vietkey/
├── core/           libvietkey C++20 engine
├── fcitx5-addon/   Fcitx5 input method plugin
├── ibus-engine/    IBus fallback (Python + ctypes)
├── daemon/         vkd prediction/emoji service
├── cli-tools/      vk-setup, vk-diagnose, vk-dict
├── data/           Dictionary, n-gram, emoji data
├── tests/          Catch2 unit and integration tests
└── packaging/      PKGBUILD and INSTALL.sh
```

## License

GPL-3.0
