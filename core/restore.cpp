/* Accent restoration: the Vietnamese a word typed without marks stands for
 * ("khong" -> "không"), ranked by the language model in context.
 *
 * Every syllable the Vietnamese model knows, and every attested syllable, is
 * indexed by its ASCII shape (marks and tone removed). A typed word's
 * candidates are its shape's syllables, plus the word as typed when either
 * model knows it as a token of its own (an English word, an abbreviation).
 * Each is scored by the same mixture as the English/Vietnamese choice
 * (score.cpp), so "can" after an English word stays "can". One word is
 * ranked alone; a run of words is decoded as a whole with a small beam
 * search, so a later word can change an earlier one ("co gang": "có"
 * alone, "cố gắng" together).
 *
 * With correction on, a word may also stand for one a key or two away
 * ("khonh" -> "không"): the candidates of each such spelling join in,
 * each scored down by how unlikely the slip is (keys.cpp), so a correction
 * wins only when the words around make it worth that. */
#include "keys.h"
#include "lm_internal.h"
#include "score.h"
#include "xrnkey_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrnkey {

using lmfmt::kUnk;

struct ShapeIndex {
    struct Entry {
        std::string text;
        uint32_t id;  /* kUnk for an attested syllable the model lacks */
    };
    std::unordered_map<std::string, std::vector<Entry>> by_shape;
};

void shape_index_free(ShapeIndex* s) { delete s; }

