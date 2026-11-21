#!/usr/bin/env python3
"""Build data/emoji/emoji-index.tsv from Unicode's emoji list and CLDR's names.

usage: generate.py fetch DOWNLOADS     download the sources and check their SHA-256
       generate.py DOWNLOADS [OUT]      write the index (default: emoji-index.tsv here)

DOWNLOADS holds the files in data/emoji/sources.json, each in a folder named
after its source.

One line per emoji, in Unicode's order (emoji-test.txt), fully-qualified
only, skin-tone variants folded into their base:

  emoji <TAB> group <TAB> skin <TAB> en name <TAB> en keywords <TAB> vi name <TAB> vi keywords

`skin` is 1 when the emoji takes the five skin tones; keywords are joined
with "|". The names and keywords are CLDR's annotations and derived
annotations (flags, keycaps, people combined), in English and Vietnamese.

Run it with python3 -I: it reads downloaded files.
"""

import hashlib
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
SKIN = {"\U0001F3FB", "\U0001F3FC", "\U0001F3FD", "\U0001F3FE", "\U0001F3FF"}


def sources():
    with open(os.path.join(HERE, "sources.json"), encoding="utf-8") as f:
        return {s["name"]: s for s in json.load(f)["sources"]}


def path_of(downloads, src):
    return os.path.join(downloads, src["name"], os.path.basename(src["url"]))


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def fetch(downloads):
    for src in sources().values():
        path = path_of(downloads, src)
        if not os.path.exists(path):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with urllib.request.urlopen(src["url"], timeout=60) as r, open(path + ".part", "wb") as out:
                out.write(r.read())
            os.replace(path + ".part", path)
        if sha256(path) != src["sha256"]:
            sys.exit(f"{path}: not the file sources.json pins")
        print("ok", src["name"])


def emoji_list(path):
    """(emoji, group) for each fully-qualified emoji, and the set of bases
    that take skin tones."""
    group = None
    out, skin = [], set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("# group:"):
                group = line.split(":", 1)[1].strip()
                continue
            if not line.strip() or line.startswith("#"):
                continue
            data = line.partition("#")[0]
            codes, _, status = data.partition(";")
            if status.strip() != "fully-qualified" or group == "Component":
                continue
            emoji = "".join(chr(int(c, 16)) for c in codes.split())
            if any(c in SKIN for c in emoji):
                skin.add("".join(c for c in emoji if c not in SKIN))
                continue
            out.append((emoji, group))
    return out, skin


def annotations(*paths):
    """emoji -> (name, keywords) from CLDR annotation files (later files
    fill what earlier ones lack)."""
    names = {}
    for path in paths:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        table = data.get("annotations", data.get("annotationsDerived", {}))
        table = table.get("annotations", {})
        for emoji, a in table.items():
            if emoji in names:
                continue
            tts = (a.get("tts") or [""])[0]
            names[emoji] = (tts, a.get("default") or [])
    return names


def look(names, emoji):
    return names.get(emoji) or names.get(emoji.replace("️", "")) or ("", [])


def clean(text):
    return " ".join(text.replace("\t", " ").replace("|", "/").split())


def build(downloads, out):
    src = sources()
    for s in src.values():
        if sha256(path_of(downloads, s)) != s["sha256"]:
            sys.exit(f"{s['name']}: not the file sources.json pins (run fetch)")
    entries, skin = emoji_list(path_of(downloads, src["emoji-test-17.0"]))
    en = annotations(path_of(downloads, src["cldr48-annotations-en"]), path_of(downloads, src["cldr48-derived-en"]))
    vi = annotations(path_of(downloads, src["cldr48-annotations-vi"]), path_of(downloads, src["cldr48-derived-vi"]))
    n = 0
    with open(out + ".part", "w", encoding="utf-8") as f:
        f.write("# xrnkey emoji index: Unicode emoji-test.txt 17.0 and CLDR 48 annotations "
                "(Unicode License v3; data/emoji/README.md). Built by generate.py.\n")
        for emoji, group in entries:
            en_name, en_kw = look(en, emoji)
            vi_name, vi_kw = look(vi, emoji)
            if not en_name:
                continue
            row = [emoji, group, "1" if emoji in skin else "0", clean(en_name),
                   "|".join(clean(k) for k in en_kw), clean(vi_name), "|".join(clean(k) for k in vi_kw)]
            f.write("\t".join(row) + "\n")
            n += 1
    os.replace(out + ".part", out)
    print(f"{n} emoji -> {out}")


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "fetch":
        fetch(sys.argv[2])
    elif len(sys.argv) in (2, 3) and sys.argv[1] != "fetch":
        build(sys.argv[1], sys.argv[2] if len(sys.argv) == 3 else os.path.join(HERE, "emoji-index.tsv"))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
