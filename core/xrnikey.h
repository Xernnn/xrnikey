#ifndef XRNIKEY_H
#define XRNIKEY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XKState XKState;

/* commit/delete_before drive instant (Unikey-style) commit: the caller should
 * delete `delete_before` codepoints immediately before the cursor (text this
 * engine previously committed for the syllable in progress), then insert
 * `commit`. delete_before is 0 for a plain append. preedit mirrors the full
 * current syllable for frontends that use a classic composition buffer
 * instead (e.g. no surrounding-text support). */
typedef struct XKResult {
    char commit[64];
    char preedit[64];
    int  consumed;
    int  reset;
    int  delete_before;
} XKResult;

typedef struct XKDict XKDict;
typedef struct XKNgram XKNgram;
typedef struct XKEmojiDB XKEmojiDB;

/* Input mode */
#define XK_MODE_TELEX 0
#define XK_MODE_VNI   1

/* Tone indices (internal) */
#define XK_TONE_NONE   0
#define XK_TONE_SAC    1  /* sắc  ´ */
#define XK_TONE_HUYEN  2  /* huyền ` */
#define XK_TONE_HOI    3  /* hỏi  ̉ */
#define XK_TONE_NGA    4  /* ngã  ~ */
#define XK_TONE_NANG   5  /* nặng . */

/* State lifecycle */
XKState* xk_state_new(int mode);
void     xk_state_free(XKState* state);
void     xk_state_reset(XKState* state);
char*    xk_state_get_preedit(XKState* state);
/* The keys of the word in progress as typed ("aa" while it shows "â"), less
 * those BackSpace took away: deleting a character takes the keys that typed,
 * marked and toned it. Newly allocated; the caller frees it. */
char*    xk_state_get_keys(XKState* state);

/* Opt in to Unikey-style instant commit: every keystroke fills
 * commit/delete_before to correct previously committed text in place,
 * instead of only committing the finalized word on Space. Off by default —
 * a frontend that only understands the classic commit-on-Space contract
 * (e.g. relies on commit staying empty until the word is finished) must not
 * enable this. */
void xk_state_set_instant_commit(XKState* state, int enabled);

/* Typing options, per state. Unknown options are ignored (get returns -1). */
#define XK_OPT_AUTO_RESTORE 1  /* default 1: a word that is not Vietnamese
                                  (or is a common English word that is not an
                                  attested syllable) is committed as typed */
#define XK_OPT_MODERN_STYLE 2  /* default 0: hòa, khỏe, thúy; 1: hoà, khoẻ, thuý */
#define XK_OPT_FREE_MARKING 3  /* default 1: tone and mark keys may follow the
                                  final consonant (toans, dangd, tienge); 0: a
                                  tone before it, a mark right after its letter */
#define XK_OPT_W_SHORTCUT   4  /* default 1: Telex w with no vowel to mark is
                                  ư (w -> ư, tw -> tư); ww -> w */
#define XK_OPT_LEARN        5  /* default 0: learn each word this state commits
                                  into the personal words of its models (off
                                  in password fields and the like) */
void xk_state_set_option(XKState* state, int option, int value);
int  xk_state_get_option(XKState* state, int option);

/* Key processing — keysym is XKB keysym, modifiers bitmask.
 *
 * Apply every result, consumed or not: delete `delete_before` codepoints
 * before the cursor, insert `commit`, and if `consumed` is 0 let the key
 * through to the application. Keys that end a word but are not text --
 * Return, Tab, Escape, arrows, Home/End, PageUp/PageDown, Delete, Ctrl/Alt
 * chords -- finish the word (preedit mode: `commit` holds it) and come back
 * with consumed = 0; they are never swallowed. A modifier key on its own
 * changes nothing. BackSpace with no word in progress is not consumed. */
XKResult xk_process_key(XKState* state, uint32_t keysym, uint32_t modifiers);

/* Finish the word in progress as a word boundary would (auto-restore
 * applied) and empty the state, for focus-out, reset and the like. The
 * result is applied like xk_process_key's; consumed is 0. */
XKResult xk_state_flush(XKState* state);

