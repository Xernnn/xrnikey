/* xk-lm-build: train a backoff n-gram model and write it as .xklm.
 *
 * usage: xk-lm-build [options] -o OUT.xklm CORPUS...
 *   --order N        n-gram order, 1..4 (default 4)
 *   --min-count C    a word needs C occurrences to enter the vocabulary (default 2)
 *   --max-vocab V    at most V words, specials included (default 65536)
 *   --prune a,b,c    drop n-grams of order 2, 3, 4 seen this many times or fewer
 *                    (default 0,1,1); each must be >= the one before
 *   --no-shape-index leave out the shape index (an English model needs none)
 *
 * Each CORPUS line is one sentence of tokens separated by spaces, already
 * normalised (data/lm/build.py writes them). Smoothing is interpolated
 * modified Kneser-Ney (Chen & Goodman), computed on the full counts; pruned
 * n-grams are then dropped and every context's backoff weight renormalised
 * so each distribution still sums to one.
 *
 * The model also carries the shape index accent restoration reads: every
 * ASCII shape with the syllables it may stand for, the vocabulary's and the
 * attested ones it lacks, worked out by the core itself (linked in).
 *
 * Counting packs an n-gram's 16-bit ids into one 64-bit key, whose numeric
 * order is the n-gram's lexicographic order, and sorts the keys: memory is
 * 8 bytes per running token for the order being counted, plus the unique
 * n-grams kept. */
#include "lm_format.h"
#include "xrnkey_internal.h"  /* syllable_shape, lexicon_attested_words: the core's own */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace xrnkey::lmfmt;

