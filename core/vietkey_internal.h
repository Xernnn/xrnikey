#ifndef VIETKEY_INTERNAL_H
#define VIETKEY_INTERNAL_H

#include "vietkey.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

namespace vietkey {

/* Unicode combining marks */
constexpr char COMB_SAC[]   = "\xCC\x81";  /* U+0301 */
constexpr char COMB_HUYEN[] = "\xCC\x80";  /* U+0300 */
constexpr char COMB_HOI[]   = "\xCC\x89";  /* U+0309 */
constexpr char COMB_NGA[]   = "\xCC\x83";  /* U+0303 */
constexpr char COMB_NANG[]  = "\xCC\xA3";  /* U+0323 */

struct UndoEntry {
    std::string buffer;
    int tone;
    int vowel_mod;
};

struct VKStateImpl {
    int mode;
    std::string buffer;
    std::string committed;
    int tone;
    bool enabled;
    bool passthrough_word;
    int char_count_no_vowel;
    std::vector<UndoEntry> undo_stack;
    std::vector<std::string> history;
    int history_idx;
    bool double_press_escape;
    char last_key;
    int last_key_time_ms;

    VKStateImpl(int m) : mode(m), tone(VK_TONE_NONE), enabled(true),
        passthrough_word(false), char_count_no_vowel(0),
        history_idx(-1), double_press_escape(false),
        last_key(0), last_key_time_ms(0) {}
};

/* telex.cpp */
void telex_process_char(VKStateImpl* s, char c, VKResult* r);
void telex_backspace(VKStateImpl* s, VKResult* r);

/* vni.cpp */
void vni_process_char(VKStateImpl* s, char c, VKResult* r);
void vni_backspace(VKStateImpl* s, VKResult* r);

/* tone_placement.cpp */
std::string apply_tone_to_syllable(const std::string& base, int tone);
std::string nfc_compose(const std::string& nfd);
std::string nfd_decompose_char(uint32_t cp);
int utf8_decode(const char* s, uint32_t* cp, int* len);
int utf8_encode(uint32_t cp, char* out);

/* syllable_validator.cpp */
bool is_valid_vn_syllable(const std::string& word);

/* Utility */
bool is_vowel_char(char c);
bool has_vietnamese_vowel_pattern(const std::string& s);
uint32_t keysym_to_char(uint32_t keysym);

} /* namespace vietkey */

#endif /* VIETKEY_INTERNAL_H */
