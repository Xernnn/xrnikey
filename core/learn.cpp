/* Personal words: what the owner types, learned on the device and nowhere
 * else, so names, slang and their own phrasing win after a couple of uses.
 *
 * Only counts are kept, of words and of word pairs (the word before and the
 * word), never sentences. Scoring (score.cpp) mixes the pairs in on top of
 * the language models:
 *
 *   P(word | prev) = (1 - b) * P_models + b * P_pair(word | prev)
 *
 * where b grows with how often `prev` was typed (b = n / (n + 2)). A pair
 * counts once typed twice: each count has 1 taken off before use, so one
 * slip teaches nothing. Learning is by pair, after the word typed before:
 * a word learned on its own ("bốt") would be pushed everywhere, and made
 * "my boots" Vietnamese with the real models. The single words' counts are
 * kept for listing and completing the owner's words.
 *
 * The file is plain text, one entry per line: "word<TAB>count" (the laptop's
 * user-dict.txt has the same shape) or "word word<TAB>count" for a pair.
 * It is written whole to a temporary file and renamed, so a crash never
 * leaves half of it. */
#include "learn.h"

#include "score.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace xrnikey {

namespace {

constexpr size_t kMaxWords = 50000;
constexpr size_t kMaxPairs = 200000;
constexpr double kPairWeight = 2.0;   /* b = n / (n + kPairWeight) */

double used(double count) { return std::max(count - 1.0, 0.0); }

std::string pair_key(const std::string& prev, const std::string& word) { return prev + " " + word; }

/* A token worth learning: a word, not a number, pause or unknown. */
bool learnable(const std::string& t) {
    return !t.empty() && t[0] != '<' && t.size() <= 64;
}

/* Over its cap, a table drops its rarest entries, the ones a typo or a
 * one-off left, until it fits. */
void trim(std::unordered_map<std::string, double>& table, size_t cap) {
    double floor = 1.0;
    while (table.size() > cap) {
        for (auto it = table.begin(); it != table.end();)
            it = it->second <= floor ? table.erase(it) : std::next(it);
        floor += 1.0;
    }
}

}  // namespace

void User::add_word(const std::string& word, double n) {
    words[word] += n;
    total += n;
    if (words.size() > kMaxWords) {
        trim(words, kMaxWords);
        recount();
    }
}

void User::add_pair(const std::string& prev, const std::string& word, double n) {
    pairs[pair_key(prev, word)] += n;
    left[prev] += n;
    if (pairs.size() > kMaxPairs) {
        trim(pairs, kMaxPairs);
        recount();
    }
}

void User::recount() {
    total = 0;
    for (const auto& [w, c] : words) total += c;
    left.clear();
    for (const auto& [k, c] : pairs) left[k.substr(0, k.find(' '))] += c;
}

double User::blend(double p_models, const std::string& prev, const std::string& word) const {
    double p = p_models;
    auto l = left.find(prev);
    if (l != left.end() && l->second > 0) {
        auto it = pairs.find(pair_key(prev, word));
        const double p_pair = it == pairs.end() ? 0.0 : used(it->second) / l->second;
        const double b = l->second / (l->second + kPairWeight);
        p = (1.0 - b) * p + b * p_pair;
    }
    return p;
}

void learn_word(User& u, const Context& c, const std::string& text) {
    const std::string w = context_token(text);
    if (!learnable(w)) return;
    u.add_word(w, 1.0);
    const std::string prev = !c.tokens.empty() ? c.tokens.back() : c.start ? "<s>" : "";
    if (!prev.empty() && prev != "<unk>") u.add_pair(prev, w, 1.0);
}

}  // namespace xrnikey

using xrnikey::User;

