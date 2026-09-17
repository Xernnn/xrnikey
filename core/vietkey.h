#ifndef VIETKEY_H
#define VIETKEY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VKState VKState;

/* commit/delete_before drive instant (Unikey-style) commit: the caller should
 * delete `delete_before` codepoints immediately before the cursor (text this
 * engine previously committed for the syllable in progress), then insert
 * `commit`. delete_before is 0 for a plain append. preedit mirrors the full
 * current syllable for frontends that use a classic composition buffer
 * instead (e.g. no surrounding-text support). */
typedef struct VKResult {
    char commit[64];
    char preedit[64];
    int  consumed;
    int  reset;
    int  delete_before;
} VKResult;

typedef struct VKDict VKDict;
typedef struct VKNgram VKNgram;
typedef struct VKEmojiDB VKEmojiDB;

/* Input mode */
#define VK_MODE_TELEX 0
#define VK_MODE_VNI   1

/* Tone indices (internal) */
#define VK_TONE_NONE   0
#define VK_TONE_SAC    1  /* sắc  ´ */
#define VK_TONE_HUYEN  2  /* huyền ` */
#define VK_TONE_HOI    3  /* hỏi  ̉ */
#define VK_TONE_NGA    4  /* ngã  ~ */
#define VK_TONE_NANG   5  /* nặng . */

/* State lifecycle */
VKState* vk_state_new(int mode);
void     vk_state_free(VKState* state);
void     vk_state_reset(VKState* state);
char*    vk_state_get_preedit(VKState* state);

/* Opt in to Unikey-style instant commit: every keystroke fills
 * commit/delete_before to correct previously committed text in place,
 * instead of only committing the finalized word on Space. Off by default —
 * a frontend that only understands the classic commit-on-Space contract
 * (e.g. relies on commit staying empty until the word is finished) must not
 * enable this. */
void vk_state_set_instant_commit(VKState* state, int enabled);

/* Key processing — keysym is XKB keysym, modifiers bitmask */
VKResult vk_process_key(VKState* state, uint32_t keysym, uint32_t modifiers);

/* Tone placement — returns newly allocated NFC string (caller frees) */
char* vk_place_tone(const char* syllable, int tone);

/* Syllable validation */
int vk_is_valid_syllable(const char* word);

/* Dictionary */
VKDict* vk_dict_load(const char* path);
void    vk_dict_free(VKDict* dict);
int     vk_dict_complete(VKDict* dict, const char* prefix, char** results, int max_results);
int     vk_dict_lookup(VKDict* dict, const char* word);
void    vk_dict_add_word(VKDict* dict, const char* word, int freq);
void    vk_dict_reload_user(VKDict* dict, const char* user_path);

/* N-gram language model */
VKNgram* vk_ngram_load(const char* arpa_path);
void     vk_ngram_free(VKNgram* ng);
float    vk_ngram_score(VKNgram* ng, const char** context, int ctx_len, const char* word);

/* Autocorrect */
typedef struct VKCandidate {
    char  text[128];
    float score;
    int   edit_distance;
    int   type; /* 0=dict 1=correction 2=prediction */
} VKCandidate;

int vk_autocorrect(VKDict* dict, VKNgram* ng, const char* word,
                   const char** context, int ctx_len,
                   VKCandidate* out, int max_out);

/* Bilingual switch */
int  vk_is_english_word(const char* word);
void vk_bilingual_set_enabled(VKState* state, int enabled);
int  vk_bilingual_is_enabled(VKState* state);
void vk_bilingual_load_state(VKState* state, const char* state_path);
void vk_bilingual_save_state(VKState* state, const char* state_path);
int  vk_bilingual_app_disabled(const char* wm_class, const char* overrides_path);

/* Emoji */
VKEmojiDB* vk_emoji_load(const char* json_path);
void       vk_emoji_free(VKEmojiDB* db);
int        vk_emoji_search(VKEmojiDB* db, const char* query,
                           char** chars, char** names, int max_results);

#ifdef __cplusplus
}
#endif

#endif /* VIETKEY_H */
