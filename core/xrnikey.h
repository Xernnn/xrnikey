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
    int   type; /* 0=dict 1=correction 2=prediction */
} XKCandidate;

int xk_autocorrect(XKDict* dict, XKNgram* ng, const char* word,
                   const char** context, int ctx_len,
                   XKCandidate* out, int max_out);

/* Bilingual switch */
int  xk_is_english_word(const char* word);
void xk_bilingual_set_enabled(XKState* state, int enabled);
int  xk_bilingual_is_enabled(XKState* state);
void xk_bilingual_load_state(XKState* state, const char* state_path);
void xk_bilingual_save_state(XKState* state, const char* state_path);
int  xk_bilingual_app_disabled(const char* wm_class, const char* overrides_path);

/* Emoji */
XKEmojiDB* xk_emoji_load(const char* json_path);
void       xk_emoji_free(XKEmojiDB* db);
int        xk_emoji_search(XKEmojiDB* db, const char* query,
                           char** chars, char** names, int max_results);

#ifdef __cplusplus
}
#endif

#endif /* XRNIKEY_H */
