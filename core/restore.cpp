/* Accent restoration: the Vietnamese a word typed without marks stands for
 * ("khong" -> "không"), ranked by the language model in context.
 *
 * Every syllable the model knows, and every attested syllable, is indexed by
 * its ASCII shape (marks and tone removed). A typed word's candidates are its
 * shape's syllables, plus the word as typed when the model knows it as a
 * token of its own (an English word, an abbreviation). One word is ranked by
 * log P(candidate | context); a run of words is decoded as a whole with a
 * small beam search, so a later word can change an earlier one ("co gang":
 * "có" alone, "cố gắng" together). */
#include "lm_internal.h"
#include "xrnikey_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrnikey {

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
};

/* The candidates for one typed word (lowercase), best spellings first in
 * no particular order: the shape's syllables, and the word itself when it
 * is not one of them and the model knows it, or nothing else is left. */
std::vector<Cand> candidates(const Lm& m, const std::string& lower, std::string* own) {
    std::vector<Cand> out;
    const ShapeIndex& idx = shapes(m);
    auto it = idx.by_shape.find(lower);
    bool typed_listed = false;
    if (it != idx.by_shape.end()) {
        for (const auto& e : it->second) {
            out.push_back({&e.text, e.id, e.text == lower});
            if (e.text == lower) typed_listed = true;
        }
    }
    if (!typed_listed) {
        const uint32_t id = lm_id(m, lower);
        if (id != kUnk || out.empty()) {
            *own = lower;
            out.push_back({own, id, true});
        }
    }
    return out;
}

/* log10 P(candidate | context): a syllable the model lacks gets its share
 * of the unknown mass, not all of it. */
float score(const Lm& m, const uint32_t* ctx, int n, uint32_t id) {
    const float p = lm_logprob(m, ctx, n, id);
    return id == kUnk ? p - lm_unk_share(m) : p;
}

std::vector<uint32_t> context_ids(const Lm& m, const char* const* ctx, int n) {
    std::vector<uint32_t> ids;
    if (!ctx || n <= 0) return ids;
    const int keep = std::min(n, static_cast<int>(m.hdr->order) - 1);
    for (int i = n - keep; i < n; ++i) ids.push_back(ctx[i] ? lm_id(m, ctx[i]) : kUnk);
    return ids;
}

}  // namespace

}  // namespace xrnikey

using namespace xrnikey;

extern "C" {

int xk_restore_word(const XKLm* lm, const char* const* context, int ctx_len,
                    const char* typed, XKCandidate* out, int max_out) {
    if (!lm || !out || max_out <= 0) return 0;
    std::string lower;
    Case cs;
    if (!typed_word(typed, &lower, &cs)) return 0;
    const Lm& m = lm_of(lm);
    const std::vector<uint32_t> ctx = context_ids(m, context, ctx_len);
    std::string own;
    std::vector<Cand> cands = candidates(m, lower, &own);
    struct Scored { Cand c; float score; };
    std::vector<Scored> scored;
    for (const Cand& c : cands)
        scored.push_back({c, score(m, ctx.data(), static_cast<int>(ctx.size()), c.id)});
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
        o.edit_distance = 0;
        o.type = scored[static_cast<size_t>(i)].c.as_typed ? XK_CAND_AS_TYPED : XK_CAND_RESTORED;
    }
    return n;
}

int xk_restore_run(const XKLm* lm, const char* const* context, int ctx_len,
                   const char* const* typed, int n, const char* next, char** out) {
    if (!lm || !typed || !out || n <= 0 || n > XK_RESTORE_MAX_RUN) return 0;
    const Lm& m = lm_of(lm);
    const int order = static_cast<int>(m.hdr->order);
    std::vector<std::string> lowers(static_cast<size_t>(n)), owns(static_cast<size_t>(n));
    std::vector<Case> cases(static_cast<size_t>(n));
    std::vector<std::vector<Cand>> cands(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (!typed_word(typed[i], &lowers[static_cast<size_t>(i)], &cases[static_cast<size_t>(i)])) return 0;
        cands[static_cast<size_t>(i)] = candidates(m, lowers[static_cast<size_t>(i)], &owns[static_cast<size_t>(i)]);
    }
    /* Beam search. A hypothesis is its last order-1 ids (what the model
     * can still see) plus the choices that led there; two with the same
     * history are merged, keeping the better. */
    struct Hyp {
        std::vector<uint32_t> hist;
        std::vector<int> pick;
        double score;
    };
    constexpr size_t kBeam = 16;
    std::vector<Hyp> beam{{context_ids(m, context, ctx_len), {}, 0.0}};
    for (int i = 0; i < n; ++i) {
        std::vector<Hyp> next;
        for (const Hyp& h : beam) {
            for (size_t c = 0; c < cands[static_cast<size_t>(i)].size(); ++c) {
                const Cand& cand = cands[static_cast<size_t>(i)][c];
                Hyp g = h;
                g.score += score(m, h.hist.data(), static_cast<int>(h.hist.size()), cand.id);
                g.hist.push_back(cand.id);
                if (static_cast<int>(g.hist.size()) > order - 1) g.hist.erase(g.hist.begin());
                g.pick.push_back(static_cast<int>(c));
                next.push_back(std::move(g));
            }
        }
        std::sort(next.begin(), next.end(), [](const Hyp& a, const Hyp& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.pick < b.pick;
        });
        beam.clear();
        for (Hyp& h : next) {
            bool merged = false;
            for (const Hyp& kept : beam)
                if (kept.hist == h.hist) { merged = true; break; }
            if (merged) continue;
            beam.push_back(std::move(h));
            if (beam.size() == kBeam) break;
        }
    }
    /* What comes next, when the caller knows it (the sentence ends, a
     * pause): each hypothesis is scored on how well it leads there. */
    if (next && *next) {
        const uint32_t nid = lm_id(m, next);
        for (Hyp& h : beam) h.score += score(m, h.hist.data(), static_cast<int>(h.hist.size()), nid);
        std::stable_sort(beam.begin(), beam.end(), [](const Hyp& a, const Hyp& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.pick < b.pick;
        });
    }
    const Hyp& best = beam.front();
    for (int i = 0; i < n; ++i) {
        const Cand& c = cands[static_cast<size_t>(i)][static_cast<size_t>(best.pick[static_cast<size_t>(i)])];
        const std::string t = with_case(*c.text, cases[static_cast<size_t>(i)]);
        out[i] = static_cast<char*>(std::malloc(t.size() + 1));
        if (out[i]) std::memcpy(out[i], t.c_str(), t.size() + 1);
    }
    return n;
}

}  /* extern "C" */
