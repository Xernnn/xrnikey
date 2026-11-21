#include "xrnikey.h"
#include "xrnikey_internal.h"
#include "lm_internal.h"
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

namespace {

/* P(next frame | language of the word before), LANG_NONE at the start of
 * a sentence. The owner writes mostly Vietnamese. English words that
 * Vietnamese text itself uses need no switch (the Vietnamese model has
 * them); a switch into English proper is rare right after a Vietnamese
 * word, and English runs are short. */
double frame_prob(uint8_t prev, uint8_t next) {
    static const double p[3][3] = {
        /* to:  none  vi     en  */
        {0.0, 0.85, 0.15},  /* from the start of a sentence */
        {0.0, 0.99, 0.01},  /* from Vietnamese */
        {0.0, 0.30, 0.70},  /* from English */
    };
    return p[prev][next];
}

/* The language of the latest word in the context, skipping numbers and
 * pauses; LANG_NONE at the start of a sentence. */
uint8_t previous_lang(const Context& c) {
    for (size_t i = c.langs.size(); i-- > 0;)
        if (c.langs[i] != LANG_NONE) return c.langs[i];
    return LANG_NONE;
}

/* log10 P(token | context) under one model; an unknown token gets its
 * share of the unknown mass. */
float token_logprob(const Lm& m, const Context& c, const std::string& token) {
    uint32_t ids[lmfmt::kMaxOrder];
    int n = 0;
    const int keep = static_cast<int>(m.hdr->order) - 1;
    std::vector<uint32_t> all;
    if (c.start) all.push_back(lmfmt::kBos);
    for (const auto& t : c.tokens) all.push_back(lm_id(m, t));
    for (size_t i = all.size() > static_cast<size_t>(keep) ? all.size() - static_cast<size_t>(keep) : 0;
         i < all.size(); ++i)
        ids[n++] = all[i];
    const uint32_t w = lm_id(m, token);
    float p = lm_logprob(m, ids, n, w);
    if (w == lmfmt::kUnk) p -= lm_unk_share(m);
    return p;
}

/* log10 P(the Vietnamese reading `vi` next). */
double vi_score(const XKStateImpl& s, const std::string& vi) {
    return std::log10(frame_prob(previous_lang(s.ctx), LANG_VI)) + token_logprob(*s.vi_lm, s.ctx, vi);
}

/* log10 P(the English reading `en` next): used in Vietnamese text, or a
 * switch into English. */
double en_score(const XKStateImpl& s, const std::string& en) {
    const uint8_t prev = previous_lang(s.ctx);
    const double in_vi = frame_prob(prev, LANG_VI) * std::pow(10.0, token_logprob(*s.vi_lm, s.ctx, en));
    const double in_en = frame_prob(prev, LANG_EN) * std::pow(10.0, token_logprob(*s.en_lm, s.ctx, en));
    return std::log10(in_vi + in_en);
}

bool is_ascii_word(const std::string& t) {
    if (t.empty()) return false;
    for (char c : t)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    return true;
}

}  // namespace

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
    return s.vi_lm != nullptr && s.en_lm != nullptr;
}

bool bilingual_prefers_english(const XKStateImpl& s, const std::string& vi, const std::string& en) {
    if (!bilingual_ready(s)) return false;
    return en_score(s, context_token(en)) > vi_score(s, context_token(vi));
}

void context_note_word(XKStateImpl& s, const std::string& text) {
    const std::string token = context_token(text);
    uint8_t lang = LANG_VI;
    if (token == "<num>") {
        if (!s.ctx.tokens.empty() && s.ctx.tokens.back() == "<num>") return;
        lang = LANG_NONE;
    } else if (token == "<unk>") {
        lang = LANG_NONE;
    } else {
        std::u32string letters;
        int tone;
        const bool syllable = syllable_split(text, &letters, &tone) && syllable_valid(letters, tone);
        if (!syllable) lang = LANG_EN;
        else if (is_ascii_word(text) && bilingual_ready(s))
            /* The same letters in both languages ("the", "can", "my"): which
             * one the model finds likelier here. */
            lang = frame_prob(previous_lang(s.ctx), LANG_EN) *
                               std::pow(10.0, token_logprob(*s.en_lm, s.ctx, token)) >
                           frame_prob(previous_lang(s.ctx), LANG_VI) *
                               std::pow(10.0, token_logprob(*s.vi_lm, s.ctx, token))
                       ? LANG_EN
                       : LANG_VI;
    }
    s.ctx.push(token, lang);
}

void context_note_char(XKStateImpl& s, char c) {
    if (c >= '0' && c <= '9') context_note_word(s, std::string(1, c));
    else if (c == ',' || c == ';' || c == ':') {
        if (s.ctx.tokens.empty() || s.ctx.tokens.back() != "<p>") s.ctx.push("<p>", LANG_NONE);
    } else if (c == '.' || c == '!' || c == '?') s.ctx.clear();
}

}  // namespace xrnikey

extern "C" {

void xk_state_set_models(XKState* state, const XKLm* vi, const XKLm* en) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->vi_lm = reinterpret_cast<const xrnikey::Lm*>(vi);
    s->en_lm = reinterpret_cast<const xrnikey::Lm*>(en);
}

void xk_state_set_context(XKState* state, const char* const* words, int n) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->ctx.clear();
    for (int i = 0; words && i < n; ++i) {
        if (!words[i]) continue;
        const std::string w = words[i];
        if (w == "<s>") s->ctx.clear();
        else if (w.size() == 1 && !((w[0] >= 'a' && w[0] <= 'z') || (w[0] >= 'A' && w[0] <= 'Z')))
            xrnikey::context_note_char(*s, w[0]);
        else xrnikey::context_note_word(*s, w);
    }
}


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
