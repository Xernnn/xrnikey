#pragma once

#include <fcitx/candidatelist.h>
#include <fcitx/text.h>
#include <xrnikey.h>
#include <vector>
#include <string>
#include <memory>
#include "config.h"

namespace fcitx {

struct XrnikeyCandidate {
    std::string text;
    std::string gloss;
    float score;
    int type; /* 0=dict 1=correction 2=prediction */
};

class CandidateProvider {
public:
    CandidateProvider();
    ~CandidateProvider();

    void setConfig(std::shared_ptr<XrnikeyConfig> config);

    /* Load the shared dictionary / n-gram / emoji data. Without this every
     * lookup returns empty, which is why the candidate window never showed
     * anything: dict_ was initialised to nullptr and nothing ever set it. */
    void load(const std::string& dataDir);
    bool ready() const { return dict_ != nullptr; }
    /* `context` is the last few finished words, used to rank completions by
     * how likely they are to follow. Pass empty for no context. */
    std::vector<XrnikeyCandidate> getCandidates(
        const std::string& prefix, int max,
        const std::vector<std::string>& context = {});
    std::vector<XrnikeyCandidate> getEmojiCandidates(const std::string& query, int max);

private:
    std::shared_ptr<XrnikeyConfig> config_;
    XKDict* dict_;
    XKNgram* ngram_;
    XKEmojiDB* emoji_;

    static constexpr const char* TYPE_ICONS[] = {"📖", "✏", "⚡"};
};

} // namespace fcitx