extern "C" {

XKUser* xk_user_new(void) { return reinterpret_cast<XKUser*>(new User); }

void xk_user_free(XKUser* user) { delete reinterpret_cast<User*>(user); }

void xk_user_learn(XKUser* user, const char* prev, const char* word) {
    if (!user || !word) return;
    auto* u = reinterpret_cast<User*>(user);
    const std::string w = xrnikey::context_token(word);
    if (!xrnikey::learnable(w)) return;
    u->add_word(w, 1.0);
    std::string p = prev && *prev ? (std::strcmp(prev, "<s>") == 0 || std::strcmp(prev, "<p>") == 0
                                         ? std::string(prev)
                                         : xrnikey::context_token(prev))
                                  : std::string("<s>");
    if (p == "<unk>") return;
    u->add_pair(p, w, 1.0);
}

double xk_user_count(const XKUser* user, const char* prev, const char* word) {
    if (!user || !word) return 0.0;
    const auto* u = reinterpret_cast<const User*>(user);
    const std::string w = xrnikey::context_token(word);
    if (!prev) {
        auto it = u->words.find(w);
        return it == u->words.end() ? 0.0 : it->second;
    }
    const std::string p = std::strcmp(prev, "<s>") == 0 ? std::string("<s>") : xrnikey::context_token(prev);
    auto it = u->pairs.find(p + " " + w);
    return it == u->pairs.end() ? 0.0 : it->second;
}

void xk_user_forget(XKUser* user, const char* word) {
    if (!user || !word) return;
    auto* u = reinterpret_cast<User*>(user);
    const std::string w = xrnikey::context_token(word);
    u->words.erase(w);
    for (auto it = u->pairs.begin(); it != u->pairs.end();) {
        const size_t sp = it->first.find(' ');
        const bool hit = it->first.compare(0, sp, w) == 0 && sp == w.size();
        const bool hit2 = it->first.compare(sp + 1, std::string::npos, w) == 0;
        it = hit || hit2 ? u->pairs.erase(it) : std::next(it);
    }
    u->recount();
}

void xk_user_decay(XKUser* user, double factor) {
    if (!user || !(factor >= 0.0 && factor <= 1.0)) return;
    auto* u = reinterpret_cast<User*>(user);
    for (auto* table : {&u->words, &u->pairs})
        for (auto it = table->begin(); it != table->end();) {
            it->second *= factor;
            it = it->second < 0.5 ? table->erase(it) : std::next(it);
        }
    u->recount();
}

int xk_user_load(XKUser* user, const char* path) {
    if (!user || !path) return -1;
    std::ifstream f(path);
    if (!f) return -1;
    auto* u = reinterpret_cast<User*>(user);
    User fresh;
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t tab = line.rfind('\t');
        if (tab == std::string::npos || tab == 0) continue;
        const std::string key = line.substr(0, tab);
        const char* num = line.c_str() + tab + 1;
        char* end = nullptr;
        errno = 0;
        const double count = std::strtod(num, &end);
        if (end == num || errno == ERANGE || !(count > 0.0) || count > 1e9) continue;
        const size_t sp = key.find(' ');
        if (sp == std::string::npos) {
            if (!xrnikey::learnable(key)) continue;
            fresh.add_word(key, count);
        } else {
            const std::string prev = key.substr(0, sp), word = key.substr(sp + 1);
            if (word.find(' ') != std::string::npos || !xrnikey::learnable(word) || prev.empty()) continue;
            fresh.add_pair(prev, word, count);
        }
        ++n;
    }
    *u = std::move(fresh);
    return n;
}

int xk_user_save(const XKUser* user, const char* path) {
    if (!user || !path) return -1;
    const auto* u = reinterpret_cast<const User*>(user);
    const std::string tmp = std::string(path) + ".part";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) return -1;
        f << "# xrnkey: words and word pairs learned on this device\n";
        auto put = [&](const std::unordered_map<std::string, double>& table) {
            std::vector<std::pair<std::string, double>> v(table.begin(), table.end());
            std::sort(v.begin(), v.end());
            char buf[32];
            for (const auto& [k, c] : v) {
                std::snprintf(buf, sizeof(buf), "%.6g", c);
                f << k << '\t' << buf << '\n';
            }
        };
        put(u->words);
        put(u->pairs);
        f.flush();
        if (!f) {
            std::remove(tmp.c_str());
            return -1;
        }
    }
    if (std::rename(tmp.c_str(), path) != 0) {
        std::remove(tmp.c_str());
        return -1;
    }
    return 0;
}

void xk_models_set_user(XKModels* models, XKUser* user) {
    if (!models) return;
    reinterpret_cast<xrnikey::Models*>(models)->user = reinterpret_cast<User*>(user);
}

}  /* extern "C" */
