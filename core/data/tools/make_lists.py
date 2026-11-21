"""Build core/data/english.txt and core/data/vi-syllables.txt.

usage: make_lists.py OUTDIR LIBXRNKEY SCOWL_FINAL_DIR HAND_ENGLISH UNIHAN_READINGS CV_TXT WS_TXT SEED_WORDLIST TATOEBA_TSV
Syllables are kept only if the engine's own validator accepts them, and are
written with the tone where the engine puts it.
"""
import ctypes, re, sys, unicodedata
out, lib, scowl, hand, unihan, cv, ws, seed, tatoeba = sys.argv[1:10]
L = ctypes.CDLL(lib)
L.xk_is_valid_syllable.argtypes = [ctypes.c_char_p]
L.xk_place_tone.argtypes = [ctypes.c_char_p, ctypes.c_int]
L.xk_place_tone.restype = ctypes.c_void_p
libc = ctypes.CDLL(None)
libc.free.argtypes = [ctypes.c_void_p]

TONE_MARKS = {"\u0301": 1, "\u0300": 2, "\u0309": 3, "\u0303": 4, "\u0323": 5}

def engine_placed(w):
    """`w` toned the way the engine writes it (the old style): the sources put
    the tone where they like ("gìa", "qúy", "tươỉ", "hoà"), and one spelling
    per syllable is enough, since lookups ignore where the tone sits."""
    d = unicodedata.normalize("NFD", w)
    tone = 0
    for mark, t in TONE_MARKS.items():
        if mark in d:
            tone, d = t, d.replace(mark, "")
    p = L.xk_place_tone(unicodedata.normalize("NFC", d).encode(), tone)
    placed = ctypes.string_at(p).decode()
    libc.free(p)
    return placed

en = set()
for name in ["english-words.10", "english-words.20", "american-words.10", "american-words.20"]:
    for line in open(f"{scowl}/{name}", encoding="latin-1"):
        w = line.strip()
        if re.fullmatch(r"[a-z]+", w):
            en.add(w)
scowl_n = len(en)
for line in open(hand, encoding="utf-8"):
    if re.fullmatch(r"[a-z]+", line.strip()):
        en.add(line.strip())
with open(f"{out}/english.txt", "w", encoding="utf-8") as f:
    f.write("# Common English words, one per line (see README.md).\n")
    for w in sorted(en):
        f.write(w + "\n")

LETTERS = "a-zàáảãạăằắẳẵặâầấẩẫậđèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵ"
LETTERS += LETTERS.upper()
WORD = re.compile(f"[{LETTERS}]+(?:-[{LETTERS}]+)*")

def tokens(text, hyphenated=True):
    """The lowercase syllables of `text`. Old texts write Đ as Ð (U+00D0),
    which would split "Ðã" into a stray "ã". hyphenated=False leaves out the
    syllables of hyphenated words, which modern text uses for foreign words
    spelt out ("pót-cát", "Sê-lê-na")."""
    text = unicodedata.normalize("NFC", text.replace("\u00d0", "\u0110").replace("\u00f0", "\u0111"))
    out = set()
    for m in WORD.finditer(text):
        if hyphenated or "-" not in m.group(0):
            out.update(s.lower() for s in m.group(0).split("-"))
    return out

sources = {}
def add(src, words):
    for w in words:
        sources.setdefault(w, set()).add(src)

uh = []
for line in open(unihan, encoding="utf-8"):
    if "\tkVietnamese\t" in line:
        uh += line.rstrip("\n").split("\t")[2].split()
add("unihan", tokens(" ".join(uh)))
add("cv", tokens(open(cv, encoding="utf-8").read()))
add("ws", tokens(open(ws, encoding="utf-8").read()))
seedw = []
for line in open(seed, encoding="utf-8"):
    if line.strip() and not line.startswith("#"):
        seedw += line.split("\t")[0].split()
add("seed", tokens(" ".join(seedw)))
# Tatoeba's per-language export: id <TAB> lang <TAB> sentence.
add("tatoeba", tokens(" ".join(line.rstrip("\n").split("\t", 2)[-1]
                               for line in open(tatoeba, encoding="utf-8")),
                      hyphenated=False))

kept = sorted({engine_placed(w) for w in sources if L.xk_is_valid_syllable(w.encode())})
with open(f"{out}/vi-syllables.txt", "w", encoding="utf-8") as f:
    f.write("# Attested Vietnamese syllables, one per line (see README.md).\n")
    for w in kept:
        f.write(w + "\n")
# Interjections and spoken forms, then everyday words and loanwords, that the
# written sources miss (README.md says which and why).
HAND = ("o ó ò í è ẻ ể ỉ ị ố ợ ù ỳ hử ừm ờm nghen ối úi ới "
        "cỏn háng líp phốt rún sít tủn vế").split()
hand = [w for w in map(engine_placed, HAND) if w not in kept and L.xk_is_valid_syllable(w.encode())]
with open(f"{out}/vi-syllables.txt", "a", encoding="utf-8") as f:
    f.write("# Added by hand: interjections, spoken forms and everyday words that written sources miss.\n")
    for w in hand:
        f.write(w + "\n")
print("hand", hand)
per = {s: len({engine_placed(w) for w in sources if s in sources[w] and L.xk_is_valid_syllable(w.encode())})
       for s in ["unihan", "cv", "ws", "seed", "tatoeba"]}
print("english", len(en), "(scowl", scowl_n, ")", "vi", len(kept), "of", len(sources), per)