namespace {

struct Options {
    int order = 4;
    uint64_t min_count = 2;
    uint32_t max_vocab = kMaxVocab;
    std::array<uint32_t, kMaxOrder + 1> prune{0, 0, 0, 1, 1, 1};
    bool shape_index = true;
    std::string out;
    std::vector<std::string> corpora;
};

[[noreturn]] void die(const std::string& msg) {
    std::cerr << "xk-lm-build: " << msg << "\n";
    std::exit(1);
}

template <typename F>
void each_sentence(const Options& o, F f) {
    std::vector<std::string_view> toks;
    std::string line;
    for (const auto& path : o.corpora) {
        std::ifstream in(path);
        if (!in) die("cannot read " + path);
        while (std::getline(in, line)) {
            toks.clear();
            size_t i = 0;
            while (i < line.size()) {
                while (i < line.size() && line[i] == ' ') ++i;
                size_t j = i;
                while (j < line.size() && line[j] != ' ') ++j;
                if (j > i) toks.emplace_back(line.data() + i, j - i);
                i = j;
            }
            if (!toks.empty()) f(toks);
        }
    }
}

inline uint64_t first_word(uint64_t key, int k) { return key >> (16 * (k - 1)); }
inline uint64_t prefix(uint64_t key) { return key >> 16; }
inline uint64_t suffix(uint64_t key, int k) {
    return k == 1 ? 0 : key & ((uint64_t{1} << (16 * (k - 1))) - 1);
}
inline uint32_t last_word(uint64_t key) { return static_cast<uint32_t>(key & 0xFFFF); }

struct Discount {
    double d[4] = {0, 0.5, 1.0, 1.5};  /* d[1], d[2], d[3+] */
    double of(uint64_t a) const { return a == 0 ? 0 : d[std::min<uint64_t>(a, 3)]; }
};

Discount discount_from(const std::vector<uint32_t>& adjusted) {
    uint64_t n[5] = {0, 0, 0, 0, 0};
    for (uint32_t a : adjusted)
        if (a >= 1 && a <= 4) ++n[a];
    Discount D;
    if (n[1] == 0 || n[2] == 0 || n[3] == 0 || n[4] == 0) return D;  /* tiny corpus */
    const double Y = double(n[1]) / (double(n[1]) + 2.0 * double(n[2]));
    D.d[1] = 1.0 - 2.0 * Y * double(n[2]) / double(n[1]);
    D.d[2] = 2.0 - 3.0 * Y * double(n[3]) / double(n[2]);
    D.d[3] = 3.0 - 4.0 * Y * double(n[4]) / double(n[3]);
    for (int i = 1; i <= 3; ++i)
        if (!(D.d[i] > 0.0) || D.d[i] > i) D.d[i] = std::min(0.5 * i, double(i));
    return D;
}

/* What smoothing needs to know about one context: the sum of its
 * children's adjusted counts, and gamma, the mass it hands to the
 * lower order. */
struct CtxStat {
    uint64_t key;
    double denom;
    double gamma;
};

struct Order {
    std::vector<uint64_t> key;      /* kept n-grams, sorted */
    std::vector<uint32_t> adjusted; /* their adjusted counts */
    std::vector<float> prob;        /* log10, filled by probabilities() */
    std::vector<float> bo;          /* log10 */
    std::vector<CtxStat> ctx;       /* contexts (order k-1) with kept children */
    Discount D;
};

int64_t find_key(const std::vector<uint64_t>& keys, uint64_t key) {
    auto it = std::lower_bound(keys.begin(), keys.end(), key);
    if (it == keys.end() || *it != key) return -1;
    return it - keys.begin();
}

struct Model {
    Options opt;
    std::vector<std::string> words;          /* by id */
    uint64_t unk_types = 0;                  /* corpus word types left out of `words` */
    std::unordered_map<std::string, uint32_t> id_of;
    std::vector<uint16_t> stream;            /* <s> w.. </s> <s> w.. </s> ... */
    std::vector<Order> ord;                  /* 1..order */
};

void build_vocab(Model& m) {
    std::unordered_map<std::string, uint64_t> counts;
    each_sentence(m.opt, [&](const std::vector<std::string_view>& toks) {
        for (auto t : toks) ++counts[std::string(t)];
    });
    counts.erase("<s>");
    counts.erase("</s>");
    counts.erase("<unk>");
    std::vector<std::pair<std::string, uint64_t>> v;
    for (auto& [w, c] : counts)
        if (c >= m.opt.min_count) v.emplace_back(w, c);
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    m.words = {"<unk>", "<s>", "</s>"};
    for (auto& [w, c] : v) {
        if (m.words.size() >= m.opt.max_vocab) break;
        m.words.push_back(w);
    }
    for (uint32_t i = 0; i < m.words.size(); ++i) m.id_of[m.words[i]] = i;
    m.unk_types = counts.size() - (m.words.size() - 3);
    std::cerr << "vocab " << m.words.size() << " of " << counts.size() + 3 << " types\n";
}

void build_stream(Model& m) {
    uint64_t oov = 0;
    each_sentence(m.opt, [&](const std::vector<std::string_view>& toks) {
        m.stream.push_back(static_cast<uint16_t>(kBos));
        for (auto t : toks) {
            auto it = m.id_of.find(std::string(t));
            uint32_t id = it == m.id_of.end() ? kUnk : it->second;
            if (id == kUnk) ++oov;
            m.stream.push_back(static_cast<uint16_t>(id));
        }
        m.stream.push_back(static_cast<uint16_t>(kEos));
    });
    std::cerr << "tokens " << m.stream.size() << " (oov " << oov << ")\n";
}

/* Every n-gram of order k in the stream that lies inside one sentence,
 * counted: unique keys, sorted, and their counts. */
void count_order(const Model& m, int k, std::vector<uint64_t>& keys, std::vector<uint32_t>& counts) {
    std::vector<uint64_t> all;
    all.reserve(m.stream.size());
    const auto& s = m.stream;
    size_t start = 0;  /* index of the current sentence's <s> */
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == kBos) start = i;
        if (i + 1 < start + static_cast<size_t>(k)) continue;  /* window would leave the sentence */
        if (k == 1 && s[i] == kBos) continue;
        uint64_t key = 0;
        for (int j = k - 1; j >= 0; --j) key = (key << 16) | s[i - static_cast<size_t>(j)];
        all.push_back(key);
    }
    std::sort(all.begin(), all.end());
    keys.clear();
    counts.clear();
    for (size_t i = 0; i < all.size();) {
        size_t j = i;
        while (j < all.size() && all[j] == all[i]) ++j;
        keys.push_back(all[i]);
        counts.push_back(static_cast<uint32_t>(std::min<size_t>(j - i, UINT32_MAX)));
        i = j;
    }
}

