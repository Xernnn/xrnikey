#include "xrnikey.h"
#include "xrnikey_internal.h"
#include "lm_internal.h"
#include "score.h"
#include <cmath>
#include <fstream>
#include <string>
#include <cstring>

/* Choosing between the English and the Vietnamese reading of a word's keys
 * in context.
 *
 * Telex keys can spell an English word and an attested syllable at once:
 * "list" is "lít", "turn" is "tủn". No answer fits every context ("my list",
 * "hai lít"), so the state keeps the last few words (as model tokens, with
 * the language each was in) and compares how likely each reading is next:
 *
 *   P(vi reading) = P(stay | prev) * P_vi(syllable | words before)
 *   P(en reading) = P(stay | prev) * P_vi(word | words before)
 *                 + P(switch | prev) * P_en(word | words before)
 *
 * where "stay" means going on in the frame the text is in. The Vietnamese
 * model knows the English words Vietnamese text uses ("file", "check") as
 * tokens of its own, so P_vi of the English word is how often Vietnamese
 * writers use it there; the English model covers switching into English.
 * The English reading wins only when it is likelier; a tie goes to
 * Vietnamese. */

namespace xrnikey {

std::string context_token(const std::string& text) {
    if (text.empty()) return "<unk>";
    std::u32string letters;
    int tone;
    if (syllable_split(text, &letters, &tone) && syllable_valid(letters, tone))
        return apply_tone_to_syllable(utf8_of(letters), tone);
    if (is_ascii_word(text)) {
        std::string low = text;
        for (char& c : low) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c | 0x20 : c);
        return low;
    }
    bool digits = true;
    for (char c : text) digits = digits && ((c >= '0' && c <= '9') || c == '.' || c == ',');
    if (digits && text[0] >= '0' && text[0] <= '9') return "<num>";
    return "<unk>";
}

bool bilingual_ready(const XKStateImpl& s) {
    return s.models && s.models->vi && s.models->en;
}

bool bilingual_prefers_english(const XKStateImpl& s, const std::string& vi, const std::string& en) {
    if (!bilingual_ready(s)) return false;
    return mix_logprob(*s.models, s.ctx, context_token(en)) >
           mix_logprob(*s.models, s.ctx, context_token(vi));
}

void context_learn_word(XKStateImpl& s, const std::string& text) {
    if (s.opt.learn && s.models && s.models->user) learn_word(*s.models->user, s.ctx, text);
}

void context_note_word(XKStateImpl& s, const std::string& text) {
    context_push_text(s.models, s.ctx, text);
}

void context_note_char(XKStateImpl& s, char c) {
    context_push_char(s.models, s.ctx, c);
}

}  // namespace xrnikey

extern "C" {

XKModels* xk_models_new(const XKLm* vi, const XKLm* en) {
    auto* m = new xrnikey::Models;
    m->vi = reinterpret_cast<const xrnikey::Lm*>(vi);
    m->en = reinterpret_cast<const xrnikey::Lm*>(en);
    return reinterpret_cast<XKModels*>(m);
}

void xk_models_free(XKModels* models) {
    delete reinterpret_cast<xrnikey::Models*>(models);
}

void xk_models_set_correction(XKModels* models, int level) {
    if (!models) return;
    reinterpret_cast<xrnikey::Models*>(models)->correction =
        level < XK_CORRECT_OFF ? XK_CORRECT_OFF : level > XK_CORRECT_STRONG ? XK_CORRECT_STRONG : level;
}

void xk_state_set_models(XKState* state, const XKModels* models) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->models = reinterpret_cast<const xrnikey::Models*>(models);
}

void xk_state_set_context(XKState* state, const char* const* words, int n) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->ctx = xrnikey::context_from(s->models, words, n, true);
}

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
