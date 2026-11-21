#ifndef XRNKEY_INTERNAL_H
#define XRNKEY_INTERNAL_H

#include "xrnkey.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

namespace xrnkey {

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
    bool soft;      /* horn put on the u of "uo" by one w: shown or not by
                       the spelling rule in word_letters() */
};

/* What a keystroke did to the word. */
enum KeyKind : uint8_t {
    K_LETTER,  /* added a letter */
    K_MARK,    /* put a mark on letters already typed */
    K_TONE,    /* set the word's tone */
};

struct KeyRec {
    uint8_t kind = K_LETTER;
    /* K_MARK: the letters it marked, by the index of the key that typed them. */
    int target = -1;
    int target2 = -1;
    /* Typing a key twice undoes it: the second key is then a letter that
     * `undoes` the first, and the first is `undone`. */
    int undoes = -1;
    bool undone = false;
};

/* The word in progress. `keys` is the source of truth: everything else is
 * recomputed from it by word_replay() after every change, so the word can
 * always be shown exactly as typed again. */
struct Word {
    std::string keys;             /* raw keystrokes, as typed */
    std::vector<KeyRec> recs;     /* what each key did */
    std::vector<Letter> letters;  /* the composition */
    int tone = XK_TONE_NONE;
    int tone_key = -1;            /* the key that set `tone` */
    uint64_t locked = 0;          /* keys (bit per a..z, 0..9) undone this word:
                                     later presses of them are just letters */
    /* Set by BackSpace in a word shown as typed: from then on every key is
     * just its letter, so editing "text" gives "tex", not "tẽ". */
    bool literal = false;

    bool empty() const { return keys.empty(); }
    void clear() {
        keys.clear(); recs.clear(); letters.clear();
        tone = XK_TONE_NONE; tone_key = -1; locked = 0; literal = false;
    }
};

struct Options {
    bool auto_restore = true;
    bool modern_style = false;
    bool free_marking = true;
    bool w_shortcut = true;
    bool learn = false;
};

struct Lm;
struct Models;

/* Which language a finished word is in, as far as choosing the next one
 * goes. Numbers and pauses have none. */
enum Lang : uint8_t { LANG_NONE = 0, LANG_VI, LANG_EN };

/* The words before the next one, as model tokens (bilingual.cpp), so the
 * choice between a word's English and Vietnamese readings can use them. */
struct Context {
    std::vector<std::string> tokens;  /* oldest first, at most kKeep */
    std::vector<uint8_t> langs;       /* Lang of each token */
    bool start = true;                /* tokens follow the start of a sentence */
    static constexpr size_t kKeep = 3;
    void clear() { tokens.clear(); langs.clear(); start = true; }
    void push(std::string token, uint8_t lang) {
        tokens.push_back(std::move(token));
        langs.push_back(lang);
        if (tokens.size() > kKeep) {
            tokens.erase(tokens.begin());
            langs.erase(langs.begin());
            start = false;
        }
    }
};

struct XKStateImpl {
    int mode;
    Word word;
    std::string committed;  /* instant mode: what is in the document for this word */
    bool enabled;
    bool instant_commit;
    Options opt;
    /* Language models for the English/Vietnamese choice; not owned. */
    const Models* models = nullptr;
    Context ctx;

    XKStateImpl(int m) : mode(m), enabled(true), instant_commit(false) {}
};

/* word.cpp */
/* Recompute letters and tone from w.keys. */
void word_replay(Word& w, int mode, const Options& o);
/* Letters as Vietnamese spells them: lowercase, marks applied, no tone. */
std::u32string word_letters(const Word& w);
/* The word as Vietnamese text (marks, tone, case). */
std::string word_render(const Word& w, const Options& o);
/* The keys exactly as typed. */
std::string word_raw(const Word& w);
/* The keys as typed, less those a repeated key undid ("ass" -> "as"). */
std::string word_raw_effective(const Word& w);
/* Bit for `locked`: a..z, then 0..9. */
uint64_t key_bit(char lc);
/* Undo the effect of key `undone` by key `i`, which becomes a letter. */
void word_undo(Word& w, int i, int undone);
/* What to show while the word is still being typed. */
std::string word_display(const Word& w, const Options& o);
/* What to commit when the word ends (auto-restore decided here). */
/* `s`, when given and holding both language models, decides between the
 * readings of keys that are both an attested syllable and an English word
 * (bilingual.cpp); without it the syllable always wins. */
std::string word_final(const Word& w, const Options& o, const XKStateImpl* s = nullptr);
/* BackSpace: delete the last character shown and recompose from the keys
 * that remain. */
void word_backspace(Word& w, int mode, const Options& o);
/* The composition can still become a Vietnamese syllable. Key handlers make
 * a change, ask this, and put the word back if not. */
bool word_prefix_ok(const Word& w);
void word_add_letter(Word& w, int key_index);

/* telex.cpp / vni.cpp: apply key `i` of w.keys to the composition so far. */
void telex_apply_key(Word& w, int i, const Options& o);
void vni_apply_key(Word& w, int i, const Options& o);

/* tone_placement.cpp */
std::string apply_tone_to_syllable(const std::string& base, int tone);
int find_tone_vowel_index(const std::string& syllable, bool modern = false);
bool is_vietnamese_vowel_glyph(const std::string& s, size_t pos, int* out_len);
std::string nfc_compose(const std::string& nfd);
/* `vowel` (lowercase, marked or not) carrying `tone`. */
char32_t vn_toned(char32_t vowel, int tone);
char32_t vn_upper(char32_t c);
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
/* Every attested syllable, as the list writes it (NFC, the engine's tone
 * placement). */
const std::vector<std::string>& lexicon_attested_words();

/* bilingual.cpp */
/* `text` as a model token: a syllable lowercase with the engine's tone
 * placement, a-z words lowercase, digits "<num>", , ; : "<p>", else "<unk>". */
std::string context_token(const std::string& text);
/* With learning on and personal words in the models, learn a finished
 * word as following the context's last one. */
void context_learn_word(XKStateImpl& s, const std::string& text);
/* Remember a finished word, or a number or pause, as context. */
void context_note_word(XKStateImpl& s, const std::string& text);
void context_note_char(XKStateImpl& s, char c);
/* Both models are there to choose with. */
bool bilingual_ready(const XKStateImpl& s);
/* The English reading `en` is likelier than the Vietnamese `vi` here. */
bool bilingual_prefers_english(const XKStateImpl& s, const std::string& vi, const std::string& en);

/* Utility */
bool is_vowel_char(char c);
bool has_vietnamese_vowel_pattern(const std::string& s);
uint32_t keysym_to_char(uint32_t keysym);

} /* namespace xrnkey */

#endif /* XRNKEY_INTERNAL_H */
