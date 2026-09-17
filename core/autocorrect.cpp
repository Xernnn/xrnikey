#include "xrnikey.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

static int damerau_levenshtein(const std::string& s1, const std::string& s2, int max_dist) {
    int m = static_cast<int>(s1.size());
    int n = static_cast<int>(s2.size());
    if (std::abs(m - n) > max_dist) return max_dist + 1;

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 0; i <= m; ++i) dp[i][0] = i;
    for (int j = 0; j <= n; ++j) dp[0][j] = j;

    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i-1][j] + 1, dp[i][j-1] + 1, dp[i-1][j-1] + cost});
            if (i > 1 && j > 1 && s1[i-1] == s2[j-2] && s1[i-2] == s2[j-1]) {
                dp[i][j] = std::min(dp[i][j], dp[i-2][j-2] + cost);
            }
        }
    }
    return dp[m][n];
}

static bool phonetically_similar(const std::string& a, const std::string& b) {
    static const char* pairs[][2] = {
        {"ch", "tr"}, {"tr", "ch"}, {"x", "s"}, {"s", "x"},
        {"gi", "d"}, {"d", "gi"}, {"r", "d"}, {"d", "r"}
    };
    for (const auto& pair : pairs) {
        if (a.find(pair[0]) != std::string::npos && b.find(pair[1]) != std::string::npos)
            return true;
    }
    return false;
}

extern "C" {

int xk_autocorrect(XKDict* dict, XKNgram* ng, const char* word,
                   const char** context, int ctx_len,
                   XKCandidate* out, int max_out) {
    if (!dict || !word || !out || max_out <= 0) return 0;

    int word_len = static_cast<int>(strlen(word));
    int max_dist = (word_len <= 6) ? 1 : 2;

    char* completions[64];
    int n = xk_dict_complete(dict, word, completions, 64);
    if (n <= 0) {
        /* Try with shorter prefix */
        if (word_len > 2) {
            char prefix[64];
            strncpy(prefix, word, word_len - 1);
            prefix[word_len - 1] = '\0';
            n = xk_dict_complete(dict, prefix, completions, 64);
        }
    }

    struct Scored { XKCandidate c; };
    std::vector<Scored> candidates;

    for (int i = 0; i < n; ++i) {
        int dist = damerau_levenshtein(word, completions[i], max_dist);
        if (dist > max_dist) { free(completions[i]); continue; }

        XKCandidate c{};
        strncpy(c.text, completions[i], sizeof(c.text) - 1);
        c.edit_distance = dist;

        int freq = xk_dict_lookup(dict, completions[i]);
        float freq_weight = freq > 0 ? std::log10(static_cast<float>(freq)) / 6.0f : 0.1f;
        float dist_penalty = 1.0f - (static_cast<float>(dist) / (max_dist + 1));
        float ngram_score = ng ? xk_ngram_score(ng, context, ctx_len, completions[i]) : -5.0f;
        float ngram_norm = (ngram_score + 10.0f) / 10.0f;

        c.score = freq_weight * 0.5f + dist_penalty * 0.3f + ngram_norm * 0.2f;

        if (phonetically_similar(word, completions[i])) c.score += 0.1f;

        c.type = (dist == 0) ? 0 : 1;
        candidates.push_back({c});
        free(completions[i]);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Scored& a, const Scored& b) { return a.c.score > b.c.score; });

    int count = std::min(max_out, static_cast<int>(candidates.size()));
    for (int i = 0; i < count; ++i) out[i] = candidates[i].c;
    return count;
}

} /* extern "C" */