void count_and_smooth(Model& m) {
    const int N = m.opt.order;
    m.ord.assign(static_cast<size_t>(N) + 1, Order{});
    std::vector<uint64_t> cont_key;    /* from order k+1: suffixes and how many left extensions */
    std::vector<uint32_t> cont_count;
    for (int k = N; k >= 1; --k) {
        std::vector<uint64_t> keys;
        std::vector<uint32_t> raw;
        count_order(m, k, keys, raw);
        /* Adjusted counts: raw at the top order and for n-grams that start
         * a sentence; otherwise the number of distinct words seen before. */
        std::vector<uint32_t> adj(raw);
        if (k < N) {
            size_t c = 0;
            for (size_t i = 0; i < keys.size(); ++i) {
                if (first_word(keys[i], k) == kBos) continue;
                while (c < cont_key.size() && cont_key[c] < keys[i]) ++c;
                adj[i] = (c < cont_key.size() && cont_key[c] == keys[i]) ? cont_count[c] : 0;
            }
        }
        Order& o = m.ord[static_cast<size_t>(k)];
        o.D = discount_from(adj);
        /* Context statistics, over every child (pruned or not). */
        const uint32_t cut = m.opt.prune[static_cast<size_t>(k)];
        for (size_t i = 0; i < keys.size();) {
            const uint64_t h = k == 1 ? 0 : prefix(keys[i]);
            size_t j = i;
            double denom = 0;
            uint64_t n[4] = {0, 0, 0, 0};
            bool kept_child = false;
            while (j < keys.size() && (k == 1 || prefix(keys[j]) == h)) {
                denom += adj[j];
                if (adj[j] >= 1) ++n[std::min<uint32_t>(adj[j], 3)];
                if (k == 1 || raw[j] > cut) kept_child = true;
                ++j;
            }
            if (kept_child && denom > 0) {
                const double g = (o.D.d[1] * n[1] + o.D.d[2] * n[2] + o.D.d[3] * n[3]) / denom;
                o.ctx.push_back({h, denom, g});
            }
            i = j;
        }
        /* Continuation counts for the order below. */
        if (k >= 2) {
            std::vector<uint64_t> suf(keys.size());
            for (size_t i = 0; i < keys.size(); ++i) suf[i] = suffix(keys[i], k);
            std::sort(suf.begin(), suf.end());
            cont_key.clear();
            cont_count.clear();
            for (size_t i = 0; i < suf.size();) {
                size_t j = i;
                while (j < suf.size() && suf[j] == suf[i]) ++j;
                cont_key.push_back(suf[i]);
                cont_count.push_back(static_cast<uint32_t>(j - i));
                i = j;
            }
        }
        /* Keep what survives pruning. Unigrams are kept by id, all of them. */
        if (k == 1) {
            o.key.resize(m.words.size());
            o.adjusted.assign(m.words.size(), 0);
            for (uint32_t id = 0; id < m.words.size(); ++id) o.key[id] = id;
            for (size_t i = 0; i < keys.size(); ++i) o.adjusted[keys[i]] = adj[i];
        } else {
            for (size_t i = 0; i < keys.size(); ++i) {
                if (raw[i] <= cut) continue;
                o.key.push_back(keys[i]);
                o.adjusted.push_back(adj[i]);
            }
        }
        std::cerr << "order " << k << ": " << keys.size() << " seen, " << o.key.size()
                  << " kept; D = " << o.D.d[1] << " " << o.D.d[2] << " " << o.D.d[3] << "\n";
    }
}

const CtxStat* stat_of(const Order& o, uint64_t h) {
    auto it = std::lower_bound(o.ctx.begin(), o.ctx.end(), h,
                               [](const CtxStat& s, uint64_t v) { return s.key < v; });
    if (it == o.ctx.end() || it->key != h) return nullptr;
    return &*it;
}

