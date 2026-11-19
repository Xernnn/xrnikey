#ifndef XRNIKEY_INTERNAL_H
#define XRNIKEY_INTERNAL_H

#include "xrnikey.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

namespace xrnikey {

/* Unicode combining marks */
constexpr char COMB_SAC[]   = "\xCC\x81";  /* U+0301 */
constexpr char COMB_HUYEN[] = "\xCC\x80";  /* U+0300 */
constexpr char COMB_HOI[]   = "\xCC\x89";  /* U+0309 */
constexpr char COMB_NGA[]   = "\xCC\x83";  /* U+0303 */
constexpr char COMB_NANG[]  = "\xCC\xA3";  /* U+0323 */

/* Marks a letter can carry (the tone is separate and belongs to the word). */
enum Mark : uint8_t {
    MARK_NONE = 0,
    MARK_HAT,    /* â ê ô */
    MARK_BREVE,  /* ă */
    MARK_HORN,   /* ơ ư */
    MARK_BAR,    /* đ */
};

/* One letter of the word being typed. */
struct Letter {
    char base;      /* the key's letter, lowercased ('a'..'z', or a VNI digit) */
    bool upper;     /* case of the key that typed it */
    uint8_t mark;   /* Mark */
    int key;        /* index into Word::keys of the key that typed it */
    int mark_key;   /* index of the key that set `mark`, -1 if none */
};

/* What a keystroke did to the word. */
enum KeyKind : uint8_t {
    K_LETTER,  /* added a letter */
    K_MARK,    /* put a mark on letters already typed */
    K_TONE,    /* set the word's tone */
    K_NOOP,    /* changed nothing (the effect was already there) */
};

struct KeyRec {
    uint8_t kind = K_LETTER;
};

/* The word in progress. `keys` is the source of truth: everything else is
 * recomputed from it by word_replay() after every change, so the word can
 * always be shown exactly as typed again. */
struct Word {
    std::string keys;             /* raw keystrokes, as typed */
    std::vector<KeyRec> recs;     /* what each key did */
    std::vector<Letter> letters;  /* the composition */
    int tone = XK_TONE_NONE;

    bool empty() const { return keys.empty(); }
    void clear() { keys.clear(); recs.clear(); letters.clear(); tone = XK_TONE_NONE; }
};

struct Options {
    bool auto_restore = true;
};

struct XKStateImpl {
    int mode;
    Word word;
    std::string committed;  /* instant mode: what is in the document for this word */
    bool enabled;
    bool instant_commit;
    Options opt;

    XKStateImpl(int m) : mode(m), enabled(true), instant_commit(false) {}
};

/* word.cpp */
/* Recompute letters and tone from w.keys. */
void word_replay(Word& w, int mode, const Options& o);
/* Letters as Vietnamese spells them: lowercase, marks applied, no tone. */
std::u32string word_letters(const Word& w);
/* The word as Vietnamese text (marks, tone, case). */
std::string word_render(const Word& w);
/* The keys exactly as typed. */
std::string word_raw(const Word& w);
/* What to show while the word is still being typed. */
std::string word_display(const Word& w, const Options& o);
/* What to commit when the word ends (auto-restore decided here). */
std::string word_final(const Word& w, const Options& o);
/* The composition can still become a Vietnamese syllable. Key handlers make
 * a change, ask this, and put the word back if not. */
bool word_prefix_ok(const Word& w);
void word_add_letter(Word& w, int key_index);

/* telex.cpp / vni.cpp: apply key `i` of w.keys to the composition so far. */
void telex_apply_key(Word& w, int i, const Options& o);
void vni_apply_key(Word& w, int i, const Options& o);

/* tone_placement.cpp */
std::string apply_tone_to_syllable(const std::string& base, int tone);
int find_tone_vowel_index(const std::string& syllable);
bool is_vietnamese_vowel_glyph(const std::string& s, size_t pos, int* out_len);
std::string nfc_compose(const std::string& nfd);
std::string nfd_decompose_char(uint32_t cp);
int utf8_decode(const char* s, uint32_t* cp, int* len);
int utf8_encode(uint32_t cp, char* out);

/* Diff old_str (previously committed) against new_str (current display) by
 * codepoint and fill r->delete_before / r->commit with the minimal
 * delete-then-insert to bring committed text in sync. Does not touch
 * r->preedit. */
void diff_commit(const std::string& old_str, const std::string& new_str, XKResult* r);

/* syllable_validator.cpp
 * `letters` is a syllable as Vietnamese spells it, lowercase, with its marks
 * (â ă ê ô ơ ư đ) and without the tone. */
bool syllable_valid(const std::u32string& letters, int tone);
/* True while more keys could still turn `letters` into a valid syllable:
 * more letters after it, marks on the letters it has, a different tone. */
bool syllable_prefix_ok(const std::u32string& letters, int tone);
/* Split any UTF-8 syllable ("Hoà", "hòa", "thuở") into lowercase letters and
 * its tone. False if it is not made of Vietnamese letters. */
bool syllable_split(const std::string& utf8, std::u32string* letters, int* tone);
bool is_valid_vn_syllable(const std::string& word);
std::string utf8_of(const std::u32string& s);

/* lexicon.cpp */
/* `word` (ASCII, any case) is in the list of common English words. */
bool lexicon_is_english(const std::string& word);
/* The syllable is in the list of attested Vietnamese syllables. */
bool lexicon_is_attested(const std::u32string& letters, int tone);

/* Utility */
bool is_vowel_char(char c);
bool has_vietnamese_vowel_pattern(const std::string& s);
uint32_t keysym_to_char(uint32_t keysym);

} /* namespace xrnikey */

#endif /* XRNIKEY_INTERNAL_H */
