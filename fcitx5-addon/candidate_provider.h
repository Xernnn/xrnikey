#pragma once

#include <fcitx/candidatelist.h>
#include <fcitx/text.h>
#include <vietkey.h>
#include <vector>
#include <string>
#include <memory>
#include "config.h"

namespace fcitx {

struct VietKeyCandidate {
    std::string text;
    std::string gloss;
    float score;
    int type; /* 0=dict 1=correction 2=prediction */
};

class CandidateProvider {
public:
    CandidateProvider();
    ~CandidateProvider();

    void setConfig(std::shared_ptr<VietKeyConfig> config);
    std::vector<VietKeyCandidate> getCandidates(const std::string& prefix, int max);
    std::vector<VietKeyCandidate> getEmojiCandidates(const std::string& query, int max);

private:
    std::shared_ptr<VietKeyConfig> config_;
    VKDict* dict_;
    VKNgram* ngram_;
    VKEmojiDB* emoji_;

    static constexpr const char* TYPE_ICONS[] = {"📖", "✏", "⚡"};
};

} // namespace fcitx
