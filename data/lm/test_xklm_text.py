"""Tests for the tokenizer the language models are trained on.

Run with the built engine: XRNIKEY_LIB=build/core/libxrnikey.so python3 -I data/lm/test_xklm_text.py
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402
import xklm_text  # noqa: E402

ENGINE = xklm_text.Engine(os.environ["XRNIKEY_LIB"])


def toks(text):
    return xklm_text.sentences(ENGINE, text)


class Tokens(unittest.TestCase):
    def test_syllables_take_the_engines_tone_placement(self):
        # Sources write new-style tones and misplaced ones; the model sees one spelling.
        self.assertEqual(toks("Hoà bình, thuý"), [["hòa", "bình", "<p>", "thúy"]])
        self.assertEqual(toks("hòa"), toks("hoà"))

    def test_capitals_are_lowered_and_old_eth_is_d_bar(self):
        self.assertEqual(toks("ĐƯỢC Không"), [["được", "không"]])
        self.assertEqual(toks("Ðã"), [["đã"]])

    def test_english_and_numbers(self):
        self.assertEqual(toks("Gửi file PDF lúc 8:30 nhé"),
                         [["gửi", "file", "pdf", "lúc", "<num>", "<p>", "<num>", "nhé"]])
        self.assertEqual(toks("Giá 1.200.000 đồng"), [["giá", "<num>", "đồng"]])

    def test_sentences_end_at_full_stops_and_pauses_do_not_trail(self):
        self.assertEqual(toks("Tôi đi. Bạn ở, nhé!"), [["tôi", "đi"], ["bạn", "ở", "<p>", "nhé"]])
        self.assertEqual(toks("Vâng, "), [["vâng"]])

    def test_letters_outside_vietnamese_are_unknown(self):
        self.assertEqual(toks("café Müller"), [["<unk>", "<unk>"]])
        self.assertEqual(toks("nguời"), [["<unk>"]])  # not a syllable, not a-z

    def test_quotes_brackets_and_dashes_are_dropped(self):
        self.assertEqual(toks('"Sê-ri" (mới) – ok'), [["sê", "ri", "mới", "ok"]])


class Typing(unittest.TestCase):
    def test_marks_strip_to_what_is_typed_without_accents(self):
        self.assertEqual(xklm_text.strip_marks("được"), "duoc")
        self.assertEqual(xklm_text.strip_marks("Đường"), "Duong")

    def test_telex_keys_type_the_syllable(self):
        self.assertEqual(xklm_text.telex_keys("được"), "dduwowcj")
        self.assertEqual(xklm_text.telex_keys("không"), "khoong")
        self.assertEqual(xklm_text.telex_keys("ba"), "ba")


class HeldOut(unittest.TestCase):
    def test_the_split_depends_on_the_tokens_alone(self):
        a = build.bucket(["tôi", "đi", "học"])
        self.assertEqual(a, build.bucket(["tôi", "đi", "học"]))
        self.assertNotEqual(a, build.bucket(["tôi", "đi", "làm"]))
        self.assertTrue(0 <= a[0] < 1000)

    def test_eval_sentences_are_mid_length_and_fully_known(self):
        self.assertTrue(build.eligible("tôi đi học về".split()))
        self.assertFalse(build.eligible("tôi đi".split()))
        self.assertFalse(build.eligible("tôi gặp <unk> hôm qua".split()))


if __name__ == "__main__":
    unittest.main()
