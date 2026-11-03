# Xrnikey Architecture

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
│  (xrnikey5 addon)    │         │  (xrnikey_engine.py) │
└──────────┬───────────┘         └──────────┬───────────┘
           │ ctypes / direct link            │ ctypes
           ▼                                ▼
┌─────────────────────────────────────────────────────────────┐
│                      libxrnikey.so                           │
│  ┌─────────┐ ┌────────┐ ┌──────┐ ┌────────┐ ┌───────────┐  │
│  │ Telex/  │ │ Tone   │ │ Dict │ │ N-gram │ │ Autocorrect│  │
│  │ VNI FSM │ │ Place  │ │ Trie │ │ Model  │ │ Engine     │  │
│  └─────────┘ └────────┘ └──────┘ └────────┘ └───────────┘  │
└──────────────────────────┬──────────────────────────────────┘
                           │ Unix socket (JSON)
                           ▼
                  ┌─────────────────┐
                  │   xkd daemon    │
                  │  prediction /   │
                  │  emoji / stats  │
                  └─────────────────┘
```

## Keypress Data Flow

1. **Kernel** delivers key event via evdev to the compositor (Wayland) or X server (X11)
2. **Fcitx5/IBus** receives the key through `input-method-v2` or XIM protocol
3. **Xrnikey engine** calls `xk_process_key(state, keysym, modifiers)`
4. **Telex/VNI FSM** processes the character:
   - Vowel modification (aa→â, dd→đ)
   - Tone mark application (s→sắc)
   - Bilingual check (English word → pass-through)
5. **Tone placement** applies Unicode combining characters per Vietnamese orthographic rules
6. **Candidate provider** queries dictionary + n-gram + autocorrect for suggestions
7. **xkd daemon** (optional) answers the same dictionary, emoji and stats queries over a Unix
   socket for other tools; the input method doesn't use it (step 6 reads the core in process),
   so it isn't started by default
8. **Preedit** rendered inline or in aux window; **commit** sent to application on Space

## Core Library (libxrnikey)

Pure C ABI (`xrnikey.h`) — no C++ in public headers. Internal implementation in C++20.

| Module | File | Purpose |
|--------|------|---------|
| Telex FSM | `telex.cpp` | Telex input encoding |
| VNI FSM | `vni.cpp` | VNI digit-based encoding |
| Tone placement | `tone_placement.cpp` | Vietnamese orthographic tone rules |
| Syllable validator | `syllable_validator.cpp` | Phonotactic validation |
| Dictionary | `dict.cpp` | Memory-mapped double-array trie |
| N-gram | `ngram.cpp` | ARPA trigram model with 4-bit quantization |
| Autocorrect | `autocorrect.cpp` | Damerau-Levenshtein + phonetic scoring |
| Bilingual | `bilingual_switch.cpp` | English word detection + whitelist |
| Emoji | `emoji.cpp` | Fuzzy emoji search |

## Memory Budget

| Component | Target |
|-----------|--------|
| libxrnikey core | < 8 MB |
| Dictionary (mmap) | < 20 MB |
| N-gram model | < 50 MB |
| xkd daemon | < 80 MB |

Dictionary uses `mmap()` for zero-copy startup. N-gram scores use 4-bit quantization. Candidate generation uses arena allocation with a 512-entry LRU cache.

## Configuration Persistence

- `~/.local/share/xrnikey/state.json` — IME enabled/disabled state
- `~/.config/xrnikey/app-overrides.json` — per-app disable list
- `~/.local/share/xrnikey/user-dict.txt` — user dictionary (hot-reloaded)
- `~/.config/fcitx5/conf/xrnikey5.conf` — Fcitx5 addon settings

## Socket Protocol (xkd)

Newline-delimited JSON over Unix domain socket at `/run/user/$UID/xrnikey.sock`:

```json
{"type": "complete", "prefix": "hap", "context": ["tôi", "rất"], "max": 5}
{"type": "emoji", "query": "smile"}
{"type": "stats"}
```