namespace {

char ascii_of(char32_t c) {
    switch (c) {
        case U'â': case U'ă': return 'a';
        case U'ê': return 'e';
        case U'ô': case U'ơ': return 'o';
        case U'ư': return 'u';
        case U'đ': return 'd';
        default: return c < 0x80 ? static_cast<char>(c) : '\0';
    }
}

/* The ASCII shape of a syllable, or "" if `word` is not one. */
std::string shape_of(const std::string& word) {
    std::u32string letters;
    int tone;
    if (!syllable_split(word, &letters, &tone) || !syllable_valid(letters, tone)) return "";
    std::string out;
    for (char32_t c : letters) {
        const char a = ascii_of(c);
        if (!a) return "";
        out += a;
    }
    return out;
}

const ShapeIndex& shapes(const Lm& m) {
    std::call_once(m.shapes_once, [&] {
        auto* idx = new ShapeIndex;
        for (uint32_t id = 3; id < m.hdr->vocab; ++id) {
            const std::string w = lm_word(m, id);
            const std::string s = shape_of(w);
            if (!s.empty()) idx->by_shape[s].push_back({w, id});
        }
        for (const std::string& w : lexicon_attested_words()) {
            if (lm_id(m, w) != kUnk) continue;
            const std::string s = shape_of(w);
            if (!s.empty()) idx->by_shape[s].push_back({w, kUnk});
        }
        m.shapes = idx;
    });
    return *m.shapes;
}

enum class Case { Lower, Capital, Upper };

/* How the word was typed: "khong", "Khong", "KHONG". Only ASCII letters
 * count; anything else means it is not a word restoration handles. */
bool typed_word(const char* typed, std::string* lower, Case* cs) {
    if (!typed || !*typed) return false;
    size_t n = 0, upper = 0;
    lower->clear();
    for (const char* p = typed; *p; ++p) {
        const char c = *p;
        if (c >= 'a' && c <= 'z') lower->push_back(c);
        else if (c >= 'A' && c <= 'Z') {
            lower->push_back(static_cast<char>(c | 0x20));
            ++upper;
        } else return false;
        ++n;
    }
    if (n > 31) return false;
    *cs = upper == 0 ? Case::Lower
        : (upper == n && n > 1) ? Case::Upper
        : (typed[0] >= 'A' && typed[0] <= 'Z') ? Case::Capital
        : Case::Lower;
    return true;
}

std::string with_case(const std::string& text, Case cs) {
    if (cs == Case::Lower) return text;
    std::string out;
    char buf[8];
    bool first = true;
    for (size_t i = 0; i < text.size();) {
        uint32_t cp;
        int len;
        utf8_decode(text.c_str() + i, &cp, &len);
        char32_t c = static_cast<char32_t>(cp);
        if (cs == Case::Upper || first) c = vn_upper(c);
        first = false;
        const int k = utf8_encode(static_cast<uint32_t>(c), buf);
        out.append(buf, static_cast<size_t>(k));
        i += static_cast<size_t>(len);
    }
    return out;
}

struct Cand {
    const std::string* text;
    uint32_t id;
    bool as_typed;
    float penalty;  /* log10 cost of the slips that make it this (0: none) */
    int slips;
};

/* Words never "corrected": chat that is no dictionary's but is meant. */
bool keep_as_typed(const std::string& w) {
    static const char* const kChat[] = {"ko", "k", "dc", "vs", "ns", "mn", "nx", "ck", "vk", "bt",
                                        "ny", "cx", "ak", "ok", "oke", "okie", "hihi", "haha",
                                        "hehe", "hic", "huhu", "kkk", "thx", "pls", "plz", "lol",
                                        "omg", "ntn", "bth"};
    for (const char* c : kChat)
        if (w == c) return true;
    return false;
}

/* The spellings at most `max_slips` keys away from `typed`, with the cost
 * of those slips. */
void spellings_near(const std::string& typed, int max_slips,
                    std::vector<std::pair<std::string, float>>& out) {
    for (size_t i = 0; i < typed.size(); ++i) {
        for (char meant : keys_near(typed[i])) {
            std::string v = typed;
            v[i] = meant;
            const double cost = typo_cost(meant, typed[i]);
            out.emplace_back(v, static_cast<float>(cost));
            if (max_slips < 2) continue;
            for (size_t j = i + 1; j < typed.size(); ++j)
                for (char meant2 : keys_near(typed[j])) {
                    std::string v2 = v;
                    v2[j] = meant2;
                    out.emplace_back(v2, static_cast<float>(cost + typo_cost(meant2, typed[j])));
                }
        }
    }
}

/* The candidates for one spelling (lowercase): its shape's syllables, and
 * the spelling itself when a model knows it as a token. */
void add_spelling(const Models& models, const std::string& spelling, float penalty, int slips,
                  bool typed, std::deque<std::string>& store, std::vector<Cand>& out) {
    const Lm& m = *models.vi;
    const ShapeIndex& idx = shapes(m);
    auto it = idx.by_shape.find(spelling);
    bool listed = false;
    if (it != idx.by_shape.end()) {
        for (const auto& e : it->second) {
            out.push_back({&e.text, e.id, typed && e.text == spelling, penalty, slips});
            if (e.text == spelling) listed = true;
        }
    }
    if (listed) return;
    const uint32_t id = lm_id(m, spelling);
    const bool english = models.en && lm_id(*models.en, spelling) != kUnk;
    /* The word as typed always stays on offer when nothing else fits. */
    if (id != kUnk || english || (typed && out.empty())) {
        store.push_back(spelling);
        out.push_back({&store.back(), id, typed, penalty, slips});
    }
}

/* The candidates for one typed word (lowercase), in no particular order:
 * the shape's syllables, and the word itself when it is not one of them
 * and a model knows it, or nothing else is left; with correction on, the
 * same for the spellings a key or two away, each with its cost. */
std::vector<Cand> candidates(const Models& models, const std::string& lower, std::deque<std::string>& store) {
    std::vector<Cand> out;
    add_spelling(models, lower, 0.0f, 0, true, store, out);
    /* Two-letter words too: most of their slips make another word ("la" for
     * "ka"), and only the words around can tell. Leaving them out, or
     * correcting only the ones that are no word, fixed 81 % of the slips
     * in held-out text (dev); with them, 91 %, still changing 0.24 % of
     * correct words. */
    if (models.correction <= 0 || lower.size() < 2 || keep_as_typed(lower)) return out;
    const size_t own = out.size();
    std::vector<std::pair<std::string, float>> near;
    const size_t two_from = models.correction >= XK_CORRECT_STRONG ? 4 : 5;
    spellings_near(lower, lower.size() >= two_from ? 2 : 1, near);
    std::vector<Cand> fixes;
    for (const auto& [spelling, cost] : near) {
        const int slips = static_cast<int>(std::count_if(spelling.begin(), spelling.end(), [&, i = size_t{0}](char c) mutable {
            return c != lower[i++];
        }));
        add_spelling(models, spelling, cost, slips, false, store, fixes);
    }
    /* Keep the likeliest corrections only, by how common each word is and
     * what its slips cost, so a long word's many spellings stay cheap. */
    constexpr size_t kMaxFixes = 24;
    if (fixes.size() > kMaxFixes) {
        const Lm& m = *models.vi;
        auto prior = [&](const Cand& c) { return lm_prob_at(m, 1, c.id < m.hdr->vocab ? c.id : kUnk) + c.penalty; };
        std::partial_sort(fixes.begin(), fixes.begin() + kMaxFixes, fixes.end(),
                          [&](const Cand& a, const Cand& b) { return prior(a) > prior(b); });
        fixes.resize(kMaxFixes);
    }
    for (const Cand& f : fixes) {
        bool dup = false;
        for (size_t i = 0; i < own; ++i)
            if (*out[i].text == *f.text) { dup = true; break; }
        if (!dup) out.push_back(f);
    }
    return out;
}

}  // namespace

}  // namespace xrnkey

using namespace xrnkey;