void probabilities(Model& m) {
    const int N = m.opt.order;
    const double V = double(m.words.size() - 1);  /* <s> is never predicted */
    std::vector<double> p_lin;  /* linear probabilities of the order below */
    {
        Order& o = m.ord[1];
        const CtxStat* s = stat_of(o, 0);
        const double denom = s ? s->denom : 1.0, gamma = s ? s->gamma : 1.0;
        p_lin.assign(m.words.size(), 0.0);
        o.prob.assign(m.words.size(), kNever);
        for (uint32_t id = 0; id < m.words.size(); ++id) {
            if (id == kBos) continue;
            const double a = o.adjusted[id];
            const double p = std::max(a - o.D.of(o.adjusted[id]), 0.0) / denom + gamma / V;
            p_lin[id] = p;
            o.prob[id] = static_cast<float>(std::log10(p));
        }
    }
    for (int k = 2; k <= N; ++k) {
        Order& o = m.ord[static_cast<size_t>(k)];
        Order& below = m.ord[static_cast<size_t>(k - 1)];
        std::vector<double> p_here(o.key.size());
        std::vector<double> sum_p(below.key.size(), 0.0), sum_lower(below.key.size(), 0.0);
        std::vector<bool> keep(o.key.size(), true);
        for (size_t i = 0; i < o.key.size(); ++i) {
            const uint64_t key = o.key[i];
            const int64_t h = k == 2 ? int64_t(first_word(key, 2)) : find_key(below.key, prefix(key));
            const int64_t lower = k == 2 ? int64_t(last_word(key)) : find_key(below.key, suffix(key, k));
            const CtxStat* st = stat_of(o, prefix(key));
            if (h < 0 || lower < 0 || !st) {  /* cannot happen with non-decreasing pruning */
                keep[i] = false;
                continue;
            }
            const double pl = p_lin[static_cast<size_t>(lower)];
            const double a = o.adjusted[i];
            const double p = std::max(a - o.D.of(o.adjusted[i]), 0.0) / st->denom + st->gamma * pl;
            p_here[i] = p;
            sum_p[static_cast<size_t>(h)] += p;
            sum_lower[static_cast<size_t>(h)] += pl;
        }
        below.bo.assign(below.key.size(), 0.0f);
        for (size_t h = 0; h < below.key.size(); ++h) {
            if (sum_p[h] == 0.0) continue;  /* no kept children: back off unchanged */
            const double num = std::max(1.0 - sum_p[h], 1e-12);
            const double den = std::max(1.0 - sum_lower[h], 1e-12);
            below.bo[h] = static_cast<float>(std::log10(num / den));
        }
        /* Drop what could not be placed, keeping the arrays parallel. */
        size_t w = 0;
        for (size_t i = 0; i < o.key.size(); ++i) {
            if (!keep[i]) continue;
            o.key[w] = o.key[i];
            o.adjusted[w] = o.adjusted[i];
            p_here[w] = p_here[i];
            ++w;
        }
        o.key.resize(w);
        o.adjusted.resize(w);
        p_here.resize(w);
        o.prob.resize(w);
        for (size_t i = 0; i < w; ++i) o.prob[i] = static_cast<float>(std::log10(p_here[i]));
        p_lin.swap(p_here);
        std::vector<CtxStat>().swap(o.ctx);
    }
    m.ord[static_cast<size_t>(N)].bo.clear();
}

/* ---- writing ---- */

struct Writer {
    std::vector<char> buf;
    uint64_t align() {
        while (buf.size() % 8) buf.push_back(0);
        return buf.size();
    }
    template <typename T>
    uint64_t put(const std::vector<T>& v) {
        const uint64_t off = align();
        const char* p = reinterpret_cast<const char*>(v.data());
        buf.insert(buf.end(), p, p + v.size() * sizeof(T));
        return off;
    }
};

struct Quant {
    std::vector<float> code;
    std::vector<uint8_t> q;
};

