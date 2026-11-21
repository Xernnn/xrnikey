# xrnkey Architecture

## Subsystem Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     User Application                         │
│              (GTK/Qt/Electron/Terminal)                        │
└──────────┬──────────────────────────────┬───────────────────┘
           │ XIM / text-input-v3            │ IBus D-Bus
           ▼                                ▼
┌──────────────────────┐         ┌──────────────────────┐
│   Fcitx5 Framework   │         │    IBus Daemon       │
│  (xrnkey addon)      │         │  (xrnkey_engine.py)  │
└──────────┬───────────┘         └──────────┬───────────┘
           │ ctypes / direct link            │ ctypes
           ▼                                ▼
┌─────────────────────────────────────────────────────────────┐
│                      libxrnkey.so                            │
│  ┌─────────┐ ┌────────┐ ┌──────┐ ┌────────┐ ┌───────────┐  │
│  │ Telex/  │ │ Tone   │ │ Dict │ │ N-gram │ │ Autocorrect│  │
│  │ VNI FSM │ │ Place  │ │ Trie │ │ Model  │ │ Engine     │  │
│  └─────────┘ └────────┘ └──────┘ └────────┘ └───────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Keypress Data Flow

1. **Kernel** delivers key event via evdev to the compositor (Wayland) or X server (X11)
2. **Fcitx5** receives the key through `input-method-v2` (Wayland apps), its D-Bus frontend (the
   GTK/Qt IM modules) or XIM (X11 apps), and hands it to the addon (`fcitx5-addon/xrnkey_addon.cpp`)
3. **The addon** decides what the key is: a pick from the suggestion window (Ctrl+1–9), a key in a
   password or sensitive field (passed through untouched), a key that ends the word (Return, Tab,
   arrows, shortcuts: the word is finished, then the key goes to the app), or a key for the core
4. **xrnkey core** (`xk_process_key(state, keysym, modifiers)`) runs the Telex/VNI state machine:
   - Vowel modification (aa→â, dd→đ)
   - Tone mark application (s→sắc)
   - Auto-restore: the word's raw keystrokes are kept, and it is shown as typed when it
     cannot be Vietnamese, or committed as typed when it is a common English word that is
     not an attested Vietnamese syllable
5. **Tone placement** applies Unicode combining characters per Vietnamese orthographic rules
6. **Candidate provider** queries dictionary + n-gram + autocorrect for suggestions (when on)
7. **Output**, chosen per word:
   - *preedit*: the word is shown inline (underlined) and committed when it ends;
   - *instant commit*: each result goes into the field at once; a correction deletes what the
     addon typed for the word (`deleteSurroundingText`) and types the new form. The addon only does
     this where the app reports surrounding text, and keeps a model of what should be before the
     cursor (its own edits, which the app reports back late when typing is fast). When a report
     does not match — a click elsewhere, the app changing or selecting text — it stops editing
     that word in place. On Wayland, Fcitx5 converts the deletion to bytes from its copy of the
     surrounding text, so the addon hands it exactly the characters it typed.

## Core Library (libxrnkey)

Pure C ABI (`xrnkey.h`) — no C++ in public headers. Internal implementation in C++20.

| Module | File | Purpose |
|--------|------|---------|
| Word model | `word.cpp` | Raw keystrokes of the word, replayed into the composition; display and auto-restore |
| Telex FSM | `telex.cpp` | Telex input encoding |
| VNI FSM | `vni.cpp` | VNI digit-based encoding |
| Tone placement | `tone_placement.cpp` | Vietnamese orthographic tone rules |
| Syllable validator | `syllable_validator.cpp` | Onset/rime/tone tables: valid syllable, valid prefix |
| Lexicon | `lexicon.cpp`, `data/` | Common English words, attested Vietnamese syllables (compiled in) |
| Dictionary | `dict.cpp` | Memory-mapped double-array trie |
| N-gram | `ngram.cpp` | ARPA trigram model with 4-bit quantization |
| Autocorrect | `autocorrect.cpp` | Damerau-Levenshtein + phonetic scoring |
| Bilingual | `bilingual_switch.cpp` | English word detection, enable state |
| Emoji | `emoji.cpp` | Fuzzy emoji search |

## Memory Budget

| Component | Target |
|-----------|--------|
| libxrnkey core | < 8 MB |
| Dictionary (mmap) | < 20 MB |
| N-gram model | < 50 MB |

Dictionary uses `mmap()` for zero-copy startup. N-gram scores use 4-bit quantization. Candidate generation uses arena allocation with a 512-entry LRU cache.

## Configuration Persistence

- `~/.config/fcitx5/conf/xrnkey.conf` — Fcitx5 addon settings (re-read on "Reload Config")
- `~/.local/share/xrnkey/user-dict.txt` — user dictionary (re-read on "Reload Config")
- `~/.local/share/xrnkey/macros.txt` — abbreviations (re-read when the file changes)

## Tests

- `tests/` — Catch2 tests of the core's C API.
- `fcitx5-addon/tests/test_fcitx5_addon.cpp` — the built addon inside an in-process
  `fcitx::Instance`, typed into by a fake client (D-Bus, Wayland input-method-v2 or XIM style, with
  optional late surrounding-text reports) that applies commits, deletions and unhandled keys to a
  text buffer.
- `fcitx5-addon/tests/dbus_session.sh` — the real `fcitx5` binary on a private D-Bus, driven over
  `org.fcitx.Fcitx.InputContext1` by `dbus_driver.py`.
