#include "xrnikey.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstring>

struct NgramEntry {
    float log_prob;
};

struct XKNgramImpl {
    std::unordered_map<std::string, NgramEntry> unigrams;
    std::unordered_map<std::string, NgramEntry> bigrams;
    std::unordered_map<std::string, NgramEntry> trigrams;
    float unk_log_prob;
};

static std::string make_key(const std::vector<std::string>& words) {
    std::string k;
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) k += '\t';
        k += words[i];
    }
    return k;
}

static float quantize_score(float log_prob) {
    /* 4-bit quantization: map to 16 levels, stored as nibble */
    float normalized = (log_prob + 20.0f) / 20.0f;
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;
    int level = static_cast<int>(normalized * 15);
    return (level / 15.0f) * 20.0f - 20.0f;
}

extern "C" {

XKNgram* xk_ngram_load(const char* arpa_path) {
    if (!arpa_path) return nullptr;

    auto* ng = new XKNgramImpl{};
    ng->unk_log_prob = -10.0f;

    std::ifstream f(arpa_path);
    if (!f.is_open()) { delete ng; return nullptr; }

    std::string line;
    int ngram_order = 0;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line.find("\\data\\") != std::string::npos) continue;
        if (line.find("ngram") != std::string::npos && line.find("=") != std::string::npos) {
            continue;
        }
        if (line.find("\\1-grams:") != std::string::npos) { ngram_order = 1; continue; }
        if (line.find("\\2-grams:") != std::string::npos) { ngram_order = 2; continue; }
        if (line.find("\\3-grams:") != std::string::npos) { ngram_order = 3; continue; }
        if (line.find("\\end\\") != std::string::npos) break;

        std::istringstream iss(line);
        float log_prob;
        iss >> log_prob;
        log_prob = quantize_score(log_prob);

        std::string word;
        std::vector<std::string> words;
        while (iss >> word) words.push_back(word);

        if (words.empty()) continue;

        NgramEntry entry{log_prob};

        if (ngram_order == 1 && words.size() >= 1) {
            ng->unigrams[words[0]] = entry;
        } else if (ngram_order == 2 && words.size() >= 2) {
            ng->bigrams[make_key({words[0], words[1]})] = entry;
        } else if (ngram_order == 3 && words.size() >= 3) {
            ng->trigrams[make_key({words[0], words[1], words[2]})] = entry;
        }
    }

    return reinterpret_cast<XKNgram*>(ng);
}

void xk_ngram_free(XKNgram* ng) {
    if (ng) delete reinterpret_cast<XKNgramImpl*>(ng);
}

float xk_ngram_score(XKNgram* ng, const char** context, int ctx_len, const char* word) {
    if (!ng || !word) return ng ? reinterpret_cast<XKNgramImpl*>(ng)->unk_log_prob : -10.0f;
    auto* n = reinterpret_cast<XKNgramImpl*>(ng);

    if (ctx_len >= 2 && context[ctx_len - 2] && context[ctx_len - 1]) {
        std::string key = make_key({context[ctx_len - 2], context[ctx_len - 1], word});
        auto it = n->trigrams.find(key);
        if (it != n->trigrams.end()) return it->second.log_prob;
    }

    if (ctx_len >= 1 && context[ctx_len - 1]) {
        std::string key = make_key({context[ctx_len - 1], word});
        auto it = n->bigrams.find(key);
        if (it != n->bigrams.end()) return it->second.log_prob;
    }

    auto it = n->unigrams.find(word);
    if (it != n->unigrams.end()) return it->second.log_prob;

    return n->unk_log_prob;
}

} /* extern "C" */