/* Tone placement — returns newly allocated NFC string (caller frees) */
char* xk_place_tone(const char* syllable, int tone);

/* Syllable validation */
int xk_is_valid_syllable(const char* word);

/* Dictionary */
XKDict* xk_dict_load(const char* path);
void    xk_dict_free(XKDict* dict);
int     xk_dict_complete(XKDict* dict, const char* prefix, char** results, int max_results);
int     xk_dict_lookup(XKDict* dict, const char* word);
void    xk_dict_add_word(XKDict* dict, const char* word, int freq);
void    xk_dict_reload_user(XKDict* dict, const char* user_path);

/* Language model (.xklm, built by tools/lm/xk-lm-build; data/lm/README.md).
 * A backoff n-gram model, memory-mapped and read in place. Tokens are what
 * data/lm/build.py writes: lowercase NFC Vietnamese syllables with the tone
 * where this engine puts it, lowercase English words, "<num>" for a number,
 * "<p>" for , ; and :, and "<s>" for the start of a sentence. */
typedef struct XKLm XKLm;
/* NULL when the file is missing or not a model this version reads. */
XKLm*       xk_lm_open(const char* path);
/* A model stored at `offset` in an open file (inside an APK); the
 * descriptor may be closed afterwards. */
XKLm*       xk_lm_open_fd(int fd, int64_t offset, int64_t length);
void        xk_lm_free(XKLm* lm);
int         xk_lm_order(const XKLm* lm);
int         xk_lm_vocab_size(const XKLm* lm);
/* The word with id 0..vocab-1 (0 is "<unk>"), owned by the model. */
const char* xk_lm_word(const XKLm* lm, int id);
/* The id of `word`, 0 when the model does not know it. */
int         xk_lm_id(const XKLm* lm, const char* word);
/* The models that score words in context, for accent restoration and the
 * English/Vietnamese choice. Either model may be NULL. The bundle does not
 * own them; free it before them. */
typedef struct XKModels XKModels;
XKModels* xk_models_new(const XKLm* vi, const XKLm* en);
void      xk_models_free(XKModels* models);

/* Personal words: what the owner types, learned on the device only. Counts
 * of words and of word pairs, never sentences; a word or pair counts once
 * typed twice. With them in the models, scoring leans towards what the
 * owner writes ("ngắn tủn" after they typed it twice). The file holds
 * "word<TAB>count" and "word word<TAB>count" lines and is saved atomically.
 * Words are given as text and stored as model tokens (lowercase). */
typedef struct XKUser XKUser;
XKUser* xk_user_new(void);
void    xk_user_free(XKUser* user);
/* Replaces what `user` holds; returns the entries read, -1 if unreadable.
 * Bad lines are skipped. */
int     xk_user_load(XKUser* user, const char* path);
/* 0 on success; the old file stays as it was otherwise. */
int     xk_user_save(const XKUser* user, const char* path);
/* `word` was committed after `prev` (NULL or "<s>" at the start of a
 * sentence, "<p>" after a comma). */
void    xk_user_learn(XKUser* user, const char* prev, const char* word);
/* How often `word` was learned (after `prev`, when not NULL). */
double  xk_user_count(const XKUser* user, const char* prev, const char* word);
/* Remove `word` and every pair it is in. */
void    xk_user_forget(XKUser* user, const char* word);
/* Multiply every count by `factor` (0..1), dropping what falls below 0.5:
 * old habits fade. */
void    xk_user_decay(XKUser* user, double factor);
/* Score with these personal words (NULL: none). The bundle does not own
 * them. */
void    xk_models_set_user(XKModels* models, XKUser* user);

/* Typo correction in accent restoration. A word may also stand for one a
 * key away on the QWERTY layout ("khonh" -> "không"), or two keys away from
 * 5 letters (4 when STRONG); each slip costs what the layout says it is
 * unlikely to be, so a correction wins only when the words around make it
 * worth it. Words of one letter, and chat abbreviations ("ko", "dc", "oke"),
 * are never corrected. Off by default. */
#define XK_CORRECT_OFF      0
#define XK_CORRECT_FLEXIBLE 1
#define XK_CORRECT_STRONG   2
void    xk_models_set_correction(XKModels* models, int level);

