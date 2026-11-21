"""Text handling shared by data/lm/build.py and tools/xk-eval.

Turns raw sentences into the tokens the language models are trained on, with
the engine itself deciding what a Vietnamese syllable is and where its tone
goes, so the model, the engine and the evaluation agree on every spelling:

  - a Vietnamese syllable: lowercase NFC, the tone placed the engine's way
    ("Hoà" -> "hòa");
  - a word of the letters a-z that is not a syllable: lowercase, as is
    ("Facebook" -> "facebook");
  - a number: "<num>";
  - , ; : -> "<p>";
  - . ! ? and ... end the sentence;
  - any other word (letters outside Vietnamese and a-z) -> "<unk>";
  - anything else (quotes, brackets, dashes, symbols) is dropped.
"""

import ctypes
import re
import unicodedata

TONE_MARKS = {"́": 1, "̀": 2, "̉": 3, "̃": 4, "̣": 5}
VI_LETTERS = set(
    "abcdefghijklmnopqrstuvwxyz"
    "àáảãạăằắẳẵặâầấẩẫậđèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵ"
)
TOKEN = re.compile(r"(\d+(?:[.,]\d+)*)|([^\W\d_]+)|([,;:])|([.!?…]+)")


class Engine:
    """The parts of libxrnikey the text tools use, through ctypes."""

    class Result(ctypes.Structure):
        _fields_ = [("commit", ctypes.c_char * 64), ("preedit", ctypes.c_char * 64),
                    ("consumed", ctypes.c_int), ("reset", ctypes.c_int),
                    ("delete_before", ctypes.c_int)]

    def __init__(self, lib_path):
        L = ctypes.CDLL(lib_path)
        L.xk_is_valid_syllable.argtypes = [ctypes.c_char_p]
        L.xk_place_tone.argtypes = [ctypes.c_char_p, ctypes.c_int]
        L.xk_place_tone.restype = ctypes.c_void_p
        L.xk_state_new.restype = ctypes.c_void_p
        L.xk_state_new.argtypes = [ctypes.c_int]
        L.xk_state_free.argtypes = [ctypes.c_void_p]
        L.xk_state_reset.argtypes = [ctypes.c_void_p]
        L.xk_process_key.restype = Engine.Result
        L.xk_process_key.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
        L.xk_state_flush.restype = Engine.Result
        L.xk_state_flush.argtypes = [ctypes.c_void_p]
        self.lib = L
        self.libc = ctypes.CDLL(None)
        self.libc.free.argtypes = [ctypes.c_void_p]
        self._norm = {}

    def place_tone(self, base, tone):
        p = self.lib.xk_place_tone(base.encode(), tone)
        out = ctypes.string_at(p).decode()
        self.libc.free(p)
        return out

    def valid(self, syllable):
        return bool(self.lib.xk_is_valid_syllable(syllable.encode()))

    def syllable(self, word):
        """`word` (lowercase NFC) as the engine writes that syllable, or None
        when it is not one."""
        hit = self._norm.get(word, False)
        if hit is not False:
            return hit
        d = unicodedata.normalize("NFD", word)
        tone = 0
        for mark, t in TONE_MARKS.items():
            if mark in d:
                tone, d = t, d.replace(mark, "")
        base = unicodedata.normalize("NFC", d)
        out = None
        if all(c in VI_LETTERS for c in base):
            placed = self.place_tone(base, tone)
            if self.valid(placed):
                out = placed
        if len(self._norm) < 2_000_000:
            self._norm[word] = out
        return out


def word_token(engine, word):
    """The token for one run of letters."""
    low = word.lower()
    syl = engine.syllable(low)
    if syl is not None:
        return syl
    if low.isascii() and low.isalpha():
        return low
    return "<unk>"


def sentences(engine, text):
    """`text` as lists of tokens, one list per sentence."""
    text = unicodedata.normalize("NFC", text.replace("Ð", "Đ").replace("ð", "đ"))
    out, cur = [], []
    for m in TOKEN.finditer(text):
        num, word, pause, end = m.groups()
        if num:
            cur.append("<num>")
        elif word:
            cur.append(word_token(engine, word))
        elif pause:
            if cur and cur[-1] != "<p>":
                cur.append("<p>")
        elif end:
            while cur and cur[-1] == "<p>":
                cur.pop()
            if cur:
                out.append(cur)
            cur = []
    while cur and cur[-1] == "<p>":
        cur.pop()
    if cur:
        out.append(cur)
    return out


def strip_marks(token):
    """A syllable with every mark and tone removed: what is typed without
    accents ("được" -> "duoc")."""
    d = unicodedata.normalize("NFD", token.replace("đ", "d").replace("Đ", "D"))
    return "".join(c for c in d if unicodedata.category(c) != "Mn")


TELEX_LETTERS = {"â": "aa", "ă": "aw", "ê": "ee", "ô": "oo", "ơ": "ow", "ư": "uw", "đ": "dd"}
TELEX_TONES = {0: "", 1: "s", 2: "f", 3: "r", 4: "x", 5: "j"}


def telex_keys(syllable):
    """The Telex keys for a syllable: its letters with their marks, then the
    tone key ("được" -> "dduwowcj")."""
    d = unicodedata.normalize("NFD", syllable)
    tone = 0
    for mark, t in TONE_MARKS.items():
        if mark in d:
            tone, d = t, d.replace(mark, "")
    base = unicodedata.normalize("NFC", d)
    return "".join(TELEX_LETTERS.get(c, c) for c in base) + TELEX_TONES[tone]
