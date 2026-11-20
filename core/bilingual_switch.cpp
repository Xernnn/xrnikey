#include "xrnikey.h"
#include "xrnikey_internal.h"
#include <fstream>
#include <string>
#include <cstring>

extern "C" {

/* The common-English list lives in data/english.txt (compiled in; it now
 * also holds the computing words that used to be hard-coded here). */
int xk_is_english_word(const char* word) {
    if (!word) return 0;
    if (xrnikey::lexicon_is_english(word)) return 1;
    if (!xk_is_valid_syllable(word)) return 1;
    return 0;
}

void xk_bilingual_set_enabled(XKState* state, int enabled) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->enabled = enabled != 0;
}

int xk_bilingual_is_enabled(XKState* state) {
    if (!state) return 1;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    return s->enabled ? 1 : 0;
}

void xk_bilingual_load_state(XKState* state, const char* state_path) {
    if (!state || !state_path) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    std::ifstream f(state_path);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("\"enabled\"") != std::string::npos) {
            s->enabled = (line.find("true") != std::string::npos);
        }
    }
}

void xk_bilingual_save_state(XKState* state, const char* state_path) {
    if (!state || !state_path) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    std::ofstream f(state_path);
    if (!f.is_open()) return;
    f << "{\n  \"enabled\": " << (s->enabled ? "true" : "false") << "\n}\n";
}

int xk_bilingual_app_disabled(const char* wm_class, const char* overrides_path) {
    if (!wm_class || !overrides_path) return 0;
    std::ifstream f(overrides_path);
    if (!f.is_open()) return 0;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content.find(wm_class) != std::string::npos ? 1 : 0;
}

} /* extern "C" */
