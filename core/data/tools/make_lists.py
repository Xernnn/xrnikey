"""Build core/data/english.txt and core/data/vi-syllables.txt.

usage: make_lists.py OUTDIR LIBXRNIKEY SCOWL_FINAL_DIR HAND_ENGLISH UNIHAN_READINGS CV_TXT WS_TXT SEED_WORDLIST
Syllables are kept only if the engine's own validator accepts them.
"""
import ctypes, re, sys, unicodedata
out, lib, scowl, hand, unihan, cv, ws, seed = sys.argv[1:9]
L = ctypes.CDLL(lib)
L.xk_is_valid_syllable.argtypes = [ctypes.c_char_p]

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
TOK = re.compile(f"[{LETTERS}]+")

def tokens(text):
    return TOK.findall(unicodedata.normalize("NFC", text).lower())

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

kept = sorted(w for w in sources if L.xk_is_valid_syllable(w.encode()))
with open(f"{out}/vi-syllables.txt", "w", encoding="utf-8") as f:
    f.write("# Attested Vietnamese syllables, one per line (see README.md).\n")
    for w in kept:
        f.write(w + "\n")
HAND = "o ó ò í è ẻ ể ỉ ị ố ợ ù ỳ hử ừm ờm nghen ối úi ới".split()
hand = [w for w in HAND if w not in kept and L.xk_is_valid_syllable(w.encode())]
with open(f"{out}/vi-syllables.txt", "a", encoding="utf-8") as f:
    f.write("# Added by hand: interjections and spoken forms that written sources miss.\n")
    for w in hand:
        f.write(w + "\n")
print("hand", hand)
per = {s: sum(1 for w in kept if s in sources[w]) for s in ["unihan", "cv", "ws", "seed"]}
print("english", len(en), "(scowl", scowl_n, ")", "vi", len(kept), "of", len(sources), per)
