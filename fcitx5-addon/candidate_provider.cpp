#include "candidate_provider.h"
#include <fcitx-utils/i18n.h>
#include <cstring>

namespace fcitx {

CandidateProvider::CandidateProvider()
    : dict_(nullptr), ngram_(nullptr), emoji_(nullptr) {}

CandidateProvider::~CandidateProvider() {
    if (dict_) vk_dict_free(dict_);
    if (ngram_) vk_ngram_free(ngram_);
    if (emoji_) vk_emoji_free(emoji_);
}

void CandidateProvider::setConfig(std::shared_ptr<VietKeyConfig> config) {
    config_ = config;
}

std::vector<VietKeyCandidate> CandidateProvider::getCandidates(
    const std::string& prefix, int max) {
    std::vector<VietKeyCandidate> results;
    if (!dict_ || prefix.empty()) return results;

    char* completions[32];
    int n = vk_dict_complete(dict_, prefix.c_str(), completions, max);
    for (int i = 0; i < n; ++i) {
        VietKeyCandidate c;
        c.text = completions[i];
        c.score = static_cast<float>(vk_dict_lookup(dict_, completions[i])) / 10000.0f;
        c.type = 0;
        c.gloss = "";
        results.push_back(c);
        free(completions[i]);
    }

    if (config_ && config_->autoCorrect.value()) {
        VKCandidate corrections[8];
        const char* ctx[] = {};
        int nc = vk_autocorrect(dict_, ngram_, prefix.c_str(), ctx, 0, corrections, 8);
        for (int i = 0; i < nc; ++i) {
            VietKeyCandidate c;
            c.text = corrections[i].text;
            c.score = corrections[i].score;
            c.type = corrections[i].type;
            c.gloss = TYPE_ICONS[c.type];
            results.push_back(c);
        }
    }

    return results;
}

std::vector<VietKeyCandidate> CandidateProvider::getEmojiCandidates(
    const std::string& query, int max) {
    std::vector<VietKeyCandidate> results;
    if (!emoji_) return results;

    char* chars[16];
    char* names[16];
    int n = vk_emoji_search(emoji_, query.c_str(), chars, names, max);
    for (int i = 0; i < n; ++i) {
        VietKeyCandidate c;
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