/* log10 P(word | context), the context oldest first ("<s>" may start it).
 * A word the model does not know gets its share of the unknown words' mass
 * (all unknown words score alike); "<unk>" itself gets all of it. */
float       xk_lm_logprob(const XKLm* lm, const char* const* context, int ctx_len,
                          const char* word);

/* N-gram language model: the old ARPA reader, still used by the Fcitx5
 * addon's suggestions. */
XKNgram* xk_ngram_load(const char* arpa_path);
void     xk_ngram_free(XKNgram* ng);
float    xk_ngram_score(XKNgram* ng, const char** context, int ctx_len, const char* word);

/* Autocorrect */
typedef struct XKCandidate {
    char  text[128];
    float score;
    int   edit_distance;
    int   type; /* 0=dict 1=correction 2=prediction, XK_CAND_* */
} XKCandidate;

int xk_autocorrect(XKDict* dict, XKNgram* ng, const char* word,
                   const char** context, int ctx_len,
                   XKCandidate* out, int max_out);

/* Accent restoration (needs the Vietnamese model in `models`; with the
 * English one too, a word typed as an English word is weighed as English,
 * as in the English/Vietnamese choice below). */
#define XK_CAND_RESTORED  3  /* XKCandidate.type: an accented reading of the typed word */
#define XK_CAND_AS_TYPED  4  /* XKCandidate.type: the word exactly as typed */
#define XK_CAND_CORRECTED 1  /* XKCandidate.type: a key or two away from what was typed
                                (edit_distance says how many) */
#define XK_RESTORE_MAX_RUN 8
/* The words `typed` (a-z and A-Z only, no marks: "khong") may stand for,
 * best first, scored log10 P(word | context): the Vietnamese syllables of
 * its shape, and the word as typed when the model knows it as a token
 * ("file") or nothing else fits ("ko"). The case of the typed letters is
 * kept ("Khong" -> "Không", "KHONG" -> "KHÔNG"). The context is the tokens
 * before it, oldest first, as the model writes them (accented, lowercase,
 * "<s>" for the start of a sentence). Returns how many were written to
 * `out`; 0 when `typed` is not such a word. */
int xk_restore_word(const XKModels* models, const char* const* context, int ctx_len,
                    const char* typed, XKCandidate* out, int max_out);
/* The best reading of a run of 1..XK_RESTORE_MAX_RUN words typed without
 * marks, decoded together, so a later word can change an earlier one:
 * "co gang" -> "cố gắng", where "co" alone is "có". `next`, when not NULL,
 * is the token known to follow the run ("</s>" when a full stop ended the
 * sentence, "<p>" for a comma), and the run is also scored on leading to
 * it: "di nhe." ends "đi nhé", where "di nhe" alone is "đi nhẹ". Writes n
 * newly allocated strings to `out` (the caller frees them) and returns n,
 * or 0 when a word is not one xk_restore_word takes. */
int xk_restore_run(const XKModels* models, const char* const* context, int ctx_len,
                   const char* const* typed, int n, const char* next, char** out);

/* The choice between a word's English and Vietnamese readings. Telex keys
 * can spell both an English word and an attested syllable ("list" is
 * "lít"); with both models set, the words before decide ("my list", "hai
 * lít"). The state keeps those words itself from what it commits (digits,
 * , ; : count too; . ! ? Return, Tab, arrows, BackSpace over committed
 * text and xk_state_reset start afresh). Without both models in the bundle
 * the syllable always wins. The state does not own the bundle; it must
 * outlive the state or be unset first. */
void xk_state_set_models(XKState* state, const XKModels* models);
/* What comes before the cursor, for a frontend that knows (oldest first;
 * "<s>" marks the start of a sentence). Replaces what the state kept. */
void xk_state_set_context(XKState* state, const char* const* words, int n);

/* Shortcuts ("gõ tắt"; the laptop's macros): an abbreviation that expands
 * when its word ends, "ko" -> "không". Abbreviations match without case;
 * with match_case the expansion takes the typed case: "Ko" -> "Không",
 * "KO" -> "KHÔNG". The file is the laptop's macros.txt, one
 * "abbreviation<TAB>expansion" per line (# starts a comment). */
