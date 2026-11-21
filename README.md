<p align="center">
  <img src="data/icons/xrnikey.svg" width="72" height="72" alt="">
</p>

<h1 align="center">Xrnikey</h1>

A Vietnamese input method (Telex and VNI) for Fcitx5 on Linux, with an optional instant-commit
mode that types each word straight into the field, without an underlined preedit.

## Features

- **Telex & VNI** — tone marks and vowel marks, placed by Vietnamese spelling rules; tone and mark
  keys may follow the final consonant (`toans`, `dangd` → `toán`, `đang`), a key typed again undoes
  itself (`aaa` → `aa`, `ass` → `as`), and in Telex `w` alone is `ư`
- **Instant commit (opt-in)** — like Unikey on Windows: each keystroke goes into the field at once
  and the word is corrected in place, so there is no underlined preedit, and the app sees the text
  as it is typed. Used only where the app's surrounding text makes it safe;
  terminals and apps without surrounding text keep the classic preedit (TROUBLESHOOTING.md §8)
- **English passes through** — a word that cannot be Vietnamese, or a common English word that is
  not a Vietnamese syllable (`terms`, not `tém`), is left as typed
- **Abbreviations** — `ko` + Space → `không`, from your own list (`xk-macro`)
- **Word suggestions (opt-in)** — dictionary completion ranked by an n-gram model, plus a user
  dictionary; picked with Ctrl+1–9 so digits always stay text. The bundled data is small: a seed
  list of about 800 words and a sample n-gram model
- **Fcitx5 on Wayland and X11** — input-method-v2 (Hyprland, Sway), the GTK/Qt modules, and XIM

## Download

```bash
git clone https://github.com/Xernnn/xrnikey.git
cd xrnikey
```

## Build and install

```bash
sudo pacman -S --needed cmake extra-cmake-modules ninja fcitx5 fcitx5-configtool python
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DXRNIKEY_BUILD_IBUS=OFF
cmake --build build
sudo cmake --install build
```

`-DCMAKE_INSTALL_PREFIX=/usr` is required: it makes Fcitx5's own CMake config put the addon in the
directories the running `fcitx5` scans (`/usr/lib/fcitx5`, `/usr/share/fcitx5/addon` and
`/usr/share/fcitx5/inputmethod`). Any other prefix, including the default `/usr/local`, installs it
where Fcitx5 never looks.

Or as a package, from the commit checked out here (tests run in `check()`; `pacman -R xrnikey`
removes it again):

```bash
cd packaging && makepkg -si
```

On other distributions, install CMake, Ninja, a C++20 compiler, extra-cmake-modules and Fcitx5's
development files, then run the same `cmake` commands, keeping `-DCMAKE_INSTALL_PREFIX=/usr`
(`sudo xargs rm < build/install_manifest.txt` removes the files again). On Debian and Ubuntu, as
in CI (add `catch2` for the tests):

```bash
sudo apt-get install cmake ninja-build g++ extra-cmake-modules \
    libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev fcitx5-modules-dev fcitx5
```

Tests: configure with `-DXRNIKEY_ENABLE_TESTS=ON` (needs `catch2`), then `ctest --test-dir build`.
They cover the core, the CLI tools, and the addon running in an in-process Fcitx5 and in the real
`fcitx5` binary on a private D-Bus (that one needs `dbus` and `python-gobject`, and is skipped
without them); neither touches a running Fcitx5 or its config. CI runs the same build and tests
on Ubuntu.

Then restart Fcitx5 (`fcitx5 -r -d`) so it finds the new addon, add Xrnikey to its input methods
(next section), and check the environment with `xk-setup` (it reports, and asks before changing
anything) and `xk-diagnose`.

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

## Typing

| Method | Keys | Gives |
|--------|------|-------|
| Telex | `aa` `ee` `oo` | `â` `ê` `ô` |
| Telex | `aw` `ow` `uw` | `ă` `ơ` `ư` |
| Telex | `dd` | `đ` |
| Telex | `s` `f` `r` `x` `j` after a vowel | sắc, huyền, hỏi, ngã, nặng |
| Telex | `w` | `ư` |
| Telex | `aaa`, `ass` | `aa`, `as` (a key typed again undoes itself) |
| Telex | `z` | removes the tone |
| VNI | `a1` `o6` `d9` | `á` `ô` `đ` |
| VNI | `0` | removes the tone |

`xin chaof` → `xin chào`, `tieengs vieetj` → `tiếng việt`.

An English word whose keys also spell an everyday Vietnamese syllable stays Vietnamese: `list` →
`lít`, `turn` → `tủn`, `cups` → `cúp` (the word lists and their sources are in
[`core/data/README.md`](core/data/README.md)).

