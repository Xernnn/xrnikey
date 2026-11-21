/* Next-word suggestions (xk_predict): the strip's words after a Space. The
 * candidates are the words each model saw after the context (its longest
 * known n-gram, then shorter ones) and the owner's own pairs; the mixture
 * that restoration uses (score.cpp) ranks them, so a suggestion agrees with
 * what restoration would make of the same word typed without marks. */
#include "learn.h"
#include "score.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace xrnkey {
namespace {

/* Candidates from one model: followers of the last 2 tokens, then of the
 * last one, until there are enough. */
void model_candidates(const Lm& m, const Context& c, std::unordered_set<std::string>& out, size_t want) {
    std::vector<uint32_t> ctx;
    if (c.start && c.tokens.size() < 2) ctx.push_back(lm_id(m, "<s>"));
    for (const auto& t : c.tokens) ctx.push_back(lm_id(m, t));
    const int order = static_cast<int>(m.hdr->order);
    constexpr int kPerOrder = 24;
    uint32_t ids[kPerOrder];
    for (int k = std::min<int>(static_cast<int>(ctx.size()), order - 1); k >= 1; --k) {
        const uint32_t* tail = ctx.data() + ctx.size() - static_cast<size_t>(k);
        bool known = true;
        for (int i = 0; i < k; ++i) known = known && tail[i] != lmfmt::kUnk;
        if (!known) continue;
        const int got = lm_followers(m, tail, k, ids, nullptr, kPerOrder);
        for (int i = 0; i < got; ++i) {
            const char* w = lm_word(m, ids[i]);
            if (w) out.insert(w);
        }
        if (out.size() >= want) return;
    }
}

/* The model's commonest words, when the context offers too few. */
void common_words(const Lm& m, std::unordered_set<std::string>& out, size_t n) {
    std::vector<std::pair<float, uint32_t>> all;
    all.reserve(m.hdr->vocab);
    for (uint32_t id = 0; id < m.hdr->vocab; ++id) all.emplace_back(lm_prob_at(m, 1, id), id);
    const size_t k = std::min(all.size(), n);
    std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(k), all.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t i = 0; i < k; ++i)
        if (const char* w = lm_word(m, all[i].second)) out.insert(w);
}

/* A word to offer: not a marker, a number or punctuation. */
bool offerable(const std::string& w) {
    if (w.empty() || w[0] == '<') return false;
    for (unsigned char ch : w)
        if (ch >= 0x80 || (ch >= 'a' && ch <= 'z')) return true;
    return false;
}

}  // namespace
}  // namespace xrnkey

using xrnkey::Models;

extern "C" int xk_predict(const XKModels* models, const char* const* context, int n, char** out, int max) {
    if (!models || !out || max <= 0) return 0;
    const auto& m = *reinterpret_cast<const Models*>(models);
    if (!m.vi && !m.en) return 0;
    /* No context at all: a sentence's start. */
    const xrnkey::Context c = xrnkey::context_from(&m, context, n, n == 0);
    std::unordered_set<std::string> cand;
    const size_t want = static_cast<size_t>(max) * 4;
    if (m.vi) xrnkey::model_candidates(*m.vi, c, cand, want);
    if (m.en) xrnkey::model_candidates(*m.en, c, cand, want);
    if (m.user) {
        const std::string prev = !c.tokens.empty() ? c.tokens.back() : c.start ? "<s>" : "";
        if (!prev.empty()) {
            const std::string key = prev + " ";
            for (const auto& [pair, count] : m.user->pairs)
                if (count >= 2.0 && pair.compare(0, key.size(), key) == 0) cand.insert(pair.substr(key.size()));
        }
    }
    if (cand.size() < want) {
        if (m.vi) xrnkey::common_words(*m.vi, cand, want);
        if (m.en && xrnkey::previous_lang(c) == xrnkey::LANG_EN) xrnkey::common_words(*m.en, cand, want);
    }
    std::vector<std::pair<double, std::string>> scored;
    for (const auto& w : cand)
        if (xrnkey::offerable(w)) scored.emplace_back(xrnkey::mix_logprob(m, c, w), w);
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    int k = 0;
    for (const auto& s : scored) {
        if (k >= max) break;
        out[k++] = strdup(s.second.c_str());
    }
    return k;
}
