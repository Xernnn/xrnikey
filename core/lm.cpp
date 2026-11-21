/* Reads an .xklm language model (lm_format.h) in place, from a memory
 * mapping of the file, or of part of one (a model stored inside an APK). */
#include "lm_internal.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace xrnkey {

using namespace lmfmt;

namespace {

template <typename T>
const T* at(const Lm& m, uint64_t off) {
    return reinterpret_cast<const T*>(m.base + off);
}

/* [off, off + n * size) lies inside the model and is aligned for T. */
template <typename T>
bool fits(const Header& h, uint64_t off, uint64_t n) {
    if (off % alignof(T) != 0) return false;
    if (off > h.total_size) return false;
    if (n > (h.total_size - off) / sizeof(T)) return false;
    return true;
}

bool check(const Lm& m) {
    const Header& h = *m.hdr;
    if (std::memcmp(h.magic, kMagic, sizeof(kMagic)) != 0) return false;
    if (h.order < 1 || h.order > kMaxOrder) return false;
    if (h.vocab < 3 || h.vocab > kMaxVocab) return false;
    if (h.count[1] != h.vocab) return false;
    if (h.total_size > m.size || h.total_size < sizeof(Header)) return false;
    if (!fits<char>(h, h.vocab_blob, h.vocab_blob_size) || h.vocab_blob_size == 0) return false;
    if (m.base[h.vocab_blob + h.vocab_blob_size - 1] != '\0') return false;
    if (!fits<uint32_t>(h, h.vocab_index, h.vocab)) return false;
    if (!fits<uint32_t>(h, h.vocab_sorted, h.vocab)) return false;
    if (!fits<float>(h, h.uni_prob, h.vocab) || !fits<float>(h, h.uni_bo, h.vocab)) return false;
    if (h.order > 1 && !fits<uint32_t>(h, h.uni_child, uint64_t{h.vocab} + 1)) return false;
    for (uint32_t k = 2; k <= h.order; ++k) {
        const OrderSection& s = h.sec[k];
        const uint64_t n = h.count[k];
        if (!fits<float>(h, s.prob_code, 256) || !fits<uint16_t>(h, s.word, n) ||
            !fits<uint8_t>(h, s.prob, n))
            return false;
        if (k < h.order &&
            (!fits<float>(h, s.bo_code, 256) || !fits<uint8_t>(h, s.bo, n) ||
             !fits<uint32_t>(h, s.child, n + 1)))
            return false;
    }
    const uint32_t* index = at<uint32_t>(m, h.vocab_index);
    const uint32_t* sorted = at<uint32_t>(m, h.vocab_sorted);
    for (uint32_t i = 0; i < h.vocab; ++i)
        if (index[i] >= h.vocab_blob_size || sorted[i] >= h.vocab) return false;
    return true;
}

/* The children of entry `idx` of order k, as a range of order k+1,
 * clamped so a damaged file can never send a read outside the model. */
void children(const Lm& m, uint32_t k, uint64_t idx, uint64_t* lo, uint64_t* hi) {
    const Header& h = *m.hdr;
    const uint32_t* child = k == 1 ? at<uint32_t>(m, h.uni_child) : at<uint32_t>(m, h.sec[k].child);
    const uint64_t n = h.count[k + 1];
    *lo = std::min<uint64_t>(child[idx], n);
    *hi = std::min<uint64_t>(child[idx + 1], n);
    if (*hi < *lo) *hi = *lo;
}

}  // namespace

Lm* lm_map(int fd, uint64_t offset, uint64_t length) {
    if (fd < 0 || length < sizeof(Header)) return nullptr;
    /* mmap wants a page-aligned file offset: map from the page the model
     * starts in, and step over the bytes before it. */
    const long page = sysconf(_SC_PAGESIZE);
    const uint64_t aligned = offset - offset % static_cast<uint64_t>(page > 0 ? page : 4096);
    const uint64_t lead = offset - aligned;
    void* p = mmap(nullptr, length + lead, PROT_READ, MAP_PRIVATE, fd, static_cast<off_t>(aligned));
    if (p == MAP_FAILED) return nullptr;
    auto* m = new Lm{};
    m->map = p;
    m->map_size = length + lead;
    m->base = static_cast<const char*>(p) + lead;
    m->size = length;
    m->hdr = reinterpret_cast<const Header*>(m->base);
    if (reinterpret_cast<uintptr_t>(m->base) % alignof(Header) != 0 || !check(*m)) {
        lm_unmap(m);
        return nullptr;
    }
    return m;
}

void lm_unmap(Lm* m) {
    if (!m) return;
    if (m->map) munmap(m->map, m->map_size);
    shape_index_free(m->shapes);
    delete m;
}

const char* lm_word(const Lm& m, uint32_t id) {
    if (id >= m.hdr->vocab) return nullptr;
    return m.base + m.hdr->vocab_blob + at<uint32_t>(m, m.hdr->vocab_index)[id];
}

uint32_t lm_id(const Lm& m, std::string_view word) {
    const uint32_t* sorted = at<uint32_t>(m, m.hdr->vocab_sorted);
    uint32_t lo = 0, hi = m.hdr->vocab;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const char* w = lm_word(m, sorted[mid]);
        const int c = std::string_view(w).compare(word);
        if (c == 0) return sorted[mid];
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    return kUnk;
}

