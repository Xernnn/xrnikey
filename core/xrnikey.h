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

/* Opt in to Unikey-style instant commit: every keystroke fills
 * commit/delete_before to correct previously committed text in place,
 * instead of only committing the finalized word on Space. Off by default —
 * a frontend that only understands the classic commit-on-Space contract
 * (e.g. relies on commit staying empty until the word is finished) must not
 * enable this. */
void xk_state_set_instant_commit(XKState* state, int enabled);

/* Key processing — keysym is XKB keysym, modifiers bitmask */
XKResult xk_process_key(XKState* state, uint32_t keysym, uint32_t modifiers);

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

/* N-gram language model */
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