| Key | What it does |
|-----|--------------|
| Space, punctuation | end the word |
| Return, Tab, Esc, arrows, Home/End, any Ctrl/Alt/Super shortcut | end the word (kept as shown), then act as usual: one Return both finishes the word and sends the newline |
| Backspace | edits the word being typed; with no word in progress, deletes as usual |
| Ctrl+1–9 | pick suggestion 1–9 (when suggestions are on) |
| Ctrl+Alt+P | turn suggestions on/off |
| Ctrl+Space (Fcitx5's trigger key) | Vietnamese on/off |

A click elsewhere, focus moving to another window, or switching input method keeps the word that
was being typed (on Wayland, a click in a GTK or Qt app is the exception: TROUBLESHOOTING.md §20).
Password and other sensitive fields get every key as typed.

## Options

In `fcitx5-configtool` (Xrnikey's settings), or in `~/.config/fcitx5/conf/xrnikey5.conf` as
`Name=Value` lines with no section header, followed by `fcitx5-remote -r` (or "Reload Config"). A
changed option applies to fields that are already open.

| Option | Default | |
|--------|---------|---|
| `InputMode` | `Telex` | `Telex` or `VNI` |
| `InstantCommit` | `False` | type into the field directly instead of a preedit, where safe |
| `AutoRestore` | `True` | a word that isn't Vietnamese is left as typed |
| `ModernStyle` | `False` | tone on the second vowel: `hoà`, `thuý` (off: `hòa`, `thúy`) |
| `FreeMarking` | `True` | tone and mark keys may come after the final consonant |
| `WShortcut` | `True` | Telex `w` alone types `ư` |
| `AppOverrideList` | terminals, VS Code, LibreOffice | apps that always get the preedit (patterns, matched against the app id; TROUBLESHOOTING.md §8) |
| `Macro` | `False` | expand abbreviations from `~/.local/share/xrnikey/macros.txt` |
| `CapitalizeMacro` | `True` | `Ko` → `Không`, `KO` → `KHÔNG` (`ko` → as written) |
| `PredictionEnabled` | `False` | show word suggestions |
| `CandidateSelectionKeys` | Ctrl+1 … Ctrl+9 | keys that pick suggestion 1–9 |
| `PredictionMaxCandidates` | `5` | how many suggestions |
| `AutoCorrect` | `True` | add spelling corrections to the suggestions |
| `AutoCorrectThreshold` | `0.85` | how close a correction must be |
| `CandidateLayout` | `Vertical` | suggestion window layout |
| `PageSize` | `9` | suggestions per page |
| `TogglePrediction` | Ctrl+Alt+P | key that turns suggestions on/off |

Example:

```
InputMode=Telex
InstantCommit=True
Macro=True
```

Files: user dictionary `~/.local/share/xrnikey/user-dict.txt` (`xk-dict`), abbreviations
`~/.local/share/xrnikey/macros.txt` (`abbreviation<TAB>expansion` per line, `xk-macro`). Both follow
`$XDG_DATA_HOME`.

## How it differs from Bamboo

- **Instant commit.** fcitx5-bamboo always holds the word in an underlined preedit. Xrnikey can type
  it into the field and correct it in place (opt-in), and checks the app's surrounding text before
  every correction, so it never deletes text it did not type; where it cannot check, it uses the
  preedit like Bamboo.
- **Suggestions and a user dictionary** (optional); Bamboo has none.
- **Abbreviations** work like Bamboo's macros (expand at word end; Tab completes one), but live in a
  plain text file edited with `xk-macro`, and with `CapitalizeMacro` a lowercase abbreviation gives
  the expansion as written (Bamboo lowercases it, so `vn` → `Việt Nam` became `việt nam`).
- **Not (yet) in Xrnikey:** Bamboo's other input methods (Telex 2, Telex W, VIQR, …), output
  charsets other than Unicode (TCVN3, VNI-Win, …), and the restore-keystrokes key.

## CLI Tools

```bash
xk-setup                    # Check the Fcitx5 setup; asks before each change (--dry-run, --rollback)
xk-diagnose                 # Diagnostic report (--output PATH saves it; --full adds host and user)
xk-dict lookup "hạnh phúc"  # Dictionary lookup
xk-dict add "từ mới" 9000   # Add to the user dictionary (then fcitx5-remote -r)
xk-macro add ko không       # Abbreviation: "ko" + Space -> "không" (needs Macro=True)
xk-macro list               # Show them; xk-macro remove ko drops one
```

## Project Structure

```
xrnikey/
├── core/           libxrnikey: the C++20 engine behind a C API
├── fcitx5-addon/   the Fcitx5 input method, and its end-to-end tests (tests/)
├── ibus-engine/    an old IBus engine (not built by the PKGBUILD; targets python-ibus)
├── cli-tools/      xk-setup, xk-diagnose, xk-dict, xk-macro
├── data/           dictionary, n-gram and emoji data, icon
├── tests/          Catch2 tests of the core
└── packaging/      PKGBUILD
```

How a key travels from the app to the engine and back: [ARCHITECTURE.md](ARCHITECTURE.md). When
something doesn't type: [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## License

MIT — see [LICENSE](LICENSE). The word lists compiled into the engine come from SCOWL, Unicode's
Unihan database, Mozilla Common Voice, Vietnamese Wikisource and Tatoeba, under their own terms;
[`core/data/README.md`](core/data/README.md) has the sources and the notices they ask for.
