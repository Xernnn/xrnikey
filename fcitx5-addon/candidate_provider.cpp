#include "candidate_provider.h"
#include "user_paths.h"

#include <cstdlib>
#include <fcitx-utils/i18n.h>
#include <algorithm>
#include <cstring>

namespace fcitx {

CandidateProvider::CandidateProvider()
    : dict_(nullptr), ngram_(nullptr) {}

CandidateProvider::~CandidateProvider() {
    if (dict_) xk_dict_free(dict_);
    if (ngram_) xk_ngram_free(ngram_);
}

void CandidateProvider::setConfig(std::shared_ptr<XrnkeyConfig> config) {
    config_ = config;
}

/* Where xk-dict add writes the user's own words. */
static std::string userDictPath() { return userDataDir() + "/user-dict.txt"; }

void CandidateProvider::load(const std::string& dataDir) {
    /* Idempotent: reloadConfig() calls this again, which is when words added with
     * xk-dict since Fcitx5 started are picked up ("Reload config", or fcitx5 -r). */
    if (!dict_) {
        dict_ = xk_dict_load((dataDir + "/dict/vi.dict").c_str());
        ngram_ = xk_ngram_load((dataDir + "/ngram/vi.arpa").c_str());
    }
    if (dict_) xk_dict_reload_user(dict_, userDictPath().c_str());
}

std::vector<XrnkeyCandidate> CandidateProvider::getCandidates(
    const std::string& prefix, int max,
    const std::vector<std::string>& context) {
    std::vector<XrnkeyCandidate> results;
    if (!dict_ || prefix.empty()) return results;

    /* Raw C pointers for the C ABI; `context` outlives this call. */
    std::vector<const char*> ctx;
    ctx.reserve(context.size());
    for (const auto& w : context) ctx.push_back(w.c_str());

    char* completions[32];
    int n = xk_dict_complete(dict_, prefix.c_str(), completions, max);
    for (int i = 0; i < n; ++i) {
        XrnkeyCandidate c;
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
            XrnkeyCandidate c;
            c.text = corrections[i].text;
            c.score = corrections[i].score;
            c.type = corrections[i].type;
            c.gloss = TYPE_ICONS[c.type];
            results.push_back(c);
        }
    }

    std::sort(results.begin(), results.end(),
              [](const XrnkeyCandidate& a, const XrnkeyCandidate& b) {
                  return a.score > b.score;
              });
    if (static_cast<int>(results.size()) > max) results.resize(max);
    return results;
}

} // namespace fcitx
