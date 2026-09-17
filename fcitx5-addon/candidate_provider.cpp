#include "candidate_provider.h"
#include <fcitx-utils/i18n.h>
#include <algorithm>
#include <cstring>

namespace fcitx {

CandidateProvider::CandidateProvider()
    : dict_(nullptr), ngram_(nullptr), emoji_(nullptr) {}

CandidateProvider::~CandidateProvider() {
    if (dict_) xk_dict_free(dict_);
    if (ngram_) xk_ngram_free(ngram_);
    if (emoji_) xk_emoji_free(emoji_);
}

void CandidateProvider::setConfig(std::shared_ptr<XrnikeyConfig> config) {
    config_ = config;
}

void CandidateProvider::load(const std::string& dataDir) {
    if (dict_) return;  /* idempotent: reloadConfig() may call this again */
    dict_ = xk_dict_load((dataDir + "/dict/vi.dict").c_str());
    ngram_ = xk_ngram_load((dataDir + "/ngram/vi.arpa").c_str());
    emoji_ = xk_emoji_load((dataDir + "/emoji/emoji.json").c_str());
}

std::vector<XrnikeyCandidate> CandidateProvider::getCandidates(
    const std::string& prefix, int max,
    const std::vector<std::string>& context) {
    std::vector<XrnikeyCandidate> results;
    if (!dict_ || prefix.empty()) return results;

    /* Raw C pointers for the C ABI; `context` outlives this call. */
    std::vector<const char*> ctx;
    ctx.reserve(context.size());
    for (const auto& w : context) ctx.push_back(w.c_str());

    char* completions[32];
    int n = xk_dict_complete(dict_, prefix.c_str(), completions, max);
    for (int i = 0; i < n; ++i) {
        XrnikeyCandidate c;
        c.text = completions[i];
        /* Dictionary frequency is the base score; the n-gram model then says
         * how well the word follows what was just typed. Without this the
         * ranking was frequency-only and the trained model went unused --
         * "chúc" should outrank "chủ" right after "xin". */
        float freq = static_cast<float>(xk_dict_lookup(dict_, completions[i]));
        c.score = freq / 10000.0f;
        if (ngram_ && !ctx.empty()) {
            c.score += 0.5f * xk_ngram_score(ngram_, ctx.data(),
                                             static_cast<int>(ctx.size()),
                                             completions[i]);
        }
        c.type = 0;
        c.gloss = "";
        results.push_back(c);
        free(completions[i]);
    }

    if (config_ && config_->autoCorrect.value()) {
        XKCandidate corrections[8];
        int nc = xk_autocorrect(dict_, ngram_, prefix.c_str(),
                                ctx.empty() ? nullptr : ctx.data(),
                                static_cast<int>(ctx.size()), corrections, 8);
        for (int i = 0; i < nc; ++i) {
            if (corrections[i].score < config_->autoCorrectThreshold.value())
                continue;
            XrnikeyCandidate c;
            c.text = corrections[i].text;
            c.score = corrections[i].score;
            c.type = corrections[i].type;
            c.gloss = TYPE_ICONS[c.type];
            results.push_back(c);
        }
    }

    std::sort(results.begin(), results.end(),
              [](const XrnikeyCandidate& a, const XrnikeyCandidate& b) {
                  return a.score > b.score;
              });
    if (static_cast<int>(results.size()) > max) results.resize(max);
    return results;
}

std::vector<XrnikeyCandidate> CandidateProvider::getEmojiCandidates(
    const std::string& query, int max) {
    std::vector<XrnikeyCandidate> results;
    if (!emoji_) return results;

    char* chars[16];
    char* names[16];
    int n = xk_emoji_search(emoji_, query.c_str(), chars, names, max);
    for (int i = 0; i < n; ++i) {
        XrnikeyCandidate c;
        c.text = chars[i];
        c.gloss = names[i];
        c.type = 0;
        c.score = 1.0f;
        results.push_back(c);
        free(chars[i]);
        free(names[i]);
    }
    return results;
}

} // namespace fcitx