typedef struct XKShortcuts XKShortcuts;
typedef struct XKShortcutCounts {
    int added;    /* abbreviations the table did not have */
    int same;     /* already there with the same expansion */
    int changed;  /* already there with another expansion: replaced */
    int skipped;  /* lines that are not an entry */
} XKShortcutCounts;
XKShortcuts* xk_shortcuts_new(void);
void         xk_shortcuts_free(XKShortcuts* sc);
/* Replace the table with the file's TAB lines; the entries read, or -1. */
int          xk_shortcuts_load(XKShortcuts* sc, const char* path);
/* Write the table, sorted, to a temporary file and rename it; 0 or -1. */
int          xk_shortcuts_save(const XKShortcuts* sc, const char* path);
/* Read a table made elsewhere: per line "abbr<TAB>expansion",
 * "abbr,expansion" (CSV, quoted fields allowed) or "abbr=expansion",
 * whichever separator comes first (TAB before all). An abbreviation is one
 * word of at most 32 bytes; the last of repeated ones counts. Fills
 * `counts` with what it would do, and does it only when `apply`. Returns
 * the entries found, or -1. */
int          xk_shortcuts_import(XKShortcuts* sc, const char* text, int apply,
                                 XKShortcutCounts* counts);
/* Add or replace one; 0 if it is not a valid entry. */
int          xk_shortcuts_set(XKShortcuts* sc, const char* abbr, const char* expansion);
void         xk_shortcuts_remove(XKShortcuts* sc, const char* abbr);
int          xk_shortcuts_count(const XKShortcuts* sc);
/* The i-th entry in order of abbreviation (owned by the table); 0 past
 * the end. */
int          xk_shortcuts_at(const XKShortcuts* sc, int i, const char** abbr,
                             const char** expansion);
/* The expansion of `typed`, newly allocated (the caller frees it), or NULL. */
char*        xk_shortcuts_expand(const XKShortcuts* sc, const char* typed, int match_case);

/* Bilingual switch */
int  xk_is_english_word(const char* word);
void xk_bilingual_set_enabled(XKState* state, int enabled);
int  xk_bilingual_is_enabled(XKState* state);
void xk_bilingual_load_state(XKState* state, const char* state_path);
void xk_bilingual_save_state(XKState* state, const char* state_path);
int  xk_bilingual_app_disabled(const char* wm_class, const char* overrides_path);

/* Emoji: the index in data/emoji/emoji-index.tsv (Unicode's list with
 * CLDR's English and Vietnamese names and keywords; data/emoji/README.md). */
XKEmojiDB* xk_emoji_load(const char* index_path);
void       xk_emoji_free(XKEmojiDB* db);
/* How many emoji, and the i-th in Unicode's order, for the panel: the
 * emoji, its group ("Smileys & Emotion" ...), its name (Vietnamese, else
 * English) and whether it takes skin tones. Strings are the index's. */
int        xk_emoji_count(const XKEmojiDB* db);
int        xk_emoji_at(const XKEmojiDB* db, int i, const char** emoji, const char** group,
                       const char** name, int* skin_tones);
/* Emoji whose name or keyword matches `query`, in Vietnamese or English,
 * with or without accents ("cuoi" finds "cười"), best first: exact before
 * prefix before inside, names before keywords. chars[i] and names[i] are
 * newly allocated (the caller frees them). */
int        xk_emoji_search(XKEmojiDB* db, const char* query,
                           char** chars, char** names, int max_results);
/* Emoji for the words just typed (oldest first), for the suggestion strip:
 * those with the last three, two, then one words as a keyword ("không
 * đời nào", "trái tim", "yêu"), leaving out keywords that over 24 emoji
 * share ("mặt"). out[i] is newly allocated. */
int        xk_emoji_suggest(const XKEmojiDB* db, const char* const* words, int n,
                            char** out, int max_out);

#ifdef __cplusplus
}
#endif

#endif /* XRNIKEY_H */