Quant quantise(const std::vector<float>& v, float floor) {
    float lo = 0, hi = floor;
    for (float x : v) {
        lo = std::min(lo, std::max(x, floor));
        hi = std::max(hi, x);
    }
    if (hi <= lo) hi = lo + 1;
    Quant out;
    out.code.resize(256);
    for (int i = 0; i < 256; ++i) out.code[static_cast<size_t>(i)] = lo + (hi - lo) * float(i) / 255.0f;
    out.q.resize(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        const float x = std::clamp(v[i], lo, hi);
        out.q[i] = static_cast<uint8_t>(std::lround((x - lo) / (hi - lo) * 255.0f));
    }
    return out;
}

std::vector<uint32_t> child_starts(const Order& parent, const Order& child, int k_parent) {
    std::vector<uint32_t> start(parent.key.size() + 1);
    size_t j = 0;
    for (size_t i = 0; i < parent.key.size(); ++i) {
        const uint64_t p = k_parent == 1 ? i : parent.key[i];
        while (j < child.key.size() && prefix(child.key[j]) < p) ++j;
        start[i] = static_cast<uint32_t>(j);
    }
    start[parent.key.size()] = static_cast<uint32_t>(child.key.size());
    return start;
}

void write(const Model& m) {
    const int N = m.opt.order;
    Writer w;
    Header h{};
    std::memcpy(h.magic, kMagic, sizeof(kMagic));
    h.order = static_cast<uint32_t>(N);
    h.vocab = static_cast<uint32_t>(m.words.size());
    h.unk_types = m.unk_types;
    w.buf.resize(sizeof(Header));

    std::vector<char> blob;
    std::vector<uint32_t> index(m.words.size());
    for (size_t i = 0; i < m.words.size(); ++i) {
        index[i] = static_cast<uint32_t>(blob.size());
        blob.insert(blob.end(), m.words[i].begin(), m.words[i].end());
        blob.push_back('\0');
    }
    std::vector<uint32_t> sorted(m.words.size());
    for (uint32_t i = 0; i < sorted.size(); ++i) sorted[i] = i;
    std::sort(sorted.begin(), sorted.end(),
              [&](uint32_t a, uint32_t b) { return m.words[a] < m.words[b]; });
    h.vocab_blob = w.put(blob);
    h.vocab_blob_size = blob.size();
    h.vocab_index = w.put(index);
    h.vocab_sorted = w.put(sorted);

    /* The shape index, in the order restoration used to build it: the
     * vocabulary's syllables by id (frequency), then attested ones it
     * lacks, in the list's order. */
    std::map<std::string, std::vector<uint32_t>> by_shape;
    std::vector<std::string> extras;
    for (uint32_t id = 3; m.opt.shape_index && id < m.words.size(); ++id) {
        const std::string s = xrnkey::syllable_shape(m.words[id]);
        if (!s.empty()) by_shape[s].push_back(id);
    }
    for (const std::string& w : xrnkey::lexicon_attested_words()) {
        if (!m.opt.shape_index || m.id_of.count(w)) continue;
        const std::string s = xrnkey::syllable_shape(w);
        if (s.empty()) continue;
        by_shape[s].push_back(static_cast<uint32_t>(m.words.size() + extras.size()));
        extras.push_back(w);
    }
    {
        std::vector<char> sblob, eblob;
        std::vector<uint32_t> skey, first{0}, cand, ekey;
        for (const auto& [shape, ids] : by_shape) {
            skey.push_back(static_cast<uint32_t>(sblob.size()));
            sblob.insert(sblob.end(), shape.begin(), shape.end());
            sblob.push_back('\0');
            cand.insert(cand.end(), ids.begin(), ids.end());
            first.push_back(static_cast<uint32_t>(cand.size()));
        }
        for (const std::string& e : extras) {
            ekey.push_back(static_cast<uint32_t>(eblob.size()));
            eblob.insert(eblob.end(), e.begin(), e.end());
            eblob.push_back('\0');
        }
        h.shape_count = skey.size();
        if (!skey.empty()) {
            h.shape_key = w.put(skey);
            h.shape_blob = w.put(sblob);
            h.shape_blob_size = sblob.size();
            h.shape_first = w.put(first);
            h.shape_cand = w.put(cand);
        }
        h.extra_count = ekey.size();
        if (!ekey.empty()) {
            h.extra_key = w.put(ekey);
            h.extra_blob = w.put(eblob);
            h.extra_blob_size = eblob.size();
        }
        std::cerr << "shape index: " << skey.size() << " shapes, " << cand.size() << " candidates, "
                  << extras.size() << " attested syllables outside the vocabulary\n";
    }

    const Order& uni = m.ord[1];
    h.count[1] = uni.key.size();
    h.uni_prob = w.put(uni.prob);
    std::vector<float> uni_bo = N > 1 ? uni.bo : std::vector<float>(uni.key.size(), 0.0f);
    h.uni_bo = w.put(uni_bo);
    if (N > 1) h.uni_child = w.put(child_starts(uni, m.ord[2], 1));

    for (int k = 2; k <= N; ++k) {
        const Order& o = m.ord[static_cast<size_t>(k)];
        OrderSection& s = h.sec[k];
        h.count[k] = o.key.size();
        Quant qp = quantise(o.prob, -12.0f);
        s.prob_code = w.put(qp.code);
        std::vector<uint16_t> word(o.key.size());
        for (size_t i = 0; i < o.key.size(); ++i) word[i] = static_cast<uint16_t>(last_word(o.key[i]));
        s.word = w.put(word);
        s.prob = w.put(qp.q);
        if (k < N) {
            Quant qb = quantise(o.bo, -12.0f);
            s.bo_code = w.put(qb.code);
            s.bo = w.put(qb.q);
            s.child = w.put(child_starts(o, m.ord[static_cast<size_t>(k + 1)], k));
        }
    }
    h.total_size = w.align();
    std::memcpy(w.buf.data(), &h, sizeof(h));

    const std::string tmp = m.opt.out + ".part";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) die("cannot write " + tmp);
        out.write(w.buf.data(), static_cast<std::streamsize>(w.buf.size()));
        if (!out) die("cannot write " + tmp);
    }
    if (std::rename(tmp.c_str(), m.opt.out.c_str()) != 0) die("cannot rename " + tmp);
    std::cerr << "wrote " << m.opt.out << " (" << w.buf.size() << " bytes";
    for (int k = 1; k <= N; ++k) std::cerr << (k == 1 ? "; " : " ") << h.count[k];
    std::cerr << " n-grams)\n";
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) die("missing value after " + a);
            return argv[++i];
        };
        if (a == "--order") o.order = std::atoi(next().c_str());
        else if (a == "--min-count") o.min_count = std::strtoull(next().c_str(), nullptr, 10);
        else if (a == "--max-vocab") o.max_vocab = static_cast<uint32_t>(std::strtoul(next().c_str(), nullptr, 10));
        else if (a == "--prune") {
            std::string v = next();
            size_t k = 2, pos = 0;
            while (pos <= v.size() && k <= kMaxOrder) {
                size_t comma = v.find(',', pos);
                if (comma == std::string::npos) comma = v.size();
                o.prune[k++] = static_cast<uint32_t>(std::strtoul(v.substr(pos, comma - pos).c_str(), nullptr, 10));
                pos = comma + 1;
            }
            for (; k <= kMaxOrder; ++k) o.prune[k] = o.prune[k - 1];
        } else if (a == "--no-shape-index") o.shape_index = false;
        else if (a == "-o") o.out = next();
        else if (!a.empty() && a[0] == '-') die("unknown option " + a);
        else o.corpora.push_back(a);
    }
    if (o.order < 1 || o.order > 4) die("--order must be 1..4 (ids are packed 16 bits each in 64)");
    if (o.max_vocab < 4 || o.max_vocab > kMaxVocab) die("--max-vocab must be 4..65536");
    for (size_t k = 3; k <= kMaxOrder; ++k)
        if (o.prune[k] < o.prune[k - 1]) die("--prune values must not decrease with the order");
    if (o.out.empty() || o.corpora.empty()) die("usage: xk-lm-build [options] -o OUT.xklm CORPUS...");
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    Model m;
    m.opt = parse(argc, argv);
    build_vocab(m);
    build_stream(m);
    count_and_smooth(m);
    std::vector<uint16_t>().swap(m.stream);
    probabilities(m);
    write(m);
    return 0;
}
