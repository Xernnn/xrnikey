#!/usr/bin/env python3
"""Generate emoji.json from CLDR emoji data."""

import json
import sys
import urllib.request
from pathlib import Path

CLDR_URL = "https://unicode.org/Public/emoji/15.1/emoji-test.txt"
OUTPUT = Path(__file__).parent / "emoji.json"

# Minimal seed emoji for offline builds
SEED_EMOJI = [
    {"char": "😀", "name": "grinning face", "shortcode": ":grinning:", "tags": "happy smile", "vi": "cười mỉm"},
    {"char": "😃", "name": "grinning face with big eyes", "shortcode": ":smiley:", "tags": "happy smile", "vi": "cười to"},
    {"char": "😄", "name": "grinning face with smiling eyes", "shortcode": ":smile:", "tags": "happy smile", "vi": "cười vui"},
    {"char": "😁", "name": "beaming face with smiling eyes", "shortcode": ":grin:", "tags": "happy smile", "vi": "cười tươi"},
    {"char": "😊", "name": "smiling face with smiling eyes", "shortcode": ":blush:", "tags": "happy smile shy", "vi": "mỉm cười"},
    {"char": "😅", "name": "grinning face with sweat", "shortcode": ":sweat_smile:", "tags": "relief smile", "vi": "cười gượng"},
    {"char": "😂", "name": "face with tears of joy", "shortcode": ":joy:", "tags": "laugh cry", "vi": "cười ra nước mắt"},
    {"char": "🤣", "name": "rolling on the floor laughing", "shortcode": ":rofl:", "tags": "laugh", "vi": "cười lăn"},
    {"char": "❤️", "name": "red heart", "shortcode": ":heart:", "tags": "love", "vi": "trái tim"},
    {"char": "👍", "name": "thumbs up", "shortcode": ":thumbsup:", "tags": "approve ok", "vi": "tán thành"},
    {"char": "👎", "name": "thumbs down", "shortcode": ":thumbsdown:", "tags": "disapprove", "vi": "không tán thành"},
    {"char": "🇻🇳", "name": "flag Vietnam", "shortcode": ":flag_vn:", "tags": "vietnam flag", "vi": "cờ Việt Nam"},
    {"char": "🎉", "name": "party popper", "shortcode": ":tada:", "tags": "celebration party", "vi": "ăn mừng"},
    {"char": "🔥", "name": "fire", "shortcode": ":fire:", "tags": "hot flame", "vi": "lửa"},
    {"char": "✅", "name": "check mark button", "shortcode": ":white_check_mark:", "tags": "done ok", "vi": "hoàn thành"},
]

def parse_cldr(text):
    entries = []
    for line in text.splitlines():
        if line.startswith("#") or not line.strip():
            continue
        if ";" in line and "#" in line:
            parts = line.split("#")
            codepoints = parts[0].strip().split(";")[0].strip()
            name = parts[1].strip()
            if "fully-qualified" in line or "component" in line:
                continue
            try:
                char = "".join(chr(int(cp, 16)) for cp in codepoints.split())
                entries.append({
                    "char": char,
                    "name": name.lower(),
                    "shortcode": f":{name.lower().replace(' ', '_').replace(',', '')}:",
                    "tags": name.lower(),
                    "vi": ""
                })
            except (ValueError, OverflowError):
                continue
    return entries

def main():
    entries = []
    try:
        print(f"Fetching CLDR emoji data from {CLDR_URL}...")
        with urllib.request.urlopen(CLDR_URL, timeout=30) as resp:
            text = resp.read().decode("utf-8")
        entries = parse_cldr(text)
        print(f"Parsed {len(entries)} emoji from CLDR")
    except Exception as e:
        print(f"Warning: could not fetch CLDR data ({e}), using seed set")
        entries = SEED_EMOJI

    if len(entries) < 100:
        entries = SEED_EMOJI

    with open(OUTPUT, "w", encoding="utf-8") as f:
        json.dump(entries, f, ensure_ascii=False, indent=2)

    print(f"Wrote {len(entries)} entries to {OUTPUT}")

if __name__ == "__main__":
    main()