extern "C" {

int xk_restore_word(const XKModels* models, const char* const* context, int ctx_len,
                    const char* typed, XKCandidate* out, int max_out) {
    const auto* mp = reinterpret_cast<const Models*>(models);
    if (!mp || !mp->vi || !out || max_out <= 0) return 0;
    std::string lower;
    Case cs;
    if (!typed_word(typed, &lower, &cs)) return 0;
    const Context ctx = context_from(mp, context, ctx_len, false);
    std::deque<std::string> store;
    std::vector<Cand> cands = candidates(*mp, lower, store);
    struct Scored { Cand c; float score; };
    std::vector<Scored> scored;
    for (const Cand& c : cands)
        scored.push_back({c, static_cast<float>(mix_logprob(*mp, ctx, *c.text) + c.penalty)});
    std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.c.as_typed != b.c.as_typed) return a.c.as_typed;
        return *a.c.text < *b.c.text;
    });
    const int n = std::min(max_out, static_cast<int>(scored.size()));
    for (int i = 0; i < n; ++i) {
        XKCandidate& o = out[i];
        std::memset(&o, 0, sizeof(o));
        const std::string t = with_case(*scored[static_cast<size_t>(i)].c.text, cs);
        std::strncpy(o.text, t.c_str(), sizeof(o.text) - 1);
        o.score = scored[static_cast<size_t>(i)].score;
        const Cand& c = scored[static_cast<size_t>(i)].c;
        o.edit_distance = c.slips;
        o.type = c.slips ? XK_CAND_CORRECTED : c.as_typed ? XK_CAND_AS_TYPED : XK_CAND_RESTORED;
    }
    return n;
}

int xk_restore_run(const XKModels* models, const char* const* context, int ctx_len,
                   const char* const* typed, int n, const char* next, char** out) {
    const auto* mp = reinterpret_cast<const Models*>(models);
    if (!mp || !mp->vi || !typed || !out || n <= 0 || n > XK_RESTORE_MAX_RUN) return 0;
    std::vector<std::string> lowers(static_cast<size_t>(n));
    std::deque<std::string> store;
    std::vector<Case> cases(static_cast<size_t>(n));
    std::vector<std::vector<Cand>> cands(static_cast<size_t>(n));
    for (size_t i = 0; i < static_cast<size_t>(n); ++i) {
        if (!typed_word(typed[i], &lowers[i], &cases[i])) return 0;
        cands[i] = candidates(*mp, lowers[i], store);
    }
    /* Beam search. A hypothesis is the context it leaves (the last words
     * and their languages: all the models can still see) plus the choices
     * that led there; two that leave the same context are merged, keeping
     * the better. */
    struct Hyp {
        Context ctx;
        std::vector<int> pick;
        double score;
    };
    constexpr size_t kBeam = 16;
    std::vector<Hyp> beam{{context_from(mp, context, ctx_len, false), {}, 0.0}};
    auto better = [](const Hyp& a, const Hyp& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.pick < b.pick;
    };
    for (size_t i = 0; i < static_cast<size_t>(n); ++i) {
        std::vector<Hyp> next_beam;
        for (const Hyp& h : beam) {
            for (size_t c = 0; c < cands[i].size(); ++c) {
                const std::string& text = *cands[i][c].text;
                uint8_t lang = LANG_VI;
                Hyp g = h;
                g.score += mix_logprob(*mp, h.ctx, text, &lang) + cands[i][c].penalty;
                g.ctx.push(text, lang);
                g.pick.push_back(static_cast<int>(c));
                next_beam.push_back(std::move(g));
            }
        }
        std::sort(next_beam.begin(), next_beam.end(), better);
        beam.clear();
        for (Hyp& h : next_beam) {
            bool merged = false;
            for (const Hyp& kept : beam)
                if (kept.ctx.tokens == h.ctx.tokens && kept.ctx.langs == h.ctx.langs &&
                    kept.ctx.start == h.ctx.start) {
                    merged = true;
                    break;
                }
            if (merged) continue;
            beam.push_back(std::move(h));
            if (beam.size() == kBeam) break;
        }
    }
    /* What comes next, when the caller knows it (the sentence ends, a
     * pause): each hypothesis is scored on how well it leads there. */
    if (next && *next) {
        for (Hyp& h : beam) h.score += mix_logprob(*mp, h.ctx, next);
        std::stable_sort(beam.begin(), beam.end(), better);
    }
    const Hyp& best = beam.front();
    for (size_t i = 0; i < static_cast<size_t>(n); ++i) {
        const Cand& c = cands[i][static_cast<size_t>(best.pick[i])];
        const std::string t = with_case(*c.text, cases[i]);
        out[i] = static_cast<char*>(std::malloc(t.size() + 1));
        if (out[i]) std::memcpy(out[i], t.c_str(), t.size() + 1);
    }
    return n;
}

}  /* extern "C" */
