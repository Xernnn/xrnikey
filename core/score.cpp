#include "score.h"

#include "learn.h"

#include <cmath>
#include <vector>

namespace xrnkey {

/* The owner writes mostly Vietnamese. English words that Vietnamese text
 * itself uses need no switch: the Vietnamese model has them ("file",
 * "check"). The switch covers English that Vietnamese text does not use,
 * which right after a Vietnamese word is rare: at 0.01 a rare syllable
 * after "tôi" lost to a common English word 0.65 % of the time (set V);
 * at 0.002 it is 0.46 %, and no other xk-eval figure moved. English runs
 * are short. */
double frame_prob(uint8_t prev, uint8_t next) {
    static const double p[3][3] = {
        /* to:  none  vi      en  */
        {0.0, 0.85,  0.15},   /* from the start of a sentence */
        {0.0, 0.998, 0.002},  /* from Vietnamese */
        {0.0, 0.30,  0.70},   /* from English */
    };
    return p[prev][next];
}

uint8_t previous_lang(const Context& c) {
    for (size_t i = c.langs.size(); i-- > 0;)
        if (c.langs[i] != LANG_NONE) return c.langs[i];
    return LANG_NONE;
}

float token_logprob(const Lm& m, const Context& c, const std::string& token) {
    uint32_t ids[lmfmt::kMaxOrder];
    const size_t keep = m.hdr->order - 1;
    std::vector<uint32_t> all;
    all.reserve(c.tokens.size() + 1);
    if (c.start) all.push_back(lmfmt::kBos);
    for (const auto& t : c.tokens) all.push_back(lm_id(m, t));
    int n = 0;
    for (size_t i = all.size() > keep ? all.size() - keep : 0; i < all.size(); ++i) ids[n++] = all[i];
    const uint32_t w = lm_id(m, token);
    float p = lm_logprob(m, ids, n, w);
    if (w == lmfmt::kUnk) p -= lm_unk_share(m);
    return p;
}

bool is_ascii_word(const std::string& t) {
    if (t.empty()) return false;
    for (char c : t)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    return true;
}

double mix_logprob(const Models& m, const Context& c, const std::string& token, uint8_t* lang) {
    const uint8_t prev = previous_lang(c);
    const bool both = m.vi && m.en;
    double pv = 0.0, pe = 0.0;
    if (m.vi) pv = (both ? frame_prob(prev, LANG_VI) : 1.0) * std::pow(10.0, token_logprob(*m.vi, c, token));
    if (m.en && is_ascii_word(token))
        pe = (both ? frame_prob(prev, LANG_EN) : 1.0) * std::pow(10.0, token_logprob(*m.en, c, token));
    if (lang) *lang = pe > pv ? LANG_EN : LANG_VI;
    double p = pv + pe;
    if (m.user) {
        const std::string prev = !c.tokens.empty() ? c.tokens.back() : c.start ? "<s>" : "";
        p = m.user->blend(p, prev, token);
    }
    return p > 0.0 ? std::log10(p) : static_cast<double>(lmfmt::kNever);
}

void context_push_text(const Models* m, Context& c, const std::string& text) {
    const std::string token = context_token(text);
    uint8_t lang = LANG_VI;
    if (token == "<num>") {
        if (!c.tokens.empty() && c.tokens.back() == "<num>") return;
        lang = LANG_NONE;
    } else if (token == "<unk>") {
        lang = LANG_NONE;
    } else {
        std::u32string letters;
        int tone;
        const bool syllable = syllable_split(text, &letters, &tone) && syllable_valid(letters, tone);
        if (!syllable) lang = LANG_EN;
        else if (is_ascii_word(token) && m && m->vi && m->en)
            /* The same letters in both languages ("the", "can", "my"): the
             * frame the models find likelier here. */
            mix_logprob(*m, c, token, &lang);
    }
    c.push(token, lang);
}

void context_push_char(const Models* m, Context& c, char ch) {
    if (ch >= '0' && ch <= '9') context_push_text(m, c, std::string(1, ch));
    else if (ch == ',' || ch == ';' || ch == ':') {
        if (c.tokens.empty() || c.tokens.back() != "<p>") c.push("<p>", LANG_NONE);
    } else if (ch == '.' || ch == '!' || ch == '?') c.clear();
}

Context context_from(const Models* m, const char* const* words, int n, bool start) {
    Context c;
    c.start = start;
    for (int i = 0; words && i < n; ++i) {
        if (!words[i]) continue;
        const std::string w = words[i];
        if (w == "<s>") c.clear();
        else if (w == "<p>") context_push_char(m, c, ',');
        else if (w == "<num>") context_push_text(m, c, "0");
        else if (w.size() == 1 && !((w[0] >= 'a' && w[0] <= 'z') || (w[0] >= 'A' && w[0] <= 'Z')))
            context_push_char(m, c, w[0]);
        else context_push_text(m, c, w);
    }
    return c;
}

}  // namespace xrnkey