int64_t lm_find(const Lm& m, const uint32_t* ids, int k) {
    const Header& h = *m.hdr;
    if (k < 1 || static_cast<uint32_t>(k) > h.order || ids[0] >= h.vocab) return -1;
    uint64_t idx = ids[0];
    for (int j = 2; j <= k; ++j) {
        uint64_t lo, hi;
        children(m, static_cast<uint32_t>(j - 1), idx, &lo, &hi);
        const uint16_t* word = at<uint16_t>(m, h.sec[j].word);
        const uint32_t want = ids[j - 1];
        const uint64_t end = hi;
        while (lo < hi) {
            const uint64_t mid = lo + (hi - lo) / 2;
            if (word[mid] < want) lo = mid + 1;
            else hi = mid;
        }
        /* Inside this entry's own children only: an empty range must not
         * match the first child of the next entry. */
        if (lo >= end || word[lo] != want) return -1;
        idx = lo;
    }
    return static_cast<int64_t>(idx);
}

float lm_prob_at(const Lm& m, int k, uint64_t idx) {
    const Header& h = *m.hdr;
    if (k == 1) return at<float>(m, h.uni_prob)[idx];
    return at<float>(m, h.sec[k].prob_code)[at<uint8_t>(m, h.sec[k].prob)[idx]];
}

float lm_bo_at(const Lm& m, int k, uint64_t idx) {
    const Header& h = *m.hdr;
    if (static_cast<uint32_t>(k) >= h.order) return 0.0f;
    if (k == 1) return at<float>(m, h.uni_bo)[idx];
    return at<float>(m, h.sec[k].bo_code)[at<uint8_t>(m, h.sec[k].bo)[idx]];
}

float lm_logprob(const Lm& m, const uint32_t* ctx, int n, uint32_t w) {
    const int order = static_cast<int>(m.hdr->order);
    if (w >= m.hdr->vocab) w = kUnk;
    if (n > order - 1) {
        ctx += n - (order - 1);
        n = order - 1;
    }
    uint32_t gram[kMaxOrder];
    float backoff = 0.0f;
    for (int s = 0; s <= n; ++s) {
        const int len = n - s;
        for (int i = 0; i < len; ++i) gram[i] = ctx[s + i] < m.hdr->vocab ? ctx[s + i] : kUnk;
        gram[len] = w;
        const int64_t g = lm_find(m, gram, len + 1);
        if (g >= 0) return backoff + lm_prob_at(m, len + 1, static_cast<uint64_t>(g));
        if (len > 0) {
            const int64_t c = lm_find(m, gram, len);
            if (c >= 0) backoff += lm_bo_at(m, len, static_cast<uint64_t>(c));
        }
    }
    return backoff + lm_prob_at(m, 1, kUnk);  /* not reached: every id is a unigram */
}

float lm_unk_share(const Lm& m) {
    return static_cast<float>(std::log10(static_cast<double>(std::max<uint64_t>(m.hdr->unk_types, 1))));
}

}  // namespace xrnkey

using xrnkey::Lm;

extern "C" {

XKLm* xk_lm_open(const char* path) {
    if (!path) return nullptr;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return nullptr;
    struct stat st;
    XKLm* lm = nullptr;
    if (fstat(fd, &st) == 0 && st.st_size > 0)
        lm = xk_lm_open_fd(fd, 0, static_cast<int64_t>(st.st_size));
    close(fd);  /* the mapping outlives the descriptor */
    return lm;
}

XKLm* xk_lm_open_fd(int fd, int64_t offset, int64_t length) {
    if (offset < 0 || length <= 0) return nullptr;
    return reinterpret_cast<XKLm*>(
        xrnkey::lm_map(fd, static_cast<uint64_t>(offset), static_cast<uint64_t>(length)));
}

void xk_lm_free(XKLm* lm) {
    xrnkey::lm_unmap(reinterpret_cast<Lm*>(lm));
}

int xk_lm_order(const XKLm* lm) {
    return lm ? static_cast<int>(reinterpret_cast<const Lm*>(lm)->hdr->order) : 0;
}

int xk_lm_vocab_size(const XKLm* lm) {
    return lm ? static_cast<int>(reinterpret_cast<const Lm*>(lm)->hdr->vocab) : 0;
}

const char* xk_lm_word(const XKLm* lm, int id) {
    if (!lm || id < 0) return nullptr;
    return xrnkey::lm_word(*reinterpret_cast<const Lm*>(lm), static_cast<uint32_t>(id));
}

int xk_lm_id(const XKLm* lm, const char* word) {
    if (!lm || !word) return 0;
    return static_cast<int>(xrnkey::lm_id(*reinterpret_cast<const Lm*>(lm), word));
}

float xk_lm_logprob(const XKLm* lm, const char* const* context, int ctx_len, const char* word) {
    if (!lm || !word) return xrnkey::lmfmt::kNever;
    const Lm& m = *reinterpret_cast<const Lm*>(lm);
    if (ctx_len < 0 || (ctx_len > 0 && !context)) ctx_len = 0;
    /* Only the last order-1 words of the context can matter. */
    const int keep = std::min(ctx_len, static_cast<int>(m.hdr->order) - 1);
    uint32_t ids[xrnkey::lmfmt::kMaxOrder];
    for (int i = 0; i < keep; ++i) {
        const char* c = context[ctx_len - keep + i];
        ids[i] = c ? xrnkey::lm_id(m, c) : xrnkey::lmfmt::kUnk;
    }
    const uint32_t id = xrnkey::lm_id(m, word);
    float p = xrnkey::lm_logprob(m, ids, keep, id);
    /* One particular unknown word gets its share of the unknown mass; the
     * token "<unk>" itself is all of it. */
    if (id == xrnkey::lmfmt::kUnk && std::strcmp(word, "<unk>") != 0) p -= xrnkey::lm_unk_share(m);
    return p;
}

}  /* extern "C" */
